// full_auto.js
// 完整 E2E 自动化：用 cli.bat 启动 IDE + automator 在模拟器中跑游戏
// 监听 consoleLog，等待 [C#] Main._Ready / canvas=WxH

const { spawnSync, spawn } = require('child_process');
const automator = require('miniprogram-automator');
const http = require('http');

const CLI_BAT = 'D:\\software\\Tencent\\微信web开发者工具\\cli.bat';
const PROJECT = 'C:\\Users\\Administrator\\AppData\\Roaming\\TRAE SOLO CN\\ModularData\\ai-agent\\work-mode-projects\\6a47e7225801ac16b95705a6\\godot4_7_mono\\wechat\\minigame';
const CLI_PORT = 9999;
const WS_ENDPOINT = `ws://127.0.0.1:${CLI_PORT}`;

function log(msg) {
  const t = new Date().toISOString().substring(11, 19);
  console.log(`[${t}] ${msg}`);
}

function runCli(args, timeoutMs = 60000) {
  log(`CLI: ${args.join(' ')}`);
  const r = spawnSync(CLI_BAT, args, {
    timeout: timeoutMs,
    encoding: 'utf8',
    windowsHide: false,
  });
  if (r.stdout) {
    const out = r.stdout.trim();
    if (out) log(`  stdout: ${out.substring(Math.max(0, out.length - 400))}`);
  }
  if (r.stderr) {
    const err = r.stderr.trim();
    if (err) log(`  stderr: ${err.substring(Math.max(0, err.length - 400))}`);
  }
  log(`  exit: ${r.status}`);
  return r;
}

function killIde() {
  log('Killing all wechatdevtools.exe ...');
  spawnSync('taskkill', ['/F', '/IM', 'wechatdevtools.exe', '/T'], {
    encoding: 'utf8',
  });
  // 等待进程退出
  for (let i = 0; i < 10; i++) {
    spawnSync('timeout', ['/t', '1', '/nobreak'], { shell: true });
    const r = spawnSync('tasklist', ['/FI', 'IMAGENAME eq wechatdevtools.exe', '/FO', 'CSV'], { encoding: 'utf8' });
    if (!r.stdout.includes('wechatdevtools.exe')) {
      log('  all killed');
      return;
    }
  }
  log('  WARN: still alive');
}

function httpGet(path) {
  return new Promise((resolve, reject) => {
    const req = http.get({
      hostname: '127.0.0.1',
      port: CLI_PORT,
      path: path,
      timeout: 5000,
    }, (res) => {
      let data = '';
      res.on('data', (c) => data += c);
      res.on('end', () => resolve({ status: res.statusCode, body: data }));
    });
    req.on('error', reject);
    req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
  });
}

async function waitPort(host, port, timeoutMs = 60000) {
  const net = require('net');
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const ok = await new Promise((resolve) => {
      const s = new net.Socket();
      s.setTimeout(1000);
      s.on('connect', () => { s.destroy(); resolve(true); });
      s.on('error', () => resolve(false));
      s.on('timeout', () => { s.destroy(); resolve(false); });
      s.connect(port, host);
    });
    if (ok) return true;
    await new Promise(r => setTimeout(r, 500));
  }
  return false;
}

async function main() {
  console.log('='.repeat(70));
  console.log('WeChat MiniGame Full Auto E2E (cli + automator)');
  console.log('='.repeat(70));

  // Step 1: kill IDE
  log('=== Step 1: Kill existing IDE ===');
  killIde();
  await new Promise(r => setTimeout(r, 2000));

  // Step 2: open project with CLI HTTP on port 9999
  log('=== Step 2: Open project with --port 9999 ===');
  runCli(['open', '--project', PROJECT, '--port', String(CLI_PORT)], 90000);

  // Step 3: enable automation
  log('=== Step 3: Enable automation ===');
  runCli(['auto', '--project', PROJECT, '--port', String(CLI_PORT), '--trust-project'], 60000);

  // Step 4: wait for WS port
  log(`=== Step 4: Wait for port ${CLI_PORT} ===`);
  const portOk = await waitPort('127.0.0.1', CLI_PORT, 30000);
  log(`  port ${CLI_PORT}: ${portOk ? 'OK' : 'NOT listening'}`);

  // Step 5: connect automator
  log('=== Step 5: Connect automator ===');
  let miniProgram;
  try {
    miniProgram = await automator.connect({ wsEndpoint: WS_ENDPOINT });
    log('  connected');
  } catch (e) {
    log(`  connect failed: ${e.message}`);
    process.exit(3);
  }

  // Step 6: set up consoleLog listener
  const keyLines = [];
  const results = {
    boot_start: false,
    wasm_ok: false,
    main_ready: false,
    game_ok: false,
    game_fail: false,
    compile_err: false,
    canvas_w: null,
    canvas_h: null,
  };
  const patterns = {
    boot_start: /WeChat Boot|Starting Godot 4\.7/i,
    wasm_ok: /WASM instantiation succeeded/i,
    main_ready: /Main\._?Ready/i,
    game_ok: /Game started successfully/i,
    game_fail: /Game failed to start|Engine start error/i,
    compile_err: /CompileError|call_indirect|CANNOT HANDLE COOKIE/i,
    canvas: /canvas\s*=\s*(\d+)\s*[x×]\s*(\d+)/i,
  };

  miniProgram.on('consoleLog', (msg) => {
    const text = msg.args ? msg.args.join(' ') : (msg.text || JSON.stringify(msg));
    for (const [k, p] of Object.entries(patterns)) {
      const m = p.exec(text);
      if (m) {
        log(`  [consoleLog:${k}] ${text.substring(0, 200)}`);
        keyLines.push([k, text.substring(0, 200)]);
        if (k === 'canvas') {
          results.canvas_w = parseInt(m[1], 10);
          results.canvas_h = parseInt(m[2], 10);
        } else {
          results[k] = true;
        }
      }
    }
  });

  // 也监听 exception
  if (miniProgram.on) {
    miniProgram.on('exception', (e) => {
      log(`  [exception] ${JSON.stringify(e).substring(0, 300)}`);
    });
  }

  // Step 7: reLaunch to start the game
  log('=== Step 7: reLaunch to start game ===');
  try {
    // 给 IDE 一些时间初始化
    await new Promise(r => setTimeout(r, 3000));
    log('  calling reLaunch ...');
    const t0 = Date.now();
    // 设短超时，避免挂死
    const launchPromise = miniProgram.reLaunch('');
    const timeoutPromise = new Promise((_, reject) =>
      setTimeout(() => reject(new Error('reLaunch timeout 30s')), 30000)
    );
    await Promise.race([launchPromise, timeoutPromise]);
    log(`  reLaunch returned in ${Date.now() - t0}ms`);
  } catch (e) {
    log(`  reLaunch failed/timeout: ${e.message}`);
  }

  // Step 8: wait for game to start (up to 120s)
  log('=== Step 8: Wait for game events (max 180s) ===');
  const deadline = Date.now() + 180000;
  while (Date.now() < deadline) {
    await new Promise(r => setTimeout(r, 2000));
    // 终止条件
    if (results.game_ok || results.game_fail || results.compile_err) {
      log(`  termination: game_ok=${results.game_ok} game_fail=${results.game_fail} compile_err=${results.compile_err}`);
      break;
    }
    if (results.main_ready && (results.canvas_w !== null)) {
      log(`  termination: main_ready + canvas=${results.canvas_w}x${results.canvas_h}`);
      break;
    }
  }

  // 额外等 5s 收集最后的日志
  await new Promise(r => setTimeout(r, 5000));

  // Step 9: report
  log('=== Step 9: Final report ===');
  console.log('');
  console.log('='.repeat(70));
  console.log('E2E Test Results');
  console.log('='.repeat(70));
  console.log(`  boot_start  : ${results.boot_start}`);
  console.log(`  wasm_ok     : ${results.wasm_ok}`);
  console.log(`  main_ready  : ${results.main_ready}`);
  console.log(`  game_ok     : ${results.game_ok}`);
  console.log(`  game_fail   : ${results.game_fail}`);
  console.log(`  compile_err : ${results.compile_err}`);
  console.log(`  canvas_size : ${results.canvas_w}x${results.canvas_h}`);
  console.log('');
  if (keyLines.length > 0) {
    console.log('Key log lines:');
    for (const [k, line] of keyLines) {
      console.log(`  [${k}] ${line}`);
    }
    console.log('');
  }

  // 判定
  let success = false;
  let reason = '';
  if (results.game_fail) reason = 'game failed to start';
  else if (results.compile_err) reason = 'compile error';
  else if (results.canvas_w !== null && results.canvas_w <= 100) reason = `canvas too small: ${results.canvas_w}x${results.canvas_h} (3x3 bug)`;
  else if (!results.main_ready) reason = 'Main._Ready never called';
  else if (results.canvas_w === null) reason = 'canvas size never reported';
  else if (results.canvas_w > 100 && results.canvas_h > 100) success = true;

  if (success) {
    console.log('RESULT: SUCCESS - Game runs, canvas size valid (>100)');
    process.exit(0);
  } else {
    console.log(`RESULT: FAIL - ${reason}`);
    process.exit(2);
  }
}

main().catch((e) => {
  log(`Fatal: ${e.message}`);
  console.error(e.stack);
  process.exit(1);
});
