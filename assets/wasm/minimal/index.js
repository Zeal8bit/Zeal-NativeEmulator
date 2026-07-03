(() => {
    const emulator = new ZealNative({
        canvas: document.getElementById('canvas'),
        romdisk: 'default.img',
        userProgram: 'microbe.bin',
    });

    window.addEventListener('load', () => {
        emulator.start().catch(error => console.error(error));
    });
})();
