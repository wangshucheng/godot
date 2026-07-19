using System;
using System.Runtime.CompilerServices;

namespace Godot;

/// <summary>
/// WeChat minigame audio adapter.
///
/// 微信小游戏环境无法使用 Godot 内置的 AudioStreamPlayer（依赖 AudioWorklet 实时合成，
/// 而微信 wx.createWebAudioContext() 返回的对象没有 audioWorklet 属性）。
/// 本类提供基于微信 InnerAudioContext 的简单音频播放 API，用于播放预录音频文件
/// （.wav/.mp3）。
///
/// 限制：
///   - 不支持实时音频合成（AudioStreamPlayer with procedural audio 无效）
///   - 不支持音效淡入淡出、音高调整等高级效果
///   - 不支持 3D 空间音频
///   - 音频文件需通过 wx.downloadFile 下载到本地或从 PCK 中提取
///
/// 桌面平台：所有方法为 stub（返回 0 / 无操作），不影响测试。
/// 微信小游戏：通过 GameGlobal.GodotAudioWX 调用 InnerAudioContext API。
///
/// 使用示例：
///   <code>
///   int bgmId = WXAudio.PlayBgm("res://music/bgm.mp3");  // 默认循环
///   int fxId = WXAudio.PlayEffect("res://sounds/shoot.wav");
///   WXAudio.SetVolume(bgmId, 0.5f);
///   WXAudio.Stop(fxId);
///   </code>
/// </summary>
public static class WXAudio {
    /// <summary>
    /// 播放音频文件。
    /// </summary>
    /// <param name="src">音频文件路径（res:// 协议或本地路径）</param>
    /// <param name="loop">是否循环播放</param>
    /// <returns>音频 id（&gt;0 成功，0 失败或不支持）</returns>
    public static int Play(string src, bool loop = false) {
        return Runtime.WxAudioPlay(src, loop);
    }

    /// <summary>
    /// 播放短音效（不循环）。适合射击、爆炸等短音频。
    /// </summary>
    public static int PlayEffect(string src) {
        return Runtime.WxAudioPlay(src, false);
    }

    /// <summary>
    /// 播放背景音乐（默认循环）。会自动停止之前的背景音乐。
    /// 注意：本类不维护 bgm id 状态，调用者需自行管理。
    /// </summary>
    public static int PlayBgm(string src, bool loop = true) {
        return Runtime.WxAudioPlay(src, loop);
    }

    /// <summary>停止指定 id 的音频并释放资源。</summary>
    public static void Stop(int id) {
        Runtime.WxAudioStop(id);
    }

    /// <summary>停止所有音频。</summary>
    public static void StopAll() {
        Runtime.WxAudioStopAll();
    }

    /// <summary>设置音量（0.0~1.0）。</summary>
    public static void SetVolume(int id, float volume) {
        Runtime.WxAudioSetVolume(id, volume);
    }

    /// <summary>暂停指定 id 的音频。</summary>
    public static void Pause(int id) {
        Runtime.WxAudioPause(id);
    }

    /// <summary>恢复指定 id 的音频。</summary>
    public static void Resume(int id) {
        Runtime.WxAudioResume(id);
    }
}
