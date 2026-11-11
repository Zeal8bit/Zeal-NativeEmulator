window.testResult = (async () => {
    const assert = (condition, message) => { if (!condition) throw new Error(message); };
    const deferred = () => {
        let resolve;
        const promise = new Promise(done => { resolve = done; });
        return {promise, resolve};
    };
    const observe = promise => promise.then(value => ({value}), error => ({error}));
    const rejected = async promise => {
        const result = await observe(promise);
        assert('error' in result, 'Expected operation to reject');
        return result.error;
    };
    const make = options => new ZealNative({canvas: document.querySelector('canvas'),
        romdisk: '/fixture/default.img', eeprom: '/fixture/eeprom.img', tf: '/fixture/tf.img',
        print: () => {}, printErr: () => {}, ...options});
    const assertLocked = async () => {
        let factoryCalls = 0;
        const contender = make({moduleFactory: async () => { factoryCalls++; }});
        const error = await rejected(contender.start());
        assert(error.message.includes('already in use'), 'Second instance must be refused storage ownership');
        assert(factoryCalls === 0, 'Second runtime must not be created while storage is owned');
    };
    const assertAvailable = async () => {
        const contender = make();
        await contender.diskImages.open();
        await contender.diskImages.close();
    };

    // Seed once, then prove every failure path preserves both image identity and bytes.
    const seed = make();
    await seed.diskImages.loadAll();
    const directory = seed.diskImages.directory;
    const snapshot = async () => {
        const manifest = await (await (await directory.getFileHandle('manifest.json')).getFile()).text();
        const hashes = [];
        for (const {file} of Object.values(JSON.parse(manifest).images)) {
            const data = await (await (await directory.getFileHandle(file)).getFile()).arrayBuffer();
            hashes.push(Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', data))));
        }
        return JSON.stringify({manifest, hashes});
    };
    const originalImages = await snapshot();
    await seed.diskImages.close();

    window.startupStage = 'throwing ready callback with delayed exit';
    const startupError = new Error('Embedding ready callback failed');
    const exitRequested = deferred();
    const events = [];
    let releaseExit;
    let created;
    const owner = make({
        onRuntimeInitialized(module) {
            created = module;
            const exit = module._zeal_exit_web;
            module._zeal_exit_web = () => {
                releaseExit = () => exit.call(module);
                exitRequested.resolve();
            };
            throw startupError;
        },
        onExit() { events.push('stopped'); },
    });
    const close = owner.diskImages.close.bind(owner.diskImages);
    owner.diskImages.close = async () => {
        assert(events.includes('stopped'), 'Runtime must stop before store close');
        assert(owner.module === null && owner.exitPromise === null, 'Stopped runtime references must be cleared');
        await assertLocked();
        events.push('closing');
        await close();
        events.push('released');
    };
    const startup = observe(owner.start());
    await exitRequested.promise;
    assert(owner.module === created, 'Runtime must remain tracked during teardown');
    assert(!events.length, 'Exit has been requested but not completed');
    await assertLocked();
    releaseExit();
    assert((await startup).error === startupError, 'Successful cleanup must preserve the original startup error');
    assert(events.join(',') === 'stopped,closing,released', 'Lock release must follow completed teardown');
    assert(owner.module === null, 'Failed startup must not leave a running module reference');
    await assertAvailable();
    assert(await snapshot() === originalImages, 'Successful cleanup must preserve images');

    // Retrying start creates a new runtime rather than returning the failed one.
    owner.onRuntimeInitialized = null;
    const restarted = await owner.start();
    assert(restarted !== created, 'Subsequent start must not return the stopped runtime');
    await owner.exit();

    window.startupStage = 'failed teardown retains ownership';
    const cleanupError = new Error('Orderly exit request failed');
    const reported = [];
    let restoreExit;
    let failedRuntime;
    let stopped = false;
    const failed = make({
        onRuntimeInitialized(module) {
            failedRuntime = module;
            const exit = module._zeal_exit_web;
            restoreExit = () => { module._zeal_exit_web = exit; };
            module._zeal_exit_web = () => { throw cleanupError; };
            throw startupError;
        },
        onExit() { stopped = true; },
        onStorageError: error => reported.push(error),
    });
    let closes = 0;
    const failedClose = failed.diskImages.close.bind(failed.diskImages);
    failed.diskImages.close = async () => { closes++; await failedClose(); };
    const failure = await rejected(failed.start());
    assert(failure instanceof AggregateError, 'Both startup and cleanup failures must be surfaced');
    assert(failure.cause === startupError && failure.errors[0] === startupError &&
        failure.errors[1] === cleanupError, 'Both original error objects must be preserved');
    assert(reported.includes(failure), 'Cleanup failure must reach the storage error observer');
    assert(failure.message.includes(startupError.message) && failure.message.includes(cleanupError.message),
        'Default diagnostic must include both failures');
    assert(closes === 0 && !stopped, 'Failed teardown must not release storage');
    assert(failed.module === failedRuntime && failed.exitPromise, 'Failed runtime must remain tracked for cleanup');
    assert(failed.storageRequest === null, 'Rejected exit request must not leave a pending request');
    assert(await rejected(failed.start()) === failure, 'Start must not return an orphaned runtime');
    assert(await rejected(failed.applyImageUpdates([])) === failure, 'Empty update must not return an orphaned runtime');
    await assertLocked();
    assert(await snapshot() === originalImages, 'Failed cleanup must preserve persisted images');
    restoreExit();
    await failed.exit();
    assert(stopped && closes === 1 && failed.module === null, 'Explicit cleanup retry must stop before releasing');
    await assertAvailable();

    window.startupStage = 'failures before runtime creation';
    for (const phase of ['assets', 'factory']) {
        const error = new Error(`${phase} startup failed`);
        let factoryCalls = 0;
        const early = make({moduleFactory: async () => { factoryCalls++; throw error; }});
        if (phase === 'assets') early.resolveBinary = async () => { throw error; };
        assert(await rejected(early.start()) === error, `${phase}: original error must be preserved`);
        assert(factoryCalls === (phase === 'assets' ? 0 : 1), `${phase}: factory creation count`);
        assert(early.module === null && !early.diskImages.releaseLock,
            `${phase}: no runtime or storage ownership may remain`);
        await assertAvailable();
        assert(await snapshot() === originalImages, `${phase}: image contents must remain unchanged`);
    }

    // Factory rejection can also happen after a runtime exists but before ready.
    // Such a runtime cannot service queued exit requests: retain ownership until
    // its exit is actually observed instead of treating rejection as termination.
    window.startupStage = 'factory failure after runtime creation';
    let options;
    const runtime = {FS: {mkdirTree() {}}, _zeal_exit_web() { throw new Error('Unready runtime cannot stop'); }};
    const partial = make({moduleFactory: async incoming => {
        options = incoming;
        incoming.onRuntimeInitialized.call(runtime);
        throw startupError;
    }});
    const partialError = await rejected(partial.start());
    assert(partialError.cause === startupError, 'Partial creation must preserve startup error');
    assert(partial.module === runtime, 'Partial runtime must remain tracked');
    await assertLocked();
    options.onExit.call(runtime, 1);
    await partial.exit();
    assert(partial.module === null, 'Observed exit must clear partial runtime');
    await assertAvailable();
    assert(await snapshot() === originalImages, 'Error recovery must never reset, replace, or delete images');
    return 'PASS: startup error identity, orderly teardown before lock release, failed teardown ownership, pre-runtime failures, image preservation';
})();
window.testResult = Promise.race([window.testResult, new Promise((_, reject) => setTimeout(() =>
    reject(new Error(`Startup cleanup test timed out at ${window.startupStage}`)), 30000))]);
window.testResult.catch(error => { document.body.append(error.stack); });
