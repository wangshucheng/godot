using BenchmarkDotNet.Attributes;
using CSharpBench.Core.Categories;

namespace CSharpBench.ConsoleHost
{
    // =====================================================================
    // BenchmarkDotNet 包装类：方法名与 Core 工作负载一一对应（报告合并依赖同名）。
    // 类名 Bench<Category> → 报告 category=<Category>。
    // 全规模类: Size = 100/10_000/1_000_000；重型类: Size = 100/10_000。
    // =====================================================================

    [MemoryDiagnoser]
    public class BenchPrimitives
    {
        [Params(100, 10_000, 1_000_000)] public int Size;
        [Benchmark] public void Int_Add_Loop() => PrimitivesW.Int_Add_Loop(Size);
        [Benchmark] public void Double_Mul_Loop() => PrimitivesW.Double_Mul_Loop(Size);
        [Benchmark] public void Decimal_Add_Loop() => PrimitivesW.Decimal_Add_Loop(Size);
        [Benchmark] public void Int_Boxing() => PrimitivesW.Int_Boxing(Size);
        [Benchmark] public void String_Concat_Plus() => PrimitivesW.String_Concat_Plus(Size);
        [Benchmark] public void String_Concat_Builder() => PrimitivesW.String_Concat_Builder(Size);
        [Benchmark] public void String_Concat_Interp() => PrimitivesW.String_Concat_Interp(Size);
        [Benchmark] public void Int_Parse() => PrimitivesW.Int_Parse(Size);
        [Benchmark] public void Int_TryParse() => PrimitivesW.Int_TryParse(Size);
        [Benchmark] public void Double_Parse() => PrimitivesW.Double_Parse(Size);
        [Benchmark] public void Guid_NewGuid() => PrimitivesW.Guid_NewGuid(Size);
        [Benchmark] public void DateTime_Now() => PrimitivesW.DateTime_Now(Size);
        [Benchmark] public void Enum_Parse() => PrimitivesW.Enum_Parse(Size);
        [Benchmark] public void String_Compare_Ordinal() => PrimitivesW.String_Compare_Ordinal(Size);
    }

    [MemoryDiagnoser]
    public class BenchCollections
    {
        [Params(100, 10_000)] public int Size;
        [Benchmark] public void Array_ForIterate() => CollectionsW.Array_ForIterate(Size);
        [Benchmark] public void Array_Clone_Sort() => CollectionsW.Array_Clone_Sort(Size);
        [Benchmark] public void List_Add_Grow() => CollectionsW.List_Add_Grow(Size);
        [Benchmark] public void List_Add_Prealloc() => CollectionsW.List_Add_Prealloc(Size);
        [Benchmark] public void List_IndexGet() => CollectionsW.List_IndexGet(Size);
        [Benchmark] public void List_Contains_Miss() => CollectionsW.List_Contains_Miss(Size);
        [Benchmark] public void List_IterateFor() => CollectionsW.List_IterateFor(Size);
        [Benchmark] public void List_IterateForeach() => CollectionsW.List_IterateForeach(Size);
        [Benchmark] public void Dictionary_Add_Grow() => CollectionsW.Dictionary_Add_Grow(Size);
        [Benchmark] public void Dictionary_Add_Prealloc() => CollectionsW.Dictionary_Add_Prealloc(Size);
        [Benchmark] public void Dictionary_Lookup_Hit() => CollectionsW.Dictionary_Lookup_Hit(Size);
        [Benchmark] public void Dictionary_Lookup_Miss() => CollectionsW.Dictionary_Lookup_Miss(Size);
        [Benchmark] public void Dictionary_IterateForeach() => CollectionsW.Dictionary_IterateForeach(Size);
        [Benchmark] public void HashSet_Add() => CollectionsW.HashSet_Add(Size);
        [Benchmark] public void HashSet_Contains_Hit() => CollectionsW.HashSet_Contains_Hit(Size);
        [Benchmark] public void Queue_Stack_PushPop() => CollectionsW.Queue_Stack_PushPop(Size);
    }

    [MemoryDiagnoser]
    public class BenchLinq
    {
        [Params(100, 10_000, 1_000_000)] public int Size;
        [Benchmark] public void Where_Filter_Count() => LinqW.Where_Filter_Count(Size);
        [Benchmark] public void Select_Project_Last() => LinqW.Select_Project_Last(Size);
        [Benchmark] public void WhereSelect_Chain_Sum() => LinqW.WhereSelect_Chain_Sum(Size);
        [Benchmark] public void Handwritten_Sum_Loop() => LinqW.Handwritten_Sum_Loop(Size);
        [Benchmark] public void OrderBy_First() => LinqW.OrderBy_First(Size);
        [Benchmark] public void OrderBy_Take10() => LinqW.OrderBy_Take10(Size);
        [Benchmark] public void GroupBy_Count() => LinqW.GroupBy_Count(Size);
        [Benchmark] public void Aggregate_Max() => LinqW.Aggregate_Max(Size);
        [Benchmark] public void Any_Miss_FullScan() => LinqW.Any_Miss_FullScan(Size);
        [Benchmark] public void FirstOrDefault_Miss() => LinqW.FirstOrDefault_Miss(Size);
        [Benchmark] public void ToList_Materialize() => LinqW.ToList_Materialize(Size);
        [Benchmark] public void ToArray_Materialize() => LinqW.ToArray_Materialize(Size);
        [Benchmark] public void Count_Predicate() => LinqW.Count_Predicate(Size);
        [Benchmark] public void Distinct_Count() => LinqW.Distinct_Count(Size);
    }

    [MemoryDiagnoser]
    public class BenchAsync
    {
        [Params(100, 10_000)] public int Size;
        [Benchmark] public void Await_CompletedTask() => AsyncW.Await_CompletedTask(Size);
        [Benchmark] public void AwaitChain_Depth4() => AsyncW.AwaitChain_Depth4(Size);
        [Benchmark] public void AwaitChain_Depth16() => AsyncW.AwaitChain_Depth16(Size);
        [Benchmark] public void Await_TaskYield() => AsyncW.Await_TaskYield(Size);
        [Benchmark] public void Await_FromResult() => AsyncW.Await_FromResult(Size);
        [Benchmark] public void TaskRun_Offload_Wait() => AsyncW.TaskRun_Offload_Wait(Size);
        [Benchmark] public void WhenAll_8() => AsyncW.WhenAll_8(Size);
        [Benchmark] public void WhenAll_64() => AsyncW.WhenAll_64(Size);
        [Benchmark] public void Tcs_SetResult_Await() => AsyncW.Tcs_SetResult_Await(Size);
        [Benchmark] public void ConfigureAwaitFalse_Chain4() => AsyncW.ConfigureAwaitFalse_Chain4(Size);
        [Benchmark] public void SemaphoreSlim_WaitRelease() => AsyncW.SemaphoreSlim_WaitRelease(Size);
        [Benchmark] public void ProducerConsumer_Queue() => AsyncW.ProducerConsumer_Queue(Size);
    }

    [MemoryDiagnoser]
    public class BenchReflection
    {
        [Params(100, 10_000)] public int Size;
        [Benchmark] public void Type_GetMethod() => ReflectionW.Type_GetMethod(Size);
        [Benchmark] public void Method_Invoke_Instance() => ReflectionW.Method_Invoke_Instance(Size);
        [Benchmark] public void Method_Invoke_Static() => ReflectionW.Method_Invoke_Static(Size);
        [Benchmark] public void Property_GetValue() => ReflectionW.Property_GetValue(Size);
        [Benchmark] public void Property_GetValue_ViaGetter() => ReflectionW.Property_GetValue_ViaGetter(Size);
        [Benchmark] public void Property_SetValue() => ReflectionW.Property_SetValue(Size);
        [Benchmark] public void Type_GetProperties() => ReflectionW.Type_GetProperties(Size);
        [Benchmark] public void CreateDelegate_ThenInvoke() => ReflectionW.CreateDelegate_ThenInvoke(Size);
        [Benchmark] public void Expression_Compile() => ReflectionW.Expression_Compile(Size);
        [Benchmark] public void Activator_CreateInstance() => ReflectionW.Activator_CreateInstance(Size);
        [Benchmark] public void GetCustomAttribute() => ReflectionW.GetCustomAttribute(Size);
    }

    [MemoryDiagnoser]
    public class BenchDelegates
    {
        [Params(100, 10_000, 1_000_000)] public int Size;
        [Benchmark] public void Action_Invoke() => DelegatesW.Action_Invoke(Size);
        [Benchmark] public void ActionInt_Invoke() => DelegatesW.ActionInt_Invoke(Size);
        [Benchmark] public void FuncInt_Invoke() => DelegatesW.FuncInt_Invoke(Size);
        [Benchmark] public void Multicast2_Invoke() => DelegatesW.Multicast2_Invoke(Size);
        [Benchmark] public void Multicast8_Invoke() => DelegatesW.Multicast8_Invoke(Size);
        [Benchmark] public void StaticLambdaNoCapture() => DelegatesW.StaticLambdaNoCapture(Size);
        [Benchmark] public void ClosureCapture_Invoke() => DelegatesW.ClosureCapture_Invoke(Size);
        [Benchmark] public void Event_AddRemove() => DelegatesW.Event_AddRemove(Size);
        [Benchmark] public void Interface_vs_DirectCall() => DelegatesW.Interface_vs_DirectCall(Size);
        [Benchmark] public void NewDelegate_Creation() => DelegatesW.NewDelegate_Creation(Size);
    }

    [MemoryDiagnoser]
    public class BenchSpanMemory
    {
        [Params(100, 10_000, 1_000_000)] public int Size;
        [Benchmark] public void Span_SliceCopy8() => SpanW.Span_SliceCopy8(Size);
        [Benchmark] public void Span_Fill64() => SpanW.Span_Fill64(Size);
        [Benchmark] public void Span_IndexerLoop() => SpanW.Span_IndexerLoop(Size);
        [Benchmark] public void Span_Reverse64() => SpanW.Span_Reverse64(Size);
        [Benchmark] public void StackAlloc_Fill128() => SpanW.StackAlloc_Fill128(Size);
        [Benchmark] public void ArrayPool_RentReturn1k() => SpanW.ArrayPool_RentReturn1k(Size);
        [Benchmark] public void Array_Copy256() => SpanW.Array_Copy256(Size);
        [Benchmark] public void Buffer_BlockCopy256() => SpanW.Buffer_BlockCopy256(Size);
        [Benchmark] public void Span_SequenceEqual256() => SpanW.Span_SequenceEqual256(Size);
        [Benchmark] public void CharSpan_Copy128() => SpanW.CharSpan_Copy128(Size);
    }

    [MemoryDiagnoser]
    public class BenchGcAlloc
    {
        [Params(100, 10_000)] public int Size;
        [Benchmark] public void Class_New_Small() => GcAllocW.Class_New_Small(Size);
        [Benchmark] public void Struct_CopyAssign_Baseline() => GcAllocW.Struct_CopyAssign_Baseline(Size);
        [Benchmark] public void Class_New_Big() => GcAllocW.Class_New_Big(Size);
        [Benchmark] public void Array_New_Int128() => GcAllocW.Array_New_Int128(Size);
        [Benchmark] public void Array_New_Class16() => GcAllocW.Array_New_Class16(Size);
        [Benchmark] public void ListPool_Reuse8() => GcAllocW.ListPool_Reuse8(Size);
        [Benchmark] public void Finalizable_New() => GcAllocW.Finalizable_New(Size);
        [Benchmark] public void String_New100() => GcAllocW.String_New100(Size);
        [Benchmark] public void Boxing_Plus_Equals() => GcAllocW.Boxing_Plus_Equals(Size);
        [Benchmark] public void GC_Collect_Gen0() => GcAllocW.GC_Collect_Gen0(Size);
    }

    [MemoryDiagnoser]
    public class BenchScenarios
    {
        [Params(100, 10_000, 1_000_000)] public int Size;
        [Benchmark] public void Json_ManualSerialize() => ScenariosW.Json_ManualSerialize(Size);
        [Benchmark] public void Json_ReflectionSerialize() => ScenariosW.Json_ReflectionSerialize(Size);
        [Benchmark] public void Csv_Parse() => ScenariosW.Csv_Parse(Size);
        [Benchmark] public void WordFreq_Dictionary() => ScenariosW.WordFreq_Dictionary(Size);
        [Benchmark] public void SortAggregate_Pipeline() => ScenariosW.SortAggregate_Pipeline(Size);
        [Benchmark] public void Text_SplitJoin() => ScenariosW.Text_SplitJoin(Size);
        [Benchmark] public void Tree_BuildWalk() => ScenariosW.Tree_BuildWalk(Size);
        [Benchmark] public void PrimeSieve() => ScenariosW.PrimeSieve(Size);
    }
}
