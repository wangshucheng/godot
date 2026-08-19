using System;
using System.Buffers;
using System.Collections.Generic;

namespace CSharpBench.Core.Categories
{
    /// <summary>Span/内存操作（10 项，依赖 System.Memory 包；Mono 无法加载时整类标 skipped）。</summary>
    public static class SpanW
    {
        static readonly Dictionary<int, byte[]> _bytes = new Dictionary<int, byte[]>();
        static readonly Dictionary<int, char[]> _chars = new Dictionary<int, char[]>();
        static byte[] Bytes(int n) { lock (_bytes) { byte[] a; if (!_bytes.TryGetValue(n, out a)) { a = new byte[n]; for (int i = 0; i < n; i++) a[i] = (byte)i; _bytes[n] = a; } return a; } }
        static char[] Chars(int n) { lock (_chars) { char[] a; if (!_chars.TryGetValue(n, out a)) { a = new char[n]; for (int i = 0; i < n; i++) a[i] = (char)(65 + (i & 15)); _chars[n] = a; } return a; } }

        public static void Add(List<Workload> l)
        {
            l.Add("SpanMemory", "Span_SliceCopy8", Span_SliceCopy8);
            l.Add("SpanMemory", "Span_Fill64", Span_Fill64);
            l.Add("SpanMemory", "Span_IndexerLoop", Span_IndexerLoop);
            l.Add("SpanMemory", "Span_Reverse64", Span_Reverse64);
            l.Add("SpanMemory", "StackAlloc_Fill128", StackAlloc_Fill128);
            l.Add("SpanMemory", "ArrayPool_RentReturn1k", ArrayPool_RentReturn1k);
            l.Add("SpanMemory", "Array_Copy256", Array_Copy256);
            l.Add("SpanMemory", "Buffer_BlockCopy256", Buffer_BlockCopy256);
            l.Add("SpanMemory", "Span_SequenceEqual256", Span_SequenceEqual256);
            l.Add("SpanMemory", "CharSpan_Copy128", CharSpan_Copy128);
        }

        public static void Span_SliceCopy8(int n)
        {
            var src = Bytes(Math.Max(n, 256));
            Span<byte> s = src;
            Span<byte> dst = stackalloc byte[8];
            for (int i = 0; i < n; i++)
                s.Slice((i * 7) % (s.Length - 8), 8).CopyTo(dst);
            Workloads.Sink = dst[0];
        }

        public static void Span_Fill64(int n)
        {
            var buf = Bytes(Math.Max(n, 256));
            Span<byte> s = buf;
            for (int i = 0; i < n; i++)
                s.Slice((i * 64) % (s.Length - 64), 64).Fill((byte)i);
        }

        public static void Span_IndexerLoop(int n)
        {
            var buf = Bytes(1024);
            Span<byte> s = buf;
            long v = 0;
            for (int i = 0; i < n; i++) v += s[i & 1023];
            Workloads.SinkLong = v;
        }

        public static void Span_Reverse64(int n)
        {
            var buf = Bytes(Math.Max(n, 256));
            Span<byte> s = buf;
            for (int i = 0; i < n; i++)
                s.Slice((i * 64) % (s.Length - 64), 64).Reverse();
        }

        /// <summary>
        /// 修复 2026-08-19：stackalloc 原先写在 for 循环体内，受影响的 JIT 路径
        /// （net7.0.11 RyuJIT InProcessEmit / 非 net7.0 的 System.Memory 包路径）
        /// 不逐次回收 localloc，栈占用随 n 线性增长（≈ n×128B），size=1,000,000
        /// 时需 ~128MB ≫ 1MB 默认栈 → 不可捕获的 StackOverflowException（0xC00000FD）。
        /// 提升到循环外后栈占用 O(1)，任意规模安全；语义为"复用栈上 Span 的 Fill+读"。
        /// </summary>
        public static void StackAlloc_Fill128(int n)
        {
            long v = 0;
            Span<byte> b = stackalloc byte[128];
            for (int i = 0; i < n; i++)
            {
                b.Fill((byte)i);
                v += b[0] + b[127];
            }
            Workloads.SinkLong = v;
        }

        public static void ArrayPool_RentReturn1k(int n)
        {
            long v = 0;
            for (int i = 0; i < n; i++)
            {
                var a = ArrayPool<byte>.Shared.Rent(1024);
                a[0] = (byte)i;
                v += a[0];
                ArrayPool<byte>.Shared.Return(a);
            }
            Workloads.SinkLong = v;
        }

        public static void Array_Copy256(int n)
        {
            var src = Bytes(512);
            var dst = new byte[256];
            for (int i = 0; i < n; i++) Array.Copy(src, i & 255, dst, 0, 256);
            Workloads.SinkLong = dst[0];
        }

        public static void Buffer_BlockCopy256(int n)
        {
            var src = Bytes(512);
            var dst = new byte[256];
            for (int i = 0; i < n; i++) Buffer.BlockCopy(src, i & 255, dst, 0, 256);
            Workloads.SinkLong = dst[0];
        }

        public static void Span_SequenceEqual256(int n)
        {
            var a = Bytes(256);
            var b = Bytes(256);
            bool eq = false;
            Span<byte> sa = a, sb = b;
            for (int i = 0; i < n; i++) eq = sa.SequenceEqual(sb);
            Workloads.Sink = eq;
        }

        public static void CharSpan_Copy128(int n)
        {
            var src = Chars(256);
            var dst = new char[128];
            Span<char> s = src, d = dst;
            for (int i = 0; i < n; i++) s.Slice(i & 128, 128).CopyTo(d);
            Workloads.Sink = d[0];
        }
    }
}
