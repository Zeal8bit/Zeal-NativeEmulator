(() => {
    const STATUS = {
        SUCCESS: 0,
        FAILURE: 1,
        NO_SUCH_ENTRY: 4,
        CANNOT_REGISTER_MORE: 20,
        NO_MORE_ENTRIES: 21,
    };

    const OPERATION = {
        OPEN: 1,
        STAT: 2,
        READ: 3,
        WRITE: 4,
        CLOSE: 5,
        OPENDIR: 6,
        READDIR: 7,
        MKDIR: 8,
        RM: 9,
    };

    const OPEN_FLAG = {
        RDONLY: 0,
        WRONLY: 1,
        RDWR: 2,
        TRUNC: 1 << 2,
        APPEND: 2 << 2,
        CREAT: 4 << 2,
    };

    class HostFS {
        constructor(directory, {onError = null} = {}) {
            if (!directory) throw new TypeError('HostFS requires a directory handle');
            this.directory = directory;
            this.onError = onError;
            this.descriptors = new Array(256).fill(null);
            this.module = null;
        }

        static get supported() {
            return window.isSecureContext && typeof window.showDirectoryPicker === 'function';
        }

        static get unavailableReason() {
            if (!window.isSecureContext) return 'HostFS unavailable: secure context required';
            if (typeof window.showDirectoryPicker !== 'function') {
                return 'HostFS unavailable: Chromium desktop required';
            }
            return null;
        }

        static async mount(options = {}) {
            if (!HostFS.supported) throw new Error(HostFS.unavailableReason);
            const directory = await window.showDirectoryPicker({mode: 'readwrite'});
            const permission = await directory.requestPermission({mode: 'readwrite'});
            if (permission !== 'granted') {
                throw new DOMException('Read/write permission was not granted', 'NotAllowedError');
            }
            return new HostFS(directory, options);
        }

        bindModule(module) {
            this.module = module;
            return this;
        }

        diagnose(operation, error) {
            console.error(`[HostFS] ${operation} failed`, error);
            this.onError?.(operation, error);
        }

        statusFor(error) {
            return error?.name === 'NotFoundError' ? STATUS.NO_SUCH_ENTRY : STATUS.FAILURE;
        }

        complete(status, registers = [0, 0, 0, 0, 0, 0]) {
            this.module._hostfs_web_complete(status, ...registers);
        }

        writeGuest(address, bytes) {
            if (!bytes.length) return;
            const pointer = this.module._malloc(bytes.length);
            if (!pointer) throw new Error('Unable to allocate HostFS transfer buffer');
            try {
                this.module.HEAPU8.set(bytes, pointer);
                this.module._hostfs_web_write_guest(address, pointer, bytes.length);
            } finally {
                this.module._free(pointer);
            }
        }

        splitPath(path) {
            if (typeof path !== 'string' || path.includes('\\')) {
                throw new DOMException('Invalid HostFS path', 'SecurityError');
            }

            let relative = path;
            const drive = relative.match(/^([A-Za-z]):(.*)$/);
            if (drive) {
                if (drive[1].toLowerCase() !== 'h' ||
                    (drive[2] !== '' && !drive[2].startsWith('/'))) {
                    throw new DOMException('Invalid HostFS drive path', 'SecurityError');
                }
                relative = drive[2].replace(/^\/+/, '');
            } else {
                // Zeal OS strips the H: drive before issuing HostFS requests,
                // leaving paths rooted at the selected directory as `/...`.
                relative = relative.replace(/^\/+/, '');
            }

            const parts = relative.split('/').filter(part => part !== '' && part !== '.');
            if (parts.some(part => part === '..')) {
                throw new DOMException('HostFS path escapes selected directory', 'SecurityError');
            }
            return parts;
        }

        async directoryFor(parts) {
            let directory = this.directory;
            for (const part of parts) {
                directory = await directory.getDirectoryHandle(part);
            }
            return directory;
        }

        async resolveParent(path) {
            const parts = this.splitPath(path);
            if (!parts.length) throw new DOMException('Root has no parent entry', 'SecurityError');
            const name = parts.pop();
            return {parent: await this.directoryFor(parts), name};
        }

        allocate(descriptor) {
            const index = this.descriptors.indexOf(null);
            if (index < 0) return -1;
            this.descriptors[index] = descriptor;
            return index;
        }

        descriptorAt(index, directory = null) {
            const descriptor = this.descriptors[index];
            if (!descriptor || (directory !== null && descriptor.directory !== directory)) {
                throw new DOMException('Invalid HostFS descriptor', 'InvalidStateError');
            }
            return descriptor;
        }

        formatName(name) {
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

        async open(request) {
            const {parent, name} = await this.resolveParent(request.path);
            try {
                const handle = await parent.getDirectoryHandle(name);
                const index = this.allocate({
                    directory: true, handle, name, entries: null, position: 0,
                });
                if (index < 0) return this.complete(STATUS.CANNOT_REGISTER_MORE);
                return this.complete(STATUS.SUCCESS, [0, 0, 0, 0, index, 1]);
            } catch (error) {
                if (error.name !== 'TypeMismatchError' && error.name !== 'NotFoundError') throw error;
            }

            const access = request.flags & 3;
            if (access > OPEN_FLAG.RDWR) throw new DOMException('Invalid open mode', 'TypeError');
            const handle = await parent.getFileHandle(name, {
                create: !!(request.flags & OPEN_FLAG.CREAT),
            });
            if ((request.flags & OPEN_FLAG.TRUNC) && access !== OPEN_FLAG.RDONLY) {
                const writer = await handle.createWritable();
                await writer.truncate(0);
                await writer.close();
            }
            const file = await handle.getFile();
            const index = this.allocate({
                directory: false,
                handle,
                name,
                readable: access !== OPEN_FLAG.WRONLY,
                writable: access !== OPEN_FLAG.RDONLY,
                append: !!(request.flags & OPEN_FLAG.APPEND),
            });
            if (index < 0) return this.complete(STATUS.CANNOT_REGISTER_MORE);
            const size = Math.min(file.size, 0xffffffff) >>> 0;
            this.complete(STATUS.SUCCESS, [
                size & 0xff, (size >>> 8) & 0xff, (size >>> 16) & 0xff,
                (size >>> 24) & 0xff, index, 0,
            ]);
        }

        async opendir(request) {
            const parts = this.splitPath(request.path);
            const handle = await this.directoryFor(parts);
            const index = this.allocate({
                directory: true,
                handle,
                name: parts.at(-1) || '',
                entries: null,
                position: 0,
            });
            if (index < 0) return this.complete(STATUS.CANNOT_REGISTER_MORE);
            this.complete(STATUS.SUCCESS, [0, 0, 0, 0, index, 1]);
        }

        async stat(request) {
            const descriptor = this.descriptorAt(request.descriptor);
            const bytes = new Uint8Array(28);
            const view = new DataView(bytes.buffer);
            if (!descriptor.directory) {
                const file = await descriptor.handle.getFile();
                view.setUint32(0, Math.min(file.size, 0xffffffff), true);
            }
            bytes.set(this.formatName(descriptor.name), 12);
            this.writeGuest(request.guestAddress, descriptor.directory ? bytes : bytes.subarray(4));
            this.complete(STATUS.SUCCESS);
        }

        async read(request) {
            const descriptor = this.descriptorAt(request.descriptor, false);
            if (!descriptor.readable) {
                throw new DOMException('Descriptor is not readable', 'NotAllowedError');
            }
            const file = await descriptor.handle.getFile();
            const bytes = new Uint8Array(
                await file.slice(request.offset, request.offset + request.length).arrayBuffer()
            );
            this.writeGuest(request.guestAddress, bytes);
            this.complete(STATUS.SUCCESS, [0, 0, 0, 0, bytes.length & 0xff, bytes.length >>> 8]);
        }

        async write(request) {
            const descriptor = this.descriptorAt(request.descriptor, false);
            if (!descriptor.writable) {
                throw new DOMException('Descriptor is not writable', 'NotAllowedError');
            }
            const file = await descriptor.handle.getFile();
            const position = descriptor.append ? file.size : request.offset;
            const writer = await descriptor.handle.createWritable({keepExistingData: true});
            try {
                await writer.write({type: 'write', position, data: request.data});
            } finally {
                await writer.close();
            }
            this.complete(STATUS.SUCCESS, [
                0, 0, 0, 0, request.data.length & 0xff, request.data.length >>> 8,
            ]);
        }

        async close(request) {
            this.descriptorAt(request.descriptor);
            this.descriptors[request.descriptor] = null;
            this.complete(STATUS.SUCCESS);
        }

        async readdir(request) {
            const descriptor = this.descriptorAt(request.descriptor, true);
            if (!descriptor.entries) {
                descriptor.entries = [];
                for await (const entry of descriptor.handle.values()) {
                    if (entry.kind === 'file' || entry.kind === 'directory') {
                        descriptor.entries.push(entry);
                    }
                }
            }
            const entry = descriptor.entries[descriptor.position++];
            if (!entry) return this.complete(STATUS.NO_MORE_ENTRIES);
            const bytes = new Uint8Array(17);
            bytes[0] = entry.kind === 'file' ? 1 : 0;
            bytes.set(this.formatName(entry.name), 1);
            this.writeGuest(request.guestAddress, bytes);
            this.complete(STATUS.SUCCESS);
        }

        async mkdir(request) {
            const {parent, name} = await this.resolveParent(request.path);
            try {
                await parent.getDirectoryHandle(name);
                throw new DOMException('Directory already exists', 'InvalidModificationError');
            } catch (error) {
                if (error.name !== 'NotFoundError') throw error;
            }
            await parent.getDirectoryHandle(name, {create: true});
            this.complete(STATUS.SUCCESS);
        }

        async remove(request) {
            const {parent, name} = await this.resolveParent(request.path);
            await parent.removeEntry(name);
            this.complete(STATUS.SUCCESS);
        }

        start(request) {
            const operations = {
                [OPERATION.OPEN]: this.open,
                [OPERATION.STAT]: this.stat,
                [OPERATION.READ]: this.read,
                [OPERATION.WRITE]: this.write,
                [OPERATION.CLOSE]: this.close,
                [OPERATION.OPENDIR]: this.opendir,
                [OPERATION.READDIR]: this.readdir,
                [OPERATION.MKDIR]: this.mkdir,
                [OPERATION.RM]: this.remove,
            };
            const operation = operations[request.operation];
            if (!operation) return this.complete(STATUS.FAILURE);
            Promise.resolve(operation.call(this, request)).catch(error => {
                this.diagnose(`operation ${request.operation}`, error);
                this.complete(this.statusFor(error));
            });
        }
    }

    window.HostFS = HostFS;
})();
