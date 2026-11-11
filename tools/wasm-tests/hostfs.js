window.testResult = (async () => {
    const assert = (condition, message) => { if (!condition) throw new Error(message); };
    const OP = {OPEN: 1, STAT: 2, READ: 3, WRITE: 4, CLOSE: 5, OPENDIR: 6, READDIR: 7, MKDIR: 8, RM: 9};
    const diagnostics = [];
    const notifications = [];
    const originalError = console.error;
    console.error = (...args) => diagnostics.push(args);
    const root = await (await navigator.storage.getDirectory()).getDirectoryHandle('hostfs-classification', {create: true});
    const put = async (directory, name, text) => {
        const writer = await (await directory.getFileHandle(name, {create: true})).createWritable();
        await writer.write(text);
        await writer.close();
    };
    const proxy = (directory, overrides) => ({
        getDirectoryHandle: directory.getDirectoryHandle.bind(directory),
        getFileHandle: directory.getFileHandle.bind(directory),
        values: directory.values.bind(directory),
        queryPermission: directory.queryPermission.bind(directory),
        ...overrides,
    });
    let host;
    let completion;
    const memory = new Uint8Array(65536);
    let allocations = 0;
    let frees = 0;
    const module = {
        HEAPU8: new Uint8Array(131072),
        _malloc: () => { allocations++; return 65536; },
        _free: () => { frees++; },
        _hostfs_web_write_guest: (address, pointer, length) => memory.set(module.HEAPU8.slice(pointer, pointer + length), address),
        _hostfs_web_complete: (status, ...registers) => completion({status, registers}),
    };
    const attach = directory => {
        host = new HostFS(directory, {onError: (operation, error) => notifications.push({operation, error})});
        host.bindModule(module);
    };
    const invoke = (operation, fields = {}) => new Promise(resolve => {
        completion = resolve;
        host.start({operation, flags: 0, descriptor: 255, offset: 0, length: 8,
            guestAddress: 256, ...fields});
    });
    const quiet = async (operation, fields, status) => {
        const before = diagnostics.length;
        const callbacks = notifications.length;
        const result = await invoke(operation, fields);
        assert(result.status === status, `Operation ${operation}: expected guest status ${status}, got ${result.status}`);
        assert(diagnostics.length === before && notifications.length === callbacks,
            `Operation ${operation}: normal guest outcome must not notify the embedding page`);
        return result;
    };
    const unexpected = async (operation, fields) => {
        const before = diagnostics.length;
        const callbacks = notifications.length;
        const result = await invoke(operation, fields);
        assert(result.status === 1, 'Infrastructure failures must return guest failure, not missing-entry');
        assert(diagnostics.length === before + 1 && notifications.length === callbacks + 1,
            'Infrastructure failure must retain console and consumer diagnostics');
        return notifications.at(-1).error;
    };
    try {
        await put(root, 'file.txt', 'hello');
        const folder = await root.getDirectoryHandle('folder', {create: true});
        await put(folder, 'child.txt', 'child');
        attach(root);
        for (const path of ['missing', '/bin/missing', 'H:/missing', '/file.txt/missing']) {
            await quiet(OP.OPEN, {path}, 4);
            await quiet(OP.OPENDIR, {path}, 4);
        }
        await quiet(OP.OPENDIR, {path: 'file.txt'}, 4);
        for (const path of ['../outside', 'H:relative', 'A:/file', 'bad\\name', 'bad\0name']) {
            for (const operation of [OP.OPEN, OP.OPENDIR, OP.MKDIR, OP.RM]) await quiet(operation, {path}, 1);
        }
        await quiet(OP.OPEN, {path: 'file.txt', flags: 3}, 1);
        await quiet(OP.STAT, {descriptor: 255}, 4);
        for (const operation of [OP.READ, OP.WRITE, OP.READDIR, OP.CLOSE]) await quiet(operation, {descriptor: 255}, 1);

        const opened = await quiet(OP.OPEN, {path: 'file.txt', flags: 2}, 0);
        const fd = opened.registers[4];
        assert(opened.registers[0] === 5 && opened.registers[5] === 0, 'Open preserves size and file descriptor protocol');
        await quiet(OP.STAT, {descriptor: fd}, 0);
        assert(new TextDecoder().decode(memory.slice(264, 272)) === 'file.txt', 'File stat layout is unchanged');
        const read = await quiet(OP.READ, {descriptor: fd, length: 5}, 0);
        assert(read.registers[4] === 5 && new TextDecoder().decode(memory.slice(256, 261)) === 'hello', 'Guest receives read data');
        await quiet(OP.WRITE, {descriptor: fd, offset: 5, data: new Uint8Array([33])}, 0);
        assert(await (await (await root.getFileHandle('file.txt')).getFile()).text() === 'hello!', 'Writes persist before successful completion');
        await quiet(OP.READDIR, {descriptor: fd}, 1);
        await quiet(OP.CLOSE, {descriptor: fd}, 0);
        await quiet(OP.CLOSE, {descriptor: fd}, 1);
        assert(host.descriptors[fd] === null, 'Close releases the descriptor');
        const readonly = (await quiet(OP.OPEN, {path: 'file.txt'}, 0)).registers[4];
        await quiet(OP.WRITE, {descriptor: readonly, data: new Uint8Array([1])}, 1);
        await quiet(OP.CLOSE, {descriptor: readonly}, 0);
        const append = (await quiet(OP.OPEN, {path: 'file.txt', flags: 1 | 8}, 0)).registers[4];
        await quiet(OP.READ, {descriptor: append}, 1);
        await quiet(OP.WRITE, {descriptor: append, offset: 0, data: new Uint8Array([65])}, 0);
        await quiet(OP.CLOSE, {descriptor: append}, 0);
        assert(await (await (await root.getFileHandle('file.txt')).getFile()).text() === 'hello!A', 'Append semantics survive');
        const truncated = await quiet(OP.OPEN, {path: 'file.txt', flags: 2 | 4}, 0);
        assert(truncated.registers[0] === 0, 'Truncation remains successful');
        await quiet(OP.CLOSE, {descriptor: truncated.registers[4]}, 0);
        const created = await quiet(OP.OPEN, {path: 'created.txt', flags: 2 | 16}, 0);
        const removedFd = created.registers[4];
        await quiet(OP.RM, {path: 'created.txt'}, 0);
        await quiet(OP.STAT, {descriptor: removedFd}, 4);
        await quiet(OP.READ, {descriptor: removedFd}, 4);
        await quiet(OP.WRITE, {descriptor: removedFd, data: new Uint8Array([1])}, 1);
        await quiet(OP.CLOSE, {descriptor: removedFd}, 0);

        const directory = await quiet(OP.OPENDIR, {path: 'H:/'}, 0);
        const dirFd = directory.registers[4];
        await quiet(OP.STAT, {descriptor: dirFd}, 0);
        const names = [];
        for (;;) {
            const result = await invoke(OP.READDIR, {descriptor: dirFd});
            if (result.status === 21) break;
            assert(result.status === 0, 'Directory listing reaches guest');
            names.push(new TextDecoder().decode(memory.slice(257, 273)).replace(/\0.*$/, ''));
        }
        assert(names.includes('file.txt') && names.includes('folder'), 'File/directory listing remains intact');
        await quiet(OP.READDIR, {descriptor: dirFd}, 21);
        await quiet(OP.READ, {descriptor: dirFd}, 1);
        await quiet(OP.CLOSE, {descriptor: dirFd}, 0);
        await quiet(OP.MKDIR, {path: 'folder'}, 1);
        await quiet(OP.MKDIR, {path: 'file.txt'}, 1);
        await quiet(OP.MKDIR, {path: 'missing/child'}, 1);
        await quiet(OP.MKDIR, {path: 'empty'}, 0);
        await quiet(OP.RM, {path: 'empty'}, 0);
        await quiet(OP.RM, {path: 'folder'}, 1); // Nonempty directory, not an emulator failure.
        await quiet(OP.RM, {path: 'missing'}, 1);
        assert(diagnostics.length === 0 && notifications.length === 0, 'All normal operations remain free of emulator diagnostics');
        assert(allocations === frees && allocations > 0, 'Guest transfer buffers are freed');
        host.descriptors.fill({directory: true});
        await quiet(OP.OPENDIR, {path: '/'}, 20);
        host.descriptors.fill(null);

        const writable = (await quiet(OP.OPEN, {path: 'file.txt', flags: 2}, 0)).registers[4];
        const fileHandle = host.descriptors[writable].handle;
        host.descriptors[writable].handle = {
            getFile: fileHandle.getFile.bind(fileHandle),
            createWritable: async () => { throw new DOMException('File is locked', 'NoModificationAllowedError'); },
        };
        await quiet(OP.WRITE, {descriptor: writable, data: new Uint8Array([2])}, 1);
        host.directory = proxy(root, {queryPermission: async ({mode}) => mode === 'readwrite' ? 'denied' : 'granted'});
        const permission = await unexpected(OP.WRITE, {descriptor: writable, data: new Uint8Array([2])});
        assert(permission.name === 'NotAllowedError', 'Revoked workspace write permission remains a consumer error');
        host.directory = root;
        for (const name of ['SecurityError', 'NotAllowedError', 'QuotaExceededError', 'InvalidStateError', 'UnknownError', 'AbortError', 'InvalidModificationError']) {
            const error = new DOMException(`Injected browser failure: ${name}`, name);
            host.descriptors[writable].handle = {getFile: async () => { throw error; }};
            assert(await unexpected(OP.WRITE, {descriptor: writable, data: new Uint8Array([2])}) === error,
                'Unexpected browser exception must be preserved');
        }
        host.descriptors[writable].handle = {getFile: async () => { throw new TypeError('Broken browser integration'); }};
        await unexpected(OP.READ, {descriptor: writable});
        host.descriptors[writable].handle = fileHandle;
        await put(root, 'file.txt', 'bridge');
        const malloc = module._malloc;
        const bridgeError = new DOMException('Missing WASM bridge resource', 'NotFoundError');
        module._malloc = () => { throw bridgeError; };
        assert(await unexpected(OP.READ, {descriptor: writable}) === bridgeError,
            'NotFoundError outside filesystem entry operations is not a guest lookup');
        module._malloc = malloc;
        await quiet(OP.CLOSE, {descriptor: writable}, 0);
        await unexpected(99);
        await unexpected(OP.OPEN, {path: null});

        // An interrupted enumeration must not leave a successful partial cache.
        const listing = (await quiet(OP.OPENDIR, {path: 'folder'}, 0)).registers[4];
        const originalDirectory = host.descriptors[listing].handle;
        host.descriptors[listing].handle = {async *values() {
            yield {name: 'partial', kind: 'file'};
            throw new Error('Directory backend failed');
        }};
        await unexpected(OP.READDIR, {descriptor: listing});
        assert(host.descriptors[listing].entries === null && host.descriptors[listing].position === 0,
            'Failed listing does not consume descriptors or cache partial data');
        host.descriptors[listing].handle = originalDirectory;
        await quiet(OP.READDIR, {descriptor: listing}, 0);
        await quiet(OP.CLOSE, {descriptor: listing}, 0);

        // Real deleted handles: the same NotFoundError must be diagnosed when
        // it is the backing workspace, rather than a missing guest entry.
        const stale = await root.getDirectoryHandle('invalidated', {create: true});
        await put(stale, 'held.txt', 'held');
        attach(stale);
        const staleFile = (await quiet(OP.OPEN, {path: 'held.txt'}, 0)).registers[4];
        const staleDir = (await quiet(OP.OPENDIR, {path: '/'}, 0)).registers[4];
        await root.removeEntry('invalidated', {recursive: true});
        for (const [operation, fields] of [
            [OP.OPEN, {path: 'missing'}], [OP.OPENDIR, {path: '/'}],
            [OP.READ, {descriptor: staleFile}], [OP.READDIR, {descriptor: staleDir}],
            [OP.MKDIR, {path: 'new'}], [OP.RM, {path: 'missing'}],
        ]) {
            assert((await unexpected(operation, fields)).name === 'NotFoundError',
                'Invalidated backing workspace must still notify the consumer');
        }
        await quiet(OP.CLOSE, {descriptor: staleFile}, 0);
        await quiet(OP.CLOSE, {descriptor: staleDir}, 0);
        return 'PASS: HostFS guest statuses without diagnostics; infrastructure failures reported; read/write/listing/persistence and descriptors intact';
    } finally { console.error = originalError; }
})();
window.testResult.catch(error => { document.body.append(error.stack); });
