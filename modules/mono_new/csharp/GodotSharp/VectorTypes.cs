using System;
using System.Runtime.InteropServices;

namespace Godot
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float x;
        public float y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static readonly Vector2 Zero = new Vector2(0, 0);
        public static readonly Vector2 One = new Vector2(1, 1);
        public static readonly Vector2 Up = new Vector2(0, -1);
        public static readonly Vector2 Down = new Vector2(0, 1);
        public static readonly Vector2 Left = new Vector2(-1, 0);
        public static readonly Vector2 Right = new Vector2(1, 0);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2i
    {
        public int x;
        public int y;

        public Vector2i(int x, int y)
        {
            this.x = x;
            this.y = y;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float x;
        public float y;
        public float z;

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public static readonly Vector3 Zero = new Vector3(0, 0, 0);
        public static readonly Vector3 One = new Vector3(1, 1, 1);
        public static readonly Vector3 Up = new Vector3(0, 1, 0);
        public static readonly Vector3 Down = new Vector3(0, -1, 0);
        public static readonly Vector3 Forward = new Vector3(0, 0, -1);
        public static readonly Vector3 Back = new Vector3(0, 0, 1);
        public static readonly Vector3 Left = new Vector3(-1, 0, 0);
        public static readonly Vector3 Right = new Vector3(1, 0, 0);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3i
    {
        public int x;
        public int y;
        public int z;

        public Vector3i(int x, int y, int z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4
    {
        public float x;
        public float y;
        public float z;
        public float w;

        public Vector4(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4i
    {
        public int x;
        public int y;
        public int z;
        public int w;

        public Vector4i(int x, int y, int z, int w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Color
    {
        public float r;
        public float g;
        public float b;
        public float a;

        public Color(float r, float g, float b, float a = 1.0f)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public static readonly Color White = new Color(1, 1, 1, 1);
        public static readonly Color Black = new Color(0, 0, 0, 1);
        public static readonly Color Red = new Color(1, 0, 0, 1);
        public static readonly Color Green = new Color(0, 1, 0, 1);
        public static readonly Color Blue = new Color(0, 0, 1, 1);
        public static readonly Color Transparent = new Color(0, 0, 0, 0);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2
    {
        public Vector2 position;
        public Vector2 size;

        public Rect2(Vector2 position, Vector2 size)
        {
            this.position = position;
            this.size = size;
        }

        public Rect2(float x, float y, float width, float height)
        {
            position = new Vector2(x, y);
            size = new Vector2(width, height);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2i
    {
        public Vector2i position;
        public Vector2i size;

        public Rect2i(Vector2i position, Vector2i size)
        {
            this.position = position;
            this.size = size;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Plane
    {
        public float x;
        public float y;
        public float z;
        public float d;

        public Plane(Vector3 normal, float d)
        {
            x = normal.x;
            y = normal.y;
            z = normal.z;
            this.d = d;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float x;
        public float y;
        public float z;
        public float w;

        public Quaternion(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AABB
    {
        public Vector3 position;
        public Vector3 size;

        public AABB(Vector3 position, Vector3 size)
        {
            this.position = position;
            this.size = size;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Basis
    {
        public Vector3 row0;
        public Vector3 row1;
        public Vector3 row2;

        public Basis(Vector3 row0, Vector3 row1, Vector3 row2)
        {
            this.row0 = row0;
            this.row1 = row1;
            this.row2 = row2;
        }

        public static readonly Basis Identity = new Basis(
            new Vector3(1, 0, 0),
            new Vector3(0, 1, 0),
            new Vector3(0, 0, 1));
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform2D
    {
        public Vector2 column0;
        public Vector2 column1;
        public Vector2 column2;

        public Transform2D(Vector2 column0, Vector2 column1, Vector2 column2)
        {
            this.column0 = column0;
            this.column1 = column1;
            this.column2 = column2;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform3D
    {
        public Basis basis;
        public Vector3 origin;

        public Transform3D(Basis basis, Vector3 origin)
        {
            this.basis = basis;
            this.origin = origin;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Projection
    {
        public Vector4 column0;
        public Vector4 column1;
        public Vector4 column2;
        public Vector4 column3;

        public Projection(Vector4 column0, Vector4 column1, Vector4 column2, Vector4 column3)
        {
            this.column0 = column0;
            this.column1 = column1;
            this.column2 = column2;
            this.column3 = column3;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RID
    {
        public ulong id;

        public RID(ulong id)
        {
            this.id = id;
        }

        public bool IsValid => id != 0;
    }
}