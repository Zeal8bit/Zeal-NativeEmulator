(() => {
    const emulator = new ZealNative({
        canvas: document.getElementById('canvas'),
        romdisk: 'default.img',
        imageStorage: 'opfs',
        onImageUpdateAvailable: image => console.info('Image update available:', image),
        userProgram: 'microbe.bin',
    });

    window.addEventListener('load', () => {
        emulator.start().catch(error => console.error(error));
    });
})();
