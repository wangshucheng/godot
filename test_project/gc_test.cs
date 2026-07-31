using Godot;
using System;

// Test 13/13: GC stress - bulk Node2D create/remove/free cycles.
// 借鉴参考项目 csharp_test/Test.cs 的 GC 压力场景（100/500/1000 + 2x500 循环，
// 原 10000 因 WASM 解释器稳定性缩减）。对 S1-S3 修复的包装对象/gchandle 路径施压。
// WASM 编码铁律: 只用字面量 GD.Print，不拼接非字面量字符串，不调 ToString，
// 不调 System.GC.*，不用带 string 参数的实例 void 方法。
public partial class gc_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[gc_test] === Test 13/13: GC stress ===");
        GD.Print("[gc_test] PASS: Scene loaded successfully");
        GD.Print("[gc_test] PASS: _Ready callback invoked");

        // --- Phase A: 100 nodes create/verify/remove ---
        int failA = RunBatch(100);
        if (failA == 0)
        {
            GD.Print("[gc_test] PASS: batch 100 nodes create/remove");
        }
        else
        {
            GD.Print("[gc_test] FAIL: batch 100 nodes create/remove");
        }

        // --- Phase B: 500 nodes ---
        int failB = RunBatch(500);
        if (failB == 0)
        {
            GD.Print("[gc_test] PASS: batch 500 nodes create/remove");
        }
        else
        {
            GD.Print("[gc_test] FAIL: batch 500 nodes create/remove");
        }

        // --- Phase C: 1000 nodes (参考项目同规模上限) ---
        int failC = RunBatch(1000);
        if (failC == 0)
        {
            GD.Print("[gc_test] PASS: batch 1000 nodes create/remove");
        }
        else
        {
            GD.Print("[gc_test] FAIL: batch 1000 nodes create/remove");
        }

        // --- Phase D: 2 repeated cycles of 500 (GC 压缩/句柄复用压力) ---
        int cyclesOk = 1;
        for (int i = 0; i < 2; i++)
        {
            if (RunBatch(500) != 0)
            {
                cyclesOk = 0;
                break;
            }
        }
        if (cyclesOk == 1)
        {
            GD.Print("[gc_test] PASS: repeated 2x500 cycles");
        }
        else
        {
            GD.Print("[gc_test] FAIL: repeated 2x500 cycles");
        }

        // --- Phase E: 托管侧分配压力（纯数组索引，WASM 安全路径）---
        int[] largeArray = new int[10000];
        for (int i = 0; i < largeArray.Length; i++)
        {
            largeArray[i] = i;
        }
        int sum = 0;
        for (int i = 0; i < largeArray.Length; i++)
        {
            sum += largeArray[i];
        }
        if (sum == 49995000)
        {
            GD.Print("[gc_test] PASS: managed array 10000 alloc + checksum");
        }
        else
        {
            GD.Print("[gc_test] FAIL: managed array checksum mismatch");
        }
        largeArray = null;

        GD.Print("[gc_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://test_menu.tscn");
    }

    // 创建 count 个 Node2D 子节点，校验数量，再移除并 queue_free。
    // 返回 0 成功，1 失败。仅 int 参数/返回值，WASM 解释器安全。
    private int RunBatch(int count)
    {
        int baseline = GetChildCount();

        Node2D[] nodes = new Node2D[count];
        for (int i = 0; i < count; i++)
        {
            Node2D n = new Node2D();
            if (n == null)
            {
                return 1;
            }
            AddChild(n);
            nodes[i] = n;
        }

        if (GetChildCount() != baseline + count)
        {
            return 1;
        }

        for (int i = 0; i < count; i++)
        {
            RemoveChild(nodes[i]);
            nodes[i].QueueFree();
            nodes[i] = null;
        }

        if (GetChildCount() != baseline)
        {
            return 1;
        }

        return 0;
    }
}
