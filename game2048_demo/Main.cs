using Godot;
using System;

namespace Game2048Demo
{
    /// <summary>
    /// 2048 Android demo 主脚本。
    /// 所有UI均通过纯C#代码动态创建（无.tscn UI依赖），演示纯C#代码热更新。
    ///
    /// 版本号内嵌在代码中，热更新后版本号变化即可验证新代码已加载。
    /// </summary>
    public partial class Main : Control
    {
        // ★★★ 热更新版本号 - 修改此值+重新编译即可验证热更新 ★★★
        public const string AppVersion = "v2.0 (HOT UPDATE!)";

        private Game2048 _game;
        private Label[,] _tileLabels;
        private ColorRect[,] _tileBgs;
        private Label _scoreLabel;
        private Label _versionLabel;
        private Label _statusLabel;
        private Button _newGameBtn;
        private Button _checkUpdateBtn;

        // 触摸滑动检测
        private Vector2 _touchStart;
        private bool _touchActive;

        // 退出计时器（热更新后延迟退出）
        private double _quitTimer = -1.0;

        // UI 布局常量
        private const float TileSize = 140.0f;
        private const float TileSpacing = 12.0f;
        private const float BoardPadding = 16.0f;

        // Godot SizeFlags 常量（自定义 GodotSharp 无枚举，使用整数值）
        private const int SizeShrinkCenter = 4;
        private const int SizeExpandFill = 3;
        // Godot LayoutPreset.FullRect = 15
        private const int PresetFullRect = 15;
        // HorizontalAlignment.Center = 1
        private const int AlignCenter = 1;

        public override void _Ready()
        {
            GD.Print("[Game2048] Starting 2048 Demo - " + AppVersion);

            // Runtime.GetUserDataDir 在当前自定义绑定中因 icall 缺失不可用，
            // 用平台硬编码 fallback 替代（见 HotUpdater.GetUserDataDirSafe）
            try
            {
                GD.Print("[Game2048] User data dir: " + Runtime.GetUserDataDir());
            }
            catch (Exception)
            {
                GD.Print("[Game2048] User data dir: (Runtime.GetUserDataDir unavailable, using fallback)");
            }

            _game = new Game2048();
            BuildUi();
            _game.NewGame();
            RefreshUi();

            // 启动时自动检查热更新 — 包大 try-catch，避免因 Mono BCL 不同步异常静默中断
            try
            {
                GD.Print("[Game2048] Calling AutoCheckUpdate()...");
                AutoCheckUpdate();
                GD.Print("[Game2048] AutoCheckUpdate() returned normally");
            }
            catch (Exception e)
            {
                GD.PrintErr("[Game2048] AutoCheckUpdate() threw exception: " + e.Message);
                GD.PrintErr("[Game2048] Stack: " + e.StackTrace);
                _statusLabel.Set("text", "Update check error");
            }
        }

        public override void _Process(double delta)
        {
            if (_quitTimer > 0)
            {
                _quitTimer -= delta;
                if (_quitTimer <= 0)
                {
                    _quitTimer = -1.0;
                    GD.Print("[HotUpdate] Quitting for restart...");
                    GetTree().Quit();
                }
            }
        }

        /// <summary>
        /// 纯C#动态创建所有UI元素（Label/Button/ColorRect）。
        /// 使用 Call()/Set() 方法操作 Godot 属性（自定义 GodotSharp 绑定方式）。
        /// </summary>
        private void BuildUi()
        {
            // 根容器：垂直布局
            var rootVBox = new VBoxContainer();
            rootVBox.Call("set_anchors_preset", PresetFullRect);
            rootVBox.Set("offset_left", 20f);
            rootVBox.Set("offset_top", 20f);
            rootVBox.Set("offset_right", -20f);
            rootVBox.Set("offset_bottom", -20f);
            rootVBox.Call("add_theme_constant_override", "separation", 16);
            AddChild(rootVBox);

            // 顶部：标题 + 版本
            var headerHBox = new HBoxContainer();
            headerHBox.Call("add_theme_constant_override", "separation", 12);
            rootVBox.AddChild(headerHBox);

            var titleLabel = new Label();
            titleLabel.Set("text", "2048");
            titleLabel.Call("add_theme_font_size_override", "font_size", 48);
            titleLabel.Call("add_theme_color_override", "font_color", new Color(0.8f, 0.7f, 0.6f));
            headerHBox.AddChild(titleLabel);

            _versionLabel = new Label();
            _versionLabel.Set("text", AppVersion);
            _versionLabel.Call("add_theme_font_size_override", "font_size", 16);
            _versionLabel.Call("add_theme_color_override", "font_color", new Color(0.5f, 0.5f, 0.5f));
            _versionLabel.Set("size_flags_vertical", SizeShrinkCenter);
            headerHBox.AddChild(_versionLabel);

            // 分数行
            var scoreHBox = new HBoxContainer();
            scoreHBox.Call("add_theme_constant_override", "separation", 8);
            rootVBox.AddChild(scoreHBox);

            var scoreTitleLabel = new Label();
            scoreTitleLabel.Set("text", "Score:");
            scoreTitleLabel.Call("add_theme_font_size_override", "font_size", 28);
            scoreHBox.AddChild(scoreTitleLabel);

            _scoreLabel = new Label();
            _scoreLabel.Set("text", "0");
            _scoreLabel.Call("add_theme_font_size_override", "font_size", 28);
            _scoreLabel.Call("add_theme_color_override", "font_color", new Color(1.0f, 0.9f, 0.4f));
            scoreHBox.AddChild(_scoreLabel);

            // 棋盘容器
            var boardContainer = new Control();
            boardContainer.Set("custom_minimum_size", new Vector2(
                TileSize * 4 + TileSpacing * 5,
                TileSize * 4 + TileSpacing * 5));
            boardContainer.Set("size_flags_horizontal", SizeShrinkCenter);
            rootVBox.AddChild(boardContainer);

            // 棋盘背景
            var boardBg = new ColorRect();
            boardBg.Set("color", new Color(0.2f, 0.18f, 0.16f));
            boardBg.Call("set_anchors_preset", PresetFullRect);
            boardBg.Set("size", new Vector2(
                TileSize * 4 + TileSpacing * 5,
                TileSize * 4 + TileSpacing * 5));
            boardContainer.AddChild(boardBg);

            // 4x4 格子
            _tileLabels = new Label[4, 4];
            _tileBgs = new ColorRect[4, 4];

            for (int r = 0; r < 4; r++)
            {
                for (int c = 0; c < 4; c++)
                {
                    float x = BoardPadding + c * (TileSize + TileSpacing);
                    float y = BoardPadding + r * (TileSize + TileSpacing);

                    var tileBg = new ColorRect();
                    tileBg.Set("color", new Color(0.3f, 0.28f, 0.25f));
                    tileBg.Set("position", new Vector2(x, y));
                    tileBg.Set("size", new Vector2(TileSize, TileSize));
                    boardContainer.AddChild(tileBg);
                    _tileBgs[r, c] = tileBg;

                    var tileLabel = new Label();
                    tileLabel.Set("text", "");
                    tileLabel.Call("add_theme_font_size_override", "font_size", 40);
                    tileLabel.Set("horizontal_alignment", AlignCenter);
                    tileLabel.Set("vertical_alignment", AlignCenter);
                    tileLabel.Set("position", new Vector2(x, y));
                    tileLabel.Set("size", new Vector2(TileSize, TileSize));
                    boardContainer.AddChild(tileLabel);
                    _tileLabels[r, c] = tileLabel;
                }
            }

            // 状态标签
            _statusLabel = new Label();
            _statusLabel.Set("text", "");
            _statusLabel.Call("add_theme_font_size_override", "font_size", 24);
            _statusLabel.Set("horizontal_alignment", AlignCenter);
            _statusLabel.Set("size_flags_horizontal", SizeExpandFill);
            rootVBox.AddChild(_statusLabel);

            // 底部按钮行
            var buttonHBox = new HBoxContainer();
            buttonHBox.Call("add_theme_constant_override", "separation", 12);
            buttonHBox.Set("size_flags_horizontal", SizeShrinkCenter);
            rootVBox.AddChild(buttonHBox);

            _newGameBtn = new Button();
            _newGameBtn.Set("text", "New Game");
            _newGameBtn.Connect("pressed", new Action(OnNewGamePressed));
            buttonHBox.AddChild(_newGameBtn);

            _checkUpdateBtn = new Button();
            _checkUpdateBtn.Set("text", "Check Update");
            _checkUpdateBtn.Connect("pressed", new Action(OnCheckUpdatePressed));
            buttonHBox.AddChild(_checkUpdateBtn);

            // 操作提示
            var hintLabel = new Label();
            hintLabel.Set("text", "Swipe to move tiles. R=New, U=Undo");
            hintLabel.Call("add_theme_font_size_override", "font_size", 14);
            hintLabel.Call("add_theme_color_override", "font_color", new Color(0.5f, 0.5f, 0.5f));
            hintLabel.Set("horizontal_alignment", AlignCenter);
            hintLabel.Set("size_flags_horizontal", SizeExpandFill);
            rootVBox.AddChild(hintLabel);
        }

        /// <summary>
        /// 刷新棋盘UI，从_game读取最新状态。
        /// </summary>
        private void RefreshUi()
        {
            for (int r = 0; r < 4; r++)
            {
                for (int c = 0; c < 4; c++)
                {
                    int val = _game.GetCell(r, c);
                    var label = _tileLabels[r, c];
                    var bg = _tileBgs[r, c];

                    if (val == 0)
                    {
                        label.Set("text", "");
                        bg.Set("color", new Color(0.3f, 0.28f, 0.25f));
                    }
                    else
                    {
                        label.Set("text", val.ToString());
                        bg.Set("color", GetTileColor(val));
                    }
                }
            }
            _scoreLabel.Set("text", _game.Score.ToString());

            if (_game.IsGameOver)
            {
                _statusLabel.Set("text", "Game Over! Tap New Game");
                _statusLabel.Call("add_theme_color_override", "font_color", new Color(1.0f, 0.3f, 0.3f));
            }
            else if (_game.HasWon)
            {
                _statusLabel.Set("text", "You Win! Keep going?");
                _statusLabel.Call("add_theme_color_override", "font_color", new Color(0.3f, 1.0f, 0.3f));
            }
            else
            {
                _statusLabel.Set("text", "");
            }
        }

        /// <summary>
        /// 根据格子数值返回对应颜色（纯C#颜色映射）。
        /// </summary>
        private Color GetTileColor(int val)
        {
            switch (val)
            {
                case 2: return new Color(0.93f, 0.89f, 0.85f);
                case 4: return new Color(0.93f, 0.88f, 0.78f);
                case 8: return new Color(0.95f, 0.69f, 0.47f);
                case 16: return new Color(0.95f, 0.59f, 0.39f);
                case 32: return new Color(0.96f, 0.49f, 0.37f);
                case 64: return new Color(0.96f, 0.37f, 0.23f);
                case 128: return new Color(0.93f, 0.81f, 0.45f);
                case 256: return new Color(0.93f, 0.80f, 0.38f);
                case 512: return new Color(0.93f, 0.78f, 0.31f);
                case 1024: return new Color(0.93f, 0.77f, 0.25f);
                case 2048: return new Color(0.93f, 0.76f, 0.18f);
                default: return new Color(0.5f, 0.3f, 0.6f);
            }
        }

        /// <summary>
        /// 输入处理：键盘 + 触摸滑动。
        /// </summary>
        public override void _Input(InputEvent @event)
        {
            // 键盘输入
            if (@event is InputEventKey keyEvent && keyEvent.Pressed && !keyEvent.Echo)
            {
                bool moved = false;
                long kc = keyEvent.Keycode;
                if (kc == (long)Key.Left || kc == (long)Key.A)
                    moved = _game.Move(0);
                else if (kc == (long)Key.Right || kc == (long)Key.D)
                    moved = _game.Move(1);
                else if (kc == (long)Key.Up || kc == (long)Key.W)
                    moved = _game.Move(2);
                else if (kc == (long)Key.Down || kc == (long)Key.S)
                    moved = _game.Move(3);
                else if (kc == (long)Key.R)
                {
                    _game.NewGame();
                    moved = true;
                }
                else if (kc == (long)Key.U || kc == (long)Key.Z)
                {
                    moved = _game.Undo();
                }

                if (moved) RefreshUi();
            }

            // 触摸滑动（Android 手势）
            if (@event is InputEventScreenTouch touch)
            {
                if (touch.Pressed)
                {
                    _touchStart = touch.Position;
                    _touchActive = true;
                }
                else if (_touchActive)
                {
                    Vector2 delta = touch.Position - _touchStart;
                    _touchActive = false;
                    float absX = Math.Abs(delta.x);
                    float absY = Math.Abs(delta.y);
                    float minSwipe = 30.0f;

                    if (absX >= minSwipe || absY >= minSwipe)
                    {
                        bool moved;
                        if (absX > absY)
                            moved = _game.Move(delta.x > 0 ? 1 : 0);
                        else
                            moved = _game.Move(delta.y > 0 ? 3 : 2);
                        if (moved) RefreshUi();
                    }
                }
            }

            // 鼠标拖拽（桌面测试用）
            if (@event is InputEventMouseButton mouseBtn)
            {
                if (mouseBtn.Pressed && mouseBtn.ButtonIndex == (long)MouseButton.Left)
                {
                    _touchStart = mouseBtn.Position;
                    _touchActive = true;
                }
                else if (!mouseBtn.Pressed && mouseBtn.ButtonIndex == (long)MouseButton.Left && _touchActive)
                {
                    Vector2 delta = mouseBtn.Position - _touchStart;
                    _touchActive = false;
                    float absX = Math.Abs(delta.x);
                    float absY = Math.Abs(delta.y);
                    float minSwipe = 30.0f;

                    if (absX >= minSwipe || absY >= minSwipe)
                    {
                        bool moved;
                        if (absX > absY)
                            moved = _game.Move(delta.x > 0 ? 1 : 0);
                        else
                            moved = _game.Move(delta.y > 0 ? 3 : 2);
                        if (moved) RefreshUi();
                    }
                }
            }
        }

        private void OnNewGamePressed()
        {
            _game.NewGame();
            RefreshUi();
        }

        private void OnCheckUpdatePressed()
        {
            string updatePath = HotUpdater.CheckForUpdate();
            if (updatePath != null)
            {
                GD.Print("[HotUpdate] Update found! Applying...");
                _statusLabel.Set("text", "Applying update...");
                _statusLabel.Call("add_theme_color_override", "font_color", new Color(0.3f, 0.8f, 1.0f));

                if (HotUpdater.ApplyUpdate(updatePath))
                {
                    HotUpdater.CleanUpUpdateSource();
                    _statusLabel.Set("text", "Update staged! Restarting...");
                    GD.Print("[HotUpdate] Update staged. Restarting app.");

                    // 延迟1秒退出，让用户看到消息
                    _quitTimer = 1.0;
                }
                else
                {
                    _statusLabel.Set("text", "Update failed! Check logs.");
                }
            }
            else
            {
                _statusLabel.Set("text", "No update. Push DLL to /sdcard/Android/data/org.godotengine.csharp_test/files/update/");
                _statusLabel.Call("add_theme_color_override", "font_color", new Color(0.5f, 0.5f, 0.5f));
                GD.Print("[HotUpdate] No update file found.");
                GD.Print("[HotUpdate] To test: adb push Game2048Demo.dll /sdcard/Android/data/org.godotengine.csharp_test/files/update/Game2048Demo.dll");
            }
        }

        /// <summary>
        /// 启动时自动检查热更新（静默模式）。
        /// 如果检测到更新文件，自动应用并提示重启。
        /// </summary>
        private void AutoCheckUpdate()
        {
            try
            {
                GD.Print("[HotUpdate] AutoCheckUpdate: calling HotUpdater.CheckForUpdate()");
                string updatePath = HotUpdater.CheckForUpdate();
                GD.Print("[HotUpdate] AutoCheckUpdate: CheckForUpdate returned = " + (updatePath ?? "null"));
                if (updatePath != null)
                {
                    GD.Print("[HotUpdate] Auto-applying update on startup...");
                    if (HotUpdater.ApplyUpdate(updatePath))
                    {
                        GD.Print("[HotUpdate] Apply succeeded, cleaning up source");
                        HotUpdater.CleanUpUpdateSource();
                        _statusLabel.Set("text", "Update ready! Restarting...");
                        _statusLabel.Call("add_theme_color_override", "font_color", new Color(0.3f, 0.8f, 1.0f));

                        // 自动重启（延迟1.5秒）
                        _quitTimer = 1.5;
                    }
                    else
                    {
                        GD.Print("[HotUpdate] ApplyUpdate returned false (staging failed)");
                        _statusLabel.Set("text", "Update apply failed");
                    }
                }
            }
            catch (Exception e)
            {
                GD.PrintErr("[HotUpdate] AutoCheckUpdate inner exception: " + e.Message);
                GD.PrintErr("[HotUpdate] Stack: " + e.StackTrace);
                _statusLabel.Set("text", "Update check error");
            }
        }
    }
}
