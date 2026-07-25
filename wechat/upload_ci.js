#!/usr/bin/env node
/**
 * miniprogram-ci 命令行上传脚本
 *
 * 用途：绕过 DevTools GUI，直接用官方 CI 工具上传小游戏代码包。
 *       DevTools 2.02.2607232 在"自动预览/上传"时存在分包配置不生效的 bug，
 *       表现为所有文件被塞进 __FULL__ 主包，报 "source size exceed max limit"。
 *       miniprogram-ci 通过 type:'miniGame' 明确指定项目类型，确保 game.json 的
 *       subpackages 配置被正确处理。
 *
 * 前置条件：
 *   1. npm install miniprogram-ci
 *   2. 在微信公众平台下载代码上传密钥
 *      路径：管理 → 开发管理 → 开发设置 → 小程序代码上传 → 下载密钥
 *   3. 配置 IP 白名单（运行本脚本的机器 IP）
 *
 * 用法：
 *   node upload_ci.js <build-dir> <private-key-path> [version] [desc]
 *
 * 示例：
 *   node upload_ci.js build_v8 ./private.key 1.0.0 "分包测试"
 *   node upload_ci.js build_v8 C:/keys/wxc07c26935264a5e5.key 1.0.1 "修复分包"
 */

const ci = require('miniprogram-ci');
const path = require('path');
const fs = require('fs');

const BUILD_DIR = process.argv[2];
const PRIVATE_KEY_PATH = process.argv[3];
const VERSION = process.argv[4] || '1.0.0';
const DESC = process.argv[5] || `CI upload ${new Date().toISOString()}`;

if (!BUILD_DIR || !PRIVATE_KEY_PATH) {
  console.error('用法: node upload_ci.js <build-dir> <private-key-path> [version] [desc]');
  console.error('示例: node upload_ci.js build_v8 ./private.key 1.0.0 "分包测试"');
  console.error('');
  console.error('私钥获取：微信公众平台 → 管理 → 开发管理 → 开发设置 → 小程序代码上传');
  process.exit(1);
}

const projectPath = path.resolve(BUILD_DIR);

// 验证 build 目录结构
const gameJsonPath = path.join(projectPath, 'game.json');
const projectConfigPath = path.join(projectPath, 'project.config.json');

if (!fs.existsSync(gameJsonPath)) {
  console.error(`错误：在 ${projectPath} 中未找到 game.json`);
  process.exit(1);
}
if (!fs.existsSync(projectConfigPath)) {
  console.error(`错误：在 ${projectPath} 中未找到 project.config.json`);
  process.exit(1);
}

const gameJson = JSON.parse(fs.readFileSync(gameJsonPath, 'utf-8'));
const projectConfig = JSON.parse(fs.readFileSync(projectConfigPath, 'utf-8'));

console.log('=== 项目信息 ===');
console.log(`路径:         ${projectPath}`);
console.log(`AppID:        ${projectConfig.appid}`);
console.log(`compileType:  ${projectConfig.compileType}`);
console.log(`subpackages:  ${JSON.stringify(gameJson.subpackages || '未配置')}`);

// 验证分包目录存在
if (gameJson.subpackages) {
  console.log('\n=== 分包目录验证 ===');
  for (const sub of gameJson.subpackages) {
    const subDir = path.join(projectPath, sub.root);
    const entryFile = path.join(subDir, 'game.js');
    const dirExists = fs.existsSync(subDir);
    const entryExists = fs.existsSync(entryFile);
    if (!dirExists || !entryExists) {
      console.error(`  ✗ ${sub.name}: root=${sub.root} 目录存在=${dirExists} 入口存在=${entryExists}`);
      console.error('    分包目录或入口 game.js 缺失，上传必然失败');
      process.exit(1);
    }
    // 统计分包大小
    let totalSize = 0;
    const calcSize = (dir) => {
      for (const item of fs.readdirSync(dir)) {
        const fullPath = path.join(dir, item);
        const stat = fs.statSync(fullPath);
        if (stat.isDirectory()) {
          calcSize(fullPath);
        } else {
          totalSize += stat.size;
        }
      }
    };
    calcSize(subDir);
    const sizeMB = (totalSize / 1024 / 1024).toFixed(2);
    console.log(`  ✓ ${sub.name}: root=${sub.root} 大小=${sizeMB}MB`);
  }
}

(async () => {
  console.log('\n=== 创建 CI 项目对象 ===');
  const project = new ci.Project({
    appid: projectConfig.appid,
    type: 'miniGame',  // ★ 明确指定小游戏类型，避免 DevTools 式的项目类型识别问题
    projectPath: projectPath,
    privateKeyPath: PRIVATE_KEY_PATH,
    ignores: ['node_modules/**/*'],
  });

  // 获取项目属性（验证 CI 正确识别了项目类型）
  console.log('正在获取项目属性...');
  const attr = await project.attr();
  console.log(`CI 识别的项目类型: ${attr.type}`);
  console.log(`CI 识别的 AppID:   ${attr.appid}`);

  console.log(`\n=== 开始上传 (版本 ${VERSION}) ===`);
  const result = await ci.upload({
    project,
    version: VERSION,
    desc: DESC,
    setting: {
      es6: false,
      minify: false,
      autoPrefixWXSS: false,
    },
    onProgressUpdate: (info) => {
      if (info.message) {
        console.log(`  [进度] ${info.message}`);
      }
    },
  });

  console.log('\n=== 上传结果 ===');
  console.log(JSON.stringify(result, null, 2));

  // 重点输出分包信息，验证分包是否生效
  if (result.subPackageInfo && result.subPackageInfo.length > 0) {
    console.log('\n=== 分包信息（关键验证） ===');
    let hasFullOnly = true;
    for (const pkg of result.subPackageInfo) {
      const sizeKB = (pkg.size / 1024).toFixed(2);
      const sizeMB = (pkg.size / 1024 / 1024).toFixed(2);
      const marker = pkg.name === '__FULL__' ? '⚠️ ' : '✓ ';
      console.log(`  ${marker}${pkg.name}: ${sizeKB} KB (${sizeMB} MB)`);
      if (pkg.name !== '__FULL__') {
        hasFullOnly = false;
      }
    }
    if (hasFullOnly) {
      console.error('\n❌ 警告：只有 __FULL__ 包，分包配置未生效！');
      console.error('   这意味着所有文件被塞进主包，会触发大小超限错误。');
    } else {
      console.log('\n✓ 分包配置已生效，主包和分包被正确拆分。');
    }
  }
})().catch(err => {
  console.error('\n=== 上传失败 ===');
  console.error(err.message || err);
  if (err.stack) {
    console.error(err.stack);
  }
  process.exit(1);
});
