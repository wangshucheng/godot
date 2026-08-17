using System;

namespace Game2048Demo
{
    /// <summary>
    /// 纯C#实现的2048游戏核心逻辑，无任何UI依赖。
    /// 负责棋盘状态、移动合并、随机生成、游戏结束判定。
    /// </summary>
    public class Game2048
    {
        public const int Size = 4;
        private readonly int[] _grid = new int[Size * Size];
        private readonly int[] _undoGrid = new int[Size * Size];
        private int _undoScore;
        private bool _undoValid;

        public int Score { get; private set; }
        public bool IsGameOver { get; private set; }
        public bool HasWon { get; private set; }

        private Random _rng = new Random(12345);

        /// <summary>
        /// 获取格子值（0=空）。纯C#属性，UI层通过此读取棋盘状态。
        /// </summary>
        public int GetCell(int row, int col)
        {
            return _grid[row * Size + col];
        }

        /// <summary>
        /// 初始化新游戏：清空棋盘，生成两个初始方块。
        /// </summary>
        public void NewGame()
        {
            for (int i = 0; i < _grid.Length; i++) _grid[i] = 0;
            Score = 0;
            IsGameOver = false;
            HasWon = false;
            _undoValid = false;
            SpawnTile();
            SpawnTile();
        }

        /// <summary>
        /// 在随机空位生成新方块（90%概率为2，10%概率为4）。
        /// </summary>
        private void SpawnTile()
        {
            int emptyCount = 0;
            for (int i = 0; i < _grid.Length; i++)
                if (_grid[i] == 0) emptyCount++;

            if (emptyCount == 0) return;

            int target = _rng.Next(emptyCount);
            int idx = 0;
            for (int i = 0; i < _grid.Length; i++)
            {
                if (_grid[i] == 0)
                {
                    if (idx == target)
                    {
                        _grid[i] = _rng.Next(10) < 9 ? 2 : 4;
                        return;
                    }
                    idx++;
                }
            }
        }

        /// <summary>
        /// 保存当前状态到撤销栈（仅一步）。
        /// </summary>
        private void SaveUndo()
        {
            Array.Copy(_grid, _undoGrid, _grid.Length);
            _undoScore = Score;
            _undoValid = true;
        }

        /// <summary>
        /// 撤销上一步操作。返回是否撤销成功。
        /// </summary>
        public bool Undo()
        {
            if (!_undoValid) return false;
            Array.Copy(_undoGrid, _grid, _grid.Length);
            Score = _undoScore;
            _undoValid = false;
            IsGameOver = false;
            return true;
        }

        /// <summary>
        /// 尝试朝指定方向移动。dir: 0=左, 1=右, 2=上, 3=下。
        /// 返回是否实际发生了移动。
        /// </summary>
        public bool Move(int dir)
        {
            if (IsGameOver) return false;

            SaveUndo();
            bool moved = false;

            if (dir == 0) moved = MoveLeft();
            else if (dir == 1) moved = MoveRight();
            else if (dir == 2) moved = MoveUp();
            else if (dir == 3) moved = MoveDown();

            if (moved)
            {
                SpawnTile();
                if (!HasWon && ContainsValue(2048))
                    HasWon = true;
                if (IsFull() && !HasMovesLeft())
                    IsGameOver = true;
            }
            else
            {
                _undoValid = false;
            }
            return moved;
        }

        private bool MoveLeft()
        {
            bool moved = false;
            for (int r = 0; r < Size; r++)
            {
                int[] line = new int[Size];
                for (int c = 0; c < Size; c++) line[c] = _grid[r * Size + c];
                if (CompressAndMerge(line))
                {
                    moved = true;
                    for (int c = 0; c < Size; c++) _grid[r * Size + c] = line[c];
                }
            }
            return moved;
        }

        private bool MoveRight()
        {
            bool moved = false;
            for (int r = 0; r < Size; r++)
            {
                int[] line = new int[Size];
                for (int c = 0; c < Size; c++) line[c] = _grid[r * Size + (Size - 1 - c)];
                if (CompressAndMerge(line))
                {
                    moved = true;
                    for (int c = 0; c < Size; c++) _grid[r * Size + (Size - 1 - c)] = line[c];
                }
            }
            return moved;
        }

        private bool MoveUp()
        {
            bool moved = false;
            for (int c = 0; c < Size; c++)
            {
                int[] line = new int[Size];
                for (int r = 0; r < Size; r++) line[r] = _grid[r * Size + c];
                if (CompressAndMerge(line))
                {
                    moved = true;
                    for (int r = 0; r < Size; r++) _grid[r * Size + c] = line[r];
                }
            }
            return moved;
        }

        private bool MoveDown()
        {
            bool moved = false;
            for (int c = 0; c < Size; c++)
            {
                int[] line = new int[Size];
                for (int r = 0; r < Size; r++) line[r] = _grid[(Size - 1 - r) * Size + c];
                if (CompressAndMerge(line))
                {
                    moved = true;
                    for (int r = 0; r < Size; r++) _grid[(Size - 1 - r) * Size + c] = line[r];
                }
            }
            return moved;
        }

        /// <summary>
        /// 压缩并合并一行/列。返回是否发生变化。
        /// </summary>
        private bool CompressAndMerge(int[] line)
        {
            int[] original = new int[Size];
            Array.Copy(line, original, Size);

            int[] temp = new int[Size];
            int pos = 0;
            for (int i = 0; i < Size; i++)
                if (line[i] != 0) temp[pos++] = line[i];

            for (int i = 0; i < pos - 1; i++)
            {
                if (temp[i] != 0 && temp[i] == temp[i + 1])
                {
                    temp[i] *= 2;
                    Score += temp[i];
                    temp[i + 1] = 0;
                    i++;
                }
            }

            int[] result = new int[Size];
            pos = 0;
            for (int i = 0; i < Size; i++)
                if (temp[i] != 0) result[pos++] = temp[i];

            Array.Copy(result, line, Size);

            for (int i = 0; i < Size; i++)
                if (original[i] != line[i]) return true;
            return false;
        }

        private bool ContainsValue(int target)
        {
            for (int i = 0; i < _grid.Length; i++)
                if (_grid[i] == target) return true;
            return false;
        }

        private bool IsFull()
        {
            for (int i = 0; i < _grid.Length; i++)
                if (_grid[i] == 0) return false;
            return true;
        }

        private bool HasMovesLeft()
        {
            for (int r = 0; r < Size; r++)
                for (int c = 0; c < Size; c++)
                {
                    if (c < Size - 1 && _grid[r * Size + c] == _grid[r * Size + (c + 1)]) return true;
                    if (r < Size - 1 && _grid[r * Size + c] == _grid[(r + 1) * Size + c]) return true;
                }
            return false;
        }
    }
}
