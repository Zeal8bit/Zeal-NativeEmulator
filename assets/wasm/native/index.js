(() => {
    const consoleOutput = document.getElementById('output');
    const canvas = document.getElementById('canvas');
    const reloadButton = document.getElementById('reload');
    const saveImagesButton = document.getElementById('save-images');
    const clearImagesButton = document.getElementById('clear-images');
    const mountButton = document.getElementById('mount-hostfs');
    const hostfsStatus = document.getElementById('hostfs-status');
    let mountedHostFS = null;
    let hostfsChanging = false;
    let emulatorLoading = false;

    function updateHostFSButton() {
        mountButton.disabled = hostfsChanging || emulatorLoading || !HostFS.supported;
    }

    function appendLog(element, text, type='log') {
        console[type]?.(`Log: ${text}`);
        const entry = document.createElement('div');
        entry.classList.add('log-entry', type);
        entry.textContent = text;
        element.append(entry);
        element.scrollTop = element.scrollHeight;
    }

    const emulator = new ZealNative({
        canvas,
        romdisk: 'default.img',
        // eeprom: 'eeprom.img',
        // tf: 'tf.img',
        imagePersistence: true,
        onImageConflict(image) {
            const useRemote = window.confirm(
                `${image.key} changed on the server, but your local image has saved changes.\n\n` +
                'Use the updated server image? Press Cancel to keep your local image.'
            );
            return useRemote ? 'remote' : 'local';
        },
        print(text) {
            appendLog(consoleOutput, text);
        },
        printErr(text) {
            appendLog(consoleOutput, text, 'error');
        },
        onLoadingChange(loading) {
            emulatorLoading = loading;
            reloadButton.disabled = loading;
            saveImagesButton.disabled = loading;
            clearImagesButton.disabled = loading;
            updateHostFSButton();
        },
    });

    function reportError(error) {
        console.error(error);
        appendLog(consoleOutput, error.message || String(error), 'error');
    }

    async function mountHostFS() {
        const hostfs = await HostFS.mount({
            onError(operation, error) {
                appendLog(
                    consoleOutput,
                    `[HostFS] ${operation}: ${error.message || error}`,
                    'error'
                );
            },
        });
        emulator.setHostFS(hostfs);
        mountedHostFS = hostfs;
        mountButton.textContent = 'Unmount HostFS';
        hostfsStatus.textContent = `HostFS mounted: ${hostfs.directory.name}`;
        await emulator.reload();
    }

    async function unmountHostFS() {
        const confirmed = window.confirm(
            'Unmounting HostFS will restart the emulator and lose any unsaved work. Continue?'
        );
        if (!confirmed) return;

        emulator.setHostFS(null);
        mountedHostFS = null;
        mountButton.textContent = 'Mount HostFS';
        hostfsStatus.textContent = 'HostFS not mounted';
        await emulator.reload();
    }

    async function toggleHostFS() {
        if (hostfsChanging) return;
        hostfsChanging = true;
        updateHostFSButton();
        try {
            await (mountedHostFS ? unmountHostFS() : mountHostFS());
        } catch (error) {
            if (error.name !== 'AbortError') reportError(error);
        } finally {
            hostfsChanging = false;
            updateHostFSButton();
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
        }
    }, true);

    document.getElementById('unmute').addEventListener('click', () => {
        emulator.resumeAudio().catch(reportError);
    });
    reloadButton.addEventListener('click', () => emulator.reload().catch(reportError));
    saveImagesButton.addEventListener('click', async () => {
        try {
            const saved = await emulator.saveDiskImages();
            appendLog(consoleOutput, `Disk images saved: ${saved.join(', ') || 'none'}`);
        } catch (error) {
            reportError(error);
        }
    });
    clearImagesButton.addEventListener('click', async () => {
        try {
            await emulator.clearDiskImages();
            appendLog(consoleOutput, 'Cached disk images cleared');
            await emulator.reload();
        } catch (error) {
            reportError(error);
        }
    });
    mountButton.addEventListener('click', toggleHostFS);
    document.getElementById('toggle-debugger').addEventListener('click', () => {
        emulator.toggleDebugger();
    });
    document.getElementById('toggle-fps').addEventListener('click', () => {
        emulator.toggleFps();
    });
    document.getElementById('canvas-smoothing').addEventListener('change', event => {
        canvas.classList.toggle('smooth', event.target.checked);
    });

    if (!HostFS.supported) {
        mountButton.disabled = true;
        hostfsStatus.textContent = HostFS.unavailableReason;
    }

    window.addEventListener('load', () => emulator.start().catch(reportError));
})();
