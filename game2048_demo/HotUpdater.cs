using Godot;
using System.IO;
using System;

namespace Game2048Demo
{
    /// <summary>
    /// 纯C#热更新管理器。
    ///
    /// 注意：由于自定义 Godot Mono 绑定中缺少 Mono.SafeStringMarshal::StringToUtf8 icall，
    /// 调用 System.Environment.* 或 System.IO 类可能触发 Marshal 异常。
    /// 因此本类做了以下防御：
    /// 1) 默认假设运行在 Android（APK场景）
    /// 2) 所有 BCL 调用都包裹在大 try-catch 中，异常用 GD.Print 打印
    /// </summary>
    public static class HotUpdater
    {
        // 包名 — 必须与 AndroidManifest.xml 中的包名一致
        private const string PackageName = "org.godotengine.csharp_test";

        // 更新源路径：adb push 的目标位置
        // ★ 使用 /sdcard/Android/data/<pkg>/files/ 下的应用专属外部目录
        // ★ 无需任何权限（WRITE_EXTERNAL_STORAGE 也不需要），Android 4.4+ 可直接访问
        private static readonly string UpdateSourceDir = "/sdcard/Android/data/" + PackageName + "/files/update";
        private static readonly string UpdateSourcePath = UpdateSourceDir + "/Game2048Demo.dll";

        // Android 平台硬编码 user_data_dir（Runtime.GetUserDataDir 因 icall 缺失不可用，
        // 但从 mono_host.cpp 日志可确定 assemblies 被解压到 /data/data/<pkg>/files/.mono/assemblies
        // 故 user_data_dir = /data/data/<pkg>/files）
        private static readonly string AndroidUserDataDir = "/data/data/" + PackageName + "/files";

        // staging area：C++层在启动时检查此目录
        private static readonly string StagingDirName = ".mono/hot_update";

        private static bool? _isAndroidCached;

        /// <summary>
        /// 判断是否运行在 Android 平台。
        /// 防御性设计：由于 BCL 调用可能因 StringToUtf8 icall 缺失而失败，
        /// 优先返回 true（因为我们的 APK 场景就是 Android），只有明确不是才返回 false。
        /// </summary>
        public static bool IsAndroidRuntime()
        {
            if (_isAndroidCached.HasValue) return _isAndroidCached.Value;

            // 1) 先尝试官方 Platform 枚举（如果可用，不触发 StringToUtf8）
            try
            {
                if (Platform.Current == OSPlatform.Android)
                {
                    _isAndroidCached = true;
                    GD.Print("[HotUpdate] Android detected via Platform.Current");
                    return true;
                }
            }
            catch (Exception e)
            {
                GD.Print("[HotUpdate] Platform.Current check failed: " + e.Message);
            }

            // 2) 尝试轻量级 BCL 检测（只查 PlatformID，不带路径字符串）
            try
            {
                // Android 的 PlatformID 是 Unix，但 iOS/Linux 也是，所以结合 Platform.Current ≠ Android 时
                // 我们仍然默认 Android（因为 APK 只在 Android 上跑）
                var pid = System.Environment.OSVersion.Platform;
                GD.Print("[HotUpdate] OSVersion.Platform=" + pid);
            }
            catch (Exception e)
            {
                GD.Print("[HotUpdate] OSVersion check failed (safe to ignore): " + e.Message);
            }

            // 3) 默认 Android=true：本 Demo 的 APK 只部署在 Android 设备上
            _isAndroidCached = true;
            GD.Print("[HotUpdate] Android assumed by default (APK context)");
            return true;
        }

        private static string GetUserDataDirSafe()
        {
            try
            {
                if (IsAndroidRuntime())
                {
                    return AndroidUserDataDir;
                }
            }
            catch (Exception e)
            {
                GD.Print("[HotUpdate] Android user_data_dir check failed: " + e.Message);
            }
            try
            {
                return Runtime.GetUserDataDir();
            }
            catch (Exception e)
            {
                GD.PrintErr("[HotUpdate] Runtime.GetUserDataDir failed: " + e.Message);
                // fallback to current directory on desktop
                try
                {
                    return Directory.GetCurrentDirectory();
                }
                catch (Exception e2)
                {
                    GD.PrintErr("[HotUpdate] GetCurrentDirectory also failed: " + e2.Message);
                    return ".";
                }
            }
        }

        /// <summary>
        /// 检查是否有待应用的更新。
        /// 返回更新文件路径，若无则返回 null。
        /// </summary>
        public static string CheckForUpdate()
        {
            try
            {
                GD.Print("[HotUpdate] CheckForUpdate() START");

                string updatePath;
                bool isAndroid = IsAndroidRuntime();
                GD.Print("[HotUpdate] IsAndroidRuntime()=" + isAndroid);

                if (isAndroid)
                {
                    // Android 路径硬编码，避免 Directory/Path 等 BCL 类参与路径拼接
                    updatePath = UpdateSourcePath;
                    GD.Print("[HotUpdate] Android update path: " + updatePath);
                }
                else
                {
                    string userDataDir = GetUserDataDirSafe();
                    updatePath = userDataDir + "/update/Game2048Demo.dll";
                    GD.Print("[HotUpdate] Desktop update path: " + updatePath);
                }

                // 检查文件是否存在（可能触发 StringToUtf8，但必须做）
                bool exists;
                try
                {
                    exists = File.Exists(updatePath);
                }
                catch (Exception e)
                {
                    GD.Print("[HotUpdate] File.Exists threw (treating as not-exists): " + e.Message);
                    exists = false;
                }

                if (exists)
                {
                    long len = -1;
                    try
                    {
                        var fi = new FileInfo(updatePath);
                        len = fi.Length;
                    }
                    catch (Exception e)
                    {
                        GD.Print("[HotUpdate] FileInfo.Length failed: " + e.Message);
                    }
                    GD.Print("[HotUpdate] Found update file: " + updatePath + " (" + len + " bytes)");
                    return updatePath;
                }

                GD.Print("[HotUpdate] No update file at: " + updatePath);
                return null;
            }
            catch (Exception e)
            {
                GD.PrintErr("[HotUpdate] CheckForUpdate() FATAL: " + e.Message);
                GD.PrintErr("[HotUpdate] Stack: " + e.StackTrace);
                return null;
            }
        }

        /// <summary>
        /// 应用热更新：将新DLL复制到staging area，然后重启应用。
        /// 返回是否成功。
        /// </summary>
        public static bool ApplyUpdate(string updateFilePath)
        {
            try
            {
                GD.Print("[HotUpdate] ApplyUpdate() START with: " + updateFilePath);

                string userDataDir = GetUserDataDirSafe();
                // 手动拼接，避免 Path.Combine（尽量少触发 BCL）
                string stagingDir = userDataDir + "/" + StagingDirName;
                string stagingPath = stagingDir + "/Game2048Demo.dll";

                GD.Print("[HotUpdate] Using user_data_dir: " + userDataDir);
                GD.Print("[HotUpdate] Staging dir: " + stagingDir);
                GD.Print("[HotUpdate] Staging path: " + stagingPath);

                // 创建 staging 目录
                try
                {
                    Directory.CreateDirectory(stagingDir);
                    GD.Print("[HotUpdate] Staging directory ensured");
                }
                catch (Exception e)
                {
                    GD.PrintErr("[HotUpdate] CreateDirectory failed: " + e.Message);
                    // 继续尝试复制，也许目录已存在
                }

                // 复制 DLL 到 staging area（overwrite: true）
                try
                {
                    File.Copy(updateFilePath, stagingPath, overwrite: true);
                    GD.Print("[HotUpdate] File.Copy -> staging OK");
                }
                catch (Exception e)
                {
                    GD.PrintErr("[HotUpdate] File.Copy FAILED: " + e.Message);
                    GD.PrintErr("[HotUpdate] Copy Stack: " + e.StackTrace);
                    return false;
                }

                long stagedSize = -1;
                try
                {
                    stagedSize = new FileInfo(stagingPath).Length;
                }
                catch (Exception e)
                {
                    GD.Print("[HotUpdate] Staged FileInfo.Length failed: " + e.Message);
                }
                GD.Print("[HotUpdate] DLL staged to: " + stagingPath + " (" + stagedSize + " bytes)");
                GD.Print("[HotUpdate] Update will be applied on next restart");

                return true;
            }
            catch (Exception e)
            {
                GD.PrintErr("[HotUpdate] ApplyUpdate() FATAL: " + e.Message);
                GD.PrintErr("[HotUpdate] Stack: " + e.StackTrace);
                return false;
            }
        }

        /// <summary>
        /// 删除已应用的更新源文件，避免重复应用。
        /// </summary>
        public static void CleanUpUpdateSource()
        {
            try
            {
                GD.Print("[HotUpdate] CleanUpUpdateSource() START");
                string updateFile;
                if (IsAndroidRuntime())
                {
                    updateFile = UpdateSourcePath;
                }
                else
                {
                    string userDataDir = GetUserDataDirSafe();
                    updateFile = userDataDir + "/update/Game2048Demo.dll";
                }

                bool exists;
                try { exists = File.Exists(updateFile); }
                catch (Exception e)
                {
                    GD.Print("[HotUpdate] File.Exists in cleanup failed: " + e.Message);
                    exists = false;
                }

                if (exists)
                {
                    try
                    {
                        File.Delete(updateFile);
                        GD.Print("[HotUpdate] Cleaned up update source file: " + updateFile);
                    }
                    catch (Exception e)
                    {
                        GD.PrintErr("[HotUpdate] File.Delete in cleanup failed: " + e.Message);
                    }
                }
                else
                {
                    GD.Print("[HotUpdate] No update source file to clean");
                }
            }
            catch (Exception e)
            {
                GD.PrintErr("[HotUpdate] CleanUpUpdateSource() FATAL: " + e.Message);
                GD.PrintErr("[HotUpdate] Stack: " + e.StackTrace);
            }
        }
    }
}
