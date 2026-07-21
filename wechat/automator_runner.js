// automator_runner.js (v2)
// 使用 automator.launch() 让 SDK 自动启动 IDE + 连接 WS + 触发 reLaunch
const automator = require('miniprogram-automator');

const CLI_BAT = 'D:\\software\\Tencent\\微信web开发者工具\\cli.bat';
const PROJECT = 'C:\\Users\\Administrator\\AppData\\Roaming\\TRAE SOLO CN\\ModularData\\ai-agent\\work-mode-projects\\6a47e7225801ac16b95705a6\\godot4_7_mono\\wechat\\minigame';
const AUTO_PORT = parseInt(process.argv[2] || '9420', 10);
const TIMEOUT_MS = parseInt(process.argv[3] || '240', 10) * 1000;

function log(msg) {
  process.stderr.write(`[automator] ${msg}\n`);
}

const patterns = {
  boot_start: /WeChat Boot|Starting Godot 4\.7/i,
  wasm_ok: /WASM instantiation succeeded/i,
  main_ready: /Main\._?Ready/i,
  game_ok: /Game started successfully/i,
  game_fail: /Game failed to start|Engine start error/i,
  compile_err: /CompileError|call_indirect|CANNOT HANDLE COOKIE/i,
  canvas: /canvas\s*=\s*(\d+)\s*[x×]\s*(\d+)/i,
  diag_canvas: /\[WeChat Diag\][^\n]*canvas\s*=\s*(\d+)\s*[x×]\s*(\d+)/i,
};

const results = {
  boot_start: false,
  wasm_ok: false,
  main_ready: false,
  game_ok: false,
  game_fail: false,
  compile_err: false,
  canvas_w: null,
  canvas_h: null,
  diag_canvas_w: null,
  diag_canvas_h: null,
};
const keyLines = [];

function handleText(text) {
  for (const [k, p] of Object.entries(patterns)) {
    const m = p.exec(text);
    if (!m) continue;
    log(`  [hit:${k}] ${text.substring(0, 200)}`);
    keyLines.push([k, text.substring(0, 200)]);
    if (k === 'canvas' && results.canvas_w === null) {
      results.canvas_w = parseInt(m[1], 10);
      results.canvas_h = parseInt(m[2], 10);
    } else if (k === 'diag_canvas') {
      results.diag_canvas_w = parseInt(m[1], 10);
      results.diag_canvas_h = parseInt(m[2], 10);
    } else {
      results[k] = true;
    }
  }
}

async function main() {
  log(`launching IDE via automator.launch() with port ${AUTO_PORT}`);
  let mp;
  try {
    const launchOpts = {
      cliPath: CLI_BAT,
      projectPath: PROJECT,
      trustProject: true,
      timeout: 60000,
    };
    // 只有当 AUTO_PORT > 0 时才指定端口
    if (AUTO_PORT > 0) {
      launchOpts.port = AUTO_PORT;
    }
    mp = await automator.launch(launchOpts);
    log('launched & connected');
  } catch (e) {
    log(`launch failed: ${e.message}`);
    process.stdout.write(JSON.stringify({ error: `launch failed: ${e.message}`, ...results, keyLines }));
    process.exit(3);
  }

  mp.on('consoleLog', (msg) => {
    let text;
    try {
      if (msg.args && msg.args.length) text = msg.args.join(' ');
      else if (msg.text) text = msg.text;
      else text = JSON.stringify(msg);
    } catch (_) { text = String(msg); }
    log(`[consoleLog] ${text.substring(0, 200)}`);
    handleText(text);
  });

  // 给 IDE 一点时间初始化
  log('waiting 5s for IDE init ...');
  await new Promise(r => setTimeout(r, 5000));

  // 触发游戏：reLaunch
  log('reLaunch ...');
  try {
    const t0 = Date.now();
    const launchP = mp.reLaunch('');
    const timeoutP = new Promise((_, rej) =>
      setTimeout(() => rej(new Error('reLaunch timeout 60s')), 60000)
    );
    await Promise.race([launchP, timeoutP]);
    log(`reLaunch returned in ${Date.now() - t0}ms`);
  } catch (e) {
    log(`reLaunch error: ${e.message}`);
  }

  // 等待游戏事件
  log(`waiting for game events (max ${TIMEOUT_MS}ms) ...`);
  const deadline = Date.now() + TIMEOUT_MS;
  while (Date.now() < deadline) {
    await new Promise(r => setTimeout(r, 2000));
    if (results.game_ok || results.game_fail || results.compile_err) {
      log(`terminating: game_ok=${results.game_ok} game_fail=${results.game_fail} compile_err=${results.compile_err}`);
      break;
    }
    if (results.main_ready && (results.canvas_w !== null || results.diag_canvas_w !== null)) {
      log(`terminating: main_ready + canvas detected`);
      break;
    }
  }
  // 多等 5s 收集日志
  await new Promise(r => setTimeout(r, 5000));

  // 输出结果
  const output = { ...results, keyLines };
  process.stdout.write(JSON.stringify(output, null, 2));

  try { await mp.disconnect(); } catch (_) {}
}

main().catch((e) => {
  log(`fatal: ${e.message}`);
  process.stdout.write(JSON.stringify({ error: e.message, ...results, keyLines }));
  process.exit(1);
});
