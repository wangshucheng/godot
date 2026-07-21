const fs = require('fs');
const s = fs.readFileSync('D:/software/Tencent/微信web开发者工具/code/package.nw/js/common/cli/index.js', 'utf8');
const re = /["'](\/[a-z][a-zA-Z\-]+)["']/g;
const m = new Set();
let r;
while (r = re.exec(s)) { m.add(r[1]); }
[...m].sort().forEach(x => console.log(x));
