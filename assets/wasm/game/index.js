(() => {
    const canvas = document.getElementById('canvas');
    const emulator = new ZealNative({
        canvas,
        romdisk: 'default.img',
        userProgram: 'microbe.bin',
    });

    window.addEventListener('load', () => {
        emulator.start()
            .then(() => emulator.clearSnesButtons())
            .catch(error => console.error(error));
    });

    const releaseButtons = [];

    function attachButtonListener(element, button) {
        let pressed = false;
        const onPress = event => {
            event.preventDefault();
            if (pressed) return;
            pressed = true;
            element.setPointerCapture?.(event.pointerId);
            emulator.resumeAudio().catch(error => console.error(error));
            emulator.setSnesButton(button, true);
        };
        const onRelease = event => {
            event.preventDefault();
            if (!pressed) return;
            pressed = false;
            emulator.setSnesButton(button, false);
        };

        element.addEventListener('pointerdown', onPress);
        element.addEventListener('pointerup', onRelease);
        element.addEventListener('pointercancel', onRelease);
        element.addEventListener('lostpointercapture', onRelease);
        releaseButtons.push(() => {
            pressed = false;
        });
    }

    function clearButtons() {
        for (const releaseButton of releaseButtons) releaseButton();
        emulator.clearSnesButtons();
    }

    const controls = [
        ['.buttons-start', 'start'],
        ['.buttons-select', 'select'],
        ['.d-pad-up', 'up'],
        ['.d-pad-right', 'right'],
        ['.d-pad-down', 'down'],
        ['.d-pad-left', 'left'],
        ['.buttons-a', 'a'],
        ['.buttons-b', 'b'],
        ['.buttons-x', 'x'],
        ['.buttons-y', 'y'],
        ['.buttons-l', 'l'],
        ['.buttons-r', 'r'],
    ];
    for (const [selector, button] of controls) {
        attachButtonListener(document.querySelector(`#controls ${selector}`), button);
    }

    window.addEventListener('blur', clearButtons);
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) clearButtons();
    });
})();
