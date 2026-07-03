(() => {
    const consoleOutput = document.getElementById('output');
    const consoleError = document.getElementById('errors');
    const canvas = document.getElementById('canvas');
    const reloadButton = document.getElementById('reload');
    const mountButton = document.getElementById('mount-hostfs');
    const hostfsStatus = document.getElementById('hostfs-status');

    function appendMessage(element, text) {
        element.append(document.createTextNode(text), document.createElement('br'));
        element.scrollTop = element.scrollHeight;
    }

    const emulator = new ZealNative({
        canvas,
        romdisk: 'default.img',
        print(text) {
            console.log(`Log: ${text}`);
            appendMessage(consoleOutput, text);
        },
        printErr(text) {
            console.error(`Error: ${text}`);
            appendMessage(consoleError, text);
        },
        onLoadingChange(loading) {
            reloadButton.disabled = loading;
            mountButton.disabled = loading || !HostFS.supported;
        },
    });

    function reportError(error) {
        console.error(error);
        appendMessage(consoleError, error.message || String(error));
    }

    async function mountHostFS() {
        try {
            const hostfs = await HostFS.mount({
                onError(operation, error) {
                    appendMessage(
                        consoleError,
                        `[HostFS] ${operation}: ${error.message || error}`
                    );
                },
            });
            emulator.setHostFS(hostfs);
            hostfsStatus.textContent = `HostFS mounted: ${hostfs.directory.name}`;
            await emulator.reload();
        } catch (error) {
            if (error.name !== 'AbortError') reportError(error);
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
    mountButton.addEventListener('click', mountHostFS);
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
