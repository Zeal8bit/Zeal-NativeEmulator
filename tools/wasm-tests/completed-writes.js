// No WASM export is called by observation or verification code.
window.testResult = new Promise((resolve, reject) => {
    window.Module = {
        print: () => {}, printErr: message => console.error(message),
        onAbort: reason => reject(new Error(`Device test aborted: ${reason}`)),
    };
    const observations = [];
    let directory;
    Module.observeWrite = async (device, accepted) => {
        try {
            const name = device === 0 ? 'tf.img' : 'eeprom.img';
            const offset = device === 0 ? 512 : 256;
            const size = device === 0 ? 512 : 128;
            const file = await (await directory.getFileHandle(name)).getFile();
            const data = new Uint8Array(await file.slice(offset, offset + size).arrayBuffer());
            const visible = data.length === size && data.every((byte, i) =>
                byte === (device === 0 ? (i * 13 + 17) & 255 : (i * 7 + 33) & 255));
            observations.push({name, accepted: !!accepted, visible});
        } catch (error) { reject(error); }
    };
    Module.writesCompleted = () => {
        if (observations.length !== 2 || observations.some(item => !item.accepted || !item.visible)) {
            reject(new Error(`Completed writes not visible in OPFS: ${JSON.stringify(observations)}`));
        } else {
            resolve('PASS: TF and EEPROM backing bytes visible immediately; both C streams remain open');
        }
    };
    (async () => {
        directory = await (await navigator.storage.getDirectory())
            .getDirectoryHandle('zeal-completed-write-test', {create: true});
        // Fixture creation happens before either device opens its stream.
        for (const [name, size] of [['tf.img', 2 * 1024 * 1024], ['eeprom.img', 65536]]) {
            const writer = await (await directory.getFileHandle(name, {create: true})).createWritable();
            await writer.write(new Uint8Array(size));
            await writer.close();
        }
        const script = document.createElement('script');
        script.src = '/wasm/wasm-tests/completed-writes.js';
        script.onerror = () => reject(new Error('Could not load device test'));
        document.body.append(script);
    })().catch(reject);
});
window.testResult.catch(error => { document.body.append(error.stack); });
