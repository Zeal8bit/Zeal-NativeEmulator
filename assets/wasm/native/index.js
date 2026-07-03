(() => {
    const ROMDISK = 'default.img';

    const consoleOutput = document.getElementById('output');
    const consoleError = document.getElementById('errors');
    const canvas = document.getElementById('canvas');
    const reloadButton = document.getElementById('reload');

    let moduleInstance = null;
    let loading = false;
    let romdiskPromise = null;

    function appendMessage(element, text) {
        element.append(document.createTextNode(text), document.createElement('br'));
        element.scrollTop = element.scrollHeight;
    }

    async function fetchRomdisk() {
        const response = await fetch(ROMDISK);
        if (!response.ok) throw new Error(`Failed to fetch ${ROMDISK}`);
        return new Uint8Array(await response.arrayBuffer());
    }

    function getRomdisk() {
        romdiskPromise ??= fetchRomdisk();
        return romdiskPromise;
    }

    async function loadModule() {
        const romdisk = await getRomdisk();
        const defaultModule = {
            print(text) {
                console.log(`Log: ${text}`);
                appendMessage(consoleOutput, text);
            },
            printErr(text) {
                console.error(`Error: ${text}`);
                appendMessage(consoleError, text);
            },
            canvas,
            onRuntimeInitialized() {
                if (!this.FS.analyzePath('/roms').exists) {
                    this.FS.mkdir('/roms');
                }
                this.FS.writeFile('/roms/default.img', romdisk);
                canvas.focus();
            },
        };

        moduleInstance = await NativeModule(defaultModule);
    }

    async function startModule() {
        if (loading) return;

        loading = true;
        reloadButton.disabled = true;
        try {
            await loadModule();
        } catch (error) {
            console.error(error);
            appendMessage(consoleError, error.message || String(error));
        } finally {
            loading = false;
            reloadButton.disabled = false;
        }
    }

    async function reloadModule() {
        if (loading) return;

        loading = true;
        reloadButton.disabled = true;
        try {
            moduleInstance?._zeal_exit();
            moduleInstance = null;
            await new Promise(resolve => setTimeout(resolve, 100));
            await loadModule();
        } catch (error) {
            console.error(error);
            appendMessage(consoleError, error.message || String(error));
        } finally {
            loading = false;
            reloadButton.disabled = false;
        }
    }

    function resumeAudioIfNeeded() {
        const devices = window.miniaudio?.devices || [];
        for (const device of devices) {
            if (device?.webaudio?.state === 'suspended') {
                device.webaudio.resume().catch(error => {
                    console.error('Failed to resume audio context', error);
                });
            }
        }
    }

    canvas.addEventListener('keydown', event => {
        event.preventDefault();
        event.stopPropagation();
    }, true);

    canvas.addEventListener('keyup', event => {
        event.preventDefault();
        event.stopPropagation();
    }, true);

    window.addEventListener('keydown', event => {
        if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' ', 'Tab', 'Escape'].includes(event.key)) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, true);

    document.getElementById('unmute').addEventListener('click', resumeAudioIfNeeded);
    reloadButton.addEventListener('click', reloadModule);

    document.getElementById('toggle-fps').addEventListener('click', () => {
        if (!moduleInstance) return;
        const showFps = !!moduleInstance.getValue(moduleInstance._show_fps, 'i8');
        moduleInstance.setValue(moduleInstance._show_fps, showFps ? 0 : 1, 'i8');
    });

    document.getElementById('canvas-smoothing').addEventListener('change', event => {
        canvas.classList.toggle('smooth', event.target.checked);
    });

    window.addEventListener('load', startModule);
})();
