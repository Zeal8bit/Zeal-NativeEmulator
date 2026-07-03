(() => {
    // TODO: edit the user program here
    const USER_PROGRAM = 'microbe.bin';
    const ROMDISK = 'default.img';

    console.log('Zeal Native Game Loading...');

    const canvas = document.getElementById('canvas');
    let moduleInstance = null;
    function loadModule(romdisk, userProgram) {
        const defaultModule = {
            arguments: ['-u', 'roms/user.bin'],
            print: function(text) {
                    console.log("Log: " + text);
                },
                printErr: function(text) {
                    console.log("Error: " + text);
            },
            canvas: (function() {
                return canvas;
            })(),
            onRuntimeInitialized: function() {
                if (!this.FS.analyzePath('/roms').exists) {
                    this.FS.mkdir('/roms');
                }
                this.FS.writeFile('/roms/default.img', romdisk);
                this.FS.writeFile('/roms/user.bin', userProgram);
                canvas.setAttribute('tabindex', '0');
                canvas.focus();
            }
        };
        NativeModule(defaultModule).then(mod => moduleInstance = mod);
    }

    async function fetchBinary(path) {
        const response = await fetch(path);
        if (!response.ok) throw new Error(`Failed to fetch ${path}`);
        return new Uint8Array(await response.arrayBuffer());
    }

    window.addEventListener('load', async () => {
        try {
            const [romdisk, userProgram] = await Promise.all([
                fetchBinary(ROMDISK),
                fetchBinary(USER_PROGRAM),
            ]);
            loadModule(romdisk, userProgram);
        } catch (err) {
            console.error(err);
        }
    });

    function resumeAudioIfNeeded() {
        if (moduleInstance?.audioContext?.state === 'suspended') {
            moduleInstance.audioContext.resume().then(() => {
                console.log("Audio context resumed");
            });
        }
    }



    const bStart = document.querySelector('#controls .buttons-start');
    const bSelect = document.querySelector('#controls .buttons-select');

    const bUp = document.querySelector('#controls .d-pad-up');
    const bRight = document.querySelector('#controls .d-pad-right');
    const bDown = document.querySelector('#controls .d-pad-down');
    const bLeft = document.querySelector('#controls .d-pad-left');

    const bA = document.querySelector('#controls .buttons-a');
    const bB = document.querySelector('#controls .buttons-b');
    const bX = document.querySelector('#controls .buttons-x');
    const bY = document.querySelector('#controls .buttons-y');

    const bL = document.querySelector('#controls .buttons-l');
    const bR = document.querySelector('#controls .buttons-r');

    function sendKey(type, key, code, keyCode) {
        const e = {
            key,
            code,
            keyCode,
            which: keyCode,
            bubbles: true,
        };
        canvas.dispatchEvent(new KeyboardEvent(type, e));
    }

    function attachKeypressListener(el, key, code, keyCode) {
        let pressed = false;

        const onPress = (e) => {
            e.preventDefault();
            if (pressed) return;
            pressed = true;
            el.setPointerCapture?.(e.pointerId);
            resumeAudioIfNeeded();
            sendKey('keydown', key, code, keyCode);
        };
        const onRelease = (e) => {
            e.preventDefault();
            if (!pressed) return;
            pressed = false;
            sendKey('keyup', key, code, keyCode);
        };

        el.addEventListener('pointerdown', onPress);
        el.addEventListener('pointerup', onRelease);
        el.addEventListener('pointercancel', onRelease);
        el.addEventListener('lostpointercapture', onRelease);
    }

    attachKeypressListener(bStart, "Enter", "Enter", 13);
    attachKeypressListener(bSelect, "'", "Quote", 222);

    attachKeypressListener(bUp, "ArrowUp", "ArrowUp", 38);
    attachKeypressListener(bRight, "ArrowRight", "ArrowRight", 39);
    attachKeypressListener(bDown, "ArrowDown", "ArrowDown", 40);
    attachKeypressListener(bLeft, "ArrowLeft", "ArrowLeft", 37);

    attachKeypressListener(bA, "x", "KeyX", 88);   // X
    attachKeypressListener(bB, "z", "KeyZ", 90);   // Z
    attachKeypressListener(bX, "s", "KeyS", 83);   // S
    attachKeypressListener(bY, "a", "KeyA", 65);   // A

    attachKeypressListener(bL, "q", "KeyQ", 81);   // Q
    attachKeypressListener(bR, "w", "KeyW", 87);   // W

    console.log('Zeal Native Game Loaded!');
})();
