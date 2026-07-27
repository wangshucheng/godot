using System;

namespace Godot
{
    /// <summary>
    /// 标记 C# 字段为导出属性，使其在检查器中可编辑并在场景序列化时保存。
    /// 仅对实例字段生效；C++ 侧通过源码扫描 + Mono 反射识别。
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public class ExportAttribute : Attribute
    {
        public PropertyHint Hint { get; }
        public string HintString { get; }

        public ExportAttribute(PropertyHint hint = PropertyHint.None, string hintString = "")
        {
            Hint = hint;
            HintString = hintString;
        }
    }

    /// <summary>
    /// 标记 C# 委托为 Godot 信号声明。
    /// 约定: 委托名以 "EventHandler" 结尾时自动去除后缀，PascalCase 转 snake_case 得到信号名。
    /// 例如: [Signal] delegate void MySignalEventHandler(int x); → 信号名 "my_signal"
    /// </summary>
    [AttributeUsage(AttributeTargets.Delegate)]
    public class SignalAttribute : Attribute
    {
    }

    /// <summary>
    /// 标记 C# 类为工具脚本（类似 GDScript 的 @tool），使其在编辑器中运行。
    /// </summary>
    [AttributeUsage(AttributeTargets.Class)]
    public class ToolAttribute : Attribute
    {
    }

    /// <summary>
    /// 属性提示类型，对应 Godot 的 PropertyHint 枚举。
    /// 仅列出常用值，与 C++ 侧 PropertyInfo::hint 配合使用。
    /// </summary>
    public enum PropertyHint
    {
        None = 0,
        Range = 1,
        Enum = 2,
        Flags = 3,
        File = 4,
        Dir = 5,
        ColorNoAlpha = 6,
        MultilineText = 7,
        PlaceholderText = 8,
    }
}
