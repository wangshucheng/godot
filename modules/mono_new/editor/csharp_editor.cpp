#include "csharp_editor.h"
#include "../mono_gd/csharp_script.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_plugin.h"

#include <stdlib.h>
#include <time.h>

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

bool csharp_editor_ensure_project_solution() {
	String project_name = get_project_name();
	String project_dir = get_project_dir();
	String csproj_path = csharp_editor_get_csproj_path();
	String sln_path = csharp_editor_get_sln_path();

	bool created = false;

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

	if (created) {
		MonoLogger::log("C# project solution files created");
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

static String find_csharp_compiler() {
	// M8 修复: 缓存编译器探测结果，避免每次保存脚本都启动子进程探测。
	// 编译器在编辑器会话内不会变化，一次探测即可。
	static String cached_compiler;
	if (!cached_compiler.is_empty()) {
		return cached_compiler;
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
		if (candidate == "dotnet") {
			// Check if dotnet is available
			String output;
			int exit_code = -1;
			Error err = OS::get_singleton()->execute(candidate, List<String>(), &output, &exit_code);
			if (err == OK) {
				cached_compiler = candidate;
				return cached_compiler;
			}
		} else if (candidate.ends_with("mcs.bat")) {
			// mcs.bat 存在不等于可用：它内部调用 mono.exe 加载 mcs.exe，
			// 系统 Mono 安装可能不完整（4.5 目录为空）。同时验证 mcs.exe 存在。
			if (FileAccess::exists(candidate)) {
				String mcs_exe = candidate.get_base_dir().path_join("..").path_join("lib").path_join("mono").path_join("4.5").path_join("mcs.exe");
				if (FileAccess::exists(mcs_exe)) {
					cached_compiler = candidate;
					return cached_compiler;
				}
				MonoLogger::log_warning(vformat("Found %s but %s is missing, skipping mcs", candidate, mcs_exe));
			}
		} else {
			if (FileAccess::exists(candidate)) {
				cached_compiler = candidate;
				return cached_compiler;
			}
		}
	}

	// Fallback: try mcs on PATH
	cached_compiler = "mcs";
	return cached_compiler;
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
	Error err = OS::get_singleton()->execute("dotnet", args, &output, &exit_code, true);

	if (err != OK) {
		MonoLogger::log_error("Failed to execute dotnet build");
		return false;
	}

	if (exit_code != 0) {
		MonoLogger::log_error(vformat("dotnet build failed with exit code %d", exit_code));
		if (!output.is_empty()) {
			MonoLogger::log_error("Build output:\n" + output);
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
			// Copy the built DLL from .mono/temp/bin/Debug/ to assemblies output
			String built_dll = project_dir.path_join(".mono").path_join("temp").path_join("bin").path_join("Debug").path_join(project_name + ".dll");
			if (FileAccess::exists(built_dll)) {
				DirAccess::copy_absolute(built_dll, output_dll);
				MonoLogger::log(vformat("Copied built assembly to: %s", output_dll));
			}
		}
	} else if (compiler == "csc") {
		// Use csc directly
		success = compile_with_mcs(compiler, project_dir, output_dll, godotsharp_ref, nuget_refs);
	} else {
		// mcs (Mono compiler)
		success = compile_with_mcs(compiler, project_dir, output_dll, godotsharp_ref, nuget_refs);
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

	// Try to compile on startup if there are .cs files
	String project_dir = get_project_dir();
	bool has_cs_files = false;

	Ref<DirAccess> dir = DirAccess::open(project_dir);
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String fname = dir->get_next();
		while (!fname.is_empty()) {
			if (!dir->current_is_dir() && fname.ends_with(".cs")) {
				has_cs_files = true;
				break;
			}
			fname = dir->get_next();
		}
		dir->list_dir_end();
	}

	if (has_cs_files) {
		MonoLogger::log("Found C# files, attempting initial compilation...");
		csharp_editor_compile_project();

		GDMono *gdmono = GDMono::get_singleton();
		if (gdmono) {
			// Clear any assemblies that GDMono auto-loaded during init
			gdmono->clear_user_assemblies();

			String latest_dll = find_latest_project_dll(csharp_editor_get_assemblies_output_dir(), get_project_name());
			if (!latest_dll.is_empty() && FileAccess::exists(latest_dll)) {
				gdmono->load_assembly(latest_dll, true);
				MonoLogger::log(vformat("Loaded project assembly: %s", latest_dll));
			} else {
				MonoLogger::log_warning("Compiled assembly not found");
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

public:
	virtual String get_name() const override { return "CSharp"; }

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override {
		export_path = p_path;
		is_debug_build = p_debug;
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

	void _deploy_mono_desktop(const String &p_exe_dir) {
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

		String latest_dll = find_latest_project_dll(project_assemblies_dir, project_name);
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
