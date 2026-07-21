/**
 * auto_run.js - 使用 miniprogram-automator 自动化微信小游戏运行
 *
 * 流程：
 *   1. 连接到已启动的 IDE 自动化端口（ws://127.0.0.1:9999）
 *   2. 重新启动小游戏
 *   3. 监听 console 输出 60s
 *   4. 验证关键标志（Game started / canvas= / Main._Ready）
 */

const automator = require('miniprogram-automator');

const WS_ENDPOINT = 'ws://127.0.0.1:9999';
const SUCCESS_KEYWORDS = ['[WeChat] Game started', '[C#] Main._Ready', 'Godot Engine'];
const FAIL_KEYWORDS = ['Game failed', 'WASM instantiation failed', 'CANNOT HANDLE COOKIE'];
const CANVAS_RE = /canvas=(\d+)x(\d+)/;

function log(msg, level = 'info') {
  const colors = {
    info: '\x1b[33m', ok: '\x1b[32m', err: '\x1b[31m',
    step: '\x1b[36m', log: '\x1b[90m', reset: '\x1b[0m'
  };
  const c = colors[level] || '';
  const r = colors.reset;
  const ts = new Date().toISOString().substr(11, 8);
  if (level === 'step') {
    console.log(`\n${c}[${ts}] === ${msg} ===${r}`);
  } else if (level === 'log') {
    console.log(`  ${c}LOG: ${msg}${r}`);
  } else {
    const prefix = { info: '[i]', ok: '[OK]', err: '[ERR]' }[level] || '[i]';
    console.log(`${c}[${ts}] ${prefix} ${msg}${r}`);
  }
}

async function main() {
  log('Connecting to IDE via automator...', 'step');
  log(`WS endpoint: ${WS_ENDPOINT}`, 'info');

  let miniProgram;
  try {
    miniProgram = await automator.connect({ wsEndpoint: WS_ENDPOINT });
    log('Connected to IDE', 'ok');
  } catch (e) {
    log(`Connect failed: ${e.message}`, 'err');
    process.exit(2);
  }

  // 监听 console 输出
  const consoleLogs = [];
  const startTime = Date.now();

  miniProgram.on('consoleLog', (msg) => {
    const text = msg.args ? msg.args.join(' ') : (msg.text || JSON.stringify(msg));
    const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
    log(`[console +${elapsed}s] ${text}`, 'log');
    consoleLogs.push({ t: elapsed, text });
  });

  // 尝试重新启动
  log('Attempting reLaunch...', 'step');
  try {
    await miniProgram.reLaunch('');
    log('reLaunch dispatched', 'ok');
  } catch (e) {
    log(`reLaunch failed: ${e.message}`, 'err');
    // 尝试 pageStack 看看当前状态
    try {
      const pages = await miniProgram.pageStack();
      log(`Current pages: ${JSON.stringify(pages)}`, 'info');
    } catch (e2) {
      log(`pageStack also failed: ${e2.message}`, 'err');
    }
  }

  // 监控 60s
  log('Monitoring console for 60s...', 'step');
  const monitorDeadline = Date.now() + 60000;
  let foundSuccess = false;
  let foundFail = false;
  let failReason = '';
  let canvasSize = '';

  while (Date.now() < monitorDeadline) {
    await new Promise(r => setTimeout(r, 500));
    const allText = consoleLogs.map(l => l.text).join('\n');

    for (const kw of SUCCESS_KEYWORDS) {
      if (allText.includes(kw)) {
        foundSuccess = true;
        log(`Found success keyword: ${kw}`, 'ok');
      }
    }
    for (const kw of FAIL_KEYWORDS) {
      if (allText.includes(kw)) {
        foundFail = true;
        failReason = kw;
        log(`Found fail keyword: ${kw}`, 'err');
      }
    }
    const m = allText.match(CANVAS_RE);
    if (m) {
      canvasSize = `${m[1]}x${m[2]}`;
      log(`Canvas size: ${canvasSize}`, 'info');
    }

    if (foundSuccess && canvasSize) break;
    if (foundFail) break;
  }

  // 最终判定
  log('', 'info');
  log('=== Final Result ===', 'step');
  log(`Total console logs: ${consoleLogs.length}`, 'info');
  log(`Success keywords found: ${foundSuccess}`, foundSuccess ? 'ok' : 'info');
  log(`Fail keywords found: ${foundFail}`, foundFail ? 'err' : 'info');
  log(`Canvas size: ${canvasSize || '(none)'}`, 'info');

  const canvasOk = canvasSize ? (() => {
    const m = canvasSize.match(/(\d+)x(\d+)/);
    return m && parseInt(m[1]) > 100 && parseInt(m[2]) > 100;
  })() : false;

  if (foundSuccess && canvasOk) {
    log('PASS: Game started with valid canvas size', 'ok');
    await miniProgram.disconnect();
    process.exit(0);
  } else if (foundFail) {
    log(`FAIL: ${failReason}`, 'err');
    await miniProgram.disconnect();
    process.exit(1);
  } else if (foundSuccess && !canvasOk) {
    log(`FAIL: Game started but canvas size invalid: ${canvasSize}`, 'err');
    await miniProgram.disconnect();
    process.exit(1);
  } else {
    log('TIMEOUT: No game activity detected in 60s', 'err');
    log('Dumping all console logs:', 'info');
    consoleLogs.forEach((l, i) => log(`[${i}] ${l.text}`, 'log'));
    await miniProgram.disconnect();
    process.exit(3);
  }
}

main().catch(e => {
  log(`Unhandled error: ${e.stack || e.message}`, 'err');
  process.exit(99);
});
