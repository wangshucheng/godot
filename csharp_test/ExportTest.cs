using Godot;

// P1/P2/P3 验证脚本：[Export] 属性 + Inspector 显示 + [GlobalClass] 全局类 + [Signal] 信号
// [GlobalClass] 使脚本出现在"添加节点"对话框中
// [Tool] 使脚本在编辑器中运行（验证 is_tool() 返回 true）
// [Signal] 使 HealthChanged 出现在编辑器信号面板中
// 挂载到 Node 上，Inspector 应显示以下可编辑属性：
//   - Speed (int, 默认 0)
//   - Health (float, 默认 0)
//   - PlayerName (string, 默认 "")
//   - IsActive (bool, 默认 false)
//   - StartPosition (Vector2, 默认 (0,0))
//   - Tint (Color, 默认 黑色)
[GlobalClass]
[Tool]
public partial class ExportTest : Node
{
	// P3: [Signal] delegate — name must end with "EventHandler".
	// The signal name is "HealthChanged" (suffix stripped).
	[Signal]
	public delegate void HealthChangedEventHandler(int newValue);

	[Export] public int Speed = 100;
	[Export] public float Health = 50.5f;
	[Export] public string PlayerName = "Hero";
	[Export] public bool IsActive = true;
	[Export] public Vector2 StartPosition = new Vector2(10, 20);
	[Export] public Color Tint = new Color(1, 0, 0, 1);

	public override void _Ready()
	{
		// 空实现：避免 Mono WASM _Ready 签名不匹配问题
	}

	public override void _Process(double delta)
	{
		// 空实现：[Tool] 脚本在编辑器中运行，不需要做任何事
	}
}
