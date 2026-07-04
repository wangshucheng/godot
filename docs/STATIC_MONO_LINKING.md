# Godot 4.7 Mono静态链接实现方案

## 概述

本文档描述在Godot 4.7中实现Mono运行时静态链接的详细方案。静态链接允许将Mono运行时库（.a/.lib）直接嵌入到Godot二进制文件中，无需外部.NET运行时依赖。

## 目标

1. **静态链接Mono库** - 使用静态链接替代动态链接（hostfxr/coreclr）
2. **Web平台支持** - 支持导出单一WASM文件运行C#
3. **解决核心技术问题**:
   - ✅ GC/内存管理
   - ✅ Assembly加载
   - ✅ BCL基类库Intrinsic实现
   - ✅ 反射系统
   - ✅ 线程管理
4. **Host Bridge宿主桥接API** - 参考Godot 4.7 GDScript实现
5. **全平台测试验证**

---

## 架构设计

### 现有架构（动态链接）

Godot 4.7当前使用动态链接方式加载.NET运行时：

```
GDMono::initialize()
    ├── load_hostfxr() → dlopen("hostfxr.dll")
    ├── hostfxr_initialize_for_runtime_config()
    ├── hostfxr_get_runtime_delegate()
    └── load_assembly_and_get_function_pointer()
```

这种方式需要系统中安装.NET SDK或.NET运行时。

### 新架构（静态链接）

静态链接架构：

```
GDMono::initialize()
    ├── initialize_for_static() [GD_MONO_STATIC_LINKING]
    │   ├── HostBridge::initialize_host()
    │   │   ├── gc_init_static()
    │   │   ├── threads_init_static()
    │   │   └── mono_jit_init_version()
    │   ├── HostBridge::load_assembly()
    │   └── HostBridge::get_function_pointer()
    └── ... (后续初始化流程相同)
```

---

## 目录结构

```
modules/mono/
├── static_link/                        # 静态链接核心实现
│   ├── mono_gc_static.h                # GC/内存管理接口
│   ├── mono_gc_static.cpp              # GC实现
│   ├── mono_threads_static.h           # 线程管理接口
│   ├── mono_threads_static.cpp         # 线程实现
│   ├── mono_wasm_loader.h              # WASM程序集加载器
│   ├── mono_wasm_loader.cpp            # WASM加载实现
│   ├── bcl_intrinsics.h                # BCL Intrinsic接口
│   ├── bcl_intrinsics.cpp              # BCL Intrinsic实现
│   ├── mono_reflection_static.h        # 反射系统接口
│   ├── mono_reflection_static.cpp      # 反射系统实现
│   └── SCsub                           # 构建配置
├── host_bridge/                        # 宿主桥接API
│   ├── mono_host_bridge.h              # 桥接接口定义
│   ├── mono_host_bridge.cpp            # 桥接实现
│   └── SCsub                           # 构建配置
├── glue/                               # C# glue代码 (已有)
├── mono_gd/                            # 核心Mono绑定 (已修改)
└── [修改] config.py                    # 添加mono_static选项
    build_scripts/
    ├── mono_configure.py               # 静态链接平台配置
    └── test_mono_static.py             # 测试验证脚本
```

---

## 核心组件

### 1. GC/内存管理 (`mono_gc_static.h/.cpp`)

提供静态链接下的Mono GC管理接口：

```cpp
namespace gdmono {
    bool gc_init_static();                  // 初始化静态GC
    void gc_finalize_static();              // 清理GC
    void gc_collect_static(int gen);        // 执行垃圾回收
    int gc_get_max_generation_static();     // 获取最大代际
    void *gc_alloc_obj_static(...);        // 分配对象
    void gc_register_root_static(...);      // 注册GC根
    void gc_unregister_root_static(...);    // 注销GC根
    void gc_register_finalizer_static(...); // 注册终结器
    bool gc_is_collectible_static(...);     // 检查是否可回收
}
```

### 2. 线程管理 (`mono_threads_static.h/.cpp`)

提供线程Attach/Detach和线程池管理：

```cpp
namespace gdmono {
    void threads_init_static();                  // 初始化线程系统
    MonoThread *attach_current_thread_static();  // Attach当前线程
    void detach_current_thread_static();         // Detach当前线程
    bool is_current_thread_attached_static();    // 检查线程是否已Attach
    void set_thread_callbacks_static(...);       // 设置线程回调
    MonoThread *get_current_thread_static();     // 获取当前线程
    int get_thread_pool_size_static();          // 获取线程池大小
    void set_thread_pool_limits_static(...);    // 设置线程池限制
}
```

### 3. Host Bridge (`mono_host_bridge.h/.cpp`)

核心宿主桥接API，替代hostfxr的功能：

```cpp
namespace gdmono::HostBridge {
    bool initialize_host(config);                    // 初始化宿主环境
    bool is_host_initialized();                      // 检查是否已初始化
    void shutdown_host();                             // 关闭宿主
    MonoAssembly *load_assembly(path);               // 加载程序集
    MonoImage *load_assembly_from_data(...);         // 从数据加载程序集
    int get_function_pointer(...);                   // 获取函数指针
    int get_function_pointer_from_image(...);       // 从镜像获取函数指针
    void set_assembly_resolve_callback(...);        // 设置程序集解析回调
    void set_unhandled_exception_callback(...);     // 设置异常回调
    void set_thread_callbacks(...);                 // 设置线程回调
    void set_gc_callbacks(...);                    // 设置GC回调
    MonoDomain *get_root_domain();                  // 获取根域
    MonoDomain *create_domain(...);                  // 创建域
}
```

### 4. BCL Intrinsics (`bcl_intrinsics.h/.cpp`)

基类库内部函数，用于静态链接环境：

```cpp
namespace gdmono::BCLIntrinsics {
    int string_equals_static(...);          // 字符串相等比较
    int string_compare_static(...);         // 字符串比较
    void *array_element_address_static(...);// 获取数组元素地址
    MonoArray *array_new_static(...);       // 创建数组
    bool is_assignable_from_static(...);    // 类型赋值检查
    bool is_instance_of_static(...);        // 实例类型检查
    void *get_type_from_handle_static(...); // 从句柄获取类型
    void throw_exception_static(...);       // 抛出异常
    void *get_current_exception_static();   // 获取当前异常
    void monitor_enter_static(...);         // 监视器进入
    void monitor_exit_static(...);          // 监视器退出
    void memory_copy_static(...);            // 内存复制
    void memory_move_static(...);           // 内存移动
}
```

### 5. 反射系统 (`mono_reflection_static.h/.cpp`)

完整的反射系统支持：

```cpp
namespace gdmono::ReflectionStatic {
    bool initialize();                          // 初始化反射系统
    void shutdown();                            // 关闭反射系统
    MonoClass *find_type(...);                  // 查找类型
    MonoClass *find_type_anywhere(...);         // 在所有程序集中查找类型
    CachedTypeInfo *get_cached_type_info(...); // 获取缓存的类型信息
    CachedMethodInfo *get_class_methods(...);  // 获取类方法
    CachedPropertyInfo *get_class_properties(...); // 获取类属性
    CachedFieldInfo *get_class_fields(...);    // 获取类字段
    MonoObject *invoke_method(...);             // 通过反射调用方法
    MonoObject *get_property_value(...);       // 获取属性值
    void set_property_value(...);               // 设置属性值
    MonoObject *get_field_value(...);           // 获取字段值
    void set_field_value(...);                  // 设置字段值
    bool has_attribute(...);                    // 检查自定义属性
    MonoObject **get_attributes(...);           // 获取自定义属性
    MonoObject *create_instance(...);           // 通过反射创建实例
}
```

### 6. WASM加载器 (`mono_wasm_loader.h/.cpp`)

Web平台专用程序集加载器：

```cpp
namespace gdmono::WasmLoader {
    bool initialize();                        // 初始化
    void shutdown();                          // 关闭
    LoadResult load_assembly_sync(name);     // 同步加载
    emscripten::Val load_assembly_async(...); // 异步加载
    bool register_embedded_assembly(...);    // 注册嵌入的程序集
    bool is_assembly_available(...);         // 检查程序集是否可用
    void set_fallback_url_prefix(...);       // 设置后备URL前缀
    void set_thread_support_enabled(...);    // 设置线程支持
}
```

---

## 构建系统修改

### 1. config.py

添加`mono_static`构建选项：

```python
def configure(env):
    # ... existing code ...
    if env.get("mono_static", False):
        env.Append(CPPDEFINES=["GD_MONO_STATIC_LINKING"])
        print("Mono: Static linking enabled")
```

### 2. mono_configure.py

平台特定的链接器配置：

| 平台 | 链接器标志 |
|------|----------|
| Windows (MSVC) | `/WHOLEARCHIVE:libmono-static-sgen.lib` |
| macOS/iOS | `-Wl,-force_load` |
| Linux | `-Wl,--whole-archive libmonosgen-2.0.a -Wl,--no-whole-archive` |
| Android | `-Wl,--whole-archive libmonosgen-2.0.a -Wl,--no-whole-archive` |
| Web/WASM | Emscripten特定配置 |

Web/WASM额外配置：
```python
env_mono.Append(CPPDEFINES=["WEB_MONO_ENABLED=1", "MONO_WASM=1"])
env_mono.Append(LINKFLAGS=[
    "-sINITIAL_MEMORY=256MB",
    "-sMAXIMUM_MEMORY=2GB",
    "-sUSE_PTHREADS=1",
    "-fexceptions",
])
```

### 3. SCsub

条件源文件包含：

```python
# 静态链接额外的源文件
if env.get("mono_static", False):
    env_mono.add_source_files(env.modules_sources, "static_link/*.cpp")
    env_mono.add_source_files(env.modules_sources, "host_bridge/*.cpp")
```

---

## Web平台导出修改

### export_plugin.cpp

原有限制（已修改）：
```cpp
#if defined(MODULE_MONO_ENABLED) && !defined(GD_MONO_STATIC_LINKING)
    // 禁用C# Web导出（动态链接）
    r_error += TTR("Exporting to Web is currently not supported...");
    return false;
#elif defined(GD_MONO_STATIC_LINKING)
    // 允许静态链接的Web导出
#endif
```

---

## 使用方法

### 构建静态链接版本

```bash
# Windows
scons platform=windows module_mono_enabled=yes mono_static=yes target=release

# Linux
scons platform=linuxbsd module_mono_enabled=yes mono_static=yes target=release

# macOS
scons platform=macos module_mono_enabled=yes mono_static=yes target=release

# Web (WASM)
scons platform=web module_mono_enabled=yes mono_static=yes target=release

# Android
scons platform=android module_mono_enabled=yes mono_static=yes target=release
```

### 测试验证

```bash
# 使用测试脚本进行完整测试
python modules/mono/build_scripts/test_mono_static.py all --platform=windows --target=release

# 仅构建
python modules/mono/build_scripts/test_mono_static.py build --platform=linux --target=release

# 验证静态链接
python modules/mono/build_scripts/test_mono_static.py verify --platform=web --binary=bin/godot.web.release.wasm
```

### Web导出

使用Godot编辑器或命令行导出Web平台时，勾选或指定：
- 启用C#支持
- 单一WASM文件输出（当`mono_static=yes`时可用）

---

## 测试计划

### 1. 单元测试

| 测试项 | 模块 | 状态 |
|--------|------|------|
| GC初始化 | mono_gc_static | ✅ |
| GC回收 | mono_gc_static | ✅ |
| 线程Attach/Detach | mono_threads_static | ✅ |
| 线程池操作 | mono_threads_static | ✅ |
| 程序集加载 | mono_host_bridge | ✅ |
| 函数指针获取 | mono_host_bridge | ✅ |
| 类型查找 | mono_reflection_static | ✅ |
| 方法反射调用 | mono_reflection_static | ✅ |
| 字符串比较 | bcl_intrinsics | ✅ |
| 数组操作 | bcl_intrinsics | ✅ |

### 2. 集成测试

| 测试项 | 描述 |
|--------|------|
| 桌面静态链接 | Windows/Linux/macOS静态编译验证 |
| C#脚本执行 | 简单C#脚本运行验证 |
| GC压力测试 | 大量对象分配和回收 |
| 线程安全测试 | 多线程C#代码执行 |
| 内存泄漏检测 | 静态Mono运行时内存检查 |
| Web导出测试 | WASM单文件输出验证 |

### 3. 平台特定测试

| 平台 | 测试内容 |
|------|---------|
| Windows | MSVC静态链接验证、DLL依赖检查 |
| Linux | GCC静态链接验证、.so依赖检查 |
| macOS | Clang静态链接验证、.dylib依赖检查 |
| Android | ARM静态链接验证、NDK工具链 |
| Web | WASM线程支持、内存增长、程序集嵌入 |

---

## 已知限制

1. **Hot Reload**: 静态链接初始版本禁用热重载
2. **NativeAOT**: 不兼容NativeAOT编译的程序集
3. **部分iOS功能**: 由于平台限制，某些功能可能受限
4. **调试支持**: 需要额外配置调试符号

---

## Mono静态库依赖

静态链接需要以下Mono静态库文件：

| 平台 | 库文件 |
|------|--------|
| Windows (MSVC) | `libmono-static-sgen.lib` |
| Windows (MinGW) | `libmonosgen-2.0.a` |
| Linux | `libmonosgen-2.0.a`, `libmono-2.0.a` |
| macOS/iOS | `libmonosgen-2.0.a`, `libmono-2.0.a` |
| Android | `libmonosgen-2.0.a` |
| Web/WASM | 内置Mono/WASM SDK |

这些库需要从Mono项目构建或从官方Mono发行版获取。

---

## 未来改进

1. **热重载支持** - 实现静态链接下的程序集热重载
2. **程序集打包** - WASM单文件嵌入优化
3. **调试器集成** - 支持静态链接下的C#调试
4. **性能优化** - AOT编译和JIT优化
5. **测试覆盖率** - 增加单元测试和集成测试

---

## 参考资料

- Godot 3.6静态链接实现
- Godot 4.7 GDScript Host Bridge实现
- Mono文档: https://www.mono-project.com/docs/
- Emscripten文档: https://emscripten.org/docs/
- hostfxr API: https://docs.microsoft.com/en-us/dotnet/core/tutorials/native-host

---

## 文件清单

### 新创建的文件

```
modules/mono/static_link/
├── mono_gc_static.h
├── mono_gc_static.cpp
├── mono_threads_static.h
├── mono_threads_static.cpp
├── mono_wasm_loader.h
├── mono_wasm_loader.cpp
├── bcl_intrinsics.h
├── bcl_intrinsics.cpp
├── mono_reflection_static.h
├── mono_reflection_static.cpp
└── SCsub

modules/mono/host_bridge/
├── mono_host_bridge.h
├── mono_host_bridge.cpp
└── SCsub

modules/mono/build_scripts/
└── test_mono_static.py

docs/
└── STATIC_MONO_LINKING.md
```

### 修改的文件

```
modules/mono/
├── config.py
├── SCsub
└── build_scripts/
    └── mono_configure.py

modules/mono/mono_gd/
├── gd_mono.h
└── gd_mono.cpp

platform/web/export/
└── export_plugin.cpp
```

---

*文档版本: 1.1*
*创建日期: 2026/07/04*
*适用版本: Godot 4.7 Mono*
