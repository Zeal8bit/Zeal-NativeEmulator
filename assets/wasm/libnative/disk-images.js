(() => {
    class DiskImageStore {
        constructor({
            enabled = false,
            database = new IndexedDBStore(),
            images = [],
            onWarning = (message, error) => console.warn(message, error),
        } = {}) {
            this.enabled = enabled;
            this.database = database;
            this.images = images;
            this.onWarning = onWarning;
        }

        descriptor(name) {
            const image = this.images.find(candidate => candidate.name === name);
            if (!image) throw new RangeError(`Unknown disk image: ${name}`);
            return image;
        }

        metaKey(image) {
            return `${image.key}:meta`;
        }

        bytes(source) {
            if (source instanceof Uint8Array) return source;
            if (source instanceof ArrayBuffer) return new Uint8Array(source);
            return null;
        }

        async fetchRemote(image) {
            const response = await fetch(image.source);
            if (!response.ok) throw new Error(`Failed to fetch ${image.source}`);
            return {
                data: new Uint8Array(await response.arrayBuffer()),
                etag: response.headers.get('ETag'),
            };
        }

        async load(name) {
            const image = this.descriptor(name);
            if (image.source == null) return null;
            const bytes = this.bytes(image.source);
            if (bytes) return bytes;
            if (typeof image.source !== 'string' && !(image.source instanceof URL)) {
                throw new TypeError('Disk image source must be a URL, Uint8Array, or ArrayBuffer');
            }

            if (!this.enabled) return (await this.fetchRemote(image)).data;

            let cachedImage;
            let cachedMeta;
            try {
                [cachedImage, cachedMeta] = await Promise.all([
                    this.database.get(image.key),
                    this.database.get(this.metaKey(image)),
                ]);
            } catch (error) {
                this.onWarning(`IndexedDB read failed for ${image.key}; using remote image`, error);
            }

            if (cachedImage != null) {
                try {
                    const response = await fetch(image.source, {method: 'HEAD'});
                    if (!response.ok) throw new Error(`Failed to validate ${image.source}`);
                    if (response.headers.get('ETag') == cachedMeta) return cachedImage;
                } catch (error) {
                    this.onWarning(`Image validation failed for ${image.key}; trying remote image`, error);
                }
            }

            try {
                const remote = await this.fetchRemote(image);
                try {
                    await this.database.put(image.key, remote.data);
                    await this.database.put(this.metaKey(image), remote.etag);
                } catch (error) {
                    this.onWarning(`IndexedDB write failed for ${image.key}`, error);
                }
                return remote.data;
            } catch (error) {
                if (cachedImage != null) {
                    this.onWarning(`Remote image failed for ${image.key}; using cached image`, error);
                    return cachedImage;
                }
                throw error;
            }
        }

        loadAll() {
            return Promise.all(this.images.map(async image => [
                image.name,
                await this.load(image.name),
            ])).then(entries => Object.fromEntries(entries));
        }

        async persist(fs) {
            if (!this.enabled) return;
            for (const image of this.images) {
                if (!image.persist || image.source == null) continue;
                await this.database.put(image.key, fs.readFile(image.path));
            }
        }

        async clear() {
            if (!this.enabled) return;
            await Promise.all(this.images.map(image => this.database.delete(image.key)));
        }

        usage() {
            return this.enabled ? this.database.usage() : Promise.resolve(0);
        }
    }

    window.DiskImageStore = DiskImageStore;
})();
