# csharp_test 测试项目审查与强化报告

> **日期**：2026-07-27
> **范围**：`csharp_test/`（23 场景工作流套件 + fuzz 套件）+ 其 C++ 测试 icall 层（`mono_icalls.cpp` Test_* 段）+ 运行器脚本
> **方法**：逐行审查 Test.cs（1490 行）与全部 Test_* icall 实现，识别"假性通过"；强化断言后运行，以抓出的真实 bug 反证测试有效性
> **结论**：原套件**框架真实但大量断言形同虚设**；强化后的新套件（25 场景，含新增场景 0 与 24）首次运行即抓出 **3 个引擎级真实 bug**，修复后 25/25 全 PASS，fuzz 10/10 PASS

---

## 一、审查结论：原套件"全部通过"为何不可信

断言框架本身是真的（`Test_Assert` 计数、`[TEST FAIL]` 打印、`Test_FinishTest` 判定 fail==0 && pass>0），约六成断言是真实往返校验。但存在四类"假性通过"：

### 1. 完全伪造的测试（永远不能失败）

| 位置 | 问题 |
|---|---|
| `godot_icall_Test_ConnectSignal` | **从未连接任何信号**——只查 `has_signal` 就返回 1（注释自承"不好创建 Callable"） |
| `godot_icall_Test_EmitSignal` | **自增计数器**——`TestGetSignalCount` 数的是自己发射的次数，不是回调送达次数 |
| `godot_icall_Test_BclListTest/BclDictTest` | 名为 BCL 测试，实际测的是 **Godot C++ 的 `Vector<int>`/`HashMap`**，与 .NET BCL 无关且恒真 |
| `godot_icall_Test_BclAsyncTest` | 无条件返回 1 |

### 2. 恒真断言（tautology，约 20 处）

`TestAssert("1j Free", 1)` 等常量 1；`_physicsCount >= 0`；`sc >= 0`；`(ray == 0 || ray == 1)`；`14f/15g/18b/18h/20a-e(WASM)/21j` 的 skip 也记为 pass（虚增通过数）。

### 3. 张冠李戴的断言（测试对象错误）

- `19e/19i`：声称测"子节点"属性，实际上下文从未移动，测的是**父节点**（19i 还覆盖了 19h 刚设的值）
- `22c`：注释说"嵌套到 bone1 下"，实际加到了 Skeleton 上
- `8b`：重复断言 8a 的创建结果，并未验证 AddToScene

### 4. 结构性缺陷

- **state 0 反射检查只打印不断言**——反射回归无法使套件失败
- **无退出判定**：汇总仅写 Debug UI，桌面运行永不退出、无退出码；主套件没有桌面运行器（H9 只在 WASM 侧）
- **运行器路径陈旧**：`run_fuzz_test.ps1` 硬编码 `bin/editor/windows/` 旧二进制，`run_p2_typedef_test.ps1` 用 `bin/` 新二进制，同一仓库两个"被测对象"
- **Fuzz10 形同虚设**：只查 `[FUZZ] START` 标记（异常前打印），回调是否实际触发无人验证——事后证实其回调**从未触发过**

---

## 二、强化措施（已实施）

### C++ icall 层（mono_icalls.cpp / mono_callable.cpp / mono_icalls.h）

1. **真实信号链路**：新增 `TestSignalReceiver`（GDCLASS，`register_types.cpp` 注册），`Test_ConnectSignal` 执行真实 `Object::connect`；`Test_EmitSignal` 删除自增；计数只由真实回调递增。
2. **新 icall**：`Test_GetStringProp`（字符串属性读回——此前字符串只能写不能验）、`Test_SelectChild`/`Test_SelectParent`（测试上下文导航，支撑真实子节点断言）。

### glue（GodotSharp）

3. `GodotBridge.cs`/`Runtime.cs`：新增 3 个 icall 声明与包装。

### Test.cs（25 处编辑）

4. 场景 0（反射）改为 9 条真实断言 + 独立 `TestFinishTest`。
5. 全部恒真断言替换为确定性断言（精确子节点数 3/2/1/2、空物理世界 raycast 必 miss、长度/内容精确值等）；skip 只记日志不计 pass。
6. 19/22 场景用 SelectChild/SelectParent 做**真实**子节点断言；22f 用 GetStringProp 做名称往返。
7. 新增**场景 24（脚本桥）**：`[Export]` 经引擎属性路径 set/get 往返（int/string/float）+ `[Signal]` 声明检查 + C# 侧 connect→emit→回调端到端（方法组 + lambda 双路径）。
8. 桌面端汇总后 `GetTree().Quit(failCount>0 ? 1 : 0)`——测试有了退出码；WASM 保持存活供 H9 JS 轮询。

### 运行器

9. 新增 `run_csharp_test.ps1`：校验 25 条 `[TEST RESULT]`、无 `[TEST FAIL]`、退出码 0；`run_fuzz_test.ps1` 二进制路径改为自动选最新（此前在测旧二进制）。

---

## 三、强化套件抓出的真实 bug（已全部修复并验证）

> 这是本次审查的核心价值证明：**假测试变真之后立刻开始抓虫。**

### Bug A：`CSharpInstance::set` 值类型字段越界覆写（P1）

`csharp_script.cpp` 字段写入路径把 Variant 装箱值（Int64/double 均 8 字节）直接 `mono_field_set_value` 写入可能只有 4 字节的 C# 字段（int/float）——**覆盖相邻字段**。场景 24 中 `Set("Speed",321)` 顺带清零了 `Health`（字段相邻）。**修复**：按字段 `MonoType` 精确转换后写入（BOOLEAN/I1..U8/R4/R8 全覆盖，其余走原装箱路径）。

### Bug B：托管信号回调体系整体失效（P0 级，影响所有 Connect）

`Godot.Callable.From` 把用户委托统一包成 `Action<object[]>` wrapper，而 native `CallableCustomMono::call` 按**逐参数**调用它——签名形状不匹配，回调要么抛异常要么收到野指针；且 icall 用**弱 GCHandle** 持有 wrapper（C# 侧无根），GC 后静默丢失。**后果：所有 C# 信号回调从未真正触发过**，Fuzz10 的"通过"是假性的（boom 消息在日志中出现 0 次）。**修复（根因重构）**：
- `Godot.Callable.From*` 改为把**原始委托**直传 native（删除 wrapper 体系）；
- `CallableCustomMono::call` 改为按委托 `Invoke` 签名**逐参数精确封送**（I4→Int32 存储、R4→float 等，引用类型走对象装箱），签名不可用或参数超限时回退旧路径；
- icall 改用**强 GCHandle**（native Callable 释放时在析构中解除）。

### Bug C：测试基建本身

`TestConnectSignal` 伪造实现（见 §一.1）随真实信号链路一并修复。

**附带发现**：`mono_static_compat.c` shim 缺 `__imp_strdup`（链接期暴露，已补）；`run_fuzz_test.ps1` 测旧二进制（已修）。

---

## 四、当前状态与运行方式

```powershell
# 桌面主套件（25 场景，~30s）
powershell -File run_csharp_test.ps1        # 25/25 PASS, exit 0
# Fuzz 套件（10+1 脚本）
powershell -File run_fuzz_test.ps1          # 10/10 PASS，Fuzz10 回调现真正触发（boom 日志 2 次）
```

验证环境：`scons platform=windows target=editor module_mono_enabled=yes accesskit=no d3d12=no`，glue 与 CSharpTest 均为 Debug 重建。

## 五、注意事项

1. **H9 WASM 基线（2026-07-27 已重建）**：WASM 端已重验通过——混合 AOT 模板 + 无头 Chromium（playwright）下 **24/24 场景 PASS**（场景 0 为桌面限定），含场景 24 的 [Export]/[Signal] 端到端（7 断言），新委托封送路径在 WASM 解释器下工作正常。运行方式：`python run_web_test.py`（HTTP 服务 + playwright，自动判定）。web 模板构建命令：`scons platform=web target=template_release mono_wasm=yes threads=no -j4`（**threads=no 必需**——预编译 Mono 静态库无 atomics；也可用 `build_web_template.ps1`）。
2. **dotnet build 增量陷阱**：MSBuild 多次跳过 CoreCompile（源码 mtime 新于 dll 仍判"最新"），排查期间造成两轮假构建。改 glue 后若不放心，删 `modules/mono/glue/GodotSharp/obj` 再构建。
3. **csharp_test 运行依赖**：编辑器二进制（`bin/` 下最新 console exe）、`bin/GodotSharp/Api/Debug/GodotSharp.dll`（glue 构建产物）、`.mono/assemblies/CSharpTest.dll`（项目构建产物）三者需同时最新；`CSharpTest.csproj` 的 HintPath 已指向 `bin/GodotSharp`（原指向 bin/editor/windows 旧副本）。
4. N1 版本化程序集机制在本次测试中首次实际生效（`.mono/assemblies/CSharpTest.dll.rev1.dll`）。
