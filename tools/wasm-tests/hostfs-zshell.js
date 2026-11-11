window.zshellTrace = [];
window.zshellErrors = [];
window.zshellLogs = [];
window.zshellKeys = [];
window.addEventListener('keydown', event => zshellKeys.push({key: event.key, keyCode: event.keyCode, target: event.target.id}));
window.zshellReady = (async () => {
    const directory = await (await navigator.storage.getDirectory())
        .getDirectoryHandle('hostfs-zshell', {create: true});
    const writer = await (await directory.getFileHandle('sample.txt', {create: true})).createWritable();
    await writer.write('HostFS guest round trip\n');
    await writer.close();
    await directory.getDirectoryHandle('bin', {create: true});
    const originalError = console.error;
    console.error = (...args) => { zshellErrors.push(args.map(String)); originalError(...args); };
    const hostfs = new HostFS(directory, {onError: (operation, error) => zshellErrors.push({operation, error: error.message})});
    let active;
    const start = hostfs.start.bind(hostfs);
    hostfs.start = request => { active = request; start(request); };
    const complete = hostfs.complete.bind(hostfs);
    hostfs.complete = (status, registers) => {
        zshellTrace.push({operation: active.operation, path: active.path, status});
        complete(status, registers);
    };
    window.emulator = new ZealNative({canvas: document.querySelector('canvas'),
        romdisk: '/fixture/zshell.img', imageStorage: 'memory', storageNamespace: 'hostfs-zshell-test', hostfs,
        print: message => zshellLogs.push(message), printErr: message => zshellLogs.push(message)});
    await emulator.start();
    window.zshellVerify = async () => {
        if (zshellErrors.length) throw new Error(`Spurious HostFS diagnostics: ${JSON.stringify(zshellErrors)}`);
        if (!zshellTrace.some(entry => entry.operation === 6 && entry.status === 0) ||
            !zshellTrace.some(entry => entry.operation === 7 && entry.status === 21)) {
            throw new Error(`ZShell directory listing did not complete: ${JSON.stringify({trace: zshellTrace, keys: zshellKeys, logs: zshellLogs})}`);
        }
        if (!zshellTrace.some(entry => entry.operation === 1 && entry.status === 4)) {
            throw new Error(`ZShell missing-file probes did not reach the guest: ${JSON.stringify({trace: zshellTrace, keys: zshellKeys, logs: zshellLogs})}`);
        }
        const copied = await (await (await directory.getFileHandle('copied.txt')).getFile()).text();
        if (copied !== 'HostFS guest round trip\n') throw new Error('ZShell HostFS copy did not persist');
        await emulator.exit();
        return 'PASS: ZShell boot, H: listing, missing executable/file probes, and guest read/write copy without HostFS errors';
    };
})();
window.zshellReady.catch(error => { document.body.append(error.stack); });
