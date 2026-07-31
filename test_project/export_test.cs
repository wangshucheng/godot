using Godot;
using System;

// P1.1 验证: [Export] 字段属性系统——set/get/序列化/枚举/引用类型。
// 独立场景，不加入 13 场景主链，避免影响基线计数。
//
// API 使用注意（2026-07-29 修正）：
// - 属性读取用泛型 Get<T>（绑定库没有非泛型 Get）；
// - Variant INT 封送到 C# 是 boxed int64（gd_mono_interop_variant.cpp:595-598），
//   必须用 Get<long> 读取，Get<int> 会在运行时抛 InvalidCastException；
// - Godot.Collections.Dictionary 尚不存在，Dictionary 正向封送是死分支（返回 null），
//   因此 get_property_list 的元素无法读取 name，只能验证列表可获取且非空。
public partial class export_test : Node2D
{
    // 基本类型
    [Export] public int IntField = 42;
    [Export] public float FloatField = 3.14f;
    [Export] public bool BoolField = true;
    [Export] public string StringField = "hello";

    // 数学类型
    [Export] public Vector2 Vec2Field = new Vector2(1, 2);
    [Export] public Color ColorField = new Color(1, 0, 0);

    // 枚举类型
    public enum MoveMode { Walk, Run, Fly }
    [Export] public MoveMode Mode = MoveMode.Run;

    public override void _Ready()
    {
        GD.Print("[export_test] === P1.1 Export field property system ===");

        // 验证 get: 通过 Godot 属性系统读取字段值（触发 CSharpInstance::get）
        // 注意：用 Get/Set 而非直接字段访问，确保走属性系统路径
        long intVal = Get<long>("IntField");
        if (intVal == 42)
        {
            GD.Print("[export_test] PASS: int field get via property system");
        }
        else
        {
            GD.Print("[export_test] FAIL: int field get expected 42");
        }

        // 验证 set: 通过 Godot 属性系统写字段值（触发 CSharpInstance::set）
        Set("IntField", 100);
        intVal = Get<long>("IntField");
        if (intVal == 100 && IntField == 100)
        {
            GD.Print("[export_test] PASS: int field set via property system");
        }
        else
        {
            GD.Print("[export_test] FAIL: int field set expected 100");
        }

        // 验证 float（Variant FLOAT 封送为 boxed double）
        Set("FloatField", 2.71);
        double floatVal = Get<double>("FloatField");
        if (floatVal > 2.70 && floatVal < 2.72)
        {
            GD.Print("[export_test] PASS: float field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: float field set/get");
        }

        // 验证 bool
        Set("BoolField", false);
        bool boolVal = Get<bool>("BoolField");
        if (boolVal == false && BoolField == false)
        {
            GD.Print("[export_test] PASS: bool field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: bool field set/get");
        }

        // 验证 string
        Set("StringField", "world");
        string strVal = Get<string>("StringField");
        if (strVal == "world" && StringField == "world")
        {
            GD.Print("[export_test] PASS: string field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: string field set/get");
        }

        // 验证 Vector2
        Set("Vec2Field", new Vector2(10, 20));
        Vector2 v2 = Get<Vector2>("Vec2Field");
        if (v2.x == 10 && v2.y == 20)
        {
            GD.Print("[export_test] PASS: Vector2 field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: Vector2 field set/get");
        }

        // 验证 Color
        Set("ColorField", new Color(0, 1, 0, 1));
        Color c = Get<Color>("ColorField");
        if (c.r == 0 && c.g == 1 && c.b == 0)
        {
            GD.Print("[export_test] PASS: Color field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: Color field set/get");
        }

        // 验证枚举（底层 int，封送为 int64）
        Set("Mode", (int)MoveMode.Fly);
        long enumVal = Get<long>("Mode");
        if (enumVal == (int)MoveMode.Fly && Mode == MoveMode.Fly)
        {
            GD.Print("[export_test] PASS: enum field set/get");
        }
        else
        {
            GD.Print("[export_test] FAIL: enum field set/get");
        }

        // 验证属性列表可获取（冒烟级：Dictionary 元素暂无法封送到 C#，只能验证列表非空，
        // 逐条读取 name 需要 P3 的 Dictionary 封送支持）
        object[] props = Call<object[]>("get_property_list");
        if (props != null && props.Length > 0)
        {
            GD.Print("[export_test] PASS: get_property_list returns non-empty list");
        }
        else
        {
            GD.Print("[export_test] FAIL: get_property_list returned null or empty");
        }

        GD.Print("[export_test] Test complete");
        GetTree().Quit();
    }
}
