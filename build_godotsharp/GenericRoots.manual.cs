// Manual AOT generic registrations - special cases not detected by analysis.
// This file is merged with auto-extracted roots and [MonoAotGeneric] attributes
// by aot_compile.py to produce GenericRoots.gen.cs.
//
// To add a new manual root: add a static field with the generic instantiation.
// The aot_compile.py parser scans static field declarations.

using System.Collections.Generic;
using System.Threading.Tasks;

internal static class ManualGenericRoots
{
    // Used by reflection in csharp_script.cpp (resolve_mono_class)
    public static readonly List<string> _root_list_string = new List<string>();
    public static readonly List<int> _root_list_int = new List<int>();
    public static readonly Dictionary<string, object> _root_dict_so = new Dictionary<string, object>();
    public static readonly Dictionary<string, int> _root_dict_si = new Dictionary<string, int>();

    // Task<T> used by SignalAwaiter via reflection
    public static readonly Task<bool> _root_task_bool = Task.FromResult(true);
    public static readonly Task<int> _root_task_int = Task.FromResult(0);
}
