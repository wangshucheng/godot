using Godot;
using System;

// ============================================================
// Phase 0.1: Delegate 可用性验证探针
//
// 目标：验证 delegate 在 WASM interpreter 下的可用性，为阶段 0
// （通知路径 SG 化）提供决策依据。
//
// 验证矩阵：
//   Test 1: C# 直接调用 delegate（基线，必然可用）
//   Test 2: C++ 通过 mono_runtime_invoke 调用 delegate.Invoke
//   Test 3: C++ 通过 mono_method_get_function_pointer 直接调用
//
// 输出：每个测试的 PASS/FAIL + 调用计数验证
// ============================================================
public partial class DelegateProbe : Node
{
    private int _callCount = 0;
    private Action _delegate;

    public override void _Ready()
    {
        GD.Print("[PROBE] START DelegateProbe");

        // 创建 delegate 绑定到实例方法
        _delegate = new Action(OnDelegateInvoked);

        // ===== Test 1: C# 直接调用 delegate（基线） =====
        _callCount = 0;
        _delegate();
        int count1 = _callCount;
        if (count1 == 1)
        {
            GD.Print("[PROBE] Test1 C# direct invoke: PASS (count=" + count1 + ")");
        }
        else
        {
            GD.Print("[PROBE] Test1 C# direct invoke: FAIL (count=" + count1 + ", expected 1)");
        }

        // ===== Test 2: C++ 通过 mono_runtime_invoke 调用 delegate.Invoke =====
        // 把 delegate 注册到 C++ 侧
        Runtime.TestRegisterDelegateProbe(_delegate);

        // 重置计数，让 C++ 触发调用
        _callCount = 0;
        int invokeResult = Runtime.TestInvokeDelegateViaMRI();
        int count2 = _callCount;
        if (invokeResult == 1 && count2 == 1)
        {
            GD.Print("[PROBE] Test2 C++ mono_runtime_invoke(delegate.Invoke): PASS (count=" + count2 + ")");
        }
        else
        {
            GD.Print("[PROBE] Test2 C++ mono_runtime_invoke(delegate.Invoke): FAIL (invokeResult=" + invokeResult + ", count=" + count2 + ", expected 1)");
        }

        // ===== Test 3: C++ 通过 mono_compile_method 拿到函数指针直接调用 =====
        _callCount = 0;
        int fptrResult = Runtime.TestInvokeDelegateViaFtnPtr();
        int count3 = _callCount;
        if (fptrResult == 1 && count3 == 1)
        {
            GD.Print("[PROBE] Test3 C++ function pointer invoke: PASS (count=" + count3 + ")");
        }
        else
        {
            GD.Print("[PROBE] Test3 C++ function pointer invoke: FAIL (fptrResult=" + fptrResult + ", count=" + count3 + ", expected 1)");
        }

        // ===== 汇总 =====
        int totalPass = (count1 == 1 ? 1 : 0) + (invokeResult == 1 && count2 == 1 ? 1 : 0) + (fptrResult == 1 && count3 == 1 ? 1 : 0);
        GD.Print("[PROBE] DONE DelegateProbe (pass=" + totalPass + "/3)");

        // 退出进程（headless 测试场景）
        GetTree().Quit();
    }

    private void OnDelegateInvoked()
    {
        _callCount++;
    }
}
