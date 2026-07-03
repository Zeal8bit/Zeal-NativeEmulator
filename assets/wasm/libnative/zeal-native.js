(() => {
    const SNES_BUTTONS = Object.freeze({
        b: 0,
        y: 1,
        select: 2,
        start: 3,
        up: 4,
        down: 5,
        left: 6,
        right: 7,
        a: 8,
        x: 9,
        l: 10,
        r: 11,
    });

    class ZealNative {
        constructor({
            canvas,
            moduleFactory = window.NativeModule,
            romdisk = null,
            userProgram = null,
            eeprom = null,
            tf = null,
            romdiskPath = '/roms/default.img',
            userProgramPath = '/roms/user.bin',
            eepromPath = '/roms/eeprom.img',
            tfPath = '/roms/tf.img',
            arguments: moduleArguments = null,
            hostfs = null,
            print = text => console.log(`Log: ${text}`),
            printErr = text => console.error(`Error: ${text}`),
            onLoadingChange = null,
            onRuntimeInitialized = null,
            onExit = null,
        } = {}) {
            if (!canvas) throw new TypeError('ZealNative requires a canvas');
            if (typeof moduleFactory !== 'function') {
                throw new TypeError('ZealNative requires a module factory');
            }

            this.canvas = canvas;
            this.moduleFactory = moduleFactory;
            this.romdisk = romdisk;
            this.userProgram = userProgram;
            this.eeprom = eeprom;
            this.tf = tf;
            this.romdiskPath = romdiskPath;
            this.userProgramPath = userProgramPath;
            this.eepromPath = eepromPath;
            this.tfPath = tfPath;
            this.arguments = moduleArguments == null ? [] : [...moduleArguments];
            this.addAssetArgument('-u', userProgramPath, userProgram);
            this.addAssetArgument('-e', eepromPath, eeprom);
            this.addAssetArgument('-t', tfPath, tf);
            this.hostfs = hostfs;
            this.print = print;
            this.printErr = printErr;
            this.onLoadingChange = onLoadingChange;
            this.onRuntimeInitialized = onRuntimeInitialized;
            this.onExit = onExit;
            this.module = null;
            this.exitPromise = null;
            this.loading = false;
            this.operationPromise = null;
            this.binaryPromises = new Map();
        }

        addAssetArgument(flag, path, source) {
            if (source != null && !this.arguments.includes(flag)) {
                this.arguments.push(flag, path.replace(/^\//, ''));
            }
        }

        async resolveBinary(source) {
            if (source == null) return null;
            if (source instanceof Uint8Array) return source;
            if (source instanceof ArrayBuffer) return new Uint8Array(source);
            if (typeof source !== 'string' && !(source instanceof URL)) {
                throw new TypeError('Binary source must be a URL, Uint8Array, or ArrayBuffer');
            }

            const key = source.toString();
            if (!this.binaryPromises.has(key)) {
                this.binaryPromises.set(key, fetch(source).then(async response => {
                    if (!response.ok) throw new Error(`Failed to fetch ${key}`);
                    return new Uint8Array(await response.arrayBuffer());
                }));
            }
            return this.binaryPromises.get(key);
        }

        ensureDirectory(fs, path) {
            const parts = path.split('/').filter(Boolean);
            let current = '';
            for (const part of parts) {
                current += `/${part}`;
                if (!fs.analyzePath(current).exists) fs.mkdir(current);
            }
        }

        writeBinary(fs, path, data) {
            const separator = path.lastIndexOf('/');
            const parent = separator > 0 ? path.slice(0, separator) : '';
            if (parent) this.ensureDirectory(fs, parent);
            fs.writeFile(path, data);
        }

        async loadModule() {
            const [romdisk, userProgram, eeprom, tf] = await Promise.all([
                this.resolveBinary(this.romdisk),
                this.resolveBinary(this.userProgram),
                this.resolveBinary(this.eeprom),
                this.resolveBinary(this.tf),
            ]);

            let resolveExit;
            const exitPromise = new Promise(resolve => {
                resolveExit = resolve;
            });
            let instance = null;
            const owner = this;
            const moduleOptions = {
                arguments: [...this.arguments],
                hostfsBackend: this.hostfs,
                print: this.print,
                printErr: this.printErr,
                canvas: this.canvas,
                onRuntimeInitialized() {
                    if (romdisk) owner.writeBinary(this.FS, owner.romdiskPath, romdisk);
                    if (userProgram) owner.writeBinary(this.FS, owner.userProgramPath, userProgram);
                    if (eeprom) owner.writeBinary(this.FS, owner.eepromPath, eeprom);
                    if (tf) owner.writeBinary(this.FS, owner.tfPath, tf);
                    owner.canvas.setAttribute('tabindex', '0');
                    owner.canvas.focus();
                    owner.onRuntimeInitialized?.(this);
                },
                onExit(exitCode) {
                    resolveExit(exitCode);
                    if (owner.module === instance || owner.module === this) {
                        owner.module = null;
                        owner.exitPromise = null;
                    }
                    owner.onExit?.(exitCode);
                },
            };

            this.hostfs?.bindModule(moduleOptions);
            instance = await this.moduleFactory(moduleOptions);
            this.hostfs?.bindModule(instance);
            this.module = instance;
            this.exitPromise = exitPromise;
            return instance;
        }

        runExclusive(operation) {
            if (this.operationPromise) return this.operationPromise;
            this.loading = true;
            this.onLoadingChange?.(true);
            this.operationPromise = Promise.resolve()
                .then(operation)
                .finally(() => {
                    this.loading = false;
                    this.operationPromise = null;
                    this.onLoadingChange?.(false);
                });
            return this.operationPromise;
        }

        start() {
            return this.runExclusive(() => this.module || this.loadModule());
        }

        async exitModule() {
            const module = this.module;
            const exitPromise = this.exitPromise;
            if (!module) return;
            module._zeal_exit_web();
            if (exitPromise) await exitPromise;
        }

        exit() {
            return this.runExclusive(() => this.exitModule());
        }

        reload() {
            return this.runExclusive(async () => {
                await this.exitModule();
                return this.loadModule();
            });
        }

        setHostFS(hostfs) {
            this.hostfs = hostfs;
            return this;
        }

        resumeAudio() {
            const contexts = new Set();
            if (this.module?.audioContext) contexts.add(this.module.audioContext);
            for (const device of window.miniaudio?.devices || []) {
                if (device?.webaudio) contexts.add(device.webaudio);
            }
            return Promise.all([...contexts]
                .filter(context => context.state === 'suspended')
                .map(context => context.resume()));
        }

        setSnesButton(button, pressed) {
            if (!Object.prototype.hasOwnProperty.call(SNES_BUTTONS, button)) {
                throw new RangeError(`Unknown SNES button: ${button}`);
            }
            if (!this.module?._zeal_snes_button_web) return false;
            this.module._zeal_snes_button_web(SNES_BUTTONS[button], pressed ? 1 : 0);
            return true;
        }

        clearSnesButtons() {
            if (!this.module?._zeal_snes_clear_web) return false;
            this.module._zeal_snes_clear_web();
            return true;
        }

        toggleDebugger() {
            if (!this.module?._zeal_debug_toggle_web) return false;
            this.module._zeal_debug_toggle_web();
            this.canvas.focus();
            return true;
        }

        toggleFps() {
            if (!this.module || !this.module._show_fps) return false;
            const showFps = !!this.module.getValue(this.module._show_fps, 'i8');
            this.module.setValue(this.module._show_fps, showFps ? 0 : 1, 'i8');
            return true;
        }
    }

    window.ZealNative = ZealNative;
})();
