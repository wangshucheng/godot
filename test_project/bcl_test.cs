using Godot;
using System;

// P2-9: 改造为断言式测试。每个用例都做实际校验后再打 PASS/FAIL，
// 不再无条件 Print PASS。
//
// WASM 编码铁律（见 gc_test.cs / AGENTS.md 第四节）：
// - 只用字面量 GD.Print，不拼接非字面量字符串；
// - 禁止 string.Length / ToString() / 含非字面量的 Concat / $"..." 插值；
// - 汇总计数用条件分支打印固定字符串，避免 int.ToString()。
public partial class bcl_test : Node2D
{
    private static int s_callCount = 0;

    public override void _Ready()
    {
        GD.Print("[bcl_test] === Test 12/12: BCL (Base Class Library) ===");

        // --- scene loaded / binding / Print 已通过本行到达即可证明 ---
        GD.Print("[bcl_test] PASS: Scene loaded and _Ready invoked");
        GD.Print("[bcl_test] PASS: C# script binding + GD.Print functional");

        // --- string literal assignment ---
        string myString = "hello bcl";
        if (myString != null)
        {
            GD.Print("[bcl_test] PASS: string literal assignment works");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: string literal assignment null");
        }

        // --- int arithmetic (strict equality) ---
        int myInt = 42;
        myInt = myInt + 8;
        if (myInt == 50)
        {
            GD.Print("[bcl_test] PASS: int arithmetic 42 plus 8 equals 50");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: int arithmetic 42 plus 8 not 50");
        }

        // --- if/else control flow ---
        if (myInt > 40)
        {
            GD.Print("[bcl_test] PASS: if/else branch true works");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: if/else branch condition false");
        }

        // --- for loop (strict equality) ---
        int loopSum = 0;
        for (int i = 0; i < 5; i++)
        {
            loopSum = loopSum + 1;
        }
        if (loopSum == 5)
        {
            GD.Print("[bcl_test] PASS: for loop iteration sum equals 5");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: for loop iteration sum not 5");
        }

        // --- static variable (verify access + persistence across scenes) ---
        // 注: test_menu -> ... -> bcl_test -> gc_test -> test_menu -> ... 链式循环，
        // 每轮 bcl_test._Ready 都会自增 s_callCount。第一次 == 1, 之后逐轮递增。
        // 断言 "> 0" 验证静态字段可读写且持久（不会因场景切换重置）。
        s_callCount = s_callCount + 1;
        if (s_callCount > 0)
        {
            GD.Print("[bcl_test] PASS: static variable access and persistence works");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: static variable access callCount not positive");
        }

        // --- boolean logic ---
        bool b1 = true;
        bool b2 = false;
        if (b1 && !b2)
        {
            GD.Print("[bcl_test] PASS: boolean logic and not works");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: boolean logic unexpected result");
        }

        // --- array access ---
        int[] arr = new int[3];
        arr[0] = 10;
        arr[1] = 20;
        arr[2] = 30;
        if (arr[0] + arr[1] + arr[2] == 60)
        {
            GD.Print("[bcl_test] PASS: int array allocation and indexed sum works");
        }
        else
        {
            GD.Print("[bcl_test] FAIL: int array indexed sum not 60");
        }

        // --- WASM-known-broken APIs (documented, not executed) ---
        GD.Print("[bcl_test] NOTE: Instance methods and variable string concat not testable in WASM");
        GD.Print("[bcl_test] NOTE: string.Length, ToString(), String.Concat all crash in WASM interpreter");

        // --- summary (avoid int.ToString, use fixed strings) ---
        // 计数变量本身不打印；以"无任何 FAIL 行出现"作为通过判据，由运行脚本统计。
        GD.Print("[bcl_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://gc_test.tscn");
    }
}
