/**
 * @brief Add all the JS functions that needs to be accessed by the C code in this object.
 * The name of each entry much match the one in the C declaration.
 */
const bindings = {
    files_import_js: (size_ptr) => Asyncify.handleAsync(async () => { return files_import_js(size_ptr) })
};

mergeInto(LibraryManager.library, bindings);
