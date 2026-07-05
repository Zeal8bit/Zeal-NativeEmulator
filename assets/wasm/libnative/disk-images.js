(() => {
    class DiskImageStore {
        constructor({
            enabled = false,
            database = new IndexedDBStore(),
            images = [],
            onWarning = (message, error) => console.warn(message, error),
            onConflict = async () => 'remote',
        } = {}) {
            this.enabled = enabled;
            this.database = database;
            this.images = images;
            this.onWarning = onWarning;
            this.onConflict = onConflict;
        }

        descriptor(name) {
            const image = this.images.find(candidate => candidate.name === name);
            if (!image) throw new RangeError(`Unknown disk image: ${name}`);
            return image;
        }

        metaKey(image) {
            return `${image.key}:meta`;
        }

        metadata(response) {
            return {
                etag: response.headers.get('ETag'),
                lastModified: response.headers.get('Last-Modified'),
                dirty: false,
            };
        }

        normalizeMetadata(metadata) {
            if (metadata && typeof metadata === 'object') {
                return {
                    etag: metadata.etag ?? null,
                    lastModified: metadata.lastModified ?? null,
                    dirty: metadata.dirty === true,
                };
            }
            return {etag: metadata ?? null, lastModified: null, dirty: false};
        }

        metadataMatches(cached, remote) {
            if (remote.etag != null) return remote.etag === cached.etag;
            if (remote.lastModified != null) {
                return remote.lastModified === cached.lastModified;
            }
            return cached.etag == null && cached.lastModified == null;
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
                metadata: this.metadata(response),
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
                    const response = await fetch(image.source, {
                        method: 'HEAD',
                        cache: 'no-cache',
                    });
                    if (!response.ok) throw new Error(`Failed to validate ${image.source}`);
                    const localMetadata = this.normalizeMetadata(cachedMeta);
                    const remoteMetadata = this.metadata(response);
                    if (this.metadataMatches(localMetadata, remoteMetadata)) {
                        return cachedImage;
                    }
                    if (localMetadata.dirty) {
                        const choice = await this.onConflict(image, {
                            local: localMetadata,
                            remote: remoteMetadata,
                        });
                        if (choice === 'local') {
                            await this.database.put(this.metaKey(image), {
                                ...remoteMetadata,
                                dirty: true,
                            });
                            return cachedImage;
                        }
                        if (choice !== 'remote') {
                            throw new RangeError(`Unknown image conflict choice: ${choice}`);
                        }
                    }
                } catch (error) {
                    if (this.normalizeMetadata(cachedMeta).dirty) {
                        this.onWarning(
                            `Image validation failed for ${image.key}; keeping local changes`,
                            error
                        );
                        return cachedImage;
                    }
                    this.onWarning(
                        `Image validation failed for ${image.key}; trying remote image`,
                        error
                    );
                }
            }

            try {
                const remote = await this.fetchRemote(image);
                try {
                    await this.database.put(image.key, remote.data);
                    await this.database.put(this.metaKey(image), remote.metadata);
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
            if (!this.enabled) return [];
            const saved = [];
            for (const image of this.images) {
                if (!image.persist || image.source == null) continue;
                const metadata = this.normalizeMetadata(
                    await this.database.get(this.metaKey(image))
                );
                await this.database.put(image.key, fs.readFile(image.path));
                await this.database.put(this.metaKey(image), {
                    ...metadata,
                    dirty: true,
                });
                const savedMetadata = this.normalizeMetadata(
                    await this.database.get(this.metaKey(image))
                );
                if (!savedMetadata.dirty) {
                    throw new Error(`Failed to mark ${image.key} as locally modified`);
                }
                saved.push(image.key);
            }
            return saved;
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
