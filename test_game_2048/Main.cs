using Godot;
using System;
using System.Collections.Generic;

public partial class Main : Control
{
	private const int GRID = 4;
	private int[] board;
	private int score;
	private int best;
	private bool gameOver;

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
		GD.Print("[2048] _Ready started (dynamic build)");

		// WASM 下实例字段初始化器不执行，需在 _Ready 中手动初始化
		board = new int[GRID * GRID];
		tileRects = new ColorRect[GRID * GRID];
		tileLabels = new Label[GRID * GRID];

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
		vbox.MouseFilter = MouseFilterEnum.Pass;
		AddChild(vbox);

		// Title "2048"
		var title = new Label();
		title.Text = "2048";
		title.HorizontalAlignment = 1;
		title.AddThemeFontSizeOverride("font_size", 48);
		title.AddThemeColorOverride("font_color", TEXT_DARK);
		title.MouseFilter = MouseFilterEnum.Ignore;
		vbox.AddChild(title);

		// Score row (HBox)
		var scoreRow = new HBoxContainer();
		scoreRow.SizeFlagsHorizontal = 3;
		scoreRow.AddThemeConstantOverride("separation", 12);
		scoreRow.MouseFilter = MouseFilterEnum.Pass;
		vbox.AddChild(scoreRow);

		BuildScoreBox(scoreRow, "SCORE", out scoreLabel);
		BuildScoreBox(scoreRow, "BEST", out bestLabel);

		// Board parent (Control)
		boardParent = new Control();
		boardParent.CustomMinimumSize = new Vector2(340, 340);
		boardParent.SizeFlagsHorizontal = 3;
		boardParent.SizeFlagsVertical = 3;
		boardParent.MouseFilter = MouseFilterEnum.Pass;
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
