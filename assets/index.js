async function files_import_js(size_ptr)
{
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
