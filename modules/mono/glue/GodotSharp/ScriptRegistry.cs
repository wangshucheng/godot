namespace Godot {
    // W5 SG PoC (v3 pre-research): compile-time script metadata registry.
    // GodotSharp.SourceGenerators emits a [ModuleInitializer] per user
    // assembly that calls RegisterGlobalClass for every [GlobalClass] type,
    // pushing the registry into the engine via icall (eval
    // eval_2026-07-26_v3_sourcegenerators.md §5.3 方式 A — C# pushes, the
    // engine never invokes managed code, so the WASM interpreter's
    // mono_runtime_invoke signature quirks are avoided entirely).
    //
    // The engine side triggers this by running <Module>'s class initializer
    // (mono_runtime_class_init) from CSharpLanguage::refresh_global_classes()
    // before falling back to the TypeDef reflection scan, so non-SG
    // assemblies keep working unchanged.
    [Preserve(AllMembers = true)]
    public static class ScriptRegistry {
        // int flags instead of bool — WASM interpreter icall convention.
        // iconPath may be null (attribute's IconPath property unset).
        public static void RegisterGlobalClass(string className, string baseType, int isTool, int isAbstract, string iconPath) {
            Bridge.godot_icall_ScriptRegistry_RegisterGlobalClass(className, baseType, isTool, isAbstract, iconPath);
        }
    }
}
