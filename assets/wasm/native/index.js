(() => {
    const ROMDISK = 'default.img';

    const consoleOutput = document.getElementById('output');
    const consoleError = document.getElementById('errors');
    const canvas = document.getElementById('canvas');
    const reloadButton = document.getElementById('reload');
    const mountButton = document.getElementById('mount-hostfs');
    const hostfsStatus = document.getElementById('hostfs-status');

    let moduleInstance = null;
    let moduleExitPromise = null;
    let loading = false;
    let romdiskPromise = null;
    let hostfsDirectory = null;

    const HostFS = {
        SUCCESS: 0,
        FAILURE: 1,
        NO_SUCH_ENTRY: 4,
        CANNOT_REGISTER_MORE: 20,
        NO_MORE_ENTRIES: 21,
        OPEN: 1,
        STAT: 2,
        READ: 3,
        WRITE: 4,
        CLOSE: 5,
        OPENDIR: 6,
        READDIR: 7,
        MKDIR: 8,
        RM: 9,
        RDONLY: 0,
        WRONLY: 1,
        RDWR: 2,
        TRUNC: 1 << 2,
        APPEND: 2 << 2,
        CREAT: 4 << 2,
    };

    function appendMessage(element, text) {
        element.append(document.createTextNode(text), document.createElement('br'));
        element.scrollTop = element.scrollHeight;
    }

    async function fetchRomdisk() {
        const response = await fetch(ROMDISK);
        if (!response.ok) throw new Error(`Failed to fetch ${ROMDISK}`);
        return new Uint8Array(await response.arrayBuffer());
    }

    function getRomdisk() {
        romdiskPromise ??= fetchRomdisk();
        return romdiskPromise;
    }

    function createHostFSBackend(root, getModule) {
        const descriptors = new Array(256).fill(null);

        function diagnose(operation, error) {
            console.error(`[HostFS] ${operation} failed`, error);
            appendMessage(consoleError, `[HostFS] ${operation}: ${error.message || error}`);
        }

        function statusFor(error) {
            return error?.name === 'NotFoundError' ? HostFS.NO_SUCH_ENTRY : HostFS.FAILURE;
        }

        function complete(status, registers = [0, 0, 0, 0, 0, 0]) {
            getModule()._hostfs_web_complete(status, ...registers);
        }

        function writeGuest(address, bytes) {
            if (!bytes.length) return;
            const module = getModule();
            const pointer = module._malloc(bytes.length);
            if (!pointer) throw new Error('Unable to allocate HostFS transfer buffer');
            try {
                module.HEAPU8.set(bytes, pointer);
                module._hostfs_web_write_guest(address, pointer, bytes.length);
            } finally {
                module._free(pointer);
            }
        }

        function splitPath(path) {
            // if (typeof path !== 'string' || path.includes('\\') ||
            //     path.startsWith('/') || /^[A-Za-z]:/.test(path)) {
            //     throw new DOMException('Absolute HostFS paths are not allowed', 'SecurityError');
            // }
            const parts = path.split('/').filter(part => part !== '' && part !== '.');
            if (parts.some(part => part === '..')) {
                throw new DOMException('HostFS path escapes selected directory', 'SecurityError');
            }
            return parts;
        }

        async function directoryFor(parts) {
            let directory = root;
            for (const part of parts) {
                directory = await directory.getDirectoryHandle(part);
            }
            return directory;
        }

        async function resolveParent(path) {
            const parts = splitPath(path);
            if (!parts.length) throw new DOMException('Root has no parent entry', 'SecurityError');
            const name = parts.pop();
            return {parent: await directoryFor(parts), name};
        }

        function allocate(descriptor) {
            const index = descriptors.indexOf(null);
            if (index < 0) return -1;
            descriptors[index] = descriptor;
            return index;
        }

        function descriptorAt(index, directory = null) {
            const descriptor = descriptors[index];
            if (!descriptor || (directory !== null && descriptor.directory !== directory)) {
                throw new DOMException('Invalid HostFS descriptor', 'InvalidStateError');
            }
            return descriptor;
        }

        function formatName(name) {
            const output = new Uint8Array(16);
            const encoded = new TextEncoder().encode(name);
            if (encoded.length <= 16) {
                output.set(encoded.subarray(0, 16));
            } else {
                output.set(encoded.subarray(0, 15));
                output[15] = '~'.charCodeAt(0);
            }
            return output;
        }

        async function open(request) {
            const {parent, name} = await resolveParent(request.path);
            try {
                const handle = await parent.getDirectoryHandle(name);
                const index = allocate({
                    directory: true,
                    handle,
                    name,
                    entries: null,
                    position: 0,
                });
                if (index < 0) return complete(HostFS.CANNOT_REGISTER_MORE);
                return complete(HostFS.SUCCESS, [0, 0, 0, 0, index, 1]);
            } catch (error) {
                if (error.name !== 'TypeMismatchError' && error.name !== 'NotFoundError') throw error;
            }

            const access = request.flags & 3;
            if (access > HostFS.RDWR) throw new DOMException('Invalid open mode', 'TypeError');
            let handle;
            try {
                handle = await parent.getFileHandle(name, {
                    create: !!(request.flags & HostFS.CREAT),
                });
            } catch (error) {
                if (error.name === 'TypeMismatchError') throw error;
                throw error;
            }
            if ((request.flags & HostFS.TRUNC) && access !== HostFS.RDONLY) {
                const writer = await handle.createWritable();
                await writer.truncate(0);
                await writer.close();
            }
            const file = await handle.getFile();
            const index = allocate({
                directory: false,
                handle,
                name,
                readable: access !== HostFS.WRONLY,
                writable: access !== HostFS.RDONLY,
                append: !!(request.flags & HostFS.APPEND),
            });
            if (index < 0) return complete(HostFS.CANNOT_REGISTER_MORE);
            const size = Math.min(file.size, 0xffffffff) >>> 0;
            complete(HostFS.SUCCESS, [
                size & 0xff, (size >>> 8) & 0xff, (size >>> 16) & 0xff,
                (size >>> 24) & 0xff, index, 0,
            ]);
        }

        async function opendir(request) {
            const handle = await directoryFor(splitPath(request.path));
            const name = splitPath(request.path).at(-1) || '';
            const index = allocate({
                directory: true,
                handle,
                name,
                entries: null,
                position: 0,
            });
            if (index < 0) return complete(HostFS.CANNOT_REGISTER_MORE);
            complete(HostFS.SUCCESS, [0, 0, 0, 0, index, 1]);
        }

        async function stat(request) {
            const descriptor = descriptorAt(request.descriptor);
            const bytes = new Uint8Array(28);
            const view = new DataView(bytes.buffer);
            if (!descriptor.directory) {
                const file = await descriptor.handle.getFile();
                view.setUint32(0, Math.min(file.size, 0xffffffff), true);
            }
            bytes.set(formatName(descriptor.name), 12);
            writeGuest(request.guestAddress, descriptor.directory ? bytes : bytes.subarray(4));
            complete(HostFS.SUCCESS);
        }

        async function read(request) {
            const descriptor = descriptorAt(request.descriptor, false);
            if (!descriptor.readable) throw new DOMException('Descriptor is not readable', 'NotAllowedError');
            const file = await descriptor.handle.getFile();
            const bytes = new Uint8Array(
                await file.slice(request.offset, request.offset + request.length).arrayBuffer()
            );
            writeGuest(request.guestAddress, bytes);
            complete(HostFS.SUCCESS, [0, 0, 0, 0, bytes.length & 0xff, bytes.length >>> 8]);
        }

        async function write(request) {
            const descriptor = descriptorAt(request.descriptor, false);
            if (!descriptor.writable) throw new DOMException('Descriptor is not writable', 'NotAllowedError');
            const file = await descriptor.handle.getFile();
            const position = descriptor.append ? file.size : request.offset;
            const writer = await descriptor.handle.createWritable({keepExistingData: true});
            try {
                await writer.write({type: 'write', position, data: request.data});
            } finally {
                await writer.close();
            }
            complete(HostFS.SUCCESS, [
                0, 0, 0, 0, request.data.length & 0xff, request.data.length >>> 8,
            ]);
        }

        async function close(request) {
            descriptorAt(request.descriptor);
            descriptors[request.descriptor] = null;
            complete(HostFS.SUCCESS);
        }

        async function readdir(request) {
            const descriptor = descriptorAt(request.descriptor, true);
            if (!descriptor.entries) {
                descriptor.entries = [];
                for await (const entry of descriptor.handle.values()) {
                    if (entry.kind === 'file' || entry.kind === 'directory') {
                        descriptor.entries.push(entry);
                    }
                }
            }
            const entry = descriptor.entries[descriptor.position++];
            if (!entry) return complete(HostFS.NO_MORE_ENTRIES);
            const bytes = new Uint8Array(17);
            bytes[0] = entry.kind === 'file' ? 1 : 0;
            bytes.set(formatName(entry.name), 1);
            writeGuest(request.guestAddress, bytes);
            complete(HostFS.SUCCESS);
        }

        async function mkdir(request) {
            const {parent, name} = await resolveParent(request.path);
            try {
                await parent.getDirectoryHandle(name);
                throw new DOMException('Directory already exists', 'InvalidModificationError');
            } catch (error) {
                if (error.name !== 'NotFoundError') throw error;
            }
            await parent.getDirectoryHandle(name, {create: true});
            complete(HostFS.SUCCESS);
        }

        async function remove(request) {
            const {parent, name} = await resolveParent(request.path);
            await parent.removeEntry(name);
            complete(HostFS.SUCCESS);
        }

        const operations = {
            [HostFS.OPEN]: open,
            [HostFS.STAT]: stat,
            [HostFS.READ]: read,
            [HostFS.WRITE]: write,
            [HostFS.CLOSE]: close,
            [HostFS.OPENDIR]: opendir,
            [HostFS.READDIR]: readdir,
            [HostFS.MKDIR]: mkdir,
            [HostFS.RM]: remove,
        };

        return {
            start(request) {
                const operation = operations[request.operation];
                if (!operation) return complete(HostFS.FAILURE);
                Promise.resolve(operation(request)).catch(error => {
                    diagnose(`operation ${request.operation}`, error);
                    complete(statusFor(error));
                });
            },
        };
    }

    async function loadModule() {
        const romdisk = await getRomdisk();
        let resolveExit;
        const exitPromise = new Promise(resolve => {
            resolveExit = resolve;
        });
        let instance = null;
        const defaultModule = {
            hostfsBackend: hostfsDirectory
                ? createHostFSBackend(hostfsDirectory, () => instance)
                : null,
            print(text) {
                console.log(`Log: ${text}`);
                appendMessage(consoleOutput, text);
            },
            printErr(text) {
                console.error(`Error: ${text}`);
                appendMessage(consoleError, text);
            },
            canvas,
            onRuntimeInitialized() {
                if (!this.FS.analyzePath('/roms').exists) {
                    this.FS.mkdir('/roms');
                }
                this.FS.writeFile('/roms/default.img', romdisk);
                canvas.focus();
            },
            onExit(exitCode) {
                resolveExit(exitCode);
                if (moduleInstance === instance) {
                    moduleInstance = null;
                    moduleExitPromise = null;
                }
            },
        };

        instance = await NativeModule(defaultModule);
        moduleInstance = instance;
        moduleExitPromise = exitPromise;
    }

    async function startModule() {
        if (loading) return;

        loading = true;
        reloadButton.disabled = true;
        try {
            await loadModule();
        } catch (error) {
            console.error(error);
            appendMessage(consoleError, error.message || String(error));
        } finally {
            loading = false;
            reloadButton.disabled = false;
        }
    }

    async function reloadModule() {
        if (loading) return;

        loading = true;
        reloadButton.disabled = true;
        try {
            const instance = moduleInstance;
            const exitPromise = moduleExitPromise;
            if (instance) {
                instance._zeal_exit_web();
                await exitPromise;
            }
            await loadModule();
        } catch (error) {
            console.error(error);
            appendMessage(consoleError, error.message || String(error));
        } finally {
            loading = false;
            reloadButton.disabled = false;
        }
    }

    async function mountHostFS() {
        if (loading || !window.isSecureContext || !window.showDirectoryPicker) return;
        try {
            const directory = await window.showDirectoryPicker({mode: 'readwrite'});
            const permission = await directory.requestPermission({mode: 'readwrite'});
            if (permission !== 'granted') {
                throw new DOMException('Read/write permission was not granted', 'NotAllowedError');
            }
            hostfsDirectory = directory;
            hostfsStatus.textContent = `HostFS mounted: ${directory.name}`;
            await reloadModule();
        } catch (error) {
            if (error.name !== 'AbortError') {
                console.error('[HostFS] Mount failed', error);
                appendMessage(consoleError, `[HostFS] Mount: ${error.message || error}`);
            }
        }
    }

    function resumeAudioIfNeeded() {
        const devices = window.miniaudio?.devices || [];
        for (const device of devices) {
            if (device?.webaudio?.state === 'suspended') {
                device.webaudio.resume().catch(error => {
                    console.error('Failed to resume audio context', error);
                });
            }
        }
    }

    function toggleDebugger() {
        if (!moduleInstance?._zeal_debug_toggle_web) return;
        moduleInstance._zeal_debug_toggle_web();
        canvas.focus();
    }

    canvas.addEventListener('keydown', event => {
        event.preventDefault();
        event.stopPropagation();
    }, true);

    canvas.addEventListener('keyup', event => {
        event.preventDefault();
        event.stopPropagation();
    }, true);

    window.addEventListener('keydown', event => {
        if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' ', 'Tab', 'Escape'].includes(event.key)) {
            event.preventDefault();
        }
    }, true);

    document.getElementById('unmute').addEventListener('click', resumeAudioIfNeeded);
    reloadButton.addEventListener('click', reloadModule);
    mountButton.addEventListener('click', mountHostFS);
    document.getElementById('toggle-debugger').addEventListener('click', toggleDebugger);

    document.getElementById('toggle-fps').addEventListener('click', () => {
        if (!moduleInstance) return;
        const showFps = !!moduleInstance.getValue(moduleInstance._show_fps, 'i8');
        moduleInstance.setValue(moduleInstance._show_fps, showFps ? 0 : 1, 'i8');
    });

    document.getElementById('canvas-smoothing').addEventListener('change', event => {
        canvas.classList.toggle('smooth', event.target.checked);
    });

    if (!window.isSecureContext || !window.showDirectoryPicker) {
        mountButton.disabled = true;
        hostfsStatus.textContent = !window.isSecureContext
            ? 'HostFS unavailable: secure context required'
            : 'HostFS unavailable: Chromium desktop required';
    }

    window.addEventListener('load', startModule);
})();
