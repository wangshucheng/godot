using Godot;
using System;
using System.Diagnostics;

// ============================================================
// Godot 4.7 Mono C# Performance Benchmark Suite
//
// Mirrors the official Godot C# micro-benchmark columns:
//   Column "G471 official" (our Mono 6.12 port, desktop JIT).
//
// Timing uses System.Diagnostics.Stopwatch (high-res on desktop).
// Output uses Runtime.DebugUiAddLine (same debug label as Test.cs).
// All benchmarks run across multiple frames (state machine) so
// mobile / WASM don't stall on long synchronous loops.
//
// Compatible with the pre-built GodotSharp.dll (no glue rebuild
// required — uses only published Godot 4.7 C# APIs).
// ============================================================
public partial class Benchmark : Node
{
    // Reusable objects for benchmarks. Never added to scene tree so
    // we don't pay scene-tree notification overhead in micro-measurements.
    private Node _obj;
    private Node3D _node3d;
    private int _iters;
    private int _state;
    private int _waitFrames;
    private bool _initialized;
    private bool _needAddChildren;

    // ---- Pure C# event / delegate benchmarks (non-Godot) ----
    public event EventHandler<int> CSharpEvent;
    private int _lastEventArg;
    private void OnCSharpEvent(object sender, int arg) { _lastEventArg = arg; }

    // ---- No-op handlers ----
    private void NoOpHandler() { }
    private void NoOpIntHandler(int x) { _lastEventArg = x; }
    private void NoOpIntEvent(object sender, int arg) { }

    // ---- Timing helpers (desktop-safe, Stopwatch wraps QueryPerformanceCounter on Windows) ----
    private Stopwatch _sw;

    // ---- Safe conversion helpers (prevent NullReference on variant casts) ----
    private static bool SafeGetBool(object v, bool fallback = false) {
        try {
            if (v == null) return fallback;
            if (v is bool) return (bool)v;
            return System.Convert.ToBoolean(v);
        } catch { return fallback; }
    }
    private static string SafeGetString(object v, string fallback = "") {
        try {
            if (v == null) return fallback;
            return v.ToString();
        } catch { return fallback; }
    }
    private static Vector3 SafeGetVector3(object v, Vector3 fallback) {
        try {
            if (v == null) return fallback;
            if (v is Vector3) return (Vector3)v;
            return fallback;
        } catch { return fallback; }
    }
    private static int SafeGetInt(object v, int fallback = 0) {
        try {
            if (v == null) return fallback;
            if (v is int) return (int)v;
            if (v is long) return (int)(long)v;
            return System.Convert.ToInt32(v);
        } catch { return fallback; }
    }

    public override void _Ready()
    {
        if (_initialized) { return; }
        _initialized = true;

        Runtime.DebugUiInit();
        Runtime.DebugUiClear();

        // Reduced from 100k → 10k for fast turnaround; still stable enough
        // to see 100ns-10μs range deltas. Raise to 100k for production numbers.
        _iters = 10000;
        _sw = new Stopwatch();

        _obj = new Node();
        if (_obj != null) { _obj.Name = "bench_node"; try { AddChild(_obj); } catch { /* deferred in Process */ _needAddChildren = true; } }
        _node3d = new Node3D();
        if (_node3d != null) { _node3d.Name = "bench_node3d"; try { if (_node3d != null) AddChild(_node3d); } catch { _needAddChildren = true; } }

        GD.Print("[Bench] _obj valid=" + (_obj != null ? "yes" : "NO"));
        GD.Print("[Bench] _node3d valid=" + (_node3d != null ? "yes" : "NO"));
        GD.Print("[Bench] iters=" + _iters);

        // Pre-connect C# event handler so Emit benchmarks only measure emit cost.
        this.CSharpEvent += OnCSharpEvent;

        PrintHeader();
        _state = 1;
    }

    public override void _Process(double delta)
    {
        if (_needAddChildren) {
            _needAddChildren = false;
            try { if (_obj != null && !_obj.IsInsideTree()) AddChild(_obj); } catch {}
            try { if (_node3d != null && !_node3d.IsInsideTree()) AddChild(_node3d); } catch {}
        }
        if (_waitFrames > 0) { _waitFrames--; return; }
        try {
        switch (_state)
        {
            case 1: Run_ObjectCalls(); break;
            case 2: Run_ExtraObjectCalls(); break;
            case 3: Run_Signals(); break;
            case 4: Run_ExtraSignals(); break;
            case 5: Run_Properties(); break;
            case 6: Run_ExtraProperties(); break;
            case 7: Run_DirectMethods(); break;
            case 8: Run_ExtraDirectCalls(); break;
            case 9: Run_DirectProperties(); break;
            case 999:
                Runtime.DebugUiAddLine("");
                Runtime.DebugUiAddLine("=== Benchmark complete. ===");
                GD.Print("");
                GD.Print("=== Benchmark complete. ===");
                // Keep window open 120 frames (~2 sec @60fps) for screenshot
                _state = 998;
                break;
            case 998:
                if (_waitFrames == 0) _waitFrames = 120;
                // After wait completes, quit (but GPU driver crash may occur, ignore)
                if (_waitFrames <= 1) {
                    try { GetTree().Quit(); } catch {}
                    _state = 1000;
                }
                break;
            case 1000:
                break;
        }
        } catch (Exception ex) {
            GD.Print("[Bench] _Process EXCEPTION state=" + _state + ": " + ex.GetType().Name + " " + ex.Message);
            GD.Print(ex.StackTrace);
            // Skip to next section to avoid infinite crash loop
            if (_state < 999) { _state++; GD.Print("[Bench] advancing to state=" + _state); }
        }
    }

    // ====================================================================
    // Output helpers (desktop-only formatting — matches ref screenshot)
    // Outputs both to the in-engine Debug UI Label AND to stdout via
    // GD.Print, so running the engine from the CLI captures a text log
    // (useful when the window is too small to show the entire table).
    // ====================================================================
    private void PrintHeader()
    {
        string line = "Benchmark                                        G471 official";
        Runtime.DebugUiAddLine(line);
        GD.Print(line);
        Runtime.DebugUiAddSeparator();
        GD.Print("----------------------------------------------------------------------");
    }
    private void PrintSection(string name)
    {
        string line = "--- " + name + " ---";
        Runtime.DebugUiAddLine(line);
        GD.Print(line);
    }
    // Pre-condition: _sw is stopped, iterations == iters.
    // Prints "name       X.XXXX ns" (4 decimal places, ns per iter).
    private void PrintRow(string name, int iters)
    {
        double totalMs = _sw.Elapsed.TotalMilliseconds;
        // ns/iter = (totalMs * 1e6) / iters
        double nsPerIter = (totalMs * 1000000.0) / (double)iters;
        string num = nsPerIter.ToString("0.0000");
        // Pad name to 48 chars, then right-align number to 18-wide
        string padded = name.PadRight(48);
        string numPadded = num.PadLeft(18);
        string full = padded + numPadded;
        Runtime.DebugUiAddLine(full);
        GD.Print(full);
    }

    // ====================================================================
    // 1. Object calls (variant marshaling + string method names)
    // ====================================================================
    private void Run_ObjectCalls()
    {
        PrintSection("Object calls");
        int N = _iters;
        if (_obj == null) {
            GD.Print("[Bench] _obj is NULL, skipping section");
            _state = 2; return;
        }

        try {
        // 1. Call (no-arg no-op: set_process_internal false — idempotent)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Call("set_process_internal", false); }
        _sw.Stop();
        PrintRow("Object.Call (set_process_internal, 1 bool)", N);

        // 2. Call (1 bool arg: set_process true)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Call("set_process", true); }
        _sw.Stop();
        PrintRow("Object.Call (set_process, 1 bool)", N);

        // 3. Call (1 string arg: rename)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Call("set_name", "bm"); }
        _sw.Stop();
        PrintRow("Object.Call (set_name, 1 string)", N);

        // 4. Set (property via string → int)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Set("name", "bm"); }
        _sw.Stop();
        PrintRow("Object.Set (name, string)", N);

        // 5. Get (property via string → string)
        object g = null;
        _sw.Restart();
        for (int i = 0; i < N; i++) { g = _obj.Get("name"); }
        _sw.Stop();
        PrintRow("Object.Get (name → string)", N);
        string _s = SafeGetString(g, "");
        if (_s == null) { /* no-op to prevent DCE */ }

        // 6. Set (bool property via string)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Set("process", true); }
        _sw.Stop();
        PrintRow("Object.Set (process, bool)", N);

        // 7. Get (bool property via string) — SAFE cast
        bool bg = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { bg = SafeGetBool(_obj.Get("process"), false); }
        _sw.Stop();
        PrintRow("Object.Get (process → bool)", N);
        if (!bg) { /* no-op */ }
        } catch (Exception ex) {
            GD.Print("[Bench] Object calls (1-7) EXCEPTION: " + ex.GetType().Name + " " + ex.Message);
        }

        // Node3D benchmarks — require _node3d to be valid
        if (_node3d == null) {
            GD.Print("[Bench] _node3d is NULL, skipping Vector3 tests");
            _state = 2; return;
        }
        try {
        int N2 = _iters;

        // 8. Set (Vector3) via Node3D.position property
        Vector3 vp = new Vector3(1, 2, 3);
        _sw.Restart();
        for (int i = 0; i < N2; i++) { _node3d.Set("position", vp); }
        _sw.Stop();
        PrintRow("Object.Set (position, Vector3)", N2);

        // 9. Get (Vector3) property — SAFE cast
        Vector3 vg = new Vector3();
        _sw.Restart();
        for (int i = 0; i < N2; i++) { vg = SafeGetVector3(_node3d.Get("position"), new Vector3()); }
        _sw.Stop();
        PrintRow("Object.Get (position → Vector3)", N2);
        if (vg.x < 0) { /* no-op */ }
        } catch (Exception ex) {
            GD.Print("[Bench] Object calls (8-9 Node3D) EXCEPTION: " + ex.GetType().Name + " " + ex.Message);
        }

        _state = 2;
    }

    private void Run_ExtraObjectCalls()
    {
        PrintSection("Object calls (continued)");
        int N = _iters;

        // 10. Connect + Disconnect pair (native signal; Action callback).
        // Signal "script_changed" always exists but won't fire for Node.
        Action cb = NoOpHandler;
        int Nsmall = N / 20;
        _sw.Restart();
        for (int i = 0; i < Nsmall; i++) {
            _obj.Connect("script_changed", cb);
            _obj.Disconnect("script_changed", cb);
        }
        _sw.Stop();
        PrintRow("Object.Connect+Disconnect (Action, pair)", Nsmall);

        // 11. EmitSignal (0 args, connected — dispatch work)
        // Use "script_changed" which always exists on Node (no args).
        Action cbEmit = NoOpHandler;
        _obj.Connect("script_changed", cbEmit);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.EmitSignal("script_changed"); }
        _sw.Stop();
        PrintRow("Object.EmitSignal (script_changed, 0 args, connected)", N);
        _obj.Disconnect("script_changed", cbEmit);

        // 12. HasSignal (string)
        bool hs = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { hs = _obj.HasSignal("ready"); }
        _sw.Stop();
        PrintRow("Object.HasSignal (ready, string)", N);
        if (!hs) { /* no-op */ }

        // 13. IsInstanceValid
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.IsInstanceValid(); }
        _sw.Stop();
        PrintRow("Object.IsInstanceValid", N);

        // 14. ToString
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.ToString(); }
        _sw.Stop();
        PrintRow("Object.ToString", N);

        // 15. GetNativePtr (internal pointer access, public API).
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.GetNativePtr(); }
        _sw.Stop();
        PrintRow("Object.GetNativePtr", N);

        _state = 3;
    }

    // ====================================================================
    // 2. Signals (Godot signals vs pure C# events)
    // ====================================================================
    private void Run_Signals()
    {
        PrintSection("Signals");
        int N = _iters;

        // 1. Godot Signal.Emit — "ready" signal already exists on every Node.
        Action rdyCb = NoOpHandler;
        _obj.Connect("ready", rdyCb);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.EmitSignal("ready"); }
        _sw.Stop();
        PrintRow("Godot Signal.Emit (ready, 0 args, connected)", N);
        _obj.Disconnect("ready", rdyCb);

        // 2. Godot Signal.Emit (0 args, different signal: tree_entered)
        Action teCb = NoOpHandler;
        _obj.Connect("tree_entered", teCb);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.EmitSignal("tree_entered"); }
        _sw.Stop();
        PrintRow("Godot Signal.Emit (tree_entered, 0 args, connected)", N);
        _obj.Disconnect("tree_entered", teCb);

        // 3. C# EventHandler<T>.Invoke
        _sw.Restart();
        for (int i = 0; i < N; i++) { CSharpEvent?.Invoke(this, 1); }
        _sw.Stop();
        PrintRow("C# EventHandler<int>.Invoke (1 int)", N);

        // 4. C# Action (pure delegate, no event)
        Action act = NoOpHandler;
        _sw.Restart();
        for (int i = 0; i < N; i++) { act(); }
        _sw.Stop();
        PrintRow("C# Action.Invoke ()", N);

        // 5. C# Action<int> (typed delegate)
        Action<int> acti = NoOpIntHandler;
        _sw.Restart();
        for (int i = 0; i < N; i++) { acti(42); }
        _sw.Stop();
        PrintRow("C# Action<int>.Invoke (1 int)", N);

        // 6. Godot Signal.Connect per-iteration (create Action, connect, disconnect).
        int Ns = N / 40;
        _sw.Restart();
        for (int i = 0; i < Ns; i++) {
            Action c = NoOpHandler;
            _obj.Connect("tree_entered", c);
            _obj.Disconnect("tree_entered", c);
        }
        _sw.Stop();
        PrintRow("Godot Signal.Connect+Disconnect (per-iter)", Ns);

        // 7. C# event add/remove per-iteration
        Ns = N / 40;
        _sw.Restart();
        for (int i = 0; i < Ns; i++) {
            this.CSharpEvent += NoOpIntEvent;
            this.CSharpEvent -= NoOpIntEvent;
        }
        _sw.Stop();
        PrintRow("C# EventHandler add+remove (per-iter)", Ns);

        _state = 4;
    }

    private void Run_ExtraSignals()
    {
        PrintSection("Signals (continued)");
        int N = _iters;

        // 8. ToSignal (SignalAwaiter create per iter, smaller iters).
        // Uses a throwaway Node so the one-shot auto-connects don't pollute _obj.
        int Nsmall = N / 100;
        Node tmp = new Node();
        AddChild(tmp); // Must be inside tree for ToSignal internal wiring
        int cmpCount = 0;
        _sw.Restart();
        for (int i = 0; i < Nsmall; i++) {
            SignalAwaiter a = tmp.ToSignal(tmp, "script_changed");
            bool cmp = a.IsCompleted;
            if (cmp) cmpCount++;
        }
        _sw.Stop();
        PrintRow("SignalAwaiter (ToSignal) create", Nsmall);
        RemoveChild(tmp);
        tmp.Free();
        if (cmpCount < 0) { /* no-op */ }

        // 9. TaskCompletionSource + C# event round-trip
        Nsmall = N / 50;
        int awaited = 0;
        _sw.Restart();
        for (int i = 0; i < Nsmall; i++) {
            var tcs = new System.Threading.Tasks.TaskCompletionSource<int>();
            EventHandler<int> handler = (s, x) => tcs.TrySetResult(x);
            this.CSharpEvent += handler;
            CSharpEvent?.Invoke(this, i);
            this.CSharpEvent -= handler;
            awaited += (tcs.Task.Result == i) ? 1 : 0;
        }
        _sw.Stop();
        PrintRow("C# event + TaskCompletionSource roundtrip", Nsmall);
        if (awaited < 0) { /* no-op */ }

        _state = 5;
    }

    // ====================================================================
    // 3. Properties (variant: Object.Set/Get via string keys)
    // ====================================================================
    private void Run_Properties()
    {
        PrintSection("Properties (variant)");
        if (_obj == null) {
            GD.Print("[Bench] _obj NULL, skip Properties");
            _state = 6; return;
        }
        int N = _iters;
        try {

        // 1. Object.Set string property
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Set("name", "bm"); }
        _sw.Stop();
        PrintRow("Property.Set (name → string)", N);

        // 2. Object.Get string property
        object s = null;
        _sw.Restart();
        for (int i = 0; i < N; i++) { s = _obj.Get("name"); }
        _sw.Stop();
        PrintRow("Property.Get (name → string)", N);
        string _s2 = SafeGetString(s, "");
        if (_s2 == null) { /* no-op */ }

        // 3. Object.Set bool property
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Set("process", true); }
        _sw.Stop();
        PrintRow("Property.Set (process → bool)", N);

        // 4. Object.Get bool property — SAFE cast
        bool bp = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { bp = SafeGetBool(_obj.Get("process"), false); }
        _sw.Stop();
        PrintRow("Property.Get (process → bool)", N);
        if (!bp) { /* no-op */ }

        } catch (Exception ex) {
            GD.Print("[Bench] Properties (1-4) EXCEPTION: " + ex.GetType().Name + " " + ex.Message);
        }

        if (_node3d == null) {
            GD.Print("[Bench] _node3d NULL, skip Vector3 Properties");
            _state = 6; return;
        }
        try {

        // 5. Object.Set Vector3 position via Node3D
        Vector3 vp = new Vector3(1.0f, 2.0f, 3.0f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.Set("position", vp); }
        _sw.Stop();
        PrintRow("Property.Set (position → Vector3)", N);

        // 6. Object.Get Vector3 position — SAFE cast
        Vector3 gp = new Vector3();
        _sw.Restart();
        for (int i = 0; i < N; i++) { gp = SafeGetVector3(_node3d.Get("position"), new Vector3()); }
        _sw.Stop();
        PrintRow("Property.Get (position → Vector3)", N);
        if (gp.x < 0) { /* no-op */ }

        // 7. Object.Set Vector3 scale
        Vector3 sc = new Vector3(1.0f, 1.0f, 1.0f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.Set("scale", sc); }
        _sw.Stop();
        PrintRow("Property.Set (scale → Vector3)", N);

        // 8. Object.Get Vector3 scale — SAFE cast
        _sw.Restart();
        for (int i = 0; i < N; i++) { sc = SafeGetVector3(_node3d.Get("scale"), new Vector3()); }
        _sw.Stop();
        PrintRow("Property.Get (scale → Vector3)", N);
        if (sc.x < 0) { /* no-op */ }

        } catch (Exception ex) {
            GD.Print("[Bench] Properties (5-8) EXCEPTION: " + ex.GetType().Name + " " + ex.Message);
        }

        _state = 6;
    }

    private void Run_ExtraProperties()
    {
        PrintSection("Properties (continued)");
        if (_node3d == null) {
            GD.Print("[Bench] _node3d NULL, skip ExtraProperties");
            _state = 7; return;
        }
        int N = _iters;
        try {

        // 9. Set Visible (Node3D — bool typed property exists in glue)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.Visible = true; }
        _sw.Stop();
        PrintRow("Property.set_Visible (Node3D, bool)", N);

        // 10. Get Visible
        bool vis = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { vis = _node3d.Visible; }
        _sw.Stop();
        PrintRow("Property.get_Visible (Node3D, bool)", N);
        if (!vis) { /* no-op */ }

        // 11. Set TopLevel (Node3D — bool typed)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.TopLevel = true; }
        _sw.Stop();
        PrintRow("Property.set_TopLevel (Node3D, bool)", N);

        // 12. Get TopLevel
        bool tl = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { tl = _node3d.TopLevel; }
        _sw.Stop();
        PrintRow("Property.get_TopLevel (Node3D, bool)", N);
        if (!tl) { /* no-op */ }

        } catch (Exception ex) {
            GD.Print("[Bench] ExtraProperties EXCEPTION: " + ex.GetType().Name + " " + ex.Message);
        }

        _state = 7;
    }

    // ====================================================================
    // 4. Direct methods (typed C# calls — no variant, no string names)
    // ====================================================================
    private void Run_DirectMethods()
    {
        PrintSection("Direct methods (typed C#)");
        int N = _iters;

        // 1. Node.SetProcess (bool — typed icall, no variant)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.SetProcess(true); }
        _sw.Stop();
        PrintRow("Node.SetProcess (bool)", N);

        // 2. Node.SetPhysicsProcess (bool)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.SetPhysicsProcess(true); }
        _sw.Stop();
        PrintRow("Node.SetPhysicsProcess (bool)", N);

        // 3. Node.SetProcessInput (bool)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.SetProcessInput(true); }
        _sw.Stop();
        PrintRow("Node.SetProcessInput (bool)", N);

        // 4. Node.IsInsideTree
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.IsInsideTree(); }
        _sw.Stop();
        PrintRow("Node.IsInsideTree", N);

        // 5. Node.GetChildCount
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.GetChildCount(); }
        _sw.Stop();
        PrintRow("Node.GetChildCount", N);

        // 6. Node.GetNodeOrNull (miss — typed generic)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.GetNodeOrNull<Node>("not_exists"); }
        _sw.Stop();
        PrintRow("Node.GetNodeOrNull<Node> (miss, typed)", N);

        // 7. Object.IsInstanceValid
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.IsInstanceValid(); }
        _sw.Stop();
        PrintRow("GodotObject.IsInstanceValid", N);

        _state = 8;
    }

    private void Run_ExtraDirectCalls()
    {
        PrintSection("Direct methods (continued)");
        int N = _iters;

        // 8. Node3D.SetPosition (Vector3) — typed method
        Vector3 vp = new Vector3(1.0f, 2.0f, 3.0f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.SetPosition(vp); }
        _sw.Stop();
        PrintRow("Node3D.SetPosition (Vector3)", N);

        // 9. Node3D.GetPosition
        Vector3 gvp = new Vector3();
        _sw.Restart();
        for (int i = 0; i < N; i++) { gvp = _node3d.GetPosition(); }
        _sw.Stop();
        PrintRow("Node3D.GetPosition (Vector3)", N);
        if (gvp.x < 0) { /* no-op */ }

        // 10. Node3D.SetRotation (Vector3)
        Vector3 rot = new Vector3(0.1f, 0.2f, 0.3f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.SetRotation(rot); }
        _sw.Stop();
        PrintRow("Node3D.SetRotation (Vector3)", N);

        // 11. Node3D.GetRotation
        _sw.Restart();
        for (int i = 0; i < N; i++) { rot = _node3d.GetRotation(); }
        _sw.Stop();
        PrintRow("Node3D.GetRotation (Vector3)", N);
        if (rot.x < 0) { /* no-op */ }

        // 12. Node3D.SetScale (Vector3)
        Vector3 sc = new Vector3(1.0f, 1.0f, 1.0f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.SetScale(sc); }
        _sw.Stop();
        PrintRow("Node3D.SetScale (Vector3)", N);

        // 13. Node3D.GetScale
        _sw.Restart();
        for (int i = 0; i < N; i++) { sc = _node3d.GetScale(); }
        _sw.Stop();
        PrintRow("Node3D.GetScale (Vector3)", N);
        if (sc.x < 0) { /* no-op */ }

        // 14. Node3D.SetGlobalPosition
        Vector3 gp = new Vector3(1.0f, 2.0f, 3.0f);
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.SetGlobalPosition(gp); }
        _sw.Stop();
        PrintRow("Node3D.SetGlobalPosition (Vector3)", N);

        // 15. Node3D.GetGlobalPosition
        _sw.Restart();
        for (int i = 0; i < N; i++) { gp = _node3d.GetGlobalPosition(); }
        _sw.Stop();
        PrintRow("Node3D.GetGlobalPosition (Vector3)", N);
        if (gp.x < 0) { /* no-op */ }

        // Run Direct properties immediately in this same frame to avoid GPU
        // crash across frame boundary (NVIDIA driver bug with short-lived windows)
        Run_DirectProperties();
    }

    // ====================================================================
    // 5. Direct properties (C# property getters/setters — typed)
    // ====================================================================
    private void Run_DirectProperties()
    {
        PrintSection("Direct properties (typed C#)");
        int N = _iters;

        // 1. Node.set_Name (string)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _obj.Name = "bm"; }
        _sw.Stop();
        PrintRow("Node.set_Name (string)", N);

        // 2. Node.get_Name (string)
        string name = "";
        _sw.Restart();
        for (int i = 0; i < N; i++) { name = _obj.Name; }
        _sw.Stop();
        PrintRow("Node.get_Name (string)", N);
        if (name == null) { /* no-op */ }

        // 3. Node3D.set_RotationOrder (long — typed property in glue)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.RotationOrder = 0; }
        _sw.Stop();
        PrintRow("Node3D.set_RotationOrder (long)", N);

        // 4. Node3D.get_RotationOrder (long)
        long ro = 0;
        _sw.Restart();
        for (int i = 0; i < N; i++) { ro = _node3d.RotationOrder; }
        _sw.Stop();
        PrintRow("Node3D.get_RotationOrder (long)", N);
        if (ro < 0) { /* no-op */ }

        // 5. Node3D.set_Visible (bool — typed)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.Visible = true; }
        _sw.Stop();
        PrintRow("Node3D.set_Visible (bool)", N);

        // 6. Node3D.get_Visible (bool)
        bool vis = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { vis = _node3d.Visible; }
        _sw.Stop();
        PrintRow("Node3D.get_Visible (bool)", N);
        if (!vis) { /* no-op */ }

        // 7. Node3D.set_TopLevel (bool — typed)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.TopLevel = true; }
        _sw.Stop();
        PrintRow("Node3D.set_TopLevel (bool)", N);

        // 8. Node3D.get_TopLevel (bool)
        bool tl = false;
        _sw.Restart();
        for (int i = 0; i < N; i++) { tl = _node3d.TopLevel; }
        _sw.Stop();
        PrintRow("Node3D.get_TopLevel (bool)", N);
        if (!tl) { /* no-op */ }

        // 9. Node3D.set_RotationEditMode (long — typed)
        _sw.Restart();
        for (int i = 0; i < N; i++) { _node3d.RotationEditMode = 0; }
        _sw.Stop();
        PrintRow("Node3D.set_RotationEditMode (long)", N);

        // 10. Node3D.get_RotationEditMode (long)
        long re = 0;
        _sw.Restart();
        for (int i = 0; i < N; i++) { re = _node3d.RotationEditMode; }
        _sw.Stop();
        PrintRow("Node3D.get_RotationEditMode (long)", N);
        if (re < 0) { /* no-op */ }

        // 11. Node3D.SetPosition + GetPosition chained (modify X write-back)
        _sw.Restart();
        for (int i = 0; i < N; i++) {
            Vector3 v = _node3d.GetPosition();
            v.x += 1.0f;
            _node3d.SetPosition(v);
        }
        _sw.Stop();
        PrintRow("Node3D Position.x r+w chain (read-modify-write)", N);

        _state = 999;
    }
}
