const assert = require('node:assert/strict');
const path = require('node:path');
(async () => {
  const create = require(path.resolve(process.argv[2]));
  const module = await create({noInitialRun: true});
  assert.equal(module._main(), 0); // Run the same C API tests in WASM first.
  const handle = module._test_debugger_create();
  const ptr = module._malloc(512);
  try {
    assert.equal(module._zeal_debug_handle(), handle);
    assert.equal(module._zeal_debug_command(handle, 0), 0); // Pause
    assert.equal(module._zeal_debug_state(handle, ptr, 24), 0);
    let view = new DataView(module.HEAPU8.buffer);
    assert.equal(view.getUint32(ptr, true), 1);
    const sequence = view.getBigUint64(ptr + 16, true);
    assert(sequence > 0n);
    module.HEAPU8.set([0x42, 0x17, 0xaa], ptr);
    assert.equal(module._zeal_debug_memory(handle, 1, 0, 0x4000, ptr, 3), 0);
    module.HEAPU8.fill(0, ptr, ptr + 3);
    assert.equal(module._zeal_debug_memory(handle, 0, 0, 0x4000, ptr, 3), 0);
    assert.deepEqual([...module.HEAPU8.subarray(ptr, ptr + 3)], [0x42, 0x17, 0xaa]);
    assert.equal(module._zeal_debug_memory(handle, 0, 0, 65535, ptr, 3), 2);
    assert.equal(module._zeal_debug_events(handle, 0, 0, ptr, 512), 0);
    view = new DataView(module.HEAPU8.buffer);
    assert(view.getUint32(ptr, true) > 0);
    assert.equal(module._zeal_debug_command(handle, 5), 0); // Stop
    assert.equal(module._zeal_debug_state(handle, ptr, 24), 0);
    assert.equal(view.getUint32(ptr + 4, true), 1);
    module._test_debugger_destroy();
    assert.equal(module._zeal_debug_state(handle, ptr, 24), 1);
  } finally { module._free(ptr); }
  console.log('WASM debugger exports: commands, memory, state, events, stale handles OK');
})().catch(error => { console.error(error); process.exitCode = 1; });
