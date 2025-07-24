/**
 * Context class, for accessing various UI elements and other dynamic items
 */
class NativeContext {
    get output() {
        return document.getElementById("output");
    }
    get error() {
        return document.getElementById("errors");
    }
    get canvas() {
        return document.getElementById("canvas");
    }
}
var ctx = new NativeContext();

/***************************************************
 * Event Handlers
 ***************************************************/
async function resumeAudioIfNeeded() {
    if (Module.audioContext && Module.audioContext.state === "suspended") {
        await Module.audioContext.resume();
        console.log("Audio context resumed");
    }
    ctx.canvas.removeEventListener("click", resumeAudioIfNeeded);
}
ctx.canvas.addEventListener("click", resumeAudioIfNeeded);

document.getElementById('canvas-smoothing').addEventListener('change', function(e) {
    const checked = e.target.checked;
    console.log('canvas-smoothing', checked);
    if(checked) {
        ctx.canvas.classList.add('smooth');
    } else {
        ctx.canvas.classList.remove('smooth');
    }
});

/***************************************************
 * C Bindings
 ***************************************************/

/**
 * extern uint8_t* files_import_js(uint32_t* size);
 * @param {*} size_ptr
 * @returns
 */
async function files_import_js(size_ptr) {
    const input = document.createElement("input");
    input.type = "file";

    return await new Promise((resolve) => {
        input.onchange = () => {
            const file = input.files[0];
            const reader = new FileReader();
            reader.onload = () => {
                const array = new Uint8Array(reader.result);
                const ptr = _malloc(array.length);
                HEAPU8.set(array, ptr);
                /* Write the size pointer with the file size */
                setValue(size_ptr, array.length, "i32");
                resolve(ptr);
            };
            reader.readAsArrayBuffer(file);
        };
        input.click();
    });
}



/***************************************************
 * Emscripten Module
 ***************************************************/

/**
 * Print to the standard console
 * @param {*} text
 */
function print(text) {
    console.log("Log: " + text);
    ctx.output.innerHTML += text + "<br>";
    ctx.output.scrollTop = output.scrollHeight;
};

/**
 * Print to the error console
 * @param {*} text
 */
function printErr(text) {
    console.log("Error: " + text);
    ctx.error.innerHTML += text + "<br>";
    ctx.error.scrollTop = error.scrollHeight;
};

/**
 * Emscripten runtime init
 */
function onRuntimeInitialized() {
    ctx.canvas.setAttribute("tabindex", "0");
    ctx.canvas.focus();

    ctx.canvas.addEventListener(
        "keydown",
        function (e) {
            e.preventDefault();
            e.stopPropagation();
        },
        true
    );

    ctx.canvas.addEventListener(
        "keyup",
        function (e) {
            e.preventDefault();
            e.stopPropagation();
        },
        true
    );

    window.addEventListener(
        "keydown",
        function (e) {
            if (
                [
                    "ArrowUp",
                    "ArrowDown",
                    "ArrowLeft",
                    "ArrowRight",
                    " ",
                    "Tab",
                    "Escape",
                ].includes(e.key)
            ) {
                e.preventDefault();
                e.stopPropagation();
            }
        },
        true
    );
};

var Module = {
    print,
    printErr,
    canvas: ctx.canvas,
    onRuntimeInitialized,
};
