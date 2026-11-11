#!/usr/bin/env node
// No npm dependencies. Activate Emscripten 7.0.0 and build the emulator first.
import {spawn, spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {readFile, writeFile, mkdtemp, mkdir, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {resolve, dirname, extname, sep} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const build = resolve(process.argv[2] || resolve(root, 'build-wasm'));
const hostfsOnly = process.argv.includes('--hostfs-only');
const chrome = process.env.CHROME || (process.platform === 'darwin'
    ? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' : 'google-chrome');
await mkdir(resolve(build, 'wasm-tests'), {recursive: true});
for (const memory of [false, true]) {
    const compile = spawnSync('emcc', ['tools/wasm-tests/devices.c', 'hw/i2c/at24c512.c',
        'hw/flash.c', 'utils/vtimer.c', 'utils/paths.c', '-Iinclude', '-Iraylib/web/include',
        '-DPLATFORM_WEB', '-D_DEFAULT_SOURCE', ...(memory ? ['-DDEVICE_MEMORY'] : []),
        '-O2', '-sASSERTIONS=1', '-sWASMFS', '-sASYNCIFY', '-sASYNCIFY_STACK_SIZE=65536',
        '-o', resolve(build, `wasm-tests/devices${memory ? '-memory' : ''}.js`)],
        {cwd: root, stdio: 'inherit'});
    if (compile.status !== 0) throw new Error('Device test compilation failed; activate Emscripten first');
}
const errorCompile = spawnSync('emcc', ['tools/wasm-tests/device-errors.c', 'utils/vtimer.c',
    '-Iinclude', '-Iraylib/web/include', '-D_DEFAULT_SOURCE', '-O2', '-sASSERTIONS=1',
    '-sWASMFS', '-o', resolve(build, 'wasm-tests/device-errors.js')], {cwd: root, stdio: 'inherit'});
if (errorCompile.status !== 0) throw new Error('Device fault test compilation failed');
const completedCompile = spawnSync('emcc', ['tools/wasm-tests/completed-writes.c',
    'hw/i2c/at24c512.c', 'utils/vtimer.c', '-Iinclude', '-Iraylib/web/include',
    '-DPLATFORM_WEB', '-D_DEFAULT_SOURCE', '-O2', '-sASSERTIONS=1', '-sWASMFS',
    '-sASYNCIFY', '-sASYNCIFY_STACK_SIZE=65536',
    '-o', resolve(build, 'wasm-tests/completed-writes.js')], {cwd: root, stdio: 'inherit'});
if (completedCompile.status !== 0) throw new Error('Completed-write test compilation failed');
const fixtures = {
    'default.img': Buffer.alloc(256 * 1024),
    'eeprom.img': Buffer.alloc(65536),
    'tf.img': Buffer.alloc(1024 * 1024),
    'microbe.bin': Buffer.from([0x76]),
};
if (process.env.ZEAL_OS_IMAGE) fixtures['zshell.img'] = await readFile(process.env.ZEAL_OS_IMAGE);
fixtures['default.img'][4] = 0x40;
fixtures['default.img'][0x40] = 1;
fixtures['default.img'][0] = 0x76; // Z80 HALT: deterministic boot without an external OS.
const server = createServer(async (request, response) => {
    const path = new URL(request.url, 'http://localhost').pathname;
    try {
        if (/^\/zeal-pascal\/chapter-(one|two)\/$/.test(path)) {
            response.writeHead(200, {'Content-Type': 'text/html'});
            response.end(await readFile(resolve(root, 'tools/wasm-tests/namespace-frame.html')));
            return;
        }
        if (path.startsWith('/template/')) {
            const [, , template, ...parts] = path.split('/');
            if (!['native', 'minimal', 'game'].includes(template)) throw new Error('Unknown template');
            const asset = parts.join('/') || 'index.html';
            const fixture = fixtures[asset];
            if (fixture) {
                response.writeHead(200, {ETag: '"fixture-v1"'});
                response.end(request.method === 'HEAD' ? undefined : fixture);
                return;
            }
            const base = asset.startsWith('libnative/') || asset.startsWith('zeal.elf.')
                ? build : resolve(root, 'assets/wasm', template);
            const file = resolve(base, asset);
            if (!file.startsWith(base + sep)) throw new Error('Invalid path');
            const data = await readFile(file);
            response.writeHead(200, {'Content-Type': {'.js': 'text/javascript', '.html': 'text/html',
                '.css': 'text/css', '.wasm': 'application/wasm'}[extname(file)] || 'application/octet-stream'});
            response.end(data);
            return;
        }
        if (path.startsWith('/fixture/')) {
            const data = fixtures[path.slice('/fixture/'.length)];
            if (!data) { response.writeHead(404).end(); return; }
            response.writeHead(200, {'Content-Type': 'application/octet-stream', ETag: '"fixture-v1"'});
            response.end(request.method === 'HEAD' ? undefined : data);
            return;
        }
        const base = path.startsWith('/wasm/') ? build : resolve(root, 'tools/wasm-tests');
        const relative = path.replace(/^\/(wasm|test)\//, '');
        const file = resolve(base, relative);
        if (!file.startsWith(base + sep)) { response.writeHead(403).end(); return; }
        const data = await readFile(file);
        const type = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'}[extname(file)];
        response.writeHead(200, {'Content-Type': type || 'application/octet-stream'});
        response.end(data);
    } catch { response.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const profile = await mkdtemp(resolve(tmpdir(), 'zeal-wasm-test-'));
let child;
let socket;
let deadline;
const launchBrowser = async () => {
    const browser = spawn(chrome, ['--headless=new', '--no-first-run', '--no-default-browser-check',
        '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
        '--autoplay-policy=no-user-gesture-required', '--remote-debugging-port=0',
        `--user-data-dir=${profile}`, 'about:blank'], {stdio: ['ignore', 'ignore', 'pipe']});
    child = browser;
    const endpoint = await new Promise((resolve, reject) => {
        let output = '';
        browser.stderr.on('data', chunk => {
            output += chunk;
            const match = output.match(/DevTools listening on (ws:\/\/[^\s]+)/);
            if (match) resolve(match[1]);
        });
        browser.on('error', reject);
        browser.on('exit', code => reject(new Error(`Chrome exited ${code}: ${output.slice(-1000)}`)));
    });
    const connection = new WebSocket(endpoint);
    socket = connection;
    await new Promise(resolve => connection.addEventListener('open', resolve));
    let next = 0;
    const pending = new Map();
    connection.addEventListener('message', event => {
        const data = JSON.parse(event.data);
        if (!data.id) return;
        const operation = pending.get(data.id);
        pending.delete(data.id);
        if (data.error) operation.reject(new Error(JSON.stringify(data.error)));
        else operation.resolve(data.result);
    });
    connection.addEventListener('close', () => {
        for (const operation of pending.values()) operation.reject(new Error('Chrome connection closed'));
        pending.clear();
    });
    return (method, params = {}, sessionId) => new Promise((resolve, reject) => {
        const id = ++next;
        pending.set(id, {resolve, reject});
        connection.send(JSON.stringify({id, method, params, sessionId}));
    });
};
try {
    deadline = setTimeout(() => { child?.kill(); console.error('Browser test timeout'); process.exitCode = 1; }, 120000);
    let call = await launchBrowser();
    const runPage = async page => {
        const {targetId} = await call('Target.createTarget', {url: `${origin}/test/${page}`});
        const {sessionId} = await call('Target.attachToTarget', {targetId, flatten: true});
        const result = await call('Runtime.evaluate', {
            expression: `new Promise((resolve,reject)=>{let n=0;const t=setInterval(async()=>{
                try { if(window.testResult){clearInterval(t);resolve(await window.testResult);}
                else if(document.body?.textContent.trim().startsWith('FAIL:')){clearInterval(t);reject(new Error(document.body.textContent));}
                else if(document.body?.textContent.trim().startsWith('PASS:')){clearInterval(t);resolve(document.body.textContent);}
                else if(++n>600){clearInterval(t);reject(new Error(document.body?.textContent));}
                }catch(error){clearInterval(t);reject(error);}},100);})`,
            awaitPromise: true, returnByValue: true,
        }, sessionId);
        if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
        console.log(`${page}: ${result.result.value}`);
        return targetId;
    };
    for (const page of hostfsOnly ? ['hostfs.html'] : ['hostfs.html', 'storage.html', 'namespaces.html', 'device-errors.html', 'devices.html', 'devices.html?memory', 'devices.html', 'startup.html', 'emulator.html']) {
        const targetId = await runPage(page);
        await call('Target.closeTarget', {targetId});
    }
    for (const template of hostfsOnly ? [] : ['native', 'minimal', 'game']) {
        const {targetId} = await call('Target.createTarget', {url: `${origin}/template/${template}/`});
        const {sessionId} = await call('Target.attachToTarget', {targetId, flatten: true});
        const result = await call('Runtime.evaluate', {
            expression: `new Promise((resolve,reject)=>{let n=0;const t=setInterval(()=>{
                if(document.activeElement?.id==='canvas'){clearInterval(t);resolve('booted');}
                else if(++n>200){clearInterval(t);reject(new Error(document.body?.textContent));}
            },100);})`, awaitPromise: true, returnByValue: true,
        }, sessionId);
        if (result.exceptionDetails) throw new Error(`${template}: ${JSON.stringify(result.exceptionDetails)}`);
        console.log(`${template} template: PASS`);
        await call('Target.closeTarget', {targetId});
    }
    if (fixtures['zshell.img']) {
        const {targetId} = await call('Target.createTarget', {url: `${origin}/test/hostfs-zshell.html`});
        const {sessionId} = await call('Target.attachToTarget', {targetId, flatten: true});
        const evaluate = async expression => {
            const result = await call('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true}, sessionId);
            if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
            return result.result.value;
        };
        const delay = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));
        await evaluate(`new Promise((resolve,reject)=>{const t=setInterval(()=>{
            if(window.zshellReady){clearInterval(t);window.zshellReady.then(resolve,reject);}},100);})`);
        await delay(2000); // Allow the OS to reach its shell input loop.
        const tap = async (key, code, windowsVirtualKeyCode, modifiers = 0) => {
            const fields = {key, code, windowsVirtualKeyCode, modifiers};
            await call('Input.dispatchKeyEvent', {type: 'keyDown', ...fields}, sessionId);
            await delay(25);
            await call('Input.dispatchKeyEvent', {type: 'keyUp', ...fields}, sessionId);
            await delay(25);
        };
        const command = async text => {
            for (const character of text) {
                if (character === ':') {
                    await call('Input.dispatchKeyEvent', {type: 'keyDown', key: 'Shift', code: 'ShiftLeft', windowsVirtualKeyCode: 16}, sessionId);
                    await delay(25);
                    await tap(':', 'Semicolon', 186, 8);
                    await call('Input.dispatchKeyEvent', {type: 'keyUp', key: 'Shift', code: 'ShiftLeft', windowsVirtualKeyCode: 16}, sessionId);
                    await delay(25); // Let the guest observe Shift release before the next key.
                } else if (/[a-z]/.test(character)) await tap(character, `Key${character.toUpperCase()}`, character.toUpperCase().charCodeAt(0));
                else {
                    const special = {' ': ['Space', 32], '/': ['Slash', 191], '.': ['Period', 190]};
                    await tap(character, ...special[character]);
                }
            }
            await tap('Enter', 'Enter', 13);
            await delay(500);
        };
        await command('cd h:/');
        await command('ls');
        await command('missingapp');
        await command('exec ./missingapp');
        await command('exec h:/bin/missing');
        await command('cat h:/missing.txt');
        await command('cp h:/sample.txt h:/copied.txt');
        const layout = await call('Page.getLayoutMetrics', {}, sessionId);
        const image = await call('Page.captureScreenshot', {format: 'png', captureBeyondViewport: true,
            clip: {...layout.cssContentSize, scale: 1}}, sessionId);
        await writeFile(resolve(build, 'wasm-tests/hostfs-zshell.png'), Buffer.from(image.data, 'base64'));
        console.log(await evaluate('zshellVerify()'));
        await call('Target.closeTarget', {targetId});
    } else {
        console.log('ZShell integration: SKIP (set ZEAL_OS_IMAGE to an OS image containing ZShell)');
    }
    if (!hostfsOnly) {
        await runPage('completed-writes.html');
        // Keep the writer page and its C FILE streams alive until the browser is
        // killed. Neither closeTarget nor any orderly emulator shutdown is used.
        const terminated = new Promise(resolve => child.once('exit', resolve));
        child.kill('SIGKILL');
        await terminated;
        socket.close();
        call = await launchBrowser();
        await runPage('completed-writes-read.html');
    }
    await call('Browser.close');
} finally {
    clearTimeout(deadline);
    socket?.close();
    child?.kill();
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
    await rm(profile, {recursive: true, force: true, maxRetries: 10, retryDelay: 100});
}
