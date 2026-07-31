using Godot;
using System;
using System.Collections.Generic;

public partial class Main : Control
{
	private const int GRID = 4;

	// 热更新演示：APP 启动 8 秒后自动从 res://048_v2.dll 复制到 user://048.dll，
	// 写 user://reload.trigger 触发热更新，重载场景让 v2 class 生效。
	// 用 res:// + user:// 绕过 Android scoped storage 对 /sdcard/ 的限制。
	// v1: 红色标题 + "Arrow keys or swipe to move tiles"
	// v2: 绿色标题 + "v2 HOT-RELOADED - swipe to move"（验证热更后可见）
	private const string VERSION = "v1";
	private const string HOTRELOAD_TRIGGER = "user://reload.trigger";
	private const string USER_DLL_PATH = "user://048.dll";
	private const string V2_DLL_RES = "res://048_v2.dll";
	private const double AUTO_RELOAD_DELAY = 8.0;

	private int[] board;
	private int score;
	private int best;
	private bool gameOver;

	// 触摸滑动状态：按下时记录起点，抬起时计算方向并触发 Move
	private float touchStartX = 0;
	private float touchStartY = 0;
	private bool touchActive = false;

	private static readonly Color BOARD_BG_COLOR = new Color(0.71f, 0.65f, 0.60f);
	private static readonly Color EMPTY_CELL_COLOR = new Color(0.78f, 0.73f, 0.68f);
	private static readonly Color TEXT_LIGHT = new Color(0.98f, 0.96f, 0.93f);
	private static readonly Color TEXT_DARK = new Color(0.47f, 0.43f, 0.39f);
	private static readonly Color BG_COLOR = new Color(0.73f, 0.68f, 0.63f);
	private static readonly Color SCORE_BG_COLOR = new Color(0.66f, 0.6f, 0.54f);
	private static readonly Color SCORE_CAPTION_COLOR = new Color(0.93f, 0.89f, 0.85f);
	private static readonly Color SCORE_NUM_COLOR = new Color(1f, 1f, 1f);
	private static readonly Color GAMEOVER_BG_COLOR = new Color(0.93f, 0.89f, 0.85f, 0.85f);

	private static readonly Color[] TILE_BG = {
		new Color(0.93f, 0.89f, 0.85f),
		new Color(0.93f, 0.88f, 0.78f),
		new Color(0.95f, 0.69f, 0.47f),
		new Color(0.96f, 0.58f, 0.39f),
		new Color(0.97f, 0.49f, 0.37f),
		new Color(0.97f, 0.37f, 0.23f),
		new Color(0.93f, 0.81f, 0.45f),
		new Color(0.93f, 0.80f, 0.38f),
		new Color(0.93f, 0.78f, 0.31f),
		new Color(0.93f, 0.77f, 0.25f),
		new Color(0.93f, 0.76f, 0.18f),
	};

	private const float PAD_RATIO = 0.035f;
	private const float CELL_RATIO = (1f - 5f * PAD_RATIO) / 4f;

	private Label scoreLabel;
	private Label bestLabel;
	private ColorRect gameOverPanel;
	private Control boardParent;
	private ColorRect[] tileRects;
	private Label[] tileLabels;

	// 自动热更新状态：v1 启动 AUTO_RELOAD_DELAY 秒后从 res://048_v2.dll 复制并触发
	private double autoReloadTimer = 0;
	private bool autoReloadStarted = false;

	private static int GetTileIndex(int val)
	{
		int idx = 0;
		int v = val;
		while (v > 2)
		{
			v = v / 2;
			idx++;
		}
		return idx;
	}

	public override void _Ready()
	{
		GD.Print(GD.Concat("[2048] _Ready started ", VERSION, " (hot reload demo)"));

		// WASM 下实例字段初始化器不执行，需在 _Ready 中手动初始化
		board = new int[GRID * GRID];
		tileRects = new ColorRect[GRID * GRID];
		tileLabels = new Label[GRID * GRID];

		// 关键修复：Main 作为 Control 默认 MouseFilter=Stop 会消费触摸事件，
		// 导致 _UnhandledInput 收不到触摸。设为 Ignore 让事件穿透到 unhandled_input 通道。
		this.MouseFilter = MouseFilterEnum.Ignore;

		// Background - FullRect cream
		var bg = new ColorRect();
		bg.Color = BG_COLOR;
		bg.MouseFilter = MouseFilterEnum.Ignore;
		bg.SetAnchorsPreset(LayoutPreset.FullRect);
		AddChild(bg);

		// VBox container - FullRect with margins
		var vbox = new VBoxContainer();
		vbox.SetAnchorsPreset(LayoutPreset.FullRect);
		vbox.OffsetLeft = 16;
		vbox.OffsetTop = 24;
		vbox.OffsetRight = -16;
		vbox.OffsetBottom = -24;
		vbox.MouseFilter = MouseFilterEnum.Ignore;
		AddChild(vbox);

		// Title "2048" + version (v1: 红色标题; v2 热更后变绿色)
		var title = new Label();
		title.Text = GD.Concat("2048 ", VERSION);
		title.HorizontalAlignment = 1;
		title.AddThemeFontSizeOverride("font_size", 48);
		title.AddThemeColorOverride("font_color", new Color(0.9f, 0.2f, 0.2f));
		title.MouseFilter = MouseFilterEnum.Ignore;
		vbox.AddChild(title);

		// Score row (HBox)
		var scoreRow = new HBoxContainer();
		scoreRow.SizeFlagsHorizontal = 3;
		scoreRow.AddThemeConstantOverride("separation", 12);
		scoreRow.MouseFilter = MouseFilterEnum.Ignore;
		vbox.AddChild(scoreRow);

		BuildScoreBox(scoreRow, "SCORE", out scoreLabel);
		BuildScoreBox(scoreRow, "BEST", out bestLabel);

		// Board parent (Control)
		boardParent = new Control();
		boardParent.CustomMinimumSize = new Vector2(340, 340);
		boardParent.SizeFlagsHorizontal = 3;
		boardParent.SizeFlagsVertical = 3;
		boardParent.MouseFilter = MouseFilterEnum.Ignore;
		vbox.AddChild(boardParent);

		// Tip label
		var tip = new Label();
		tip.Text = "Arrow keys or swipe to move tiles";
		tip.HorizontalAlignment = 1;
		tip.AddThemeFontSizeOverride("font_size", 16);
		tip.AddThemeColorOverride("font_color", TEXT_DARK);
		tip.MouseFilter = MouseFilterEnum.Ignore;
		vbox.AddChild(tip);

		// GameOver panel (FullRect overlay, initially hidden)
		gameOverPanel = new ColorRect();
		gameOverPanel.Color = GAMEOVER_BG_COLOR;
		gameOverPanel.Visible = false;
		gameOverPanel.SetAnchorsPreset(LayoutPreset.FullRect);
		gameOverPanel.MouseFilter = MouseFilterEnum.Ignore;
		AddChild(gameOverPanel);

		var gameOverVBox = new VBoxContainer();
		gameOverVBox.SetAnchorsPreset(LayoutPreset.Center);
		gameOverVBox.OffsetLeft = -140;
		gameOverVBox.OffsetTop = -60;
		gameOverVBox.OffsetRight = 140;
		gameOverVBox.OffsetBottom = 60;
		gameOverVBox.AddThemeConstantOverride("separation", 8);
		gameOverVBox.MouseFilter = MouseFilterEnum.Ignore;
		gameOverPanel.AddChild(gameOverVBox);

		var gameOverText = new Label();
		gameOverText.Text = "Game Over!";
		gameOverText.HorizontalAlignment = 1;
		gameOverText.AddThemeFontSizeOverride("font_size", 40);
		gameOverText.AddThemeColorOverride("font_color", TEXT_DARK);
		gameOverText.MouseFilter = MouseFilterEnum.Ignore;
		gameOverVBox.AddChild(gameOverText);

		var restartText = new Label();
		restartText.Text = "Tap or press Enter to restart";
		restartText.HorizontalAlignment = 1;
		restartText.AddThemeFontSizeOverride("font_size", 18);
		restartText.AddThemeColorOverride("font_color", TEXT_DARK);
		restartText.MouseFilter = MouseFilterEnum.Ignore;
		gameOverVBox.AddChild(restartText);

		GD.Print("[2048] UI built OK");

		BuildBoard();
		NewGame();
		GD.Print("[2048] Game initialized");
	}

	private void BuildScoreBox(HBoxContainer parent, string caption, out Label valueLabel)
	{
		var box = new ColorRect();
		box.Color = SCORE_BG_COLOR;
		box.CustomMinimumSize = new Vector2(140, 58);
		box.SizeFlagsHorizontal = 3;
		box.MouseFilter = MouseFilterEnum.Ignore;
		parent.AddChild(box);

		var captionLabel = new Label();
		captionLabel.Text = caption;
		captionLabel.HorizontalAlignment = 1;
		captionLabel.AddThemeFontSizeOverride("font_size", 14);
		captionLabel.AddThemeColorOverride("font_color", SCORE_CAPTION_COLOR);
		captionLabel.SetAnchorsPreset(LayoutPreset.FullRect);
		captionLabel.OffsetTop = 4;
		captionLabel.OffsetBottom = -26;
		captionLabel.MouseFilter = MouseFilterEnum.Ignore;
		box.AddChild(captionLabel);

		valueLabel = new Label();
		valueLabel.Text = "0";
		valueLabel.HorizontalAlignment = 1;
		valueLabel.AddThemeFontSizeOverride("font_size", 26);
		valueLabel.AddThemeColorOverride("font_color", SCORE_NUM_COLOR);
		valueLabel.SetAnchorsPreset(LayoutPreset.FullRect);
		valueLabel.OffsetTop = 20;
		valueLabel.MouseFilter = MouseFilterEnum.Ignore;
		box.AddChild(valueLabel);
	}

	public override void _Process(double delta)
	{
		// 热更新轮询：检测 user://reload.trigger 文件是否存在
		// 触发来源：① DoAutoReload 自动写入；② 外部 adb push（如有权限）
		if (FileAccess.FileExists(HOTRELOAD_TRIGGER))
		{
			GD.Print(GD.Concat("[2048] Hot reload triggered, VERSION=", VERSION));
			// 删除触发文件（避免循环触发）
			FileAccess.Remove(HOTRELOAD_TRIGGER);
			// 加载新 DLL + reload_all_scripts
			bool ok = GD.HotReloadAssembly(USER_DLL_PATH);
			GD.Print(GD.Concat("[2048] HotReloadAssembly result: ", ok ? "true" : "false"));
			// 重载当前场景，让新 class 实例化生效。
			// ChangeSceneToFile 内部用 call_deferred，在 _Process 中调用安全。
			GetTree().ChangeSceneToFile("res://Main.tscn");
			return;
		}

		// 自动热更新：v1 启动 AUTO_RELOAD_DELAY 秒后，从 res://048_v2.dll 复制到 user://
		// 并写 trigger 触发热更新。autoReloadStarted 防止重复触发。
		if (!autoReloadStarted && VERSION == "v1")
		{
			autoReloadTimer += delta;
			if (autoReloadTimer > AUTO_RELOAD_DELAY)
			{
				autoReloadStarted = true;
				DoAutoReload();
				return;
			}
		}

		if (gameOver)
		{
			if (Input.IsActionJustPressed("ui_accept"))
			{
				NewGame();
			}
			return;
		}

		if (Input.IsActionJustPressed("ui_up")) { Move(0); }
		else if (Input.IsActionJustPressed("ui_down")) { Move(1); }
		else if (Input.IsActionJustPressed("ui_left")) { Move(2); }
		else if (Input.IsActionJustPressed("ui_right")) { Move(3); }
	}

	// 触摸滑动处理：_UnhandledInput 在 GUI 系统之后接收事件。
	// 已在 _Ready 中将 Main 及所有子 Control 的 MouseFilter 设为 Ignore，
	// 确保触摸事件不被 GUI 消费，能到达 unhandled_input 通道。
	public override void _UnhandledInput(InputEvent @event)
	{
		if (@event == null || gameOver)
		{
			return;
		}
		string className = @event.GetClass();
		// 只处理触摸和鼠标按键事件，过滤掉 drag/move 等
		if (className != "InputEventScreenTouch" && className != "InputEventMouseButton")
		{
			return;
		}

		bool pressed = @event.Get<bool>("pressed");
		Vector2 pos = @event.Get<Vector2>("position");
		GD.Print(GD.Concat("[2048] touch pressed=", pressed ? "1" : "0",
			" x=", GD.ToString((int)pos.x), " y=", GD.ToString((int)pos.y)));

		if (pressed)
		{
			touchStartX = pos.x;
			touchStartY = pos.y;
			touchActive = true;
		}
		else if (touchActive)
		{
			HandleSwipe(pos.x, pos.y);
			touchActive = false;
		}
	}

	// 根据起止点计算滑动方向并触发 Move
	private void HandleSwipe(float endX, float endY)
	{
		float dx = endX - touchStartX;
		float dy = endY - touchStartY;
		float absX = dx < 0 ? -dx : dx;
		float absY = dy < 0 ? -dy : dy;

		GD.Print(GD.Concat("[2048] swipe dx=", GD.ToString((int)dx), " dy=", GD.ToString((int)dy)));

		// 太短的滑动忽略（避免误触）
		if (absX < 30 && absY < 30)
		{
			return;
		}

		if (absX > absY)
		{
			if (dx > 0) { Move(3); } // right
			else { Move(2); }        // left
		}
		else
		{
			if (dy > 0) { Move(1); } // down
			else { Move(0); }        // up
		}
	}

	// 自动热更新：从 res://048_v2.dll 读字节，写到 user://048.dll，再写 trigger。
	// 用 res://（APK 内资源）和 user://（APP 内部存储）绕过 Android scoped storage。
	private void DoAutoReload()
	{
		GD.Print(GD.Concat("[2048] Auto-reload starting after ", GD.ToString((int)AUTO_RELOAD_DELAY), "s, checking ", V2_DLL_RES));
		if (!FileAccess.FileExists(V2_DLL_RES))
		{
			GD.Print(GD.Concat("[2048] Auto-reload: ", V2_DLL_RES, " NOT FOUND in APK"));
			return;
		}
		byte[] v2Bytes = FileAccess.GetFileAsBytes(V2_DLL_RES);
		if (v2Bytes == null || v2Bytes.Length == 0)
		{
			GD.Print("[2048] Auto-reload: v2 dll empty or null");
			return;
		}
		GD.Print(GD.Concat("[2048] Auto-reload: v2 dll size=", GD.ToString(v2Bytes.Length)));
		Error writeErr = FileAccess.WriteFile(USER_DLL_PATH, v2Bytes);
		GD.Print(GD.Concat("[2048] Auto-reload: write to ", USER_DLL_PATH, " err=", GD.ToString((int)writeErr)));
		if (writeErr != Error.OK)
		{
			return;
		}
		Error triggerErr = FileAccess.WriteString(HOTRELOAD_TRIGGER, "auto");
		GD.Print(GD.Concat("[2048] Auto-reload: trigger write err=", GD.ToString((int)triggerErr)));
	}

	private void BuildBoard()
	{
		var bg = new ColorRect();
		bg.Color = BOARD_BG_COLOR;
		bg.MouseFilter = MouseFilterEnum.Ignore;
		bg.SetAnchorsPreset(LayoutPreset.FullRect);
		boardParent.AddChild(bg);

		for (int r = 0; r < GRID; r++)
		{
			for (int c = 0; c < GRID; c++)
			{
				float aL = PAD_RATIO + c * (CELL_RATIO + PAD_RATIO);
				float aT = PAD_RATIO + r * (CELL_RATIO + PAD_RATIO);
				float aR = aL + CELL_RATIO;
				float aB = aT + CELL_RATIO;

				int idx = r * GRID + c;

				var cellBg = new ColorRect();
				cellBg.Color = EMPTY_CELL_COLOR;
				cellBg.MouseFilter = MouseFilterEnum.Ignore;
				cellBg.AnchorLeft = aL;
				cellBg.AnchorTop = aT;
				cellBg.AnchorRight = aR;
				cellBg.AnchorBottom = aB;
				boardParent.AddChild(cellBg);
				tileRects[idx] = cellBg;

				var lbl = new Label();
				lbl.Text = "";
				lbl.HorizontalAlignment = 1;
				lbl.VerticalAlignment = 1;
				lbl.MouseFilter = MouseFilterEnum.Ignore;
				lbl.AnchorLeft = aL;
				lbl.AnchorTop = aT;
				lbl.AnchorRight = aR;
				lbl.AnchorBottom = aB;
				lbl.AddThemeFontSizeOverride("font_size", 28);
				boardParent.AddChild(lbl);
				tileLabels[idx] = lbl;
			}
		}
	}

	private void NewGame()
	{
		for (int i = 0; i < GRID * GRID; i++)
			board[i] = 0;
		score = 0;
		gameOver = false;
		gameOverPanel.Visible = false;
		SpawnTile();
		SpawnTile();
		UpdateUI();
	}

	private void SpawnTile()
	{
		// WASM 下 List<int> 不可用，改用固定数组收集空位
		int[] emptyBuf = new int[GRID * GRID];
		int emptyCount = 0;
		for (int i = 0; i < GRID * GRID; i++)
		{
			if (board[i] == 0)
			{
				emptyBuf[emptyCount] = i;
				emptyCount++;
			}
		}
		if (emptyCount == 0) return;
		int idx = (int)(GD.Randi() % (uint)emptyCount);
		int pos = emptyBuf[idx];
		board[pos] = (GD.Randi() % 10) < 9 ? 2 : 4;
	}

	private void UpdateUI()
	{
		// WASM 下 int.ToString() 会触发签名不匹配，改用 GD.ToString(int)
		scoreLabel.Text = GD.ToString(score);
		bestLabel.Text = GD.ToString(best);

		for (int i = 0; i < GRID * GRID; i++)
		{
			int val = board[i];
			var rect = tileRects[i];
			var lbl = tileLabels[i];

			if (val == 0)
			{
				rect.Color = EMPTY_CELL_COLOR;
				lbl.Text = "";
				continue;
			}

			int idx = GetTileIndex(val);
			if (idx < 0) idx = 0;
			if (idx >= TILE_BG.Length) idx = TILE_BG.Length - 1;
			rect.Color = TILE_BG[idx];

			bool darkText = val <= 4;
			int fontSize = val < 100 ? 32 : val < 1000 ? 26 : 22;

			lbl.Text = GD.ToString(val);
			lbl.AddThemeFontSizeOverride("font_size", fontSize);
			lbl.AddThemeColorOverride("font_color", darkText ? TEXT_DARK : TEXT_LIGHT);
		}
	}

	private void Move(int dir)
	{
		if (gameOver) return;
		bool moved = false;
		bool[] merged = new bool[GRID * GRID];

		int rStart = 0, rEnd = GRID, rStep = 1;
		int cStart = 0, cEnd = GRID, cStep = 1;

		switch (dir)
		{
			case 0: rStart = 1; rEnd = GRID; rStep = 1; break;
			case 1: rStart = GRID - 2; rEnd = -1; rStep = -1; break;
			case 2: cStart = 1; cEnd = GRID; cStep = 1; break;
			case 3: cStart = GRID - 2; cEnd = -1; cStep = -1; break;
		}

		for (int r = rStart; r != rEnd; r += rStep)
		{
			for (int c = cStart; c != cEnd; c += cStep)
			{
				int idx = r * GRID + c;
				if (board[idx] == 0) continue;
				int nr = r, nc = c;
				while (true)
				{
					int tr = nr + (dir == 0 ? -1 : dir == 1 ? 1 : 0);
					int tc = nc + (dir == 2 ? -1 : dir == 3 ? 1 : 0);
					if (tr < 0 || tr >= GRID || tc < 0 || tc >= GRID) break;
					int tidx = tr * GRID + tc;
					if (board[tidx] == 0) { nr = tr; nc = tc; }
					else if (board[tidx] == board[idx] && !merged[tidx])
					{
						nr = tr; nc = tc; merged[tidx] = true; break;
					}
					else break;
				}
				int nidx = nr * GRID + nc;
				if (nidx != idx)
				{
					if (merged[nidx])
					{
						board[nidx] *= 2;
						score += board[nidx];
						if (score > best) best = score;
						board[idx] = 0;
					}
					else
					{
						board[nidx] = board[idx];
						board[idx] = 0;
					}
					moved = true;
				}
			}
		}

		if (moved)
		{
			SpawnTile();
			UpdateUI();
			CheckGameOver();
		}
	}

	private void CheckGameOver()
	{
		for (int i = 0; i < GRID * GRID; i++)
			if (board[i] == 0) return;
		for (int r = 0; r < GRID; r++)
		{
			for (int c = 0; c < GRID; c++)
			{
				int idx = r * GRID + c;
				int v = board[idx];
				if (r + 1 < GRID && board[(r + 1) * GRID + c] == v) return;
				if (c + 1 < GRID && board[r * GRID + c + 1] == v) return;
			}
		}
		gameOver = true;
		gameOverPanel.Visible = true;
	}
}
