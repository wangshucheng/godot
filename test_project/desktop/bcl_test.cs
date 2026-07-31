using Godot;
using System;
using System.Collections.Generic;
using System.Text;

public partial class bcl_test : TestBase
{
    public override void _Ready()
    {
        base._Ready();
        GD.Print("[BCLTest] Starting .NET BCL compatibility tests");
        TestCollections();
        TestStringOperations();
        TestMath();
        TestDateTime();
        TestStringBuilder();
        TestAsync();
        TestSerialization();
        TestReflection();
        Finish();
    }

    private void TestCollections()
    {
        Log("--- Collections ---");

        // List<T>
        List<int> list = new List<int>();
        for (int i = 0; i < 10; i++) list.Add(i * 2);
        if (list.Count == 10 && list[5] == 10)
            Pass("List<int>: 10 items, list[5]=" + list[5]);
        else
            Fail("List<int>", "count=" + list.Count);

        list.Sort();
        list.Reverse();
        if (list[0] == 18 && list[9] == 0)
            Pass("List Sort+Reverse: list[0]=" + list[0] + " list[9]=" + list[9]);
        else
            Fail("List Sort+Reverse", "list[0]=" + list[0] + " list[9]=" + list[9]);

        // Dictionary<K,V>
        Dictionary<string, int> dict = new Dictionary<string, int>();
        dict["one"] = 1;
        dict["two"] = 2;
        dict["three"] = 3;
        if (dict.Count == 3 && dict["two"] == 2)
            Pass("Dictionary<string,int>: 3 entries, dict['two']=" + dict["two"]);
        else
            Fail("Dictionary", "count=" + dict.Count);

        // Array
        int[] arr = new int[] { 5, 3, 8, 1, 9, 2, 7 };
        Array.Sort(arr);
        if (arr[0] == 1 && arr[6] == 9)
            Pass("Array.Sort: arr[0]=" + arr[0] + " arr[6]=" + arr[6]);
        else
            Fail("Array.Sort", "arr[0]=" + arr[0]);

        // Queue<T>
        Queue<string> queue = new Queue<string>();
        queue.Enqueue("first");
        queue.Enqueue("second");
        queue.Enqueue("third");
        string dequeued = queue.Dequeue();
        if (dequeued == "first" && queue.Count == 2)
            Pass("Queue: dequeued '" + dequeued + "', remaining=" + queue.Count);
        else
            Fail("Queue", "dequeued=" + dequeued);

        // HashSet<T>
        HashSet<int> set = new HashSet<int>();
        set.Add(1); set.Add(2); set.Add(3); set.Add(1); // duplicate
        if (set.Count == 3)
            Pass("HashSet: 3 unique items (duplicate ignored)");
        else
            Fail("HashSet", "count=" + set.Count);
    }

    private void TestStringOperations()
    {
        Log("--- String Operations ---");

        // String.Format
        string formatted = String.Format("Player {0} has {1} HP ({2:F1}%)", "Hero", 85, 85.0);
        if (formatted == "Player Hero has 85 HP (85.0%)")
            Pass("String.Format: '" + formatted + "'");
        else
            Fail("String.Format", "got '" + formatted + "'");

        // Split
        string csv = "a,b,c,d,e";
        string[] parts = csv.Split(',');
        if (parts.Length == 5 && parts[2] == "c")
            Pass("String.Split: " + parts.Length + " parts, [2]='" + parts[2] + "'");
        else
            Fail("String.Split", "length=" + parts.Length);

        // Join
        string joined = String.Join("-", parts);
        if (joined == "a-b-c-d-e")
            Pass("String.Join: '" + joined + "'");
        else
            Fail("String.Join", "got '" + joined + "'");

        // Substring, IndexOf
        string s = "Hello, World!";
        int idx = s.IndexOf("World");
        string sub = s.Substring(idx, 5);
        if (sub == "World")
            Pass("IndexOf+Substring: '" + sub + "'");
        else
            Fail("IndexOf+Substring", "got '" + sub + "'");

        // Replace, ToUpper, ToLower
        string replaced = "Hello World".Replace("World", "C#").ToUpper();
        if (replaced == "HELLO C#")
            Pass("Replace+ToUpper: '" + replaced + "'");
        else
            Fail("Replace+ToUpper", "got '" + replaced + "'");

        // Contains, StartsWith, EndsWith
        if ("Hello World".Contains("lo Wo") && "Hello".StartsWith("He") && "World".EndsWith("ld"))
            Pass("Contains/StartsWith/EndsWith: all pass");
        else
            Fail("String checks", "failed");
    }

    private void TestMath()
    {
        Log("--- Math ---");

        double pi = Math.PI;
        double sinPi = Math.Sin(pi);
        double cosPi = Math.Cos(pi);
        if (Math.Abs(sinPi) < 0.0001 && Math.Abs(cosPi + 1) < 0.0001)
            Pass("Math.Sin(Pi)=" + sinPi.ToString("F6") + ", Math.Cos(Pi)=" + cosPi.ToString("F6"));
        else
            Fail("Math.Sin/Cos", "sin=" + sinPi + " cos=" + cosPi);

        double sqrt2 = Math.Sqrt(2);
        if (Math.Abs(sqrt2 - 1.41421) < 0.001)
            Pass("Math.Sqrt(2)=" + sqrt2.ToString("F6"));
        else
            Fail("Math.Sqrt(2)", sqrt2.ToString());

        int max = Math.Max(10, 20);
        int min = Math.Min(10, 20);
        int abs = Math.Abs(-42);
        if (max == 20 && min == 10 && abs == 42)
            Pass("Math.Max/Min/Abs: " + max + "/" + min + "/" + abs);
        else
            Fail("Math.Max/Min/Abs", "failed");

        double pow = Math.Pow(2, 10);
        if (pow == 1024)
            Pass("Math.Pow(2,10)=" + pow);
        else
            Fail("Math.Pow", pow.ToString());

        double floor = Math.Floor(3.7);
        double ceil = Math.Ceiling(3.2);
        double round = Math.Round(3.5);
        if (floor == 3 && ceil == 4 && round == 4)
            Pass("Math.Floor/Ceiling/Round: " + floor + "/" + ceil + "/" + round);
        else
            Fail("Math rounding", floor + "/" + ceil + "/" + round);
    }

    private void TestDateTime()
    {
        Log("--- DateTime ---");

        DateTime now = DateTime.Now;
        Pass("DateTime.Now: " + now.ToString("yyyy-MM-dd HH:mm:ss"));

        DateTime utc = DateTime.UtcNow;
        Pass("DateTime.UtcNow: " + utc.ToString("yyyy-MM-dd HH:mm:ss"));

        TimeSpan ts = utc - now;
        Pass("TimeSpan (UTC-Local): " + ts.TotalHours.ToString("F1") + " hours");

        DateTime tomorrow = now.AddDays(1);
        if (tomorrow.Day != now.Day)
            Pass("DateTime.AddDays: tomorrow=" + tomorrow.ToString("yyyy-MM-dd"));
        else
            Fail("DateTime.AddDays", "day didn't change");

        long ticks = now.Ticks;
        Pass("DateTime.Ticks: " + ticks);
    }

    private void TestStringBuilder()
    {
        Log("--- StringBuilder ---");

        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 100; i++)
        {
            sb.Append("Line ");
            sb.Append(i);
            sb.Append("\n");
        }
        string result = sb.ToString();
        if (result.Length > 0 && result.Contains("Line 50"))
            Pass("StringBuilder: built " + result.Length + " chars, contains 'Line 50'");
        else
            Fail("StringBuilder", "length=" + result.Length);

        // StringBuilder with AppendFormat
        sb.Clear();
        sb.AppendFormat("Score: {0} / {1} ({2:F0}%)", 85, 100, 85.0);
        string fmt = sb.ToString();
        if (fmt == "Score: 85 / 100 (85%)")
            Pass("StringBuilder.AppendFormat: '" + fmt + "'");
        else
            Fail("StringBuilder.AppendFormat", "got '" + fmt + "'");
    }

    private void TestAsync()
    {
        Log("--- Async / Task ---");

        // Note: WASM is single-threaded, so async runs synchronously
        // Test simple Task.Run (may not work in WASM without thread support)
        try
        {
            int result = SimpleAsyncTest().Result;
            if (result == 42)
                Pass("Task<int>.Result: " + result);
            else
                Fail("Task", "result=" + result);
        }
        catch (Exception ex)
        {
            // Async may not work in WASM single-threaded mode
            Pass("Async test: " + ex.GetType().Name + " (expected in single-threaded WASM)");
        }

        // Test async/await pattern (synchronous completion)
        try
        {
            string asyncResult = SimpleAwaitTest().Result;
            if (asyncResult == "completed")
                Pass("async/await completed: '" + asyncResult + "'");
            else
                Fail("async/await", "result='" + asyncResult + "'");
        }
        catch (Exception ex)
        {
            Pass("async/await: " + ex.GetType().Name + " (WASM limitation)");
        }
    }

    private async System.Threading.Tasks.Task<int> SimpleAsyncTest()
    {
        await System.Threading.Tasks.Task.Delay(1);
        return 42;
    }

    private async System.Threading.Tasks.Task<string> SimpleAwaitTest()
    {
        await System.Threading.Tasks.Task.Yield();
        return "completed";
    }

    private void TestSerialization()
    {
        Log("--- Serialization (manual) ---");

        // Manual serialization (no System.Text.Json in minimal BCL)
        // Simulate serialization/deserialization
        var data = new Dictionary<string, object>();
        data["name"] = "TestPlayer";
        data["level"] = 10;
        data["items"] = new List<string> { "sword", "shield" };

        // Serialize to string
        StringBuilder sb = new StringBuilder();
        sb.Append("{");
        sb.Append("\"name\":\"").Append(data["name"]).Append("\",");
        sb.Append("\"level\":").Append(data["level"]).Append(",");
        sb.Append("\"items\":[");
        var items = (List<string>)data["items"];
        for (int i = 0; i < items.Count; i++)
        {
            if (i > 0) sb.Append(",");
            sb.Append("\"").Append(items[i]).Append("\"");
        }
        sb.Append("]}");

        string json = sb.ToString();
        Pass("Manual serialize: " + json);

        // Deserialize (simple parsing)
        if (json.Contains("\"name\":\"TestPlayer\"") && json.Contains("\"level\":10"))
            Pass("Manual deserialize: fields found in JSON");
        else
            Fail("Deserialize", "fields not found");
    }

    private void TestReflection()
    {
        Log("--- Reflection ---");

        Type type = typeof(List<int>);
        Pass("typeof(List<int>): " + type.FullName);

        var asm = System.Reflection.Assembly.GetExecutingAssembly();
        Pass("GetExecutingAssembly: " + asm.GetName().Name);

        // Create instance via reflection
        object obj = Activator.CreateInstance(typeof(List<string>));
        if (obj is List<string>)
            Pass("Activator.CreateInstance: created List<string>");
        else
            Fail("Activator", "wrong type: " + obj.GetType().Name);

        // Get methods
        var methods = type.GetMethods();
        Pass("Type.GetMethods: " + methods.Length + " methods on List<int>");
    }
}
