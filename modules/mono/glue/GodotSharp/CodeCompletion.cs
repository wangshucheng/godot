using System;

namespace Godot {
    // A2 (W4): engine-data completion categories, mirroring
    // gdmono::CompletionKind in modules/mono/editor/code_completion.h.
    // Numeric values are part of the icall ABI — keep in sync.
    public enum CompletionKind {
        InputActions = 0,
        NodePaths = 1,
        ResourcePaths = 2,
        ScenePaths = 3,
        ShaderParams = 4,
        Signals = 5,
        ThemeColors = 6,
        ThemeConstants = 7,
        ThemeFonts = 8,
        ThemeFontSizes = 9,
        ThemeStyles = 10
    }

    // A2 (W4): editor-side engine-data completion provider (ported from the
    // old mono module). Suggestions are quoted string literals (e.g.
    // "\"ui_accept\"", "\"res://scenes/main.tscn\"") intended for editor
    // tooling that surfaces them as C# completions.
    [Preserve(AllMembers = true)]
    public static class CodeCompletion {
        // Editor builds only. In export templates the native icall is not
        // registered (TOOLS_ENABLED); invoking it raises
        // EntryPointNotFoundException, converted here to an empty result so
        // tooling can call unconditionally.
        public static string[] GetCodeCompletion(CompletionKind kind, string scriptFile) {
            string joined;
            try {
                joined = Bridge.godot_icall_Editor_GetCodeCompletion((int)kind, scriptFile ?? string.Empty);
            } catch (EntryPointNotFoundException) {
                return Array.Empty<string>();
            }
            if (string.IsNullOrEmpty(joined)) {
                return Array.Empty<string>();
            }
            return joined.Split('\n');
        }
    }
}
