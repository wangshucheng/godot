// data_pkg_1 subpackage entry - loads base64 .data chunk 1
console.log('[data_pkg_1] entry loaded');
try {
  var part = require('./data_part1.js');
  globalThis._dataChunk1 = part;
  console.log('[data_pkg_1] chunk 1 loaded, size=' + part.size + ' offset=' + part.offset);
} catch (e) {
  console.log('[data_pkg_1] FAILED: ' + e.message);
  globalThis._dataChunk1 = null;
  globalThis._dataChunk1Error = e.message;
}
