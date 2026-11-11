window.testResult = (async () => {
    const assert = (condition, message) => { if (!condition) throw new Error(message); };
    const rejects = async (action, message) => {
        let rejected = false;
        try { await action(); } catch { rejected = true; }
        assert(rejected, message);
    };
    const nativeFetch = window.fetch;
    let version = 1;
    let offline = false;
    let failImage = null;
    const notices = [];
    const warnings = [];
    window.confirm = () => { throw new Error('Storage must never prompt'); };
    window.fetch = async (source, options = {}) => {
        if (!String(source).startsWith('fixture:')) return nativeFetch(source, options);
        if (offline || source === failImage) throw new Error('Simulated network failure');
        return new Response(options.method === 'HEAD' ? null : new Uint8Array([version, 2, 3]),
            {headers: {ETag: `"version-${version}"`}});
    };
    const images = ['romdisk', 'eeprom', 'tf'].map(name =>
        ({name, source: `fixture:${name}`, key: name, path: `/roms/${name}.img`}));
    const makeStore = storage => new DiskImageStore({storage, images: structuredClone(images),
        onWarning: (...args) => warnings.push(args), onUpdateAvailable: update => notices.push(update)});
    const store = makeStore('opfs');
    const read = async image => new Uint8Array(await (await
        (await store.directory.getFileHandle(store.manifest.images[image.id].file)).getFile()).arrayBuffer());
    await store.loadAll();
    assert((await read(store.images[0]))[0] === 1, 'First boot downloads images');
    const second = makeStore('opfs');
    await rejects(() => second.open(), 'Second writer must fail');
    offline = true;
    await store.loadAll();
    assert((await store.checkUpdates()).length === 0, 'Offline boot retains images');
    offline = false;
    const localFile = store.manifest.images[store.images[0].id].file;
    await store.writeFile(localFile, new Uint8Array([99, 2, 3]));
    version = 2;
    const updates = await store.checkUpdates();
    assert(updates.length === 3 && notices.length === 3, 'Every changed image is reported');
    assert((await read(store.images[0]))[0] === 99, 'Notification preserves local modifications');
    const before = JSON.stringify(store.manifest);
    failImage = 'fixture:tf';
    await rejects(() => store.stageUpdates(['eeprom', 'tf']), 'Failed download rejects update');
    assert(JSON.stringify(store.manifest) === before, 'Failed staging preserves manifest');
    failImage = null;
    const staged = await store.stageUpdates(['romdisk']);
    const write = store.writeFile.bind(store);
    store.writeFile = async (name, data) => {
        if (name === 'manifest.json') throw new DOMException('Quota exceeded', 'QuotaExceededError');
        return write(name, data);
    };
    await rejects(() => store.commit(staged), 'Failed activation rejects update');
    assert(JSON.stringify(store.manifest) === before, 'Failed activation preserves active images');
    assert((await read(store.images[0]))[0] === 99, 'Failed activation preserves local bytes');
    store.writeFile = write;
    await store.removeFiles(staged.files);
    await store.commit(await store.stageUpdates(['romdisk']));
    assert((await read(store.images[0]))[0] === 2, 'Explicit update installs selected remote');
    assert((await read(store.images[1]))[0] === 1, 'Unselected image stays local');
    await store.close();
    await store.open();
    assert((await read(store.images[0]))[0] === 2, 'Manifest survives reopen');
    await store.clear();
    await store.close();
    for (const backend of ['indexeddb', 'memory']) {
        const fallback = makeStore(backend);
        const oldDatabase = new IndexedDBStore();
        await oldDatabase.put('legacy-sentinel', new Uint8Array([42]));
        const loaded = await fallback.loadAll();
        assert(loaded.tf[0] === 2, `${backend} loads remote images`);
        await fallback.persist({readFile: () => new Uint8Array([88])});
        assert((await fallback.loadAll()).tf[0] === 88, `${backend} saves working images`);
        await fallback.clear();
        assert((await oldDatabase.get('legacy-sentinel'))[0] === 42, 'Legacy saves are untouched');
        await oldDatabase.delete('legacy-sentinel');
        await fallback.close();
    }
    window.fetch = nativeFetch;
    return 'PASS: OPFS/IndexedDB/memory, notifications, offline boot, locks, failed download/commit, explicit replacement';
})();
window.testResult.then(result => { document.body.textContent = result; }, error => {
    document.body.textContent = error.stack;
});
