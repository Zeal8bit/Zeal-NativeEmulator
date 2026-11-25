# WASM images and storage

ROM images are supplied at runtime by the embedding page. The emulator build
does not copy or embed `default.img`; serve the configured image alongside the
page or provide its URL through the template configuration.

The browser build uses the Emscripten SDL3 port, Emscripten 7.0.0, WasmFS and Asyncify. Chrome
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

`imageStorage` selects `opfs` (default) or `memory`. There is no silent fallback.
IndexedDB storage is no longer supported; selecting `indexeddb` throws
`RangeError`. Existing IndexedDB records are left intact and are not migrated
automatically. The old `imagePersistence`/`onImageConflict` API is replaced.
Memory mode retains changes only for the lifetime of the `ZealNative` instance.
HostFS remains a separate feature.

### Application workspaces

`storageNamespace` is an optional initialization string that selects an application
workspace. If omitted (or `undefined`), the namespace is still the SHA-256 hash of
`new URL('.', document.baseURI).href`. Existing page-directory storage locations,
manifest keys, and locks are unchanged.

An explicit namespace is an exact, case-sensitive identifier shared across pages
on the same origin. It must be a non-empty primitive string; empty/whitespace-only
strings, `null`, and other types throw `TypeError` during initialization, before
storage is accessed. Names are not URL-resolved, trimmed, or normalized, so use
the same spelling and trailing slash in every chapter. Explicit identifiers are
hashed in a separate domain from the default page-directory names; arbitrary
input is never used directly as a filesystem path.

For a Hugo book deployed under a GitHub Pages project path:

```js
const emulator = new ZealNative({
    canvas,
    romdisk: '/zeal-pascal/media/zshell.img',
    tf: '/zeal-pascal/media/tf.img',
    eeprom: '/zeal-pascal/media/eeprom.img',
    imageStorage: 'opfs',
    storageNamespace: '/zeal-pascal/',
});
```

Every chapter using `/zeal-pascal/` and the same image keys shares those persisted
disks. Another project on the same origin can use `/other-project/` for separate
disks, manifests, backend metadata, and Web Locks. Different origins are already
isolated by the browser. This is entirely JavaScript configuration: the same
WASM binary serves every project without recompilation.

OPFS files live below `zeal-native/<namespace hash>/`. Image keys identify
individual disks inside the selected workspace: `diskImages: {tf: {key: 'my-card'}}`
can explicitly identify a card. The default key is its configured source URL, or
the image filename for byte sources. Keep keys stable across chapters; the
absolute image URLs in the example provide stable default keys.

Choose the namespace when creating `ZealNative`. Selecting another namespace
does not delete, reset, or migrate data from the old one; removing the option
returns to that page's original store. Explicit workspaces do not automatically
adopt old page-directory disks. Namespace selection applies to both
backends; `memory` data still lasts only for that instance. Only one emulator can
own a workspace in a given backend at a time, even across different chapter URLs.
Storage ownership is released after orderly exit; closing the tab also releases
its Web Locks. HostFS is independent of this selection.

## HostFS guest outcomes and diagnostics

HostFS sends ordinary filesystem outcomes back through the guest status protocol.
Missing file/directory probes (including executable searches) and wrong entry
kinds return `NO_SUCH_ENTRY` (4) for lookup operations. Invalid descriptors or
access modes, invalid guest paths, existing-directory creation, nonempty-directory
removal, and file-specific write conflicts return a guest failure as appropriate.
`STAT` of an invalid descriptor returns 4; other invalid descriptor operations
return `FAILURE` (1). Failed create/remove/write operations use 1. Descriptor
exhaustion/end-of-listing still return 20/21. These outcomes do not call
`console.error` or notify the embedding page.

`HostFS.mount({onError(operation, error) { ... }})` and the `HostFS` constructor's
same option are reserved for **unexpected emulator/browser integration failures**.
The callback signature and operation labels are unchanged; consumers do not need
to parse operation numbers to distinguish guest errors from emulator failures.
The native template can continue displaying every `onError` notification as an
integration error. This callback is separate from `ZealNative.onStorageError`.

Classification happens around filesystem entry operations, not by globally
ignoring exception names. Before translating a missing entry, wrong entry kind,
or file write conflict, HostFS verifies that the mounted workspace is still
accessible with the needed permission. A removed workspace can also produce
`NotFoundError`; it remains diagnostic and returns guest failure (1), rather than
being mistaken for a missing guest file (4). Lost workspace permissions, browser security,
quota/I/O failures, unexpected JavaScript exceptions, and WASM bridge failures
still call `console.error` and `onError`. No permission prompts or storage resets
are performed during this check.

## Device write completion

WasmFS reads and writes the OPFS files directly; there are no periodic full-image
snapshots or book-side autosave. Guest software owns its buffers. Only writes
that have reached the emulated device are handled here:

- TF flushes a full 512-byte sector after receiving its payload and CRC bytes,
  before returning `TF_WRITE_ACCEPTED`. A short write or failed flush returns
  `TF_WRITE_ERROR` and logs a diagnostic.
- EEPROM writes and flushes its 128-byte backing page at the completed I2C
  page-write transaction's STOP, including transactions containing fewer than
  128 guest data bytes. Seek, write, and flush failures are logged through the
  existing error output; the STOP callback has no error-return channel.
- Incomplete sectors and in-progress EEPROM transactions do not trigger flushes.
  No guest filesystem buffers are inspected or drained.

For the pinned **Emscripten 7.0.0, WasmFS, Asyncify, non-pthread** build,
`fflush(FILE*)` is sufficient to reach persistent OPFS backing storage. It drains
libc's output buffer into WasmFS. The backend's
[`FileSystemAsyncAccessHandle.write()`](https://github.com/emscripten-core/emscripten/blob/7.0.0/src/lib/libwasmfs_opfs.js)
awaits both `FileSystemWritableFileStream.write()` and `close()` before returning;
its separate `flush()` is a no-op. No additional `fsync`/`syncfs` is needed at
these device boundaries. This contract must be rechecked if the backend,
threading configuration, or SDK changes. All device I/O stays on the emulator's
existing Asyncify execution path, not unrelated JavaScript callbacks.

Native builds use the same portable checked stdio flushes, without adding
per-sector/page `fsync()` calls or changing the image format or guest protocol.
This is not a claim of power-loss durability. Browser storage remains subject to
quota, eviction, and the user's site-data controls. A write/flush error can leave
partial data; there is no rollback or atomicity across sectors or guest filesystem
operations. Data still held by the guest, incomplete device transactions, and
NOR changes still buffered by the emulator are not covered by this guarantee.

NOR flash retains its existing policy: it stays in device memory until Save or
orderly exit. `saveDiskImages()` still flushes all three devices; `reload()` and
`exit()` flush before teardown. Memory mode keeps snapshots in the current
`ZealNative` instance; it does not persist data across page reloads.

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
Previously active files are removed only after a successful commit. Failed
manifest commits remove their staged files, including during initial boot, so
retries do not accumulate unused images.

`clearDiskImages()` stops a running emulator, removes configured images and their
metadata from the selected backend, then restarts and downloads fresh copies.
It is destructive; the template is responsible for confirmation. It does not
clear unrelated images, old IndexedDB records, or HostFS files.
`diskImageUsage()` returns active image bytes, excluding metadata and staged files.

## Building and checking

Activate Emscripten 7.0.0 and build from the repository root.

```sh
meson setup build-wasm --cross-file frontend/wasm/wasm-cross.build
meson compile -C build-wasm
```

Emscripten downloads and caches its SDL3 port on first use. Place the configured
images in the output directory and serve it over localhost or HTTPS. Select a
template with `-Dwasm_template=native|minimal|game`. The embedder passes image
sources and a focusable canvas to `new ZealNative({...})`. Keyboard events go to
the focused canvas. SDL maps connected gamepads to SNES controllers. The middle
mouse button toggles capture and the wheel adjusts SNES mouse sensitivity.

### Frontend boundary

Emscripten builds select `frontend/wasm/` (`-Dfrontend=wasm`, or `auto`). It owns
the SDL3 canvas renderer, browser input/audio, loop, Emscripten exports,
OPFS startup, and HostFS bridge. With `-Denable_debugger=true`, it also exposes
the emulator debugger and video previews to JavaScript. The desktop debugger
panels remain in `frontend/sdl-fltk/`.

The native web template no longer offers a debugger button. For compatibility
with existing embedding code, `toggleDebugger()` remains callable and returns
`false`; the old C toggle export is a no-op. FPS, virtual SNES controls, Save,
reload, exit, and storage APIs retain their behavior and export names.

### Browser debugger session

Build with `-Denable_debugger=true` and construct `ZealNative` with
`debugger: true` to start a paused session. The instance's `debugger` property
provides asynchronous `pause()`, `continue()`, `step()`, and `reset()` methods.
Listen to its `statechange` event for the execution state, stop reason, current
registers, and T-states. The newest snapshot is also available as `state`.
A step resolves after its instruction completes. Debugger commands are handled
on the emulator loop between instructions. Sessions created with default
options continue to boot and run automatically.

Add or enable a user breakpoint with `setBreakpoint(address, enabled = true)`
and remove it with `removeBreakpoint(address)`. Set breakpoints before the
first `continue()` to catch the entry instruction. `stepOver()` starts execution
and uses the native debugger's temporary breakpoint. Watch for the next
`statechange` to observe where it stopped.

`disassemble(address, count = 20)` returns instruction rows with addresses,
labels, assembly text, and copied opcode bytes. Supply map bytes to
`loadSymbols(bytes)` and resolve names with `findSymbol(name)`.

`inspectMemory(address, count)` returns copied logical memory bytes and four
current MMU mappings from the same instruction boundary. The read wraps at the
end of 16-bit address space. Pass zero to get the mappings alone. Use
`readMemory(address, count)` for just the bytes. `writeMemory(address, bytes)`
requires a paused machine and returns what the mapped devices report back
after the write.

`inspectVideo(view)` returns a packed RGB preview and its geometry for
`layer0`, `layer1`, `tileset`, `palette`, or `font`. For `sprites`, it returns
the sprite attributes and individual RGB previews with transparency masks.
These previews use the same renderer as the desktop debugger. Enable
`setBreakOnVBlank(true)` to pause at each subsequent emulated vertical
blank edge while running.
