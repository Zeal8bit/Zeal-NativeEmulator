window.testResult = (async () => {
    const assert = (condition, message) => { if (!condition) throw new Error(message); };
    const frame = async path => {
        const element = document.createElement('iframe');
        const loaded = new Promise(resolve => element.onload = resolve);
        element.src = path;
        document.body.append(element);
        await loaded;
        assert(typeof element.contentWindow.makeEmulator === 'function', 'Chapter frame loaded');
        return element.contentWindow;
    };
    const one = await frame('/zeal-pascal/chapter-one/');
    const two = await frame('/zeal-pascal/chapter-two/');
    assert(one.location.href !== two.location.href, 'Use genuinely different chapter URLs');
    const digest = async value => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',
        new TextEncoder().encode(value))), byte => byte.toString(16).padStart(2, '0')).join('');
    const write = async (directory, name, data) => {
        const writer = await (await directory.getFileHandle(name, {create: true})).createWritable();
        await writer.write(data);
        await writer.close();
    };
    const read = async store => {
        const results = {};
        for (const image of store.images) {
            const file = store.manifest.images[image.id].file;
            const data = store.storage === 'opfs'
                ? new Uint8Array(await (await (await store.directory.getFileHandle(file)).getFile()).arrayBuffer())
                : await store.database.get(file);
            results[image.name] = Array.from(data);
        }
        return results;
    };
    const modify = async (store, marker) => {
        if (store.storage === 'opfs') {
            for (const image of store.images) {
                await write(store.directory, store.manifest.images[image.id].file, new Uint8Array([marker]));
            }
        } else {
            await store.persist({readFile: () => new Uint8Array([marker])});
        }
    };
    const locked = async store => {
        let error;
        try { await store.open(); } catch (caught) { error = caught; }
        assert(error?.message.includes('already in use'), 'Same workspace rejects the second chapter writer');
    };
    for (const imageStorage of ['opfs', 'indexeddb']) {
        const make = (chapter, storageNamespace) => chapter.makeEmulator({imageStorage, storageNamespace}).diskImages;
        const first = make(one, '/zeal-pascal/');
        await first.loadAll();
        assert(first.storageNamespace === '/zeal-pascal/', 'ZealNative passes the initialization option through');
        assert(/^[a-f0-9]{64}$/.test(first.namespace), 'Filesystem namespace must be a hash');
        assert(first.images.every(image => image.key.startsWith('book-')), 'Image keys are independent of workspace');
        const identity = JSON.stringify(first.manifest);
        await modify(first, 71);
        const expected = await read(first);
        const second = make(two, '/zeal-pascal/');
        await locked(second);

        const other = make(two, '/other-project/');
        await other.loadAll(); // Must acquire a different lock while the book is running.
        assert(other.namespace !== first.namespace, 'Different projects have different namespace hashes');
        assert(JSON.stringify(other.manifest) !== identity, 'Different projects have separate manifests and files');
        assert((await read(other)).romdisk[0] !== 71, 'Different project starts from its own source image');
        await modify(other, 93);
        assert(JSON.stringify(await read(first)) === JSON.stringify(expected), 'Other workspace writes cannot change book disks');
        if (imageStorage === 'opfs') {
            assert(first.images.every(image => image.path.startsWith(`/opfs/zeal-native/${first.namespace}/`)),
                'OPFS image paths belong to the selected hashed workspace');
            assert(other.images.every(image => image.path.startsWith(`/opfs/zeal-native/${other.namespace}/`)),
                'Other OPFS images have separate paths');
        } else {
            assert(JSON.stringify(await first.database.get(`${first.namespace}:manifest`)) === identity,
                'IndexedDB metadata is keyed by workspace');
            assert(JSON.stringify(await other.database.get(`${other.namespace}:manifest`)) !== identity,
                'Other workspace metadata is independent');
        }
        await other.close();
        await first.close();
        await second.loadAll();
        assert(second.namespace === first.namespace, 'Different chapters resolve the same explicit workspace');
        assert(JSON.stringify(second.manifest) === identity, 'Chapter reuses persisted image files and remote metadata');
        assert(JSON.stringify(await read(second)) === JSON.stringify(expected), 'Chapter sees the persisted disk changes');
        const reopenedOther = make(one, '/other-project/');
        await reopenedOther.loadAll();
        assert((await read(reopenedOther)).romdisk[0] === 93, 'Switching workspaces never resets or migrates images');
        await reopenedOther.close();

        // Seed the old on-disk format independently of open() to prove the exact
        // historical hash, path and metadata key remain readable without migration.
        const legacyHash = await digest(new URL('.', one.document.baseURI).href);
        const legacyManifest = {version: 1, images: {}};
        let directory;
        const database = new one.IndexedDBStore();
        if (imageStorage === 'opfs') {
            const app = await (await navigator.storage.getDirectory()).getDirectoryHandle('zeal-native', {create: true});
            directory = await app.getDirectoryHandle(legacyHash, {create: true});
        }
        for (const key of ['book-rom', 'book-eeprom', 'book-tf']) {
            const file = `${crypto.randomUUID()}.img`;
            legacyManifest.images[await digest(key)] = {file, etag: '"legacy"', lastModified: null};
            if (directory) await write(directory, file, new Uint8Array([52]));
            else await database.put(file, new Uint8Array([52]));
        }
        if (directory) await write(directory, 'manifest.json', JSON.stringify(legacyManifest));
        else await database.put(`${legacyHash}:manifest`, legacyManifest);
        const legacy = one.makeEmulator({imageStorage}).diskImages;
        await legacy.loadAll();
        assert(legacy.namespace === legacyHash, 'Omitted namespace uses the exact legacy page-directory hash');
        assert(JSON.stringify(legacy.manifest) === JSON.stringify(legacyManifest), 'Legacy records are not rewritten');
        assert((await read(legacy)).tf[0] === 52, 'Existing legacy images load without migration');
        const legacyOtherChapter = two.makeEmulator({imageStorage}).diskImages;
        await legacyOtherChapter.open();
        assert(legacyOtherChapter.namespace === await digest(new URL('.', two.document.baseURI).href) &&
            legacyOtherChapter.namespace !== legacyHash, 'Default chapter isolation is preserved');
        await legacyOtherChapter.close();
        const namedLikeLegacy = make(one, new URL('.', one.document.baseURI).href);
        await namedLikeLegacy.open();
        assert(namedLikeLegacy.namespace !== legacyHash, 'Explicit names do not accidentally alias legacy locations');
        await namedLikeLegacy.close();

        // Validation runs at initialization, before acquiring a lock or touching storage.
        const before = JSON.stringify(await read(second));
        for (const invalid of [null, '', ' \t\n', false, 0, NaN, {}, [], new one.String('/zeal-pascal/')]) {
            for (const construct of [
                () => one.makeEmulator({imageStorage, storageNamespace: invalid}),
                () => new one.DiskImageStore({storage: imageStorage, storageNamespace: invalid}),
            ]) {
                let error;
                try { construct(); } catch (caught) { error = caught; }
                assert(error?.name === 'TypeError' && error.message.includes('storageNamespace'),
                    'Invalid namespace fails clearly during initialization');
            }
        }
        assert(JSON.stringify(await read(second)) === before && JSON.stringify(second.manifest) === identity,
            'Invalid namespaces cannot change images or metadata');
        await locked(make(one, '/zeal-pascal/'));
        assert((await read(legacy)).tf[0] === 52, 'Invalid namespaces leave legacy disks intact');
        await legacy.close();
        await second.close();
    }
    // Memory storage remains per instance, but namespace selection and writer
    // exclusion still use the same rules as the persistent backends.
    const memory = one.makeEmulator({imageStorage: 'memory', storageNamespace: '/zeal-pascal/'}).diskImages;
    await memory.open();
    await locked(two.makeEmulator({imageStorage: 'memory', storageNamespace: '/zeal-pascal/'}).diskImages);
    const otherMemory = two.makeEmulator({imageStorage: 'memory', storageNamespace: '/other-project/'}).diskImages;
    await otherMemory.open();
    assert(otherMemory.namespace !== memory.namespace, 'Memory backend lock namespaces remain isolated');
    await otherMemory.close();
    await memory.close();
    return 'PASS: cross-chapter shared disks and locks, namespace isolation, exact legacy locations, validation without data changes, all backends';
})();
window.testResult.catch(error => { document.body.append(error.stack); });
