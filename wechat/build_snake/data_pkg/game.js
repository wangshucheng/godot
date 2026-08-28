// data_pkg subpackage entry (auto-generated)
// 本分包承载 godot.web.template_release.wasm32.nothreads.dat（BCL 二进制资源）。
// 主防御：packOptions.include 强制包含 data_pkg 目录（已写入公共和私有配置双保险）。
// 二级防御：以下 wx.getFileSystemManager 调用的字符串路径可能被
//   DevTools 静态分析识别为文件依赖，进一步确保 .dat 文件被打入分包。
var data_pkg = { loaded: true, payload: 'godot.web.template_release.wasm32.nothreads.dat' };
console.log('[data_pkg] subpackage entry loaded');
(function() {
  try {
    var _fs = wx.getFileSystemManager();
    var _p = 'data_pkg/godot.web.template_release.wasm32.nothreads.dat';
    _fs.accessSync(_p);
    console.log('[data_pkg] data file accessible: ' + _p);
  } catch (_e) {
    console.warn('[data_pkg] data file probe failed: ' + _e.message);
  }
})();
