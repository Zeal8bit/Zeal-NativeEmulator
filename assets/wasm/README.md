# WASM images and storage

The browser build uses Raylib 6.2, Emscripten 7.0.0, WasmFS and Asyncify. Chrome
is the supported browser. Serve over HTTPS (or localhost for development).
GitHub Pages works without custom response headers, cross-origin isolation,
SharedArrayBuffer, or service workers. Other browsers may work with the same APIs.

## Configuring images

Every template constructs `ZealNative`. Sources can be URLs, `Uint8Array`, or
`ArrayBuffer`. Only configured images are loaded; EEPROM and TF are optional.

```js
const emulator = new ZealNative({
    canvas: document.querySelector('canvas'),
    romdisk: 'default.img',
    eeprom: 'eeprom.img',
    tf: 'tf.img',
    imageStorage: 'opfs',
    onImageUpdateAvailable(update) {
        // Display your own notice. This callback does not approve replacement.
        console.info(update.name, update.local, update.remote);
    },
    onStorageError(error) {
        console.error(error);
    },
});
await emulator.start();
```

`imageStorage` selects `opfs` (default), `indexeddb`, or `memory`. There is no
silent fallback. The old `imagePersistence`/`onImageConflict` API is replaced.
Existing IndexedDB records are left intact and are not migrated automatically.
IndexedDB mode retains explicit Save behavior; memory mode retains changes only
for the lifetime of the `ZealNative` instance. HostFS remains a separate feature.

OPFS files live below `zeal-native/<hash of page directory URL>/`. Different page
directories have separate stores. A manifest maps image keys to active files;
`diskImages: {tf: {key: 'my-card'}}` can explicitly identify a card. The default
key is its configured source URL, or the image filename for byte sources. Keep
keys stable to reuse images. Only one emulator can own a store at a time, using
Web Locks. Closing the tab releases ownership.

WasmFS reads and writes the OPFS files directly. There is no periodic full-image
copy. The non-threaded Emscripten OPFS backend uses asynchronous file streams
and commits each filesystem write; it is slower than memory storage. A local
Chrome run measured 32 sector writes to a 2 MB TF image at about 285–340 ms,
versus 0.4 ms in memory. This is an indicative small-image benchmark, not a
comparison with native desktop disk I/O. IndexedDB remains available when
explicit snapshot saves are preferable. EEPROM and TF retain normal C stdio buffering. NOR flash is kept in device
memory until Save or orderly exit. `saveDiskImages()` flushes all three devices;
`reload()` and `exit()` flush before teardown. Operations are queued and reject
on failure. Abrupt tab closure or a crash can lose unflushed buffers; browser
unload callbacks cannot guarantee saving them. OPFS remains subject to browser
quota, eviction, and the user's site-data controls.

## Remote updates

Startup checks do not delay booting a local image. A check uses ETag, falling back
to Last-Modified. Servers must allow fetching the source and expose validators
through CORS when cross-origin. Without validators the library logs that update
detection is unavailable. Validation failures leave local images untouched.

```js
const updates = await emulator.checkImageUpdates();
// Your UI decides when to apply updates. This discards guest RAM and replaces
// local image contents, including guest modifications to the selected images.
await emulator.applyImageUpdates(updates.map(update => update.name)); // Empty list is a no-op.
```

The callback and check results contain `{name, source, local, remote}`, where
`local` and `remote` contain `{etag, lastModified}`. A different validator means a
changed remote version, not necessarily a chronologically newer one. Checks
never replace local images, even if the local copy is unmodified. The library
does not show prompts. The native template offers a nonmodal update button;
minimal/game templates log availability for their host UI to handle.

Replacement downloads complete before stopping a running emulator. A manifest
commit activates selected replacements together, then the emulator restarts.
Failed downloads leave it running; failed activation preserves the old manifest.
Previously active files are removed only after a successful commit.

`clearDiskImages()` stops a running emulator, removes configured images and their
metadata from the selected backend, then restarts and downloads fresh copies.
It is destructive; the template is responsible for confirmation. It does not
clear unrelated images, old IndexedDB records, or HostFS files.
`diskImageUsage()` returns active image bytes, excluding metadata and staged files.

## Building and checking

Activate Emscripten 7.0.0, then:

```sh
meson setup build-wasm --cross-file wasm-cross.build
meson compile -C build-wasm
node tools/wasm-tests/run.mjs build-wasm
```

The test runner uses Node 22+ and Chrome, with no npm dependencies. Set `CHROME`
to the Chrome executable if it is not at the default macOS path or named
`google-chrome` on other platforms. It starts an ordinary localhost server and
an isolated browser profile, tests real device writes and persistence across
page reloads, update safety, backend selection, and emulator lifecycle, then
removes its profile. Generated test modules stay in the build directory.

See `raylib/web/README.md` for the reproducible dependency rebuild. Native
Raylib bundles are independent of the web dependency.
