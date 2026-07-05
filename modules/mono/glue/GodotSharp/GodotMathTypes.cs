using System;
using System.Runtime.InteropServices;

namespace Godot {
    [Preserve]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2 : IEquatable<Vector2> {
        public float x;
        public float y;

        public static readonly Vector2 Zero = new Vector2(0, 0);
        public static readonly Vector2 One = new Vector2(1, 1);
        public static readonly Vector2 Up = new Vector2(0, -1);
        public static readonly Vector2 Down = new Vector2(0, 1);
        public static readonly Vector2 Left = new Vector2(-1, 0);
        public static readonly Vector2 Right = new Vector2(1, 0);

        public Vector2(float x, float y) {
            this.x = x;
            this.y = y;
        }

        public float Length => (float)Math.Sqrt(x * x + y * y);
        public float LengthSquared => x * x + y * y;
        public Vector2 Normalized() {
            float len = Length;
            if (len == 0) return Zero;
            return new Vector2(x / len, y / len);
        }

        public static Vector2 operator +(Vector2 a, Vector2 b) => new Vector2(a.x + b.x, a.y + b.y);
        public static Vector2 operator -(Vector2 a, Vector2 b) => new Vector2(a.x - b.x, a.y - b.y);
        public static Vector2 operator *(Vector2 a, float s) => new Vector2(a.x * s, a.y * s);
        public static Vector2 operator *(float s, Vector2 a) => new Vector2(a.x * s, a.y * s);
        public static Vector2 operator /(Vector2 a, float s) => new Vector2(a.x / s, a.y / s);
        public static Vector2 operator -(Vector2 a) => new Vector2(-a.x, -a.y);
        public static bool operator ==(Vector2 a, Vector2 b) => a.x == b.x && a.y == b.y;
        public static bool operator !=(Vector2 a, Vector2 b) => a.x != b.x || a.y != b.y;

        public bool Equals(Vector2 other) => x == other.x && y == other.y;
        public override bool Equals(object obj) => obj is Vector2 v && Equals(v);
        public override int GetHashCode() => (x, y).GetHashCode();
        public override string ToString() => $"({x:F2}, {y:F2})";
    }

    [Preserve]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3 : IEquatable<Vector3> {
        public float x;
        public float y;
        public float z;

        public static readonly Vector3 Zero = new Vector3(0, 0, 0);
        public static readonly Vector3 One = new Vector3(1, 1, 1);
        public static readonly Vector3 Up = new Vector3(0, 1, 0);
        public static readonly Vector3 Down = new Vector3(0, -1, 0);
        public static readonly Vector3 Left = new Vector3(-1, 0, 0);
        public static readonly Vector3 Right = new Vector3(1, 0, 0);
        public static readonly Vector3 Forward = new Vector3(0, 0, -1);
        public static readonly Vector3 Back = new Vector3(0, 0, 1);

        public Vector3(float x, float y, float z) {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public float Length => (float)Math.Sqrt(x * x + y * y + z * z);
        public float LengthSquared => x * x + y * y + z * z;
        public Vector3 Normalized() {
            float len = Length;
            if (len == 0) return Zero;
            return new Vector3(x / len, y / len, z / len);
        }

        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3 operator *(Vector3 a, float s) => new Vector3(a.x * s, a.y * s, a.z * s);
        public static Vector3 operator *(float s, Vector3 a) => new Vector3(a.x * s, a.y * s, a.z * s);
        public static Vector3 operator /(Vector3 a, float s) => new Vector3(a.x / s, a.y / s, a.z / s);
        public static Vector3 operator -(Vector3 a) => new Vector3(-a.x, -a.y, -a.z);
        public static bool operator ==(Vector3 a, Vector3 b) => a.x == b.x && a.y == b.y && a.z == b.z;
        public static bool operator !=(Vector3 a, Vector3 b) => a.x != b.x || a.y != b.y || a.z != b.z;

        public bool Equals(Vector3 other) => x == other.x && y == other.y && z == other.z;
        public override bool Equals(object obj) => obj is Vector3 v && Equals(v);
        public override int GetHashCode() => (x, y, z).GetHashCode();
        public override string ToString() => $"({x:F2}, {y:F2}, {z:F2})";
    }

    [Preserve]
    [StructLayout(LayoutKind.Sequential)]
    public struct Color : IEquatable<Color> {
        public float r;
        public float g;
        public float b;
        public float a;

        public static readonly Color White = new Color(1, 1, 1, 1);
        public static readonly Color Black = new Color(0, 0, 0, 1);
        public static readonly Color Red = new Color(1, 0, 0, 1);
        public static readonly Color Green = new Color(0, 1, 0, 1);
        public static readonly Color Blue = new Color(0, 0, 1, 1);
        public static readonly Color Transparent = new Color(0, 0, 0, 0);
        public static readonly Color Yellow = new Color(1, 1, 0, 1);
        public static readonly Color Cyan = new Color(0, 1, 1, 1);
        public static readonly Color Magenta = new Color(1, 0, 1, 1);

        public Color(float r, float g, float b, float a = 1f) {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public Color(byte r, byte g, byte b, byte a = 255) {
            this.r = r / 255f;
            this.g = g / 255f;
            this.b = b / 255f;
            this.a = a / 255f;
        }

        public byte R8 => (byte)(r * 255);
        public byte G8 => (byte)(g * 255);
        public byte B8 => (byte)(b * 255);
        public byte A8 => (byte)(a * 255);

        public float Luminance => 0.2126f * r + 0.7152f * g + 0.0722f * b;

        public static Color operator +(Color a, Color b) => new Color(a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a);
        public static Color operator -(Color a, Color b) => new Color(a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a);
        public static Color operator *(Color a, float s) => new Color(a.r * s, a.g * s, a.b * s, a.a * s);
        public static Color operator *(Color a, Color b) => new Color(a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a);
        public static bool operator ==(Color a, Color b) => a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        public static bool operator !=(Color a, Color b) => a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;

        public bool Equals(Color other) => r == other.r && g == other.g && b == other.b && a == other.a;
        public override bool Equals(object obj) => obj is Color c && Equals(c);
        public override int GetHashCode() => (r, g, b, a).GetHashCode();
        public override string ToString() => $"({r:F2}, {g:F2}, {b:F2}, {a:F2})";
    }

    [Preserve]
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2 : IEquatable<Rect2> {
        public Vector2 position;
        public Vector2 size;

        public Rect2(Vector2 position, Vector2 size) {
            this.position = position;
            this.size = size;
        }

        public Rect2(float x, float y, float width, float height) {
            position = new Vector2(x, y);
            size = new Vector2(width, height);
        }

        public float X => position.x;
        public float Y => position.y;
        public float Width => size.x;
        public float Height => size.y;
        public Vector2 End => position + size;
        public Vector2 Center => position + size * 0.5f;

        public bool HasPoint(Vector2 point) {
            return point.x >= position.x && point.y >= position.y &&
                   point.x < position.x + size.x && point.y < position.y + size.y;
        }

        public static bool operator ==(Rect2 a, Rect2 b) => a.position == b.position && a.size == b.size;
        public static bool operator !=(Rect2 a, Rect2 b) => a.position != b.position || a.size != b.size;
        public bool Equals(Rect2 other) => position.Equals(other.position) && size.Equals(other.size);
        public override bool Equals(object obj) => obj is Rect2 r && Equals(r);
        public override int GetHashCode() => (position, size).GetHashCode();
        public override string ToString() => $"[P:{position}, S:{size}]";
    }
}
