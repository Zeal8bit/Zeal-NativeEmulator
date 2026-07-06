(() => {
    class IndexedDBStore {
        constructor({
            database = 'ZealStorage',
            version = 1,
            store = 'images',
        } = {}) {
            this.database = database;
            this.version = version;
            this.store = store;
            this.databasePromise = null;
        }

        open() {
            if (this.databasePromise) return this.databasePromise;
            this.databasePromise = new Promise((resolve, reject) => {
                const request = indexedDB.open(this.database, this.version);
                request.onerror = () => reject(request.error);
                request.onupgradeneeded = () => {
                    const database = request.result;
                    if (!database.objectStoreNames.contains(this.store)) {
                        database.createObjectStore(this.store);
                    }
                };
                request.onsuccess = () => {
                    const database = request.result;
                    database.onversionchange = () => database.close();
                    resolve(database);
                };
            }).catch(error => {
                this.databasePromise = null;
                throw error;
            });
            return this.databasePromise;
        }

        async transaction(mode, operation) {
            const database = await this.open();
            return new Promise((resolve, reject) => {
                const transaction = database.transaction(this.store, mode);
                const objectStore = transaction.objectStore(this.store);
                let result;
                try {
                    result = operation(objectStore);
                } catch (error) {
                    reject(error);
                    return;
                }
                transaction.oncomplete = () => resolve(result?.result);
                transaction.onerror = () => reject(transaction.error);
                transaction.onabort = () => reject(transaction.error);
            });
        }

        get(key) {
            return this.transaction('readonly', store => store.get(key));
        }

        put(key, value) {
            return this.transaction('readwrite', store => store.put(value, key));
        }

        delete(key) {
            return this.transaction('readwrite', store => store.delete(key));
        }

        async usage() {
            const database = await this.open();
            return new Promise((resolve, reject) => {
                let total = 0;
                const transaction = database.transaction(this.store, 'readonly');
                const request = transaction.objectStore(this.store).openCursor();
                request.onerror = () => reject(request.error);
                request.onsuccess = () => {
                    const cursor = request.result;
                    if (!cursor) {
                        resolve(total);
                        return;
                    }
                    const value = cursor.value;
                    if (value?.byteLength !== undefined) total += value.byteLength;
                    else if (value?.length !== undefined) total += value.length;
                    cursor.continue();
                };
            });
        }
    }

    window.IndexedDBStore = IndexedDBStore;
})();
