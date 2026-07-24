using Godot;
using System;

// ============================================================
// Game2048 - Enhanced 2048 game integrated with the project's
// Runtime.GameUi* icall UI layer (C++ draws the tiles).
//
// Enhancements over the original version:
//   - Undo (U or Z): one-step undo, restored on every successful move
//   - Mouse drag swipe: WASM/Touch friendly (Input.IsMouseButtonPressed)
//   - Continue after win (C): keep playing past 2048
//   - WASD alternate keys (in addition to arrow keys)
//
// WASM-safe design (AGENTS.md §4.3):
//   - Pure int state, LCG random (no System.Random)
//   - No instance void methods with string params
//   - All UI updates go through Runtime.GameUi* icalls (static, int params)
//   - Edge-triggered input via Input.IsKeyPressed / IsMouseButtonPressed
// ============================================================
public partial class Game2048 : Control
{
    private const int N = 4;

    // Game state (pure int - WASM-safe)
    private int[] _grid = new int[N * N];
    private int _score;
    private bool _gameOver;
    private bool _won;
    private bool _continueAfterWin;
    private bool _uiBuilt;
    private int _frame;

    // Simple LCG random (avoids System.Random WASM issues)
    private int _rngState = 12345;

    // Undo (one-step history)
    private int[] _undoGrid = new int[N * N];
    private int _undoScore;
    private bool _undoValid;

    // Input edge detection
    private bool _prevLeft, _prevRight, _prevUp, _prevDown;
    private bool _prevR, _prevU, _prevZ, _prevC;
    private bool _prevMouseLeft;
    private Vector2 _mouseStart;

    public override void _Process(double delta)
    {
        _frame++;
        if (!_uiBuilt)
        {
            Runtime.GameUiInit();
            InitGame();
            _uiBuilt = true;
            UpdateUi();
            return;
        }

        HandleKeyboard();
        HandleMouseSwipe();
    }

    private void HandleKeyboard()
    {
        bool left = Input.IsKeyPressed(Key.Left) || Input.IsKeyPressed(Key.A);
        bool right = Input.IsKeyPressed(Key.Right) || Input.IsKeyPressed(Key.D);
        bool up = Input.IsKeyPressed(Key.Up) || Input.IsKeyPressed(Key.W);
        bool down = Input.IsKeyPressed(Key.Down) || Input.IsKeyPressed(Key.S);
        bool r = Input.IsKeyPressed(Key.R);
        bool u = Input.IsKeyPressed(Key.U);
        bool z = Input.IsKeyPressed(Key.Z);
        bool c = Input.IsKeyPressed(Key.C);

        bool moved = false;

        if (left && !_prevLeft) moved = TryMove(0);
        else if (right && !_prevRight) moved = TryMove(1);
        else if (up && !_prevUp) moved = TryMove(2);
        else if (down && !_prevDown) moved = TryMove(3);

        if (r && !_prevR)
        {
            InitGame();
            moved = true;
        }
        if ((u && !_prevU) || (z && !_prevZ))
        {
            if (Undo()) moved = true;
        }
        if (c && !_prevC)
        {
            if (_won && !_continueAfterWin)
            {
                _continueAfterWin = true;
                moved = true;
            }
        }

        if (moved) UpdateUi();

        _prevLeft = left; _prevRight = right;
        _prevUp = up; _prevDown = down;
        _prevR = r; _prevU = u; _prevZ = z; _prevC = c;
    }

    private void HandleMouseSwipe()
    {
        bool mouseLeft = Input.IsMouseButtonPressed(MouseButton.Left);
        // Edge: button just pressed -> record start
        if (mouseLeft && !_prevMouseLeft)
        {
            _mouseStart = Input.GetMousePosition();
        }
        // Edge: button just released -> compute swipe
        else if (!mouseLeft && _prevMouseLeft)
        {
            Vector2 end = Input.GetMousePosition();
            float dx = end.x - _mouseStart.x;
            float dy = end.y - _mouseStart.y;
            float absX = dx < 0 ? -dx : dx;
            float absY = dy < 0 ? -dy : dy;
            int minSwipe = 30;
            bool moved = false;
            if (absX >= minSwipe || absY >= minSwipe)
            {
                if (absX > absY)
                    moved = TryMove(dx > 0 ? 1 : 0);
                else
                    moved = TryMove(dy > 0 ? 3 : 2);
            }
            if (moved) UpdateUi();
        }
        _prevMouseLeft = mouseLeft;
    }

    private void InitGame()
    {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                _grid[(int)(r) * N + (int)(c)] = 0;
        _score = 0;
        _gameOver = false;
        _won = false;
        _continueAfterWin = false;
        _undoValid = false;
        AddRandomTile();
        AddRandomTile();
    }

    private int NextRandom()
    {
        // LCG (glibc), masked to non-negative int (WASM-safe)
        _rngState = (int)(((uint)(_rngState * 1103515245) + 12345u) & 0x7FFFFFFFu);
        return _rngState;
    }

    private void AddRandomTile()
    {
        // Count empty cells
        int emptyCount = 0;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (_grid[(int)(r) * N + (int)(c)] == 0) emptyCount++;

        if (emptyCount == 0) return;

        // Pick a random empty cell
        int target = NextRandom() % emptyCount;
        int idx = 0;
        for (int r = 0; r < N; r++)
        {
            for (int c = 0; c < N; c++)
            {
                if (_grid[(int)(r) * N + (int)(c)] == 0)
                {
                    if (idx == target)
                    {
                        // 90% chance of 2, 10% chance of 4
                        _grid[(int)(r) * N + (int)(c)] = (NextRandom() % 10 < 9) ? 2 : 4;
                        return;
                    }
                    idx++;
                }
            }
        }
    }

    private void SaveUndo()
    {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                _undoGrid[(int)(r) * N + (int)(c)] = _grid[(int)(r) * N + (int)(c)];
        _undoScore = _score;
        _undoValid = true;
    }

    private bool Undo()
    {
        if (!_undoValid) return false;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                _grid[(int)(r) * N + (int)(c)] = _undoGrid[(int)(r) * N + (int)(c)];
        _score = _undoScore;
        _undoValid = false;
        _gameOver = false;
        return true;
    }

    // dir: 0=left, 1=right, 2=up, 3=down
    // Returns true if any tile moved
    private bool TryMove(int dir)
    {
        if (_gameOver) return false;
        if (_won && !_continueAfterWin) return false;

        SaveUndo();

        bool moved = DoMove(dir);
        if (moved)
        {
            AddRandomTile();
            if (!_won && HasValue(2048))
            {
                _won = true;
            }
            if (IsGameOver()) _gameOver = true;
        }
        else
        {
            // No-op move: don't consume the undo slot
            _undoValid = false;
        }
        return moved;
    }

    private bool DoMove(int dir)
    {
        bool moved = false;

        if (dir == 0)
        {
            // Left: process each row
            for (int r = 0; r < N; r++)
            {
                int[] line = new int[N];
                for (int c = 0; c < N; c++) line[c] = _grid[(int)(r) * N + (int)(c)];
                bool rowMoved = CompressAndMerge(line);
                if (rowMoved)
                {
                    moved = true;
                    for (int c = 0; c < N; c++) _grid[(int)(r) * N + (int)(c)] = line[c];
                }
            }
        }
        else if (dir == 1)
        {
            // Right: reverse, process, reverse back
            for (int r = 0; r < N; r++)
            {
                int[] line = new int[N];
                for (int c = 0; c < N; c++) line[c] = _grid[(int)(r) * N + (int)(N - 1 - c)];
                bool rowMoved = CompressAndMerge(line);
                if (rowMoved)
                {
                    moved = true;
                    for (int c = 0; c < N; c++) _grid[(int)(r) * N + (int)(N - 1 - c)] = line[c];
                }
            }
        }
        else if (dir == 2)
        {
            // Up: process each column
            for (int c = 0; c < N; c++)
            {
                int[] line = new int[N];
                for (int r = 0; r < N; r++) line[r] = _grid[(int)(r) * N + (int)(c)];
                bool colMoved = CompressAndMerge(line);
                if (colMoved)
                {
                    moved = true;
                    for (int r = 0; r < N; r++) _grid[(int)(r) * N + (int)(c)] = line[r];
                }
            }
        }
        else if (dir == 3)
        {
            // Down: reverse column, process, reverse back
            for (int c = 0; c < N; c++)
            {
                int[] line = new int[N];
                for (int r = 0; r < N; r++) line[r] = _grid[(int)(N - 1 - r) * N + (int)(c)];
                bool colMoved = CompressAndMerge(line);
                if (colMoved)
                {
                    moved = true;
                    for (int r = 0; r < N; r++) _grid[(int)(N - 1 - r) * N + (int)(c)] = line[r];
                }
            }
        }

        return moved;
    }

    private bool HasValue(int target)
    {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (_grid[(int)(r) * N + (int)(c)] == target) return true;
        return false;
    }

    // Compress (remove zeros) and merge adjacent equal tiles.
    // Modifies line in-place. Returns true if anything changed.
    private bool CompressAndMerge(int[] line)
    {
        int[] original = new int[N];
        for (int i = 0; i < N; i++) original[i] = line[i];

        // Compress: move non-zero elements to the front
        int[] temp = new int[N];
        int pos = 0;
        for (int i = 0; i < N; i++)
        {
            if (line[i] != 0) temp[pos++] = line[i];
        }

        // Merge adjacent equal tiles
        for (int i = 0; i < pos - 1; i++)
        {
            if (temp[i] != 0 && temp[i] == temp[i + 1])
            {
                temp[i] *= 2;
                _score += temp[i];
                temp[i + 1] = 0;
                i++; // skip the merged tile
            }
        }

        // Compress again after merge
        int[] result = new int[N];
        pos = 0;
        for (int i = 0; i < N; i++)
        {
            if (temp[i] != 0) result[pos++] = temp[i];
        }

        // Copy back to line
        for (int i = 0; i < N; i++) line[i] = result[i];

        // Check if anything changed
        bool changed = false;
        for (int i = 0; i < N; i++)
        {
            if (original[i] != line[i]) { changed = true; break; }
        }
        return changed;
    }

    private bool IsGameOver()
    {
        // Check for empty cells
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (_grid[(int)(r) * N + (int)(c)] == 0) return false;

        // Check for adjacent equal tiles (horizontal)
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N - 1; c++)
                if (_grid[(int)(r) * N + (int)(c)] == _grid[(int)(r) * N + (int)(c + 1)]) return false;

        // Check for adjacent equal tiles (vertical)
        for (int c = 0; c < N; c++)
            for (int r = 0; r < N - 1; r++)
                if (_grid[(int)(r) * N + (int)(c)] == _grid[(int)(r + 1) * N + (int)(c)]) return false;

        return true;
    }

    private void UpdateUi()
    {
        // Update 4x4 grid tiles
        for (int r = 0; r < N; r++)
        {
            for (int c = 0; c < N; c++)
            {
                Runtime.GameUiSetTile(r, c, _grid[(int)(r) * N + (int)(c)]);
            }
        }

        // Update score
        Runtime.GameUiSetScore(_score);

        // Update status. Runtime.GameUiSetStatus supports:
        //   0 = playing, 1 = win, 2 = gameover
        // For "won but continuing", fall back to playing (0)
        // so the player can keep going past 2048.
        if (_gameOver)
        {
            Runtime.GameUiSetStatus(2);
        }
        else if (_won && !_continueAfterWin)
        {
            Runtime.GameUiSetStatus(1);
        }
        else
        {
            Runtime.GameUiSetStatus(0);
        }
    }
}
