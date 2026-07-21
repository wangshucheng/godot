// data_pkg_2 subpackage entry - loads base64 .data chunk 2
console.log('[data_pkg_2] entry loaded');
try {
  var part = require('./data_part2.js');
  globalThis._dataChunk2 = part;
  console.log('[data_pkg_2] chunk 2 loaded, size=' + part.size + ' offset=' + part.offset);
} catch (e) {
  console.log('[data_pkg_2] FAILED: ' + e.message);
  globalThis._dataChunk2 = null;
  globalThis._dataChunk2Error = e.message;
}
