(() => {
  // TODO: edit the user program here
  const USER_PROGRAM = "microbe.bin";
  const ROMDISK = 'default.img';

  console.log("Zeal Native Minimal Loading...");

  const canvas = document.getElementById("canvas");
  let moduleInstance = null;
  function loadModule(romdisk, userProgram) {
    const defaultModule = {
      arguments: ["-u", "roms/user.bin"],
      print: function (text) {
        console.log("Log: " + text);
      },
      printErr: function (text) {
        console.log("Error: " + text);
      },
      canvas: (function () {
        return canvas;
      })(),
      onRuntimeInitialized: function () {
        if(!this.FS.analyzePath('/roms').exists) {
          this.FS.mkdir('/roms');
        }
        this.FS.writeFile("/roms/default.img", romdisk);
        this.FS.writeFile("/roms/user.bin", userProgram);
        canvas.setAttribute("tabindex", "0");
        canvas.focus();
      },
    };
    NativeModule(defaultModule).then((mod) => (moduleInstance = mod));
  }

  async function load () {

    const [romdisk, userProgram] = await Promise.all([
      fetch(ROMDISK).then((response) => {
        if(!response.ok) throw new Error(`Failed to fetch ${ROMDISK}`);
        return response.arrayBuffer();
      }).then((buffer) => new Uint8Array(buffer)),
      fetch(USER_PROGRAM).then((response) => {
        if(!response.ok) throw new Error(`Failed to fetch ${USER_PROGRAM}`);
        return response.arrayBuffer();
      }).then((buffer) => new Uint8Array(buffer))
    ]).catch((err) => console.error(err));
    loadModule(romdisk, userProgram);
  };

  window.addEventListener("load", load);

  // call this to re-initialize sound
  // usually triggered by a user action (click)
  function resumeAudioIfNeeded() {
    if (NativeModule.audioContext && NativeModule.audioContext.state === "suspended") {
      NativeModule.audioContext.resume().then(() => {
        console.log("Audio context resumed");
      });
    }
  }

  console.log("Zeal Native Minimal Loaded!");
})();
