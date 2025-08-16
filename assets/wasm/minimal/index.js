(() => {
    // TODO: edit the user program here
    const USER_PROGRAM = 'microbe.bin';

    console.log('Zeal Native Minimal Loading...');

    const canvas = document.getElementById('canvas');
    let moduleInstance = null;
    function loadModule(userProgram) {
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
                this.FS.writeFile('/roms/user.bin', userProgram);
                canvas.setAttribute('tabindex', '0');
                canvas.focus();
            }
        };
        Module(defaultModule).then(mod => moduleInstance = mod);
    }

    window.addEventListener('load', async () => {
        await fetch(USER_PROGRAM).then((response) => {
            if(!response.ok) throw new Error(`Failed to fetch ${USER_PROGRAM}`);
            return response.arrayBuffer();
        }).then((buffer) => {
            console.log('buffer', buffer);
            return new Uint8Array(buffer);
        }).then(file => {
            loadModule(file);
        }).catch(err => console.error(err));
    });

    function resumeAudioIfNeeded() {
        if (Module.audioContext && Module.audioContext.state === 'suspended') {
            Module.audioContext.resume().then(() => {
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
        console.log('key', type, key, code);
        canvas.dispatchEvent(new KeyboardEvent(type, e));
    }

    function attachKeypressListener(el, key, code, keyCode) {
        const onPress = (e) => {
            e.preventDefault();
            sendKey('keydown', key, code, keyCode);
        }
        const onRelease = (e) => {
            e.preventDefault();
            sendKey('keyup', key, code, keyCode);
        }

        el.addEventListener('mousedown', onPress);
        el.addEventListener('mouseup', onRelease);

        el.addEventListener('touchstart', onPress, { passive: false });
        el.addEventListener('touchend', onRelease);
        el.addEventListener('touchcancel', onRelease);
    }

    attachKeypressListener(bStart, "Enter", "Enter", 13);
    attachKeypressListener(bStart, "'", "Quote", 222);

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

    console.log('Zeal Native Minimal Loaded!');
})();
