// 微信小游戏入口 - game.js
// 加载顺序: adapter → index.js → 启动 Engine
//
// 本文件由微信开发者工具直接执行，是小游戏的主入口。
// 不能使用 ES6 import/export，必须用 require。

require('./adapter/wechat_adapter.js');
require('./index.js');
