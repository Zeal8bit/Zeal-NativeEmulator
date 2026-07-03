(() => {
    const canvas = document.getElementById('canvas');
    const emulator = new ZealNative({
        canvas,
        romdisk: 'default.img',
        userProgram: 'microbe.bin',
    });

    window.addEventListener('load', () => {
        emulator.start().catch(error => console.error(error));
    });

    function sendKey(type, key, code, keyCode) {
        canvas.dispatchEvent(new KeyboardEvent(type, {
            key,
            code,
            keyCode,
            which: keyCode,
            bubbles: true,
        }));
    }

    function attachKeypressListener(element, key, code, keyCode) {
        let pressed = false;
        const onPress = event => {
            event.preventDefault();
            if (pressed) return;
            pressed = true;
            element.setPointerCapture?.(event.pointerId);
            emulator.resumeAudio().catch(error => console.error(error));
            sendKey('keydown', key, code, keyCode);
        };
        const onRelease = event => {
            event.preventDefault();
            if (!pressed) return;
            pressed = false;
            sendKey('keyup', key, code, keyCode);
        };

        element.addEventListener('pointerdown', onPress);
        element.addEventListener('pointerup', onRelease);
        element.addEventListener('pointercancel', onRelease);
        element.addEventListener('lostpointercapture', onRelease);
    }

    const controls = [
        ['.buttons-start', 'Enter', 'Enter', 13],
        ['.buttons-select', "'", 'Quote', 222],
        ['.d-pad-up', 'ArrowUp', 'ArrowUp', 38],
        ['.d-pad-right', 'ArrowRight', 'ArrowRight', 39],
        ['.d-pad-down', 'ArrowDown', 'ArrowDown', 40],
        ['.d-pad-left', 'ArrowLeft', 'ArrowLeft', 37],
        ['.buttons-a', 'x', 'KeyX', 88],
        ['.buttons-b', 'z', 'KeyZ', 90],
        ['.buttons-x', 's', 'KeyS', 83],
        ['.buttons-y', 'a', 'KeyA', 65],
        ['.buttons-l', 'q', 'KeyQ', 81],
        ['.buttons-r', 'w', 'KeyW', 87],
    ];
    for (const [selector, key, code, keyCode] of controls) {
        attachKeypressListener(document.querySelector(`#controls ${selector}`), key, code, keyCode);
    }
})();
