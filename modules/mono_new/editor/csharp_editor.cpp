#include "csharp_editor.h"
#include "../mono_gd/csharp_script.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_plugin.h"

#include <stdlib.h>
#include <time.h>

// R3: dotnet 子进程跳过开关（C# 编辑器启动时的编译器探测阶段）。
//
// 默认 0 = 按正常顺序检查 dotnet SDK 可用性（推荐，用户机器上 dotnet 7.0.401
// 是唯一 100% 确认存在可用的编译器）。
//
// 如果你确认本机完全不需要 dotnet（所有用户工程只走 Mono mcs.bat 或 csc），
// 可以在编译前手动把下面改成 1 或在 config.h/SCons CPPFLAGS 里预定义为 1，
// 启动时会少开 3 次 dotnet 子进程（约省 1~3 秒）。
#ifndef GD_MONO_SKIP_DOTNET_PROBE
#define GD_MONO_SKIP_DOTNET_PROBE 0
#endif

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

static String get_project_name() {
	String name;
	if (ProjectSettings::get_singleton()->has_setting("application/config/name")) {
		name = ProjectSettings::get_singleton()->get_setting("application/config/name");
	}
	if (name.is_empty()) {
		name = "GodotProject";
	}
	// Sanitize: keep only valid C# identifier characters
	String sanitized;
	for (int i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
			(i > 0 && c >= '0' && c <= '9')) {
			sanitized += c;
		} else if (c == ' ' || c == '-' || c == '.') {
			sanitized += '_';
		}
	}
	if (sanitized.is_empty()) {
		sanitized = "GodotProject";
	}
	return sanitized;
}

static String get_project_dir() {
	// res:// as an absolute filesystem path
	return ProjectSettings::get_singleton()->globalize_path("res://");
}

String csharp_editor_get_csproj_path() {
	return get_project_dir().path_join(get_project_name() + ".csproj");
}

String csharp_editor_get_sln_path() {
	return get_project_dir().path_join(get_project_name() + ".sln");
}

String csharp_editor_get_assemblies_output_dir() {
	// Output compiled DLL to res://.mono/assemblies/ which GDMono scans
	String dir = get_project_dir().path_join(".mono").path_join("assemblies");
	if (!DirAccess::exists(dir)) {
		Error err = DirAccess::make_dir_recursive_absolute(dir);
		if (err != OK) {
			MonoLogger::log_warning(vformat("Failed to create assemblies dir: %s", dir));
		}
	}
	return dir;
}

// ---------------------------------------------------------------------------
// .csproj generation
// ---------------------------------------------------------------------------

static String generate_csproj_content(const String &p_project_name) {
	// Use a simple non-SDK .csproj that works with mcs/MSBuild without
	// requiring the Godot.NET.Sdk package. All .cs files under the project
	// directory are included via a globbing Compile item.
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String godotsharp_ref = exe_dir.path_join("GodotSharp.dll");

	String csproj;
	csproj += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	csproj += "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n";
	csproj += "  <PropertyGroup>\n";
	csproj += "    <Configuration Condition=\" '$(Configuration)' == '' \">Debug</Configuration>\n";
	csproj += "    <Platform Condition=\" '$(Platform)' == '' \">AnyCPU</Platform>\n";
	csproj += "    <ProjectGuid>{" + String::num_int64((int64_t)time(nullptr), 16) + "}</ProjectGuid>\n";
	csproj += "    <OutputType>Library</OutputType>\n";
	csproj += "    <RootNamespace>" + p_project_name + "</RootNamespace>\n";
	csproj += "    <AssemblyName>" + p_project_name + "</AssemblyName>\n";
	csproj += "    <TargetFrameworkVersion>v4.8</TargetFrameworkVersion>\n";
	csproj += "    <LangVersion>latest</LangVersion>\n";
	csproj += "    <FileAlignment>512</FileAlignment>\n";
	csproj += "  </PropertyGroup>\n";
	csproj += "  <PropertyGroup Condition=\" '$(Configuration)|$(Platform)' == 'Debug|AnyCPU' \">\n";
	csproj += "    <DebugSymbols>true</DebugSymbols>\n";
	csproj += "    <DebugType>full</DebugType>\n";
	csproj += "    <Optimize>false</Optimize>\n";
	csproj += "    <OutputPath>.mono\\temp\\bin\\Debug\\</OutputPath>\n";
	csproj += "    <DefineConstants>DEBUG;TRACE;GODOT;GODOT_WINDOWS;TOOLS</DefineConstants>\n";
	csproj += "    <ErrorReport>prompt</ErrorReport>\n";
	csproj += "    <WarningLevel>4</WarningLevel>\n";
	csproj += "  </PropertyGroup>\n";
	csproj += "  <PropertyGroup Condition=\" '$(Configuration)|$(Platform)' == 'Release|AnyCPU' \">\n";
	csproj += "    <DebugType>pdbonly</DebugType>\n";
	csproj += "    <Optimize>true</Optimize>\n";
	csproj += "    <OutputPath>.mono\\temp\\bin\\Release\\</OutputPath>\n";
	csproj += "    <DefineConstants>TRACE;GODOT;GODOT_WINDOWS</DefineConstants>\n";
	csproj += "    <ErrorReport>prompt</ErrorReport>\n";
	csproj += "    <WarningLevel>4</WarningLevel>\n";
	csproj += "  </PropertyGroup>\n";
	csproj += "  <ItemGroup>\n";
	csproj += "    <Reference Include=\"GodotSharp\">\n";
	csproj += "      <HintPath>" + godotsharp_ref.replace("\\", "/") + "</HintPath>\n";
	csproj += "      <Private>False</Private>\n";
	csproj += "    </Reference>\n";
	csproj += "  </ItemGroup>\n";
	csproj += "  <ItemGroup>\n";
	csproj += "    <Compile Include=\"**\\*.cs\" />\n";
	csproj += "  </ItemGroup>\n";
	csproj += "  <Import Project=\"$(MSBuildToolsPath)\\Microsoft.CSharp.targets\" />\n";
	csproj += "</Project>\n";
	return csproj;
}

// ---------------------------------------------------------------------------
// .sln generation
// ---------------------------------------------------------------------------

static String generate_sln_content(const String &p_project_name, const String &p_csproj_file) {
	// Generate a stable-ish GUID from the project name
	uint32_t hash = 5381;
	for (int i = 0; i < p_project_name.length(); i++) {
		hash = ((hash << 5) + hash) + (uint32_t)p_project_name[i];
	}

	String guid = String::num_int64(hash, 16);
	// Pad to 32 hex chars
	while (guid.length() < 32) {
		guid = "0" + guid;
	}
	guid = guid.substr(0, 8) + "-" + guid.substr(8, 4) + "-" + guid.substr(12, 4) + "-" + guid.substr(16, 4) + "-" + guid.substr(20, 12);

	String sln;
	sln += "Microsoft Visual Studio Solution File, Format Version 12.00\n";
	sln += "# Visual Studio 2012\n";
	sln += "Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"" + p_project_name + "\", \"" + p_csproj_file + "\", \"{" + guid + "}\"\n";
	sln += "EndProject\n";
	sln += "Global\n";
	sln += "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
	sln += "\t\tDebug|Any CPU = Debug|Any CPU\n";
	sln += "\t\tRelease|Any CPU = Release|Any CPU\n";
	sln += "\tEndGlobalSection\n";
	sln += "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
	sln += "\t\t{" + guid + "}.Debug|Any CPU.ActiveCfg = Debug|Any CPU\n";
	sln += "\t\t{" + guid + "}.Debug|Any CPU.Build.0 = Debug|Any CPU\n";
	sln += "\t\t{" + guid + "}.Release|Any CPU.ActiveCfg = Release|Any CPU\n";
	sln += "\t\t{" + guid + "}.Release|Any CPU.Build.0 = Release|Any CPU\n";
	sln += "\tEndGlobalSection\n";
	sln += "EndGlobal\n";
	return sln;
}

// ---------------------------------------------------------------------------
// Project solution creation
// ---------------------------------------------------------------------------

// Forward declaration: remove_dir_recursive is defined later in this file
// (after compile_with_mcs) but used here (foreign csproj cleanup).
static void remove_dir_recursive(const String &p_dir);

bool csharp_editor_ensure_project_solution() {
	String project_name = get_project_name();
	String project_dir = get_project_dir();
	String csproj_path = csharp_editor_get_csproj_path();
	String sln_path = csharp_editor_get_sln_path();

	bool created = false;
	bool regenerated_existing = false;

	// -----------------------------------------------------------------------
	// 外来 csproj 检测：如果 csproj 已存在，但它是 Godot.NET.Sdk 官方格式
	// （Sdk= 属性）、TargetFramework=net8.0/6.0/7.0、或不包含我们模块约定的
	// `HintPath=.../GodotSharp.dll`，则判定为「外来格式」——典型是 Chickensoft
	// GameDemo 模板。这种 csproj 即使用 dotnet build 成功，产出的 DLL 也是
	// net8.0 + 依赖官方 GodotSharp.SourceGenerators，**与本模块 net48 + 静态链接
	// Mono 6.12 的运行时完全不兼容**，必须替换。
	//
	// 处理策略（可回滚）：
	//   1. 把旧 csproj → <name>.csproj.<timestamp>.mono_new_backup
	//      旧 global.json → global.json.<timestamp>.mono_new_backup
	//      旧 sln → <name>.sln.<timestamp>.mono_new_backup
	//   2. 生成我们自己的 net48 旧式 csproj + sln
	//   3. 生成一份空的 global.json（显式不锁 SDK 版本，rollForward=latestMajor），
	//      这样 Chickensoft 原来锁 8.0.423 就不会把 dotnet build 搞挂
	//   4. 打一条醒目的 INFO 日志：告诉用户备份位置、如何回滚、如果他们想保留
	//      原来的 Godot.NET.Sdk 方案就用官方 Godot 4 .NET 模块而不是 mono_new。
	// -----------------------------------------------------------------------
	if (FileAccess::exists(csproj_path)) {
		String existing;
		{
			Error err;
			Ref<FileAccess> f = FileAccess::open(csproj_path, FileAccess::READ, &err);
			if (f.is_valid()) {
				existing = f->get_as_text();
				f->close();
			}
		}
		bool has_sdk_attr = existing.contains("Sdk=\"Godot.NET.Sdk") ||
		                     existing.contains("Sdk='Godot.NET.Sdk") ||
		                     existing.find("<Project Sdk=") >= 0;
		bool has_netcore_tfm = existing.contains("net8.0") ||
		                        existing.contains("net7.0") ||
		                        existing.contains("net6.0") ||
		                        existing.contains("TargetFramework>net");
		bool missing_our_godotsharp_hint = !existing.contains("GodotSharp.dll") &&
		                                    !existing.contains("HintPath") &&
		                                    has_sdk_attr;
		if (has_sdk_attr || has_netcore_tfm || missing_our_godotsharp_hint) {
			MonoLogger::log_warning(vformat(
					"Detected FOREIGN C# project format at:\n  %s\n"
					"  Project uses Sdk-style / Godot.NET.Sdk / net6/7/8.0 TFM which is INCOMPATIBLE with\n"
					"  the mono_new module (net48 + statically linked Mono 6.12).\n"
					"  Auto-replacing with module-generated project.\n"
					"  Backup files (safe to delete) will be renamed to *.mono_new_backup — "
					"rename them back without the .mono_new_backup suffix to roll back.\n"
					"  If you intended to use official Godot .NET 6+/CoreCLR, disable the mono_new module\n"
					"  and use official Godot 4 .NET builds instead.",
					csproj_path));

			String ts = String::num_int64((int64_t)time(nullptr));
			{
				String backup = csproj_path + "." + ts + ".mono_new_backup";
				DirAccess::copy_absolute(csproj_path, backup);
				Error re = DirAccess::remove_absolute(csproj_path);
				MonoLogger::log(vformat("  Backed up: %s  ->  %s  (removed original: err=%d)",
				                        csproj_path, backup, (int)re));
			}
			if (FileAccess::exists(sln_path)) {
				String backup = sln_path + "." + ts + ".mono_new_backup";
				DirAccess::copy_absolute(sln_path, backup);
				DirAccess::remove_absolute(sln_path);
				MonoLogger::log(vformat("  Backed up: %s  ->  %s", sln_path, backup));
			}
			String global_json = project_dir.path_join("global.json");
			if (FileAccess::exists(global_json)) {
				String backup = global_json + "." + ts + ".mono_new_backup";
				DirAccess::copy_absolute(global_json, backup);
				DirAccess::remove_absolute(global_json);
				MonoLogger::log(vformat("  Backed up: %s  ->  %s (pinned SDK version removed so dotnet build works)",
				                        global_json, backup));
				// 写一份宽松的 global.json：显式不锁版本，用本机最新已安装 SDK
				{
					Error gerr;
					Ref<FileAccess> gf = FileAccess::open(global_json, FileAccess::WRITE, &gerr);
					if (gf.is_valid()) {
						gf->store_string(
							"{\n"
							"  \"sdk\": {\n"
							"    \"rollForward\": \"latestMajor\",\n"
							"    \"allowPrerelease\": false\n"
							"  }\n"
							"}\n");
						gf->close();
						MonoLogger::log(vformat("  Wrote relaxed global.json: %s", global_json));
					}
				}
			}
			// 同时删掉旧的 obj/ 和 .mono/temp，避免旧缓存干扰新 csproj 编译
			{
				String obj_dir = project_dir.path_join("obj");
				if (DirAccess::exists(obj_dir)) remove_dir_recursive(obj_dir);
				String temp_bin = project_dir.path_join(".mono").path_join("temp");
				if (DirAccess::exists(temp_bin)) remove_dir_recursive(temp_bin);
			}
			regenerated_existing = true;
		}
	}

	if (!FileAccess::exists(csproj_path)) {
		MonoLogger::log(vformat("Generating C# project file: %s", csproj_path));
		String content = generate_csproj_content(project_name);
		Error err;
		Ref<FileAccess> f = FileAccess::open(csproj_path, FileAccess::WRITE, &err);
		if (err != OK || f.is_null()) {
			MonoLogger::log_error(vformat("Failed to create .csproj: %s", csproj_path));
			return false;
		}
		f->store_string(content);
		f->close();
		created = true;
	}

	if (!FileAccess::exists(sln_path)) {
		MonoLogger::log(vformat("Generating C# solution file: %s", sln_path));
		String csproj_file = project_name + ".csproj";
		String content = generate_sln_content(project_name, csproj_file);
		Error err;
		Ref<FileAccess> f = FileAccess::open(sln_path, FileAccess::WRITE, &err);
		if (err != OK || f.is_null()) {
			MonoLogger::log_error(vformat("Failed to create .sln: %s", sln_path));
			return false;
		}
		f->store_string(content);
		f->close();
		created = true;
	}

	// Ensure assemblies output dir exists
	String assemblies_dir = csharp_editor_get_assemblies_output_dir();
	(void)assemblies_dir;

	if (created || regenerated_existing) {
		MonoLogger::log(vformat("C# project solution files %s",
		                        regenerated_existing ? "regenerated (foreign format backed up)" : "created"));
	}

	return true;
}

// ---------------------------------------------------------------------------
// Compiler discovery and invocation
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// NuGet package restore
// ---------------------------------------------------------------------------

// Run the nuget_restore.py script to restore packages from packages.config.
// Returns a list of absolute DLL reference paths (POSIX slashes).
// If packages.config is absent or restore fails, returns an empty list
// and logs a warning (compilation proceeds without NuGet references).
static Vector<String> nuget_restore(const String &p_project_dir) {
	String packages_config = p_project_dir.path_join("packages.config");
	if (!FileAccess::exists(packages_config)) {
		return Vector<String>();
	}

	MonoLogger::log("Found packages.config - running NuGet restore...");

	// Locate the restore script relative to the module directory.
	// The script ships at modules/mono_new/scripts/nuget_restore.py
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String script_path = exe_dir.path_join("..").path_join("modules").path_join("mono_new").path_join("scripts").path_join("nuget_restore.py");
	if (!FileAccess::exists(script_path)) {
		// Try relative to source tree (dev builds)
		script_path = "modules/mono_new/scripts/nuget_restore.py";
	}
	if (!FileAccess::exists(script_path)) {
		MonoLogger::log_warning("nuget_restore.py not found - skipping NuGet restore");
		return Vector<String>();
	}

	// Find a Python interpreter.
	String python = "python";
	{
		String output;
		int exit_code = -1;
		Error err = OS::get_singleton()->execute(python, List<String>(), &output, &exit_code);
		if (err != OK) {
			python = "python3";
			err = OS::get_singleton()->execute(python, List<String>(), &output, &exit_code);
			if (err != OK) {
				MonoLogger::log_warning("Python interpreter not found - skipping NuGet restore");
				return Vector<String>();
			}
		}
	}

	// Invoke: python nuget_restore.py --project-dir <dir> --print-references
	List<String> args;
	args.push_back(script_path);
	args.push_back("--project-dir");
	args.push_back(p_project_dir);
	args.push_back("--print-references");

	String output;
	int exit_code = -1;
	Error err = OS::get_singleton()->execute(python, args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		MonoLogger::log_warning(vformat("NuGet restore failed (exit %d). Compiling without NuGet references.", exit_code));
		if (!output.is_empty()) {
			MonoLogger::log_warning(output);
		}
		return Vector<String>();
	}

	// Parse stdout: one reference path per line.
	Vector<String> references;
	Vector<String> lines = output.split("\n", false);
	for (const String &line : lines) {
		String trimmed = line.strip_edges();
		if (trimmed.is_empty()) continue;
		// Skip [NuGet] log lines (they go to stderr, but be defensive).
		if (trimmed.begins_with("[NuGet")) continue;
		if (FileAccess::exists(trimmed)) {
			references.push_back(trimmed);
		}
	}

	MonoLogger::log(vformat("NuGet restore: %d reference(s) resolved", references.size()));
	return references;
}

// ---------------------------------------------------------------------------
// Execute-with-timeout: 同步 OS::execute 的看门狗包装。
// 根因：Windows 下 dotnet.exe 可能被 Windows Defender / SDK resolver
// 或被 global.json 锁定未装 SDK 永久卡住；此时同步 OS::execute 在主
// 线程被 block 后编辑器会「完全无响应」。解决方案：启动独立 watchdog
// 线程，超过 timeout_ms 还没返回就 force kill 所有 dotnet/dotnet.exe
// 子进程（探测阶段副作用可接受），让 OS::execute 提前返回错误并回
// 退到 csc/mcs，保证编辑器 3s 内必进主窗口。
// ---------------------------------------------------------------------------
struct _ExecWatchdogCtx {
	Mutex state_mut;
	bool armed = false;       // watchdog 激活 = execute 仍在跑
	bool fired = false;       // 已触发 timeout + kill
	int timeout_ms = 3000;    // 默认 3000ms
};

static void _watchdog_thread_proc(void *p_user) {
	_ExecWatchdogCtx *ctx = (_ExecWatchdogCtx *)p_user;
	const int step_ms = 20;
	int waited = 0;
	while (waited < ctx->timeout_ms) {
		{
			MutexLock lock(ctx->state_mut);
			if (!ctx->armed) {
				return; // execute 正常返回，提前退出
			}
		}
		OS::get_singleton()->delay_usec(step_ms * 1000);
		waited += step_ms;
	}

	bool do_kill = false;
	{
		MutexLock lock(ctx->state_mut);
		if (ctx->armed) {
			ctx->fired = true;
			do_kill = true;
		}
	}
	if (!do_kill) return;

	MonoLogger::log_warning(
			"[C#] dotnet compiler probe exceeded timeout (3000ms) on main thread. "
			"Force-killing dotnet child processes; falling back to csc/mcs. "
			"Fix: install a .NET SDK version matching your project's global.json, "
			"or relax/remove the SDK pin inside global.json.");
	// 只杀本编辑器进程派生的 dotnet 子进程树（按父 PID 过滤），避免误杀
	// 系统上其它无关的 dotnet 进程（其它编辑器实例、VS MSBuild 后台节点等）。
	const String our_pid = itos((int64_t)OS::get_singleton()->get_process_id());
#ifdef WINDOWS_ENABLED
	List<String> killer_args;
	killer_args.push_back("-NoProfile");
	killer_args.push_back("-Command");
	killer_args.push_back(
			"Get-CimInstance Win32_Process | "
			"Where-Object { $_.Name -eq 'dotnet.exe' -and $_.ParentProcessId -eq " + our_pid + " } | "
			"ForEach-Object { taskkill /F /T /PID $($_.ProcessId) > $null 2>&1 }");
	String ignore_out;
	int ignore_ec = -1;
	OS::get_singleton()->execute("powershell.exe", killer_args, &ignore_out, &ignore_ec, true);
#else
	// pkill -P：只杀以本进程为父的 dotnet 子进程。
	List<String> killer_args;
	killer_args.push_back("-9");
	killer_args.push_back("-P");
	killer_args.push_back(our_pid);
	killer_args.push_back("dotnet");
	String ignore_out;
	int ignore_ec = -1;
	OS::get_singleton()->execute("pkill", killer_args, &ignore_out, &ignore_ec, true);
#endif
}

static Error _execute_with_watchdog(const String &p_path, const List<String> &p_arguments,
		String *r_pipe, int *r_exitcode, bool p_read_stderr = false, int p_timeout_ms = 3000) {
	_ExecWatchdogCtx ctx;
	ctx.timeout_ms = p_timeout_ms;

	Thread *wd_thread = memnew(Thread);
	{
		MutexLock lock(ctx.state_mut);
		ctx.armed = true;
		ctx.fired = false;
	}
	Thread::Settings lowpri;
	lowpri.priority = Thread::PRIORITY_LOW;
	wd_thread->start(_watchdog_thread_proc, &ctx, lowpri);

	// 真正的同步 execute（若 dotnet 卡死，watchdog 会 kill 让系统调用返回）
	Error err = OS::get_singleton()->execute(p_path, p_arguments, r_pipe, r_exitcode, p_read_stderr);

	bool fired_copy = false;
	{
		MutexLock lock(ctx.state_mut);
		ctx.armed = false;
		fired_copy = ctx.fired;
	}
	if (wd_thread->is_started()) {
		wd_thread->wait_to_finish();
	}
	memdelete(wd_thread);

	if (fired_copy) {
		if (r_exitcode) *r_exitcode = -1;
		return ERR_TIMEOUT;
	}
	return err;
}

// M8/M12 编译器探测缓存：文件级静态变量（失效由 invalidate_csharp_compiler_cache 控制）。
// 在以下场景会失效：(a) mcs.bat 实际编译失败走 dotnet fallback 时，
// 说明这个 mcs.bat 是假阳性（Companion mcs.exe 缺失的精简 Mono 安装），
// 清缓存后下次 find_csharp_compiler 会自动跳过它、直接命中可用的 dotnet。
static String s_csharp_compiler_cache;

void invalidate_csharp_compiler_cache() {
	s_csharp_compiler_cache = String();
}

static String find_csharp_compiler() {
	// 缓存编译器探测结果，避免每次保存脚本都启动子进程探测。
	// 编译器在编辑器会话内一般不会变化；一旦某次候选（如残缺 mcs.bat）被实锤失败，
	// 调 invalidate_csharp_compiler_cache 即可触发下一轮重新探测。
	if (!s_csharp_compiler_cache.is_empty()) {
		return s_csharp_compiler_cache;
	}

	// 1. Try mcs from Mono installation
	Vector<String> candidates;

	// System Mono path (Windows)
	String mono_bin = OS::get_singleton()->get_environment("MONO_PREFIX");
	if (!mono_bin.is_empty()) {
		candidates.push_back(mono_bin.path_join("bin").path_join("mcs"));
	}

#ifdef WINDOWS_ENABLED
	candidates.push_back("C:/Program Files/Mono/bin/mcs.bat");
	candidates.push_back("C:/Program Files (x86)/Mono/bin/mcs.bat");
#endif

	// Try dotnet
	candidates.push_back("dotnet");

	// Try csc (Windows SDK / Roslyn)
#ifdef WINDOWS_ENABLED
	candidates.push_back("csc");
#endif

	for (const String &candidate : candidates) {
#if GD_MONO_SKIP_DOTNET_PROBE
		// R3 保险：跳过 dotnet 子进程探测链，直接走 csc/mcs 兜底。
		// 本项目用户工程都是 net48 + Mono BCL，不依赖 dotnet SDK。
		if (candidate == "dotnet") {
			continue;
		}
#endif
		if (candidate == "dotnet") {
			// 不能只检查 dotnet 命令能否启动（无参 dotnet 会打印帮助然后 exit 0）。
			// 必须验证：本机装了至少一个 SDK，且所有探测命令全部在 exe_dir 沙箱下运行
			// （引擎 bin 目录，保证没有项目的 global.json 干扰）。否则被 Chickensoft 一类
			// 模板锁定 SDK 版本的 global.json 误伤，会直接把 dotnet 标成不可用。
			// 三步探测任何一步失败，回退到 csc/mcs。
			bool ok = false;
			String installed_sdks_preview;
			String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
			String old_cwd = OS::get_singleton()->get_cwd();
			OS::get_singleton()->set_cwd(exe_dir);
			{
				String out_ver;
				int ec = -1;
				List<String> ver_args;
				ver_args.push_back("--version");
				MonoLogger::log(vformat("Probing dotnet: step 1/3 `dotnet --version` (sandboxed at exe_dir: %s) ...", exe_dir));
				Error err = _execute_with_watchdog(
						"dotnet", ver_args, &out_ver, &ec);
				if (err == OK && ec == 0 && !out_ver.strip_edges().is_empty()) {
					ok = true;
					MonoLogger::log(vformat("Probing dotnet: step 1/3 OK -> runtime reports SDK version '%s'", out_ver.strip_edges()));
				} else {
					MonoLogger::log_warning(vformat(
							"Probing dotnet: step 1/3 FAILED (err=%d exit=%d, sandboxed at exe_dir). "
							"dotnet --version output:\n%s"
							"Sandboxed probe (no global.json influence) means this machine genuinely has no usable .NET SDK. "
							"Install a .NET 7.x SDK (any 7.0.4xx) so 'dotnet --version' prints a valid version.",
							(int)err, ec, out_ver));
				}
			}
			if (ok) {
				String out_sdks;
				int ec_sdks = -1;
				List<String> sdks_args;
				sdks_args.push_back("--list-sdks");
				MonoLogger::log("Probing dotnet: step 2/3 `dotnet --list-sdks` (sandboxed at exe_dir) ...");
				Error err_sdks = _execute_with_watchdog(
						"dotnet", sdks_args, &out_sdks, &ec_sdks);
				Vector<String> lines = out_sdks.split("\n");
				int available = 0;
				for (int i = 0; i < lines.size(); i++) {
					if (!lines[i].strip_edges().is_empty()) {
						available++;
						if (installed_sdks_preview.length() < 200) {
							if (!installed_sdks_preview.is_empty()) installed_sdks_preview += ", ";
							installed_sdks_preview += lines[i].strip_edges();
						}
					}
				}
				if (err_sdks != OK || ec_sdks != 0 || available == 0) {
					MonoLogger::log_warning(vformat(
							"Probing dotnet: step 2/3 FAILED (installed sdks count=%d, err=%d exit=%d). "
							"Falling back to csc/mcs. Install a .NET 7.x SDK (any 7.0.4xx) to enable dotnet build. "
							"Installed SDKs preview: %s",
							available, (int)err_sdks, ec_sdks, installed_sdks_preview));
					ok = false;
				} else {
					MonoLogger::log(vformat(
							"Probing dotnet: step 2/3 OK -> %d installed SDKs. First few: %s",
							available, installed_sdks_preview));
				}
			}
			if (ok) {
				List<String> help_args;
				help_args.push_back("build");
				help_args.push_back("--help");
				String out_help;
				int ec_help = -1;
				MonoLogger::log("Probing dotnet: step 3/3 `dotnet build --help` (sandboxed at exe_dir) ...");
				Error err_help = _execute_with_watchdog(
						"dotnet", help_args, &out_help, &ec_help, true);
				if (err_help != OK || ec_help != 0) {
					MonoLogger::log_warning(vformat(
							"Probing dotnet: step 3/3 FAILED (err=%d exit=%d). Falling back to csc/mcs. "
							"Output tail:\n%s",
							(int)err_help, ec_help, out_help.substr(out_help.length() - MIN(out_help.length(), 1200))));
					ok = false;
				} else {
					MonoLogger::log("Probing dotnet: step 3/3 OK -> dotnet build --help succeeded.");
				}
			}
			OS::get_singleton()->set_cwd(old_cwd);
			if (ok) {
				s_csharp_compiler_cache = candidate;
				MonoLogger::log("Using dotnet CLI as C# compiler.");
				return s_csharp_compiler_cache;
			}
		} else if (candidate.ends_with("mcs.bat")) {
			// mcs.bat 是 Mono 官方分发包在 Windows 上的入口脚本。
			// 典型的"精简安装"（只装了运行时，没装完整 SDK）会导致 mcs.bat 存在但
			// companion 的 lib/mono/4.5/mcs.exe 缺失，强行调用会直接 exit code 2
			// + 一串 ERROR 日志，然后 fallback 到 dotnet build。
			//
			// 所以这里加一次 Companion EXE 实锤校验：
			//   - mcs.exe 找到 → 信任这个 mcs.bat，写入缓存并返回
			//   - mcs.exe 缺失 → 不打 WARNING（避免日志噪音），直接 continue 跳过，
			//     下一个候选就是 dotnet（本机已通过完整三探测，可用）。这样下次
			//     保存脚本直接走 dotnet，不会再先触发必定失败的 mcs.bat。
			if (FileAccess::exists(candidate)) {
				String expected_mcs_exe = candidate.get_base_dir().path_join("..").path_join("lib").path_join("mono").path_join("4.5").path_join("mcs.exe");
				if (FileAccess::exists(expected_mcs_exe)) {
					s_csharp_compiler_cache = candidate;
					return s_csharp_compiler_cache;
				}
				// Companion 缺失 → info 级日志仅提示一次（DEBUG 会话内可能想知道为什么没走 mcs）。
				MonoLogger::log(vformat(
						"mcs.bat at %s skipped (companion mcs.exe was not found at %s). "
						"Falling through to the next compiler candidate (dotnet/csc).",
						candidate, expected_mcs_exe));
				continue;
			}
		} else {
			if (FileAccess::exists(candidate)) {
				s_csharp_compiler_cache = candidate;
				return s_csharp_compiler_cache;
			}
		}
	}

	// C3 终极兜底：
	//
	// 如果上面全落空（典型：C:/Program Files/Mono/bin/mcs.bat 存在但 4.5/mcs.exe 缺失，
	// 之前旧逻辑直接 skip；又没装 csc；又因为 SKIP_DOTNET_PROBE=1 跳过 dotnet），
	// 最后不要把 s_csharp_compiler_cache 盲目写成 "mcs" 字符串——那只会导致 CreateProcess 报
	// ERROR_FILE_NOT_FOUND（你日志里的 Error 29）。
	//
	// 正确兜底顺序：
	//   1. 用 `where mcs` (Windows) / `which mcs` (Unix) 查 PATH 中是否真的有 mcs；
	//   2. 如果 mcs 不在 PATH，但前面 dotnet 分支被 SKIP 或因为 global.json 失败，
	//      再单独跑一次 dotnet --version，只求找到一个能产生人类可读错误的编译器；
	//   3. 真的全都没有时，返回空字符串并打一条明确的 ERROR 日志，告诉用户安装哪个。
#ifdef WINDOWS_ENABLED
	{
		String where_out;
		int where_ec = -1;
		List<String> where_args;
		where_args.push_back("mcs");
		// `where` 是 Windows shell 内置，成功率 > PATH 手动拼（我们不需要真正定位到文件，只看 exit code）。
		Error werr = _execute_with_watchdog("where.exe", where_args, &where_out, &where_ec, false, 2000);
		if (werr == OK && where_ec == 0 && !where_out.strip_edges().is_empty()) {
			s_csharp_compiler_cache = "mcs";
			MonoLogger::log_warning("No preferred compiler found (mcs.bat/dotnet/csc all unavailable). "
			                        "Falling back to `mcs` discovered on PATH via where.exe (user assembly builds may fail).");
			return s_csharp_compiler_cache;
		}
	}
#endif

#if GD_MONO_SKIP_DOTNET_PROBE
	// 用户配置了强跳过 dotnet，但前面所有非 dotnet 候选都没命中，说明系统压根没有 Mono/csc。
	// 为不彻底堵死，这里做一次性 soft-fallback：只跑 dotnet --version 一个最小探测，
	// 不再跑 list-sdks / build --help，省时间 + 避开 global.json 冲突。
	{
		String out_ver;
		int ec = -1;
		List<String> ver_args;
		ver_args.push_back("--version");
		Error err = _execute_with_watchdog("dotnet", ver_args, &out_ver, &ec, false, 4000);
		if (err == OK && ec == 0 && !out_ver.strip_edges().is_empty()) {
			s_csharp_compiler_cache = "dotnet";
			MonoLogger::log_warning(vformat(
					"GD_MONO_SKIP_DOTNET_PROBE=1 but no Mono/csc on PATH; soft-fallback to dotnet %s anyway. "
					"If this fails due to global.json mismatch, relax your project's pinned SDK version.",
					out_ver.strip_edges()));
			return s_csharp_compiler_cache;
		}
	}
#else
	// 非 SKIP 模式下，dotnet 已经在前面的 for 循环完整跑过三探测，成功就 return 了，
	// 能走到这里只能是 dotnet 真的失败（global.json 卡住 / 无 SDK）。
	(void)0;
#endif

	// 最后，明确告诉用户「什么都没有」，而不是默默塞 "mcs" 导致 Error 29 无头公案。
	MonoLogger::log_error(
			"No usable C# compiler found on this machine.\n"
			"  Checked (in order): MONO_PREFIX/bin/mcs, C:/Program Files/Mono/bin/mcs.bat,\n"
			"                       dotnet SDK (--version/--list-sdks/build --help),\n"
			"                       csc (Windows SDK / Roslyn), where.exe mcs on PATH.\n"
			"  Fix options (choose ONE):\n"
			"    (a) Install a .NET 7.x SDK (any 7.0.4xx) and make sure dotnet --version prints it;\n"
			"    (b) Install full Mono 6.12 for Windows (so C:/Program Files/Mono/lib/mono/4.5/mcs.exe exists);\n"
			"    (c) Install Visual Studio Build Tools (adds csc.exe to Developer Command Prompt PATH).\n"
			"  Until a compiler is available, C# script auto-compile on save will fail with Error 29.");
	s_csharp_compiler_cache = "";
	return s_csharp_compiler_cache;
}

static bool compile_with_mcs(const String &p_compiler, const String &p_project_dir,
		const String &p_output_dll, const String &p_godotsharp_ref,
		const Vector<String> &p_nuget_references = Vector<String>()) {
	// Collect all .cs files in the project directory
	Vector<String> cs_files;
	Ref<DirAccess> dir = DirAccess::open(p_project_dir);
	if (dir.is_null()) {
		MonoLogger::log_error(vformat("Cannot open project dir for compilation: %s", p_project_dir));
		return false;
	}

	// Recursive scan for .cs files
	List<String> dirs_to_scan;
	dirs_to_scan.push_back(p_project_dir);
	while (!dirs_to_scan.is_empty()) {
		String current_dir = dirs_to_scan.front()->get();
		dirs_to_scan.pop_front();

		Ref<DirAccess> d = DirAccess::open(current_dir);
		if (d.is_null()) continue;

		d->list_dir_begin();
		String fname = d->get_next();
		while (!fname.is_empty()) {
			if (fname == "." || fname == ".." || fname == ".git" || fname == ".mono" || fname == "addons") {
				fname = d->get_next();
				continue;
			}
			String full = current_dir.path_join(fname);
			if (d->current_is_dir()) {
				dirs_to_scan.push_back(full);
			} else if (fname.ends_with(".cs")) {
				cs_files.push_back(full);
			}
			fname = d->get_next();
		}
		d->list_dir_end();
	}

	if (cs_files.is_empty()) {
		MonoLogger::log_warning("No .cs files found to compile");
		return false;
	}

	MonoLogger::log(vformat("Found %d C# files to compile", cs_files.size()));

	// Build mcs command: mcs -target:library -out:<dll> -r:GodotSharp.dll <files...>
	List<String> args;
	args.push_back("-target:library");
	args.push_back("-unsafe");
	args.push_back("-out:" + p_output_dll);
	args.push_back("-r:" + p_godotsharp_ref);

	// Add reference to System assemblies from Mono BCL.
	// 优先用引擎自带的 BCL（exe_dir/mono/lib/mono/4.5）， fallback 到系统 Mono。
	// 同时设置 MONO_PATH 环境变量，让 mcs.bat 调用的 mono.exe 能加载 mscorlib.dll
	// （系统 Mono 安装可能不完整，4.5 目录可能为空）。
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String engine_bcl = exe_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
	bool engine_bcl_ok = FileAccess::exists(engine_bcl.path_join("mscorlib.dll"));

	String mono_lib = OS::get_singleton()->get_environment("MONO_PREFIX");
	if (mono_lib.is_empty()) {
#ifdef WINDOWS_ENABLED
		mono_lib = "C:/Program Files/Mono";
#endif
	}

	String bcl_dir;
	if (engine_bcl_ok) {
		bcl_dir = engine_bcl;
		// 让 mcs.bat 内部的 mono.exe 优先从引擎 BCL 加载 mscorlib.dll
		OS::get_singleton()->set_environment("MONO_PATH", engine_bcl);
	} else if (!mono_lib.is_empty()) {
		bcl_dir = mono_lib.path_join("lib").path_join("mono").path_join("4.5");
	}

	if (!bcl_dir.is_empty()) {
		args.push_back("-lib:" + bcl_dir);
	}

	// Add NuGet references (-r:<path> for each restored package DLL).
	for (const String &ref : p_nuget_references) {
		args.push_back("-r:" + ref);
	}

	for (const String &cs : cs_files) {
		args.push_back(cs);
	}

	String output;
	int exit_code = -1;
	MonoLogger::log(vformat("Invoking compiler: %s", p_compiler));
	Error err = OS::get_singleton()->execute(p_compiler, args, &output, &exit_code, true);

	if (err != OK) {
		MonoLogger::log_error(vformat("Failed to execute compiler: %s (error: %d)", p_compiler, err));
		return false;
	}

	if (exit_code != 0) {
		MonoLogger::log_error(vformat("Compilation failed with exit code %d", exit_code));
		if (!output.is_empty()) {
			MonoLogger::log_error("Compiler output:\n" + output);
		}
		return false;
	}

	MonoLogger::log(vformat("Compilation succeeded: %s", p_output_dll));
	return true;
}

// Recursively remove a directory tree. DirAccess::remove() only works on
// empty directories, so we must empty subtrees bottom-up before removing
// the directory itself.
static void remove_dir_recursive(const String &p_dir) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	String fname = dir->get_next();
	while (!fname.is_empty()) {
		if (fname != "." && fname != "..") {
			String full = p_dir.path_join(fname);
			if (dir->current_is_dir()) {
				remove_dir_recursive(full);
			} else {
				Error e = dir->remove(fname);
				if (e != OK) {
					MonoLogger::log_warning(vformat("Failed to remove file: %s (err %d)", full, e));
				}
			}
		}
		fname = dir->get_next();
	}
	dir->list_dir_end();
	dir.unref(); // release handle before removing

	// Now the directory is empty; remove it via its parent.
	String parent = p_dir.get_base_dir();
	String base = p_dir.get_file();
	Ref<DirAccess> parent_dir = DirAccess::open(parent);
	if (parent_dir.is_valid()) {
		Error e = parent_dir->remove(base);
		if (e != OK) {
			MonoLogger::log_warning(vformat("Failed to remove dir: %s (err %d)", p_dir, e));
		}
	}
}

// 从 dotnet build 的已知输出位置查找编译产物并复制到程序集输出目录。
// 4 个候选按优先级排列：生成 csproj 的 OutputPath（.mono/temp/bin/Debug）、
// SDK 风格 net48 输出、旧式输出、平台特定 AnyCPU 输出。
// 找到并复制成功返回 true；所有候选都不存在返回 false（由调用方打 WARNING）。
static bool copy_built_assembly_to_output(const String &p_project_dir, const String &p_project_name, const String &p_output_dll) {
	Vector<String> candidate_built_dlls;
	candidate_built_dlls.push_back(p_project_dir.path_join(".mono").path_join("temp").path_join("bin").path_join("Debug").path_join(p_project_name + ".dll"));
	candidate_built_dlls.push_back(p_project_dir.path_join("bin").path_join("Debug").path_join("net48").path_join(p_project_name + ".dll"));
	candidate_built_dlls.push_back(p_project_dir.path_join("bin").path_join("Debug").path_join(p_project_name + ".dll"));
	candidate_built_dlls.push_back(p_project_dir.path_join("bin").path_join("Debug").path_join("AnyCPU").path_join(p_project_name + ".dll"));
	String built_dll;
	for (const String &cand : candidate_built_dlls) {
		if (FileAccess::exists(cand)) {
			built_dll = cand;
			break;
		}
	}
	if (built_dll.is_empty()) {
		return false;
	}
	DirAccess::copy_absolute(built_dll, p_output_dll);
	MonoLogger::log(vformat("Copied built assembly: %s  ->  %s", built_dll, p_output_dll));
	return true;
}

static bool compile_with_dotnet(const String &p_project_dir, const String &p_csproj_path) {
	// Clean stale obj/ artifacts before building. dotnet build generates
	// obj/<Config>/.NETFramework,Version=v4.8.AssemblyAttributes.cs per
	// configuration; if a previous build used a different config (e.g.
	// Release), both files linger and CS0579 "duplicate
	// TargetFrameworkAttribute" fires. Wiping obj/ is the standard remedy.
	{
		String obj_dir = p_project_dir.path_join("obj");
		if (DirAccess::exists(obj_dir)) {
			remove_dir_recursive(obj_dir);
		}
	}

	// Use dotnet build to compile the project
	List<String> args;
	args.push_back("build");
	args.push_back(p_csproj_path);
	args.push_back("-c");
	args.push_back("Debug");

	String output;
	int exit_code = -1;
	MonoLogger::log(vformat("Invoking compiler: dotnet build %s (Debug)", p_csproj_path.get_file()));
	Error err = OS::get_singleton()->execute("dotnet", args, &output, &exit_code, true);

	if (err != OK) {
		MonoLogger::log_error(vformat(
				"Failed to execute dotnet build (err=%d). This typically means:\n"
				"  (a) dotnet is not on PATH; or\n"
				"  (b) antivirus blocked CreateProcess for dotnet.exe.\n"
				"Fix: verify 'dotnet --version' works in a normal cmd.exe first.",
				(int)err));
		return false;
	}

	if (exit_code != 0) {
		bool is_sdk_not_found = output.contains("A compatible .NET SDK was not found") ||
		                        output.contains("Requested SDK version:") ||
		                        output.contains("The command could not be loaded, possibly because");
		bool is_package_not_found = output.contains("error NU1101") ||
		                             output.contains("Unable to find package");
		bool is_tfm_mismatch = output.contains("net48") && output.contains("not compatible") ||
		                        output.contains("The reference assemblies for .NETFramework,Version=v4.8 were not found");
		MonoLogger::log_error(vformat("dotnet build failed with exit code %d", exit_code));
		if (!output.is_empty()) {
			MonoLogger::log_error("Build output:\n" + output);
		}
		if (is_sdk_not_found) {
			MonoLogger::log_error(
					"  REMEDY (SDK-not-found): The project directory still contains a global.json that pins\n"
					"  to an uninstalled .NET SDK version. Either:\n"
					"    (a) Delete the project's global.json (recommended);\n"
					"    (b) Edit global.json to remove the \"version\" field or set rollForward: \"latestMajor\";\n"
					"    (c) Install the exact .NET SDK version requested in the error output above.\n"
					"  You can also temporarily build from a clean cmd.exe with:\n"
					"    cd \"<engine_bin>/windows\" && dotnet build \"<project>/<Name>.csproj\" -c Debug");
		} else if (is_package_not_found) {
			MonoLogger::log_error(
					"  REMEDY (NuGet): The project references NuGet packages (Chickensoft.* / Godot.NET.Sdk / etc.)\n"
					"  which are not available for net48 + Mono. The mono_new module uses a hand-written GodotSharp\n"
					"  binding that does NOT use official Godot.NET.Sdk. Fix options:\n"
					"    (a) Remove all <PackageReference> entries from the .csproj and use only GodotSharp API;\n"
					"    (b) Copy the required NuGet DLLs manually to a references/ folder and add <Reference Include=... HintPath=...>;\n"
					"    (c) If you need Godot.NET.Sdk / Chickensoft ecosystem, use official Godot 4 .NET (dotnet module) instead of mono_new.");
		} else if (is_tfm_mismatch) {
			MonoLogger::log_error(
					"  REMEDY (TFM): The installed .NET SDK targeting pack doesn't include net48 reference assemblies.\n"
					"  Install .NET Framework 4.8 Developer Pack (https://dotnet.microsoft.com/download/visual-studio-sdks)\n"
					"  or use Mono mcs.exe as the compiler instead.");
		} else {
			MonoLogger::log_error(
					"  REMEDY: Check the build output above for specific CSxxxx errors.\n"
					"  The mono_new module targets net48 + Godot 3-style hand-written bindings (not official Godot 4 .NET API).\n"
					"  Common porting items: Connect(signal, owner, method) → Callable form; don't use double.ToString()/string interpolation; use GD.Print() instead of Console.WriteLine().");
		}
		return false;
	}

	MonoLogger::log("dotnet build succeeded");
	return true;
}

bool csharp_editor_compile_project() {
	if (!csharp_editor_ensure_project_solution()) {
		return false;
	}

	String project_name = get_project_name();
	String project_dir = get_project_dir();
	String output_dir = csharp_editor_get_assemblies_output_dir();

	// Use a versioned filename to avoid Win32 error 1224
	// (ERROR_USER_MAPPED_FILE) when the previous DLL is still loaded by Mono.
	String timestamp = String::num_int64((int64_t)time(nullptr));
	String output_dll = output_dir.path_join(project_name + "_" + timestamp + ".dll");

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String godotsharp_ref = exe_dir.path_join("GodotSharp.dll");

	// Ensure GodotSharp.dll exists in the exe dir (it should be deployed there)
	if (!FileAccess::exists(godotsharp_ref)) {
		// Try looking in assemblies path
		godotsharp_ref = exe_dir.path_join(".mono").path_join("assemblies").path_join("GodotSharp.dll");
	}

	// Best-effort cleanup of old versioned DLLs (those already loaded by Mono
	// cannot be deleted and will be skipped silently).
	{
		Ref<DirAccess> dir = DirAccess::open(output_dir);
		if (dir.is_valid()) {
			dir->list_dir_begin();
			String fname = dir->get_next();
			while (!fname.is_empty()) {
				if (!dir->current_is_dir() &&
						fname.ends_with(".dll") &&
						fname != "GodotSharp.dll" &&
						fname != output_dll.get_file()) {
					String old_path = output_dir.path_join(fname);
					// Try to remove; if it fails (locked), just skip
					dir->remove(fname);
				}
				fname = dir->get_next();
			}
			dir->list_dir_end();
		}
	}

	// Try mcs first (works with Mono installation)
	String compiler = find_csharp_compiler();
	bool success = false;

	// C3 companion: compiler 为空说明 find_csharp_compiler 已经打了完整的 ERROR 日志，
	// 这里不要继续走 compile_with_mcs("") → 否则 OS::execute("") 直接报 Error 29，
	// 用户在日志里看不到我们真正写的「请安装 xxx」说明。
	if (compiler.is_empty()) {
		// 明确的 ERROR 已经在 find_csharp_compiler 里打了，这里只负责失败。
		return false;
	}

	// Run NuGet restore if packages.config exists. The returned references
	// are passed to the mcs/csc compiler. dotnet build handles restore
	// itself via the .csproj, so we skip it for the dotnet path.
	Vector<String> nuget_refs;
	if (compiler != "dotnet") {
		nuget_refs = nuget_restore(project_dir);
	}

	if (compiler == "dotnet") {
		success = compile_with_dotnet(project_dir, csharp_editor_get_csproj_path());
		if (success) {
			// Copy the built DLL from known output locations to assemblies output.
			if (!copy_built_assembly_to_output(project_dir, project_name, output_dll)) {
				MonoLogger::log_warning(vformat(
						"dotnet build reported SUCCESS but no compiled DLL was found at expected locations.\n"
						"  Checked (in order):\n"
						"    %s\\.mono\\temp\\bin\\Debug\\%s.dll\n"
						"    %s\\bin\\Debug\\net48\\%s.dll\n"
						"    %s\\bin\\Debug\\%s.dll\n"
						"    %s\\bin\\Debug\\AnyCPU\\%s.dll\n"
						"  This usually means the csproj's OutputPath was overridden or TargetFramework changed.\n"
						"  Remedy: locate the built DLL manually and copy it to:\n"
						"    %s\n"
						"  or regenerate the csproj via the C# menu.",
						project_dir, project_name,
						project_dir, project_name,
						project_dir, project_name,
						project_dir, project_name,
						output_dll));
				success = false;
			}
		}
	} else if (compiler == "csc") {
		// Use csc directly
		success = compile_with_mcs(compiler, project_dir, output_dll, godotsharp_ref, nuget_refs);
	} else {
		// mcs (Mono compiler) — or any compiler path we couldn't classify:
		// treat it as CLI-compatible with mcs argument shape.
		success = compile_with_mcs(compiler, project_dir, output_dll, godotsharp_ref, nuget_refs);
		if (!success && compiler.ends_with("mcs.bat")) {
			// C2 放宽策略的回收路径：mcs.bat 实际运行失败（典型：4.5/mcs.exe 真的缺失），
			// 但本机还装了 dotnet 就立刻 fallback 一次，不要让用户因为 WARNING 放宽反而
			// 在有 dotnet 的情况下彻底编不过。
			{
				String out_ver;
				int ec = -1;
				List<String> ver_args;
				ver_args.push_back("--version");
				Error err = _execute_with_watchdog("dotnet", ver_args, &out_ver, &ec, false, 4000);
				if (err != OK || ec != 0 || out_ver.strip_edges().is_empty()) {
					return false;  // dotnet 也没有，真的失败
				}
			}
			MonoLogger::log_warning("mcs.bat failed to compile; automatically falling back to dotnet build. "
			                        "This mcs.bat has been marked unavailable for the rest of the editor session; "
			                        "next save will go directly to dotnet without re-attempting mcs.bat.");
			invalidate_csharp_compiler_cache();
			s_csharp_compiler_cache = "dotnet";
			success = compile_with_dotnet(project_dir, csharp_editor_get_csproj_path());
			if (success) {
				if (!copy_built_assembly_to_output(project_dir, project_name, output_dll)) {
					MonoLogger::log_warning(vformat(
							"dotnet build (fallback from mcs.bat) reported SUCCESS but no compiled DLL found.\n"
							"  Checked the same 4 locations as the dotnet path (see 'dotnet build' warning).\n"
							"  Copy the actual build output DLL manually to: %s",
							output_dll));
					success = false;
				}
			}
		}
	}

	return success;
}

// Find the most recently created project DLL in the assemblies dir.
static String find_latest_project_dll(const String &p_output_dir, const String &p_project_name) {
	String latest_path;
	uint64_t latest_mtime = 0;
	Ref<DirAccess> dir = DirAccess::open(p_output_dir);
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String fname = dir->get_next();
		while (!fname.is_empty()) {
			if (!dir->current_is_dir() && fname.ends_with(".dll") && fname != "GodotSharp.dll") {
				String full = p_output_dir.path_join(fname);
				uint64_t mtime = FileAccess::get_modified_time(full);
				if (mtime > latest_mtime) {
					latest_mtime = mtime;
					latest_path = full;
				}
			}
			fname = dir->get_next();
		}
		dir->list_dir_end();
	}
	return latest_path;
}

// ---------------------------------------------------------------------------
// Script save callback
// ---------------------------------------------------------------------------

void csharp_editor_on_script_saved(const String &p_path) {
	if (!p_path.ends_with(".cs")) {
		return;
	}

	MonoLogger::log(vformat("C# script saved: %s", p_path));

	// Ensure project solution exists
	csharp_editor_ensure_project_solution();

	// Compile the project so the new class is available
	if (csharp_editor_compile_project()) {
		GDMono *gdmono = GDMono::get_singleton();
		if (gdmono) {
			MonoLogger::log("Loading newly compiled assembly...");

			// Find the latest compiled DLL
			String latest_dll = find_latest_project_dll(csharp_editor_get_assemblies_output_dir(), get_project_name());
			if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
				// Use reload_assembly which does full AppDomain reload on desktop
				// (unloads old assembly, frees memory) and pseudo-reload on WASM.
				gdmono->reload_assembly(latest_dll);
				MonoLogger::log(vformat("Hot reload completed for: %s", latest_dll));

				// Reload all CSharpScripts so they pick up the new class
				if (CSharpLanguage::get_singleton()) {
					CSharpLanguage::get_singleton()->reload_all_scripts();
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Module lifecycle
// ---------------------------------------------------------------------------

static bool csharp_editor_initialized = false;

void initialize_csharp_editor() {
	MonoLogger::log("Initializing C# editor integration...");

	// Ensure project solution files exist when the editor starts
	csharp_editor_ensure_project_solution();

	// Try to compile on startup if there are .cs files (recursive, including src/
	// 子目录). 旧实现只扫 project_dir 顶层，用户按 Godot 惯例把脚本放 src/app/
	// 时 has_cs_files=False，启动阶段不编译，也不加载任何项目 DLL，导致后续
	// 出现 6 条连续 "C# class not found (not yet compiled?): App/Menu/Splash"。
	String project_dir = get_project_dir();
	bool has_cs_files = false;
	int cs_file_count = 0;

	{
		// 轻量级 BFS 递归扫 .cs（不用重入函数，避免栈爆）；Depth 限 6 层足够覆盖
		// src/app、src/menu/splash 等典型结构。
		Vector<String> stack;
		stack.push_back(project_dir);
		int depth = 0;
		const int MAX_DEPTH = 6;
		while (!stack.is_empty() && depth < MAX_DEPTH) {
			Vector<String> next_layer;
			for (const String &cur : stack) {
				Ref<DirAccess> dir = DirAccess::open(cur);
				if (!dir.is_valid()) continue;
				dir->list_dir_begin();
				String fname = dir->get_next();
				while (!fname.is_empty()) {
					if (fname == "." || fname == "..") {
						fname = dir->get_next();
						continue;
					}
					String full = cur.path_join(fname);
					if (dir->current_is_dir()) {
						// 跳过典型的编译产物 / 缓存目录，加速启动
						if (fname != ".mono" && fname != "obj" && fname != "bin" && fname != ".git" && fname != "node_modules") {
							next_layer.push_back(full);
						}
					} else if (fname.ends_with(".cs")) {
						has_cs_files = true;
						cs_file_count++;
					}
					fname = dir->get_next();
				}
				dir->list_dir_end();
			}
			stack = next_layer;
			depth++;
			if (has_cs_files && cs_file_count > 1) {
				// 找到多个 .cs 就可以停止更深层扫描（只用于 yes/no + 数量估算）
				break;
			}
		}
	}

	// 兜底：即便没有任何 .cs 文件在项目里（极端情况），但 .mono/assemblies 里
	// 已经有当前项目名的版本化 DLL（用户可能把源文件移走、或用预编译 DLL），
	// 也尝试直接加载，避免初始化阶段错过加载机会。
	bool has_existing_project_dll = false;
	{
		String asm_dir = csharp_editor_get_assemblies_output_dir();
		String latest = find_latest_project_dll(asm_dir, get_project_name());
		if (!latest.is_empty() && FileAccess::exists(latest)) {
			has_existing_project_dll = true;
		}
	}

	if (has_cs_files) {
		MonoLogger::log(vformat("Found %d C# files (recursive, up to 6 levels), attempting initial compilation...", cs_file_count));
		csharp_editor_compile_project();

		GDMono *gdmono = GDMono::get_singleton();
		if (gdmono) {
			// Clear any assemblies that GDMono auto-loaded during init
			// (典型是 exe_dir fallback 里的跨项目残留 Chickensoft DLL，不要让它
			//  和真正的项目 DLL 共存导致类查找混乱)。
			gdmono->clear_user_assemblies();

			String latest_dll = find_latest_project_dll(csharp_editor_get_assemblies_output_dir(), get_project_name());
			if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
				gdmono->load_assembly(latest_dll, true);
				MonoLogger::log(vformat("Loaded project assembly: %s", latest_dll));
			} else {
				MonoLogger::log_warning("Compiled assembly not found");
			}
		}
	} else if (has_existing_project_dll) {
		// 兜底：脚本没被 recursive 扫描命中（或极端情况无源文件），
		// 但 .mono/assemblies 里已有 DLL，先把它加载起来；
		// 总比加载 exe_dir fallback 里的跨项目 DLL 好。
		GDMono *gdmono = GDMono::get_singleton();
		if (gdmono) {
			gdmono->clear_user_assemblies();
			String latest_dll = find_latest_project_dll(csharp_editor_get_assemblies_output_dir(), get_project_name());
			if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
				gdmono->load_assembly(latest_dll, true);
				MonoLogger::log(vformat("Loaded existing project assembly (no .cs scan hit): %s", latest_dll));
			}
		}
	}

	csharp_editor_initialized = true;
	MonoLogger::log("C# editor integration initialized");
}

void uninitialize_csharp_editor() {
	MonoLogger::log("Cleaning up C# editor integration...");
	csharp_editor_initialized = false;
}

// ---------------------------------------------------------------------------
// Export plugin: deploys Mono runtime alongside exported executables
// ---------------------------------------------------------------------------

class CSharpEditorExportPlugin : public EditorExportPlugin {
	GDCLASS(CSharpEditorExportPlugin, EditorExportPlugin);

	String export_path;
	bool is_debug_build = false;
	HashSet<String> current_features;   // 缓存 _export_begin 收到的 features（Desktop/Web/iOS/Android 等），
	                                    // 给 _export_end 判定平台用——Godot 4 的 EditorExportPlugin
	                                    // 没有 has_feature() 成员（has_feature 只在 EditorExportPlatform 上存在）。

public:
	virtual String get_name() const override { return "CSharp"; }

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override {
		export_path = p_path;
		is_debug_build = p_debug;
		current_features = p_features;   // 保存：_export_end 没有参数，需要靠这个判断平台
		String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		bool is_windows = p_features.has("windows");
		bool is_macos = p_features.has("macos");
		bool is_linux = p_features.has("linux");
		bool is_web = p_features.has("web");
		bool is_ios = p_features.has("ios");
		bool is_android = p_features.has("android");

		if (is_windows || is_macos || is_linux) {
			_deploy_mono_desktop(exe_dir);
			_deploy_user_assemblies();
		} else if (is_web) {
			_deploy_mono_web(exe_dir);
			_deploy_user_assemblies_web();
		} else if (is_ios || is_android) {
			// iOS/Android 骨架支持：打包 BCL + GodotSharp.dll + 用户程序集到 PCK。
			// 运行时（gd_mono.cpp）已预设从 res://mono/lib/mono/4.5/ 和
			// res://.mono/assemblies/ 加载，Android preload hook 也会从
			// res://.godot/mono/publish/<arch>/ 查找。
			// 注意：当前使用桌面版 BCL，真机可能需要平台专用 BCL（类似 WASM 专用 BCL）。
			_deploy_mono_mobile(exe_dir, p_features);
			_deploy_user_assemblies_mobile(p_features);
		}
	}

	virtual void _export_end() override {
		// K3：Desktop 导出时给导出目录（即用户 build 目录，就是 export_path 的 base dir）
		// 也复制一份 BCL / GodotSharp / 用户 DLL 的物理副本，双保险：
		//   - 用户把 EXE 单独拷走时，.pck 里的 preload hook 能保证启动（见
		//     gd_mono.cpp install_universal_assembly_preload_hook）；
		//   - 用户把 EXE+PCK 整个文件夹拷走时，物理副本 + preload 都能命中，
		//     即使 mono_new_stub / 旧版 gd_mono 没装 hook 也能跑。
		bool is_windows = current_features.has("windows");
		bool is_macos = current_features.has("macos");
		bool is_linux = current_features.has("linux");
		if (!(is_windows || is_macos || is_linux)) return;

		String editor_exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		String out_dir = export_path.get_base_dir();

		// BCL 源：导出开始时同一份候选解析
		String bcl_dir = editor_exe_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = editor_exe_dir.path_join("..").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = editor_exe_dir.path_join("..").path_join("bin").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}
		if (FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			String target_bcl = out_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
			DirAccess::make_dir_recursive_absolute(target_bcl);
			Ref<DirAccess> bcl_d = DirAccess::open(bcl_dir);
			if (bcl_d.is_valid()) {
				bcl_d->list_dir_begin();
				String fname = bcl_d->get_next();
				while (!fname.is_empty()) {
					if (!bcl_d->current_is_dir() && fname.ends_with(".dll")) {
						String src = bcl_dir.path_join(fname);
						String dst = target_bcl.path_join(fname);
						Error copy_err = DirAccess::copy_absolute(src, dst);
						if (copy_err != OK) {
							MonoLogger::log_warning(vformat("Export end: BCL sidecar copy failed for %s (err=%d)", fname, (int)copy_err));
						}
					}
					fname = bcl_d->get_next();
				}
				bcl_d->list_dir_end();
			}
		}

		// GodotSharp.dll sidecar
		String gs_src = bcl_dir.path_join("GodotSharp.dll");
		if (!FileAccess::exists(gs_src)) gs_src = editor_exe_dir.path_join("GodotSharp.dll");
		if (!FileAccess::exists(gs_src)) gs_src = editor_exe_dir.path_join(".mono").path_join("assemblies").path_join("GodotSharp.dll");
		if (FileAccess::exists(gs_src)) {
			Error e = DirAccess::copy_absolute(gs_src, out_dir.path_join("GodotSharp.dll"));
			if (e != OK) MonoLogger::log_warning(vformat("Export end: GodotSharp.dll sidecar copy failed (err=%d)", (int)e));
		}

		// 用户 DLL sidecar
		String project_name = get_project_name();
		String project_dir = get_project_dir();
		String project_assemblies_dir = project_dir.path_join(".mono").path_join("assemblies");
		String latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
		if (latest_dll.is_empty() && csharp_editor_compile_project()) {
			latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
		}
		if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
			String target_assemblies = out_dir.path_join(".mono").path_join("assemblies");
			DirAccess::make_dir_recursive_absolute(target_assemblies);
			Error e = DirAccess::copy_absolute(latest_dll, target_assemblies.path_join(project_name + ".dll"));
			if (e != OK) MonoLogger::log_warning(vformat("Export end: user assembly sidecar copy failed (err=%d)", (int)e));
			else MonoLogger::log(vformat("Export end: user assembly sidecar copied to %s\\%s.dll", target_assemblies, project_name));
		}
	}

private:
	void _deploy_user_assemblies() {
		String project_name = get_project_name();
		String project_dir = get_project_dir();
		String project_assemblies_dir = project_dir.path_join(".mono").path_join("assemblies");

		String latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
		if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
			String target_path = ".mono/assemblies/" + project_name + ".dll";
			PackedByteArray data = FileAccess::get_file_as_bytes(latest_dll);
			add_file(target_path, data, false);
			MonoLogger::log(vformat("Export: deployed user assembly to %s (%d bytes)", target_path, data.size()));
		} else {
			MonoLogger::log_warning("Export: no compiled user assembly found, attempting compilation...");
			if (csharp_editor_compile_project()) {
				latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
				if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
					String target_path = ".mono/assemblies/" + project_name + ".dll";
					PackedByteArray data = FileAccess::get_file_as_bytes(latest_dll);
					add_file(target_path, data, false);
					MonoLogger::log(vformat("Export: deployed newly compiled user assembly to %s", target_path));
				}
			} else {
				MonoLogger::log_warning("Export: C# project compilation failed");
			}
		}

		// Deploy NuGet dependency DLLs alongside the user assembly.
		_deploy_nuget_assemblies();
	}

	// Run IL trimming (monolinker + mono-cil-strip) on an assembly for Web
	// export. Returns the path to the trimmed DLL, or empty string on failure
	// (caller falls back to the original untrimmed assembly).
	//
	// Trimming is skipped for:
	//   - Debug builds (keep full debug info for development)
	//   - Missing Python or Mono SDK tools (graceful degradation)
	//
	// Conservative settings: --preserve-public is always set because C++ ->
	// C# interop via mono_runtime_invoke is invisible to the linker's static
	// reachability analysis. --strip-debug is only added for release builds.
	String _trim_assembly(const String &p_input_dll) {
		if (is_debug_build) {
			return String(); // skip trimming in debug builds
		}

		// Locate trim_assemblies.py (ships at modules/mono_new/scripts/)
		String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		String script_path = exe_dir.path_join("..").path_join("modules").path_join("mono_new").path_join("scripts").path_join("trim_assemblies.py");
		if (!FileAccess::exists(script_path)) {
			script_path = "modules/mono_new/scripts/trim_assemblies.py"; // dev build fallback
		}
		if (!FileAccess::exists(script_path)) {
			MonoLogger::log("Export (web): trim_assemblies.py not found, skipping IL trimming");
			return String();
		}

		// Find Python interpreter (same logic as nuget_restore)
		String python = "python";
		{
			String output;
			int exit_code = -1;
			Error err = OS::get_singleton()->execute(python, List<String>(), &output, &exit_code);
			if (err != OK) {
				python = "python3";
				err = OS::get_singleton()->execute(python, List<String>(), &output, &exit_code);
				if (err != OK) {
					MonoLogger::log("Export (web): Python not found, skipping IL trimming");
					return String();
				}
			}
		}

		// Prepare output directory (temp dir under the project's .mono/)
		String project_dir = get_project_dir();
		String trim_output_dir = project_dir.path_join(".mono").path_join("trimmed");
		DirAccess::make_dir_recursive_absolute(trim_output_dir);

		// BCL reference for type resolution
		String bcl_dir = exe_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = exe_dir.path_join("..").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}

		// Build command args
		List<String> args;
		args.push_back(script_path);
		args.push_back("--input");
		args.push_back(p_input_dll);
		args.push_back("--output");
		args.push_back(trim_output_dir);
		args.push_back("--preserve-public");
		args.push_back("--strip-debug");

		if (FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			args.push_back("--references");
			args.push_back(bcl_dir.path_join("mscorlib.dll"));
		}

		MonoLogger::log(vformat("Export (web): running IL trimming on %s", p_input_dll.get_file()));

		String output;
		int exit_code = -1;
		Error err = OS::get_singleton()->execute(python, args, &output, &exit_code, true);
		if (err != OK || exit_code != 0) {
			MonoLogger::log_warning(vformat("Export (web): IL trimming failed (exit %d), using untrimmed assembly", exit_code));
			if (!output.is_empty()) {
				MonoLogger::log_warning(output);
			}
			return String();
		}

		String trimmed_dll = trim_output_dir.path_join(p_input_dll.get_file());
		if (FileAccess::exists(trimmed_dll)) {
			uint64_t orig_size = FileAccess::get_file_as_bytes(p_input_dll).size();
			uint64_t trimmed_size = FileAccess::get_file_as_bytes(trimmed_dll).size();
			double reduction = orig_size > 0 ? (1.0 - (double)trimmed_size / orig_size) * 100.0 : 0.0;
			MonoLogger::log(vformat("Export (web): IL trimming OK: %llu -> %llu bytes (%.1f%% reduction)",
					(uint64_t)orig_size, (uint64_t)trimmed_size, reduction));
			return trimmed_dll;
		}

		MonoLogger::log_warning("Export (web): trimmed assembly not found in output dir, using untrimmed");
		return String();
	}

	void _deploy_user_assemblies_web() {
		String project_name = get_project_name();
		String project_dir = get_project_dir();
		String project_assemblies_dir = project_dir.path_join(".mono").path_join("assemblies");

		String latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
		if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
			// Try IL trimming for release Web exports to reduce .data size.
			String deploy_dll = latest_dll;
			String trimmed = _trim_assembly(latest_dll);
			if (!trimmed.is_empty() && FileAccess::exists(trimmed)) {
				deploy_dll = trimmed;
			}

			String target_path = ".mono/assemblies/" + project_name + ".dll";
			PackedByteArray data = FileAccess::get_file_as_bytes(deploy_dll);
			add_file(target_path, data, false);
			MonoLogger::log(vformat("Export (web): deployed user assembly to %s (%d bytes)", target_path, data.size()));
		}

		// Deploy NuGet dependency DLLs alongside the user assembly so the
		// Mono runtime can resolve them at load time.
		_deploy_nuget_assemblies();
	}

	// Deploy NuGet package DLLs to .mono/assemblies/ in the export.
	// Reads packages.config and scans packages/<id>.<version>/lib/<tfm>/
	// for managed assemblies. Skipped silently if packages.config is absent.
	void _deploy_nuget_assemblies() {
		String project_dir = get_project_dir();
		String packages_config = project_dir.path_join("packages.config");
		if (!FileAccess::exists(packages_config)) {
			return;
		}

		String packages_dir = project_dir.path_join("packages");
		if (!DirAccess::exists(packages_dir)) {
			MonoLogger::log_warning("Export: packages.config exists but packages/ dir not found. Run restore first.");
			return;
		}

		// Scan packages/<id>.<version>/ directories.
		Ref<DirAccess> dir = DirAccess::open(packages_dir);
		if (dir.is_null()) {
			return;
		}

		int deployed = 0;
		dir->list_dir_begin();
		String pkg_dir_name = dir->get_next();
		while (!pkg_dir_name.is_empty()) {
			if (dir->current_is_dir() && pkg_dir_name != "." && pkg_dir_name != "..") {
				// Find lib/<tfm>/ subdirectory with .dll files.
				String lib_dir = packages_dir.path_join(pkg_dir_name).path_join("lib");
				if (DirAccess::exists(lib_dir)) {
					Ref<DirAccess> lib_d = DirAccess::open(lib_dir);
					if (lib_d.is_valid()) {
						lib_d->list_dir_begin();
						String tfm = lib_d->get_next();
						// Pick the first tfm that contains .dll files (preferring
						// net48/net45/net40 in that order is done by the restore
						// script; here we just iterate).
						while (!tfm.is_empty()) {
							if (lib_d->current_is_dir()) {
								String tfm_dir = lib_dir.path_join(tfm);
								Ref<DirAccess> tfm_d = DirAccess::open(tfm_dir);
								if (tfm_d.is_valid()) {
									tfm_d->list_dir_begin();
									String fname = tfm_d->get_next();
									bool found_dll = false;
									while (!fname.is_empty()) {
										if (!tfm_d->current_is_dir() && fname.ends_with(".dll") && !fname.ends_with(".ni.dll")) {
											String src = tfm_dir.path_join(fname);
											String target = ".mono/assemblies/" + fname;
											PackedByteArray data = FileAccess::get_file_as_bytes(src);
											add_file(target, data, false);
											deployed++;
											found_dll = true;
										}
										fname = tfm_d->get_next();
									}
									tfm_d->list_dir_end();
									if (found_dll) {
										break; // Use the first tfm with DLLs.
									}
								}
							}
							tfm = lib_d->get_next();
						}
					}
				}
			}
			pkg_dir_name = dir->get_next();
		}
		dir->list_dir_end();

		if (deployed > 0) {
			MonoLogger::log(vformat("Export: deployed %d NuGet assembly DLL(s)", deployed));
		}
	}

	// 静态模式下部署 etc/ 目录：DFS 遍历 p_root 下所有文件，
	// 用 add_file 以 "mono/etc/<relative>" 路径打进 PCK。
	// p_root 是 etc 的绝对根（用于计算相对路径），
	// p_current 是当前迭代目录（首次调用 = p_root）。
	void _deploy_etc_dir_recursive(const String &p_root, const String &p_current, const String &p_target_prefix) {
		Ref<DirAccess> d = DirAccess::open(p_current);
		if (d.is_null()) {
			return;
		}
		d->list_dir_begin();
		String fname = d->get_next();
		while (!fname.is_empty()) {
			if (fname != "." && fname != "..") {
				String abs = p_current.path_join(fname);
				if (d->current_is_dir()) {
					_deploy_etc_dir_recursive(p_root, abs, p_target_prefix);
				} else {
					PackedByteArray data = FileAccess::get_file_as_bytes(abs);
					if (data.size() > 0) {
						String rel = abs.replace_first(p_root + "/", "");
						if (rel == abs) {
							rel = abs.replace_first(p_root + "\\", "");
						}
						if (!rel.is_empty()) {
							add_file(p_target_prefix + "/" + rel, data, false);
						}
					}
				}
			}
			fname = d->get_next();
		}
		d->list_dir_end();
	}

	void _deploy_mono_desktop(const String &p_exe_dir) {
#ifdef MONO_STATIC_BUILD
		// ---- 静态链接模式（本项目的默认构建方式）----
		// Mono 运行时代码（mono-2.0-sgen）已被链接进 EXE，
		// 不再需要本地部署 mono-2.0-sgen.dll / MonoPosixHelper.dll /
		// libmono-btls-shared.dll 这三个原生 DLL。如果强制尝试，
		// 在任何未安装完整 Mono 的机器上都会弹出系统级错误对话框：
		//   "由于找不到 mono-2.0-sgen.dll，无法继续执行代码。"
		// 所以这里直接跳过。

		// 但 BCL 目录（mono/lib/mono/4.5/*.dll）+ GodotSharp.dll + 用户 DLL
		// 仍然是运行时需要加载的托管程序集，必须部署到目标 EXE 旁边，
		// 否则 mono_jit_init_version 之后会在根域加载阶段直接断言失败。

		// 查找 BCL 目录：优先 exe_dir/mono/lib/mono/4.5/（template_release 构建
		// 后 bin/windows/mono 就是这个层级）；如果不存在（例如编辑器模板模式下
		// 用户的 EXE 目录是 exports/<project>/），再退到 ../mono/lib/mono/4.5/
		// 和 exe_dir/../bin/mono/lib/mono/4.5/。
		String bcl_dir = p_exe_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = p_exe_dir.path_join("..").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = p_exe_dir.path_join("..").path_join("bin").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}
		if (FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			Ref<DirAccess> bcl_d = DirAccess::open(bcl_dir);
			if (bcl_d.is_valid()) {
				bcl_d->list_dir_begin();
				String fname = bcl_d->get_next();
				int bcl_count = 0;
				while (!fname.is_empty()) {
					if (!bcl_d->current_is_dir() && fname.ends_with(".dll")) {
						String src = bcl_dir.path_join(fname);
						String target = "mono/lib/mono/4.5/" + fname;
						PackedByteArray data = FileAccess::get_file_as_bytes(src);
						if (data.size() > 0) {
							add_file(target, data, false);
							bcl_count++;
						}
					}
					fname = bcl_d->get_next();
				}
				bcl_d->list_dir_end();
				MonoLogger::log(vformat("Export (desktop static): deployed %d BCL assemblies from %s",
						bcl_count, bcl_dir));
			}
		} else {
			MonoLogger::log_warning(
					"Export (desktop static): BCL not found (tried mono/lib/mono/4.5/, "
					"../mono/lib/mono/4.5/, ../bin/mono/lib/mono/4.5/); "
					"C# runtime will fail to initialize at startup");
		}

		// 部署 GodotSharp.dll：优先 BCL 目录（运行时实际加载的那一份），
		// 其次 exe_dir/GodotSharp.dll，最后回退 exe_dir/.mono/assemblies/GodotSharp.dll。
		String godotsharp_src = bcl_dir.path_join("GodotSharp.dll");
		if (!FileAccess::exists(godotsharp_src)) {
			godotsharp_src = p_exe_dir.path_join("GodotSharp.dll");
		}
		if (!FileAccess::exists(godotsharp_src)) {
			godotsharp_src = p_exe_dir.path_join(".mono").path_join("assemblies").path_join("GodotSharp.dll");
		}
		if (FileAccess::exists(godotsharp_src)) {
			PackedByteArray data = FileAccess::get_file_as_bytes(godotsharp_src);
			if (data.size() > 0) {
				add_file(".mono/assemblies/GodotSharp.dll", data, false);
				MonoLogger::log(vformat("Export (desktop static): deployed GodotSharp.dll from %s (%d bytes)",
						godotsharp_src, data.size()));
			}
		} else {
			MonoLogger::log_warning("Export (desktop static): GodotSharp.dll not found; "
					"C# user scripts will fail to load at runtime");
		}

		// mono 子目录（如果存在额外数据，如 etc/ 配置）也一并打包，
		// 保证 gd_mono.cpp 的 config_dir 候选路径不失效。
		String mono_etc_dir = p_exe_dir.path_join("mono").path_join("etc");
		if (DirAccess::exists(mono_etc_dir)) {
			// 静态模式下按 add_file 的逐文件方式递归打包 etc/，避免依赖
			// 未定义的共享辅助函数。使用 DirAccess 实例 + 手动 DFS，
			// 与 Godot 3/4 通用的 DirAccess API 对齐。
			_deploy_etc_dir_recursive(mono_etc_dir, mono_etc_dir, "mono/etc");
		}
#else
		// ---- 动态链接模式（仅系统完整安装 Mono 时才可用）----
		Vector<String> dlls = {
			"mono-2.0-sgen.dll",
			"MonoPosixHelper.dll",
			"libmono-btls-shared.dll",
			"GodotSharp.dll",
		};

		for (const String &dll : dlls) {
			String src = p_exe_dir.path_join(dll);
			if (FileAccess::exists(src)) {
				add_shared_object(src, Vector<String>(), String());
			}
		}

		String mono_dir = p_exe_dir.path_join("mono");
		if (DirAccess::exists(mono_dir)) {
			add_shared_object(mono_dir, Vector<String>(), String());
		}

		String data_mono_dir = p_exe_dir.path_join("data").path_join("mono");
		if (DirAccess::exists(data_mono_dir)) {
			add_shared_object(data_mono_dir, Vector<String>(), String());
		}

		String assemblies_dir = p_exe_dir.path_join(".mono").path_join("assemblies");
		String godotsharp_in_assemblies = assemblies_dir.path_join("GodotSharp.dll");
		if (FileAccess::exists(godotsharp_in_assemblies) && !FileAccess::exists(p_exe_dir.path_join("GodotSharp.dll"))) {
			add_shared_object(godotsharp_in_assemblies, Vector<String>(), String());
		}
#endif  // MONO_STATIC_BUILD
	}

	void _deploy_mono_web(const String &p_exe_dir) {
		// 优先从 BCL 目录读取 GodotSharp.dll（运行时实际加载的版本），
		// exe 目录作为 fallback。BCL 目录总是与引擎同步更新，
		// 而 exe 目录可能残留旧版（如构建后未同步），导致 WASM 下
		// icall 签名不匹配（function signature mismatch）。
		String bcl_dll = p_exe_dir.path_join("..").path_join("mono").path_join("lib").path_join("mono").path_join("4.5").path_join("GodotSharp.dll");
		String exe_dll = p_exe_dir.path_join("GodotSharp.dll");

		String source_dll;
		if (FileAccess::exists(bcl_dll)) {
			source_dll = bcl_dll;
		} else if (FileAccess::exists(exe_dll)) {
			source_dll = exe_dll;
		} else {
			MonoLogger::log_warning(vformat("Export (web): GodotSharp.dll not found (tried %s and %s)", bcl_dll, exe_dll));
			return;
		}

		// Try IL trimming for release Web exports to reduce .data size.
		// GodotSharp.dll benefits from trimming since it contains many
		// wrapper methods that are never reached by user code.
		String deploy_dll = source_dll;
		String trimmed = _trim_assembly(source_dll);
		if (!trimmed.is_empty() && FileAccess::exists(trimmed)) {
			deploy_dll = trimmed;
		}

		PackedByteArray data = FileAccess::get_file_as_bytes(deploy_dll);
		MonoLogger::log(vformat("Export (web): deploying GodotSharp.dll from %s (%d bytes)", deploy_dll, data.size()));
		add_file(".mono/assemblies/GodotSharp.dll", data, false);
	}

	// --------------------------------------------------------------------------
	// iOS/Android 移动端部署（骨架实现）
	// --------------------------------------------------------------------------
	// 运行时路径约定（与 gd_mono.cpp 的 android_load_assembly_from_pck 候选路径一致）：
	//   1. res://.godot/mono/publish/<arch>/<name>.dll  (Android preload hook 优先)
	//   2. res://.mono/assemblies/<name>.dll            (通用回退)
	//   3. res://mono/lib/mono/4.5/<name>.dll           (BCL 回退)
	//
	// 本骨架实现把：
	//   - BCL        → res://mono/lib/mono/4.5/
	//   - GodotSharp → res://.mono/assemblies/
	//   - 用户 DLL    → res://.mono/assemblies/（_deploy_user_assemblies_mobile）
	//
	// ⚠️ 当前使用桌面版 BCL。真机可能需要平台专用 BCL（类似 WASM 专用 BCL），
	//    否则可能触发 "invalid CIL image" 或签名不匹配。待交叉编译 Mono 静态库
	//    时一并产出平台专用 BCL 后替换。
	void _deploy_mono_mobile(const String &p_exe_dir, const HashSet<String> &p_features) {
		String platform_tag = p_features.has("android") ? "android" : "ios";

		// 定位 BCL 目录（与 _deploy_mono_web 同样的查找逻辑）
		String bcl_dir = p_exe_dir.path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			bcl_dir = p_exe_dir.path_join("..").path_join("mono").path_join("lib").path_join("mono").path_join("4.5");
		}
		if (!FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
			MonoLogger::log_warning(vformat("Export (%s): BCL not found at %s, C# runtime will fail to initialize",
					platform_tag, bcl_dir));
			return;
		}

		MonoLogger::log_warning(vformat("Export (%s): using desktop BCL as fallback — real device may require platform-specific BCL",
				platform_tag));

		// 打包 BCL 到 res://mono/lib/mono/4.5/
		// 只打包运行时必需的核心 BCL 程序集，避免 PCK 膨胀
		Vector<String> bcl_dlls = {
			"mscorlib.dll",
			"System.dll",
			"System.Core.dll",
			"System.Numerics.dll",
			"System.Xml.dll",
			"System.Xml.Linq.dll",
			"I18N.dll",
			"I18N.West.dll",
		};

		int bcl_count = 0;
		for (const String &dll : bcl_dlls) {
			String src = bcl_dir.path_join(dll);
			if (FileAccess::exists(src)) {
				PackedByteArray data = FileAccess::get_file_as_bytes(src);
				if (data.size() > 0) {
					String target = "mono/lib/mono/4.5/" + dll;
					add_file(target, data, false);
					bcl_count++;
				}
			}
		}
		MonoLogger::log(vformat("Export (%s): deployed %d BCL assemblies to res://mono/lib/mono/4.5/",
				platform_tag, bcl_count));

		// 打包 GodotSharp.dll 到 res://.mono/assemblies/
		// 优先从 BCL 目录读取（与 _deploy_mono_web 同样的逻辑）
		String godotsharp_src = bcl_dir.path_join("GodotSharp.dll");
		if (!FileAccess::exists(godotsharp_src)) {
			godotsharp_src = p_exe_dir.path_join("GodotSharp.dll");
		}
		if (FileAccess::exists(godotsharp_src)) {
			PackedByteArray data = FileAccess::get_file_as_bytes(godotsharp_src);
			if (data.size() > 0) {
				add_file(".mono/assemblies/GodotSharp.dll", data, false);
				MonoLogger::log(vformat("Export (%s): deployed GodotSharp.dll (%d bytes) to res://.mono/assemblies/",
						platform_tag, data.size()));
			}
		} else {
			MonoLogger::log_warning(vformat("Export (%s): GodotSharp.dll not found", platform_tag));
		}
	}

	// 打包用户程序集 + NuGet 依赖到 res://.mono/assemblies/
	// 同时尝试打包到 res://.godot/mono/publish/<arch>/（Android preload hook 优先路径）
	void _deploy_user_assemblies_mobile(const HashSet<String> &p_features) {
		String project_name = get_project_name();
		String project_dir = get_project_dir();
		String project_assemblies_dir = project_dir.path_join(".mono").path_join("assemblies");
		String platform_tag = p_features.has("android") ? "android" : "ios";

		MonoLogger::log(vformat("Export (%s): project_name='%s', project_dir='%s', assemblies_dir='%s'",
				platform_tag, project_name, project_dir, project_assemblies_dir));

		// 列出 assemblies_dir 下的所有文件，便于诊断
		{
			Ref<DirAccess> diag_dir = DirAccess::open(project_assemblies_dir);
			if (diag_dir.is_valid()) {
				diag_dir->list_dir_begin();
				String fname = diag_dir->get_next();
				while (!fname.is_empty()) {
					if (!diag_dir->current_is_dir()) {
						MonoLogger::log(vformat("Export (%s): assemblies_dir entry: %s", platform_tag, fname));
					}
					fname = diag_dir->get_next();
				}
				diag_dir->list_dir_end();
			} else {
				MonoLogger::log_warning(vformat("Export (%s): assemblies_dir NOT accessible: %s", platform_tag, project_assemblies_dir));
			}
		}

		String latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
		MonoLogger::log(vformat("Export (%s): find_latest_project_dll result: '%s'", platform_tag, latest_dll));
		if (latest_dll.is_empty() || !FileAccess::exists(latest_dll)) {
			MonoLogger::log_warning(vformat("Export (%s): no compiled user assembly found, attempting compilation...", platform_tag));
			if (csharp_editor_compile_project()) {
				latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
			}
		}

		if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
			PackedByteArray data = FileAccess::get_file_as_bytes(latest_dll);
			if (data.size() > 0) {
				// 通用路径（preload hook 候选路径 2）
				String target = ".mono/assemblies/" + project_name + ".dll";
				add_file(target, data, false);
				MonoLogger::log(vformat("Export (%s): deployed user assembly to %s (%d bytes)",
						platform_tag, target, data.size()));

				// 架构特定路径（Android preload hook 候选路径 1）
				// 从 features 推断 Godot 架构名
				String arch = _get_mobile_arch_name(p_features);
				if (!arch.is_empty()) {
					String arch_target = ".godot/mono/publish/" + arch + "/" + project_name + ".dll";
					add_file(arch_target, data, false);
					MonoLogger::log(vformat("Export (%s): deployed user assembly to %s", platform_tag, arch_target));
				}
			}
		} else {
			MonoLogger::log_warning(vformat("Export (%s): C# project compilation failed or no assembly found", platform_tag));
		}

		// 热更新演示：若项目根目录存在 <project_name>_v2.dll（如 048_v2.dll），
		// 将其作为 res://<project_name>_v2.dll 部署到 PCK。Godot 默认不会导出
		// 项目根目录下的 .dll 文件（非已导入资源），需在此显式 add_file。
		// C# 代码启动后从 res:// 读取该 v2 dll 字节，写入 user:// 触发热更新。
		String v2_dll_src = project_dir.path_join(project_name + "_v2.dll");
		if (FileAccess::exists(v2_dll_src)) {
			PackedByteArray v2_data = FileAccess::get_file_as_bytes(v2_dll_src);
			if (v2_data.size() > 0) {
				String v2_target = project_name + "_v2.dll";
				add_file(v2_target, v2_data, false);
				MonoLogger::log(vformat("Export (%s): deployed v2 hot-reload assembly to res://%s (%d bytes)",
						platform_tag, v2_target, v2_data.size()));
			} else {
				MonoLogger::log_warning(vformat("Export (%s): %s_v2.dll is empty", platform_tag, project_name));
			}
		}

		// NuGet 依赖
		_deploy_nuget_assemblies();
	}

	// 从 export features 推断 Godot 内部架构名
	//（与 Engine::get_architecture_name() 返回值一致）
	static String _get_mobile_arch_name(const HashSet<String> &p_features) {
		// Android ABI feature tags
		if (p_features.has("arm64-v8a")) return "arm64";
		if (p_features.has("armeabi-v7a")) return "arm32";
		if (p_features.has("x86_64")) return "x86_64";
		if (p_features.has("x86")) return "x86_32";
		// iOS 只有 arm64（模拟器 arm64-x86_64 暂不区分）
		if (p_features.has("ios") || p_features.has("android")) return "arm64";
		return String();
	}
};

static Ref<CSharpEditorExportPlugin> csharp_export_plugin;

static void _register_csharp_export_plugin() {
	csharp_export_plugin.instantiate();
	EditorExport::get_singleton()->add_export_plugin(csharp_export_plugin);
	MonoLogger::log("C# export plugin registered");
}

void register_csharp_export_plugin() {
	EditorNode::add_init_callback(_register_csharp_export_plugin);
}

void unregister_csharp_export_plugin() {
	if (csharp_export_plugin.is_valid()) {
		if (EditorExport::get_singleton()) {
			EditorExport::get_singleton()->remove_export_plugin(csharp_export_plugin);
		}
		csharp_export_plugin.unref();
	}
}

#endif // TOOLS_ENABLED
