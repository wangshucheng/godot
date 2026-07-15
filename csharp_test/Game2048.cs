using Godot;
using System;

public partial class Game2048 : Control
{
    private const int N = 4;

    // Game state (pure int - WASM-safe)
    private int[,] _grid = new int[N, N];
    private int _score;
    private bool _gameOver;
    private bool _won;
    private bool _uiBuilt;
    private int _frame;

    // Simple LCG random (avoids System.Random WASM issues)
    private int _rngState = 12345;

    // Input edge detection
    private bool _prevLeft, _prevRight, _prevUp, _prevDown, _prevR;

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

        bool left = Input.IsKeyPressed(Key.Left);
        bool right = Input.IsKeyPressed(Key.Right);
        bool up = Input.IsKeyPressed(Key.Up);
        bool down = Input.IsKeyPressed(Key.Down);
        bool r = Input.IsKeyPressed(Key.R);

        bool moved = false;

        if (left && !_prevLeft) moved = DoMove(0);
        else if (right && !_prevRight) moved = DoMove(1);
        else if (up && !_prevUp) moved = DoMove(2);
        else if (down && !_prevDown) moved = DoMove(3);
        else if (r && !_prevR)
        {
            InitGame();
            moved = true;
        }

        if (moved) UpdateUi();

        _prevLeft = left;
        _prevRight = right;
        _prevUp = up;
        _prevDown = down;
        _prevR = r;
    }

    void InitGame()
    {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                _grid[r, c] = 0;
        _score = 0;
        _gameOver = false;
        _won = false;
        AddRandomTile();
        AddRandomTile();
    }

    int NextRandom()
    {
        _rngState = (_rngState * 1103515245 + 12345) & 0x7FFFFFFF;
        return _rngState;
    }

    void AddRandomTile()
    {
        // Count empty cells
        int emptyCount = 0;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (_grid[r, c] == 0) emptyCount++;

        if (emptyCount == 0) return;

        // Pick a random empty cell
        int target = NextRandom() % emptyCount;
        int idx = 0;
        for (int r = 0; r < N; r++)
        {
            for (int c = 0; c < N; c++)
            {
                if (_grid[r, c] == 0)
                {
                    if (idx == target)
                    {
                        // 90% chance of 2, 10% chance of 4
                        _grid[r, c] = (NextRandom() % 10 < 9) ? 2 : 4;
                        return;
                    }
                    idx++;
                }
            }
        }
    }

    // dir: 0=left, 1=right, 2=up, 3=down
    // Returns true if any tile moved
    bool DoMove(int dir)
    {
        if (_gameOver) return false;

        bool moved = false;

        if (dir == 0)
        {
            // Left: process each row
            for (int r = 0; r < N; r++)
            {
                int[] line = new int[N];
                for (int c = 0; c < N; c++) line[c] = _grid[r, c];
                bool rowMoved = CompressAndMerge(line);
                if (rowMoved)
                {
                    moved = true;
                    for (int c = 0; c < N; c++) _grid[r, c] = line[c];
                }
            }
        }
        else if (dir == 1)
        {
            // Right: reverse, process, reverse back
            for (int r = 0; r < N; r++)
            {
                int[] line = new int[N];
                for (int c = 0; c < N; c++) line[c] = _grid[r, N - 1 - c];
                bool rowMoved = CompressAndMerge(line);
                if (rowMoved)
                {
                    moved = true;
                    for (int c = 0; c < N; c++) _grid[r, N - 1 - c] = line[c];
                }
            }
        }
        else if (dir == 2)
        {
            // Up: process each column
            for (int c = 0; c < N; c++)
            {
                int[] line = new int[N];
                for (int r = 0; r < N; r++) line[r] = _grid[r, c];
                bool colMoved = CompressAndMerge(line);
                if (colMoved)
                {
                    moved = true;
                    for (int r = 0; r < N; r++) _grid[r, c] = line[r];
                }
            }
        }
        else if (dir == 3)
        {
            // Down: reverse column, process, reverse back
            for (int c = 0; c < N; c++)
            {
                int[] line = new int[N];
                for (int r = 0; r < N; r++) line[r] = _grid[N - 1 - r, c];
                bool colMoved = CompressAndMerge(line);
                if (colMoved)
                {
                    moved = true;
                    for (int r = 0; r < N; r++) _grid[N - 1 - r, c] = line[r];
                }
            }
        }

        if (moved)
        {
            AddRandomTile();
            if (!_won)
            {
                for (int r = 0; r < N; r++)
                    for (int c = 0; c < N; c++)
                        if (_grid[r, c] == 2048) _won = true;
            }
            if (IsGameOver()) _gameOver = true;
        }

        return moved;
    }

    // Compress (remove zeros) and merge adjacent equal tiles.
    // Modifies line in-place. Returns true if anything changed.
    bool CompressAndMerge(int[] line)
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

    bool IsGameOver()
    {
        // Check for empty cells
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (_grid[r, c] == 0) return false;

        // Check for adjacent equal tiles (horizontal)
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N - 1; c++)
                if (_grid[r, c] == _grid[r, c + 1]) return false;

        // Check for adjacent equal tiles (vertical)
        for (int c = 0; c < N; c++)
            for (int r = 0; r < N - 1; r++)
                if (_grid[r, c] == _grid[r + 1, c]) return false;

        return true;
    }

    void UpdateUi()
    {
        // Update 4x4 grid tiles
        for (int r = 0; r < N; r++)
        {
            for (int c = 0; c < N; c++)
            {
                Runtime.GameUiSetTile(r, c, _grid[r, c]);
            }
        }

        // Update score
        Runtime.GameUiSetScore(_score);

        // Update status
        if (_gameOver)
        {
            Runtime.GameUiSetStatus(2);
        }
        else if (_won)
        {
            Runtime.GameUiSetStatus(1);
        }
        else
        {
            Runtime.GameUiSetStatus(0);
        }
    }
}
