// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
#define TEST9

using System;
using System.Numerics;
using System.Runtime.CompilerServices;

public struct MyStruct
{
    public int a;

    public MyStruct(int val)
    {
        a = val;
    }
}

public class MyObj
{
    public int ct;
    public MyStruct str;

    public MyObj(int val)
    {
        str = new MyStruct(val);
        ct = 10;
    }
}

public struct MyStruct2
{
    public int ct;
    public MyStruct str;

    public MyStruct2(int val)
    {
        str = new MyStruct(val);
        ct = 20;
    }
}

public struct TestStruct
{
    public int a;
    public int b;
    public int c;
    public int d;
    public int e;
    public int f;
}

public struct TestStruct2
{
    public int a;
    public int b;
}

public struct TestStruct4ii
{
    public int a;
    public int b;
    public int c;
    public int d;
}

public struct TestStruct4if
{
    public int a;
    public int b;
    public float c;
    public float d;
}

public struct TestStruct4fi
{
    public float a;
    public float b;
    public int c;
    public int d;
}

public struct TestStruct4ff
{
    public float a;
    public float b;
    public float c;
    public float d;
}

public class InterpreterTest
{
#if TEST0
    static void TestCallingConvention(int a, float b, int c, double d, int e, double f)
    {
        Console.WriteLine("TestCallingConvention: a = {0}, b = {1}, c = {2}, d = {3}, e = {4}, f = {5}", a, b, c, d, e, f);
    }
#elif TEST1
    static void TestCallingConvention(TestStruct s)
    {
        Console.WriteLine("TestCallingConvention: a = {0}, b = {1}, c = {2}, d = {3}, e = {4}, f = {5}", s.a, s.b, s.c, s.d, s.e, s.f);
    }
#elif TEST2
    static TestStruct2 TestCallingConvention()
    {
        TestStruct2 s;
        s.a = 1;
        s.b = 2;
        return s;
    }
#elif TEST3
    static Vector2 TestCallingConvention()
    {
        Vector2 v = new Vector2(1, 2);
        return v;
    }
#elif TEST4
    static TestStruct TestCallingConvention()
    {
        TestStruct s;
        s.a = 1;
        s.b = 2;
        s.c = 3;
        s.d = 4;
        s.e = 5;
        s.f = 6;
        return s;
    }
#elif TEST5
    static TestStruct4ii TestCallingConvention()
    {
        TestStruct4ii s;
        s.a = 1;
        s.b = 2;
        s.c = 3;
        s.d = 4;
        return s;
    }
#elif TEST6
    static TestStruct4if TestCallingConvention()
    {
        TestStruct4if s;
        s.a = 1;
        s.b = 2;
        s.c = 3.0f;
        s.d = 4.0f;
        return s;
    }
#elif TEST7
    static TestStruct4fi TestCallingConvention()
    {
        TestStruct4fi s;
        s.a = 1.0f;
        s.b = 2.0f;
        s.c = 3;
        s.d = 4;
        return s;
    }
#elif TEST8
    static TestStruct4ff TestCallingConvention()
    {
        TestStruct4ff s;
        s.a = 1.0f;
        s.b = 2.0f;
        s.c = 3.0f;
        s.d = 4.0f;
        return s;
    }
#elif TEST9
    static void TestCallingConvention(TestStruct4fi s)
    {
        Console.WriteLine("TestCallingConvention: a = {0}, b = {1}, c = {2}, d = {3}", s.a, s.b, s.c, s.d);
    }
#endif

    static int Main(string[] args)
    {
        jitField1 = 42;
        jitField2 = 43;
#if TEST0
        TestCallingConvention(1, 2.0f, 3, 4.0, 5, 6.0);
#elif TEST1
        TestStruct s = new TestStruct();
        s.a = 1;
        s.b = 2;
        s.c = 3;
        s.d = 4;
        s.e = 5;
        s.f = 6;
        TestCallingConvention(s);
#elif TEST2
        TestStruct2 s = TestCallingConvention();
#elif TEST3
        Vector2 v = TestCallingConvention();
#elif TEST4
        TestStruct s = TestCallingConvention();
#elif TEST5
        TestStruct4ii s = TestCallingConvention();
#elif TEST6
        TestStruct4if s = TestCallingConvention();
#elif TEST7
        TestStruct4fi s = TestCallingConvention();
#elif TEST8
        TestStruct4ff s = TestCallingConvention();
#elif TEST9
        TestStruct4fi s = new TestStruct4fi();
        s.a = 1.0f;
        s.b = 2.0f;
        s.c = 3;
        s.d = 4;
        TestCallingConvention(s);
#endif
        RunInterpreterTests();
        return 100;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public static void RunInterpreterTests()
    {
#if TEST0
        // win_x64 ok
        // linux_x64 ok
        TestCallingConvention(1, 2.0f, 3, 4.0, 5, 6.0);
#elif TEST1
        // win_x64 ok
        // linux_x64 ok
        TestStruct s = new TestStruct();
        s.a = 1;
        s.b = 2;
        s.c = 3;
        s.d = 4;
        s.e = 5;
        s.f = 6;
        TestCallingConvention(s);
#elif TEST2
        // win_x64 ok
        // linux_x64 ok
        TestStruct2 s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
#elif TEST3
        // win_x64 - interpreter stack alignment issue problem, otherwise would work
        // linux_x64 ok
        Vector2 v = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: v = ");
        Console.WriteLine(v[0]);
        Console.WriteLine(v[1]);
#elif TEST4
        // win_x64 ok
        // linux_x64 ok
        TestStruct s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
        Console.WriteLine(s.c);
        Console.WriteLine(s.d);
        Console.WriteLine(s.e);
        Console.WriteLine(s.f);
#elif TEST5
        // linux_x64 ok
        TestStruct4ii s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
        Console.WriteLine(s.c);
        Console.WriteLine(s.d);
#elif TEST6
        // linux_x64 ok
        TestStruct4if s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
        Console.WriteLine(s.c);
        Console.WriteLine(s.d);
#elif TEST7
        // linux_x64 ok
        TestStruct4fi s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
        Console.WriteLine(s.c);
        Console.WriteLine(s.d);
#elif TEST8
        // linux_x64 ok
        TestStruct4ff s = TestCallingConvention();
        Console.WriteLine("TestCallingConvention: s = ");
        Console.WriteLine(s.a);
        Console.WriteLine(s.b);
        Console.WriteLine(s.c);
        Console.WriteLine(s.d);
#elif TEST9
        TestStruct4fi s = new TestStruct4fi();
        s.a = 1.0f;
        s.b = 2.0f;
        s.c = 3;
        s.d = 4;
        TestCallingConvention(s);
#endif

        //      Console.WriteLine("Run interp tests");
        if (SumN(50) != 1275)
            Environment.FailFast(null);
        if (Mul4(53, 24, 13, 131) != 2166216)
            Environment.FailFast(null);

        TestSwitch();

        if (!PowLoop(20, 10, 1661992960))
            Environment.FailFast(null);
        if (!TestJitFields())
            Environment.FailFast(null);
        // Disable below tests because they are potentially unstable since they do allocation
        // and we currently don't have GC support. They should pass locally though.
        //        if (!TestFields())
        //            Environment.FailFast(null);
        //        if (!TestSpecialFields())
        //            Environment.FailFast(null);
        if (!TestFloat())
            Environment.FailFast(null);
    }

    public static int Mul4(int a, int b, int c, int d)
    {
        return a * b * c * d;
    }

    public static long SumN(int n)
    {
        if (n == 1)
            return 1;
        return (long)SumN(n - 1) + n;
    }

    public static int SwitchOp(int a, int b, int op)
    {
        switch (op)
        {
            case 0:
                return a + b;
            case 1:
                return a - b;
            case 2:
                return a * b;
            default:
                return 42;
        }
    }

    public static void TestSwitch()
    {
        int n0 = SwitchOp(20, 6, 0); // 26
        int n1 = SwitchOp(20, 6, 1); // 14
        int n2 = SwitchOp(20, 6, 2); // 120
        int n3 = SwitchOp(20, 6, 3); // 42

        if ((n0 + n1 + n2 + n3) != 202)
            Environment.FailFast(null);
    }

    public static bool PowLoop(int n, long nr, int expected)
    {
        long ret = 1;
        for (int i = 0; i < n; i++)
            ret *= nr;
        return (int)ret == expected;
    }

    public static int jitField1;
    [ThreadStatic]
    public static int jitField2;

    public static bool TestJitFields()
    {
        // These fields are initialized by the JIT
        // Test that interpreter accesses the correct address
        if (jitField1 != 42)
            return false;
        if (jitField2 != 43)
            return false;
        return true;
    }

    public static MyObj staticObj;
    public static MyStruct2 staticStr;

    public static void WriteInt(ref int a, int ct)
    {
        a = ct;
    }

    public static int ReadInt(ref int a)
    {
        return a;
    }

    public static bool TestFields()
    {
        MyObj obj = new MyObj(1);
        MyStruct2 str = new MyStruct2(2);

        int sum = obj.str.a + str.str.a + obj.ct + str.ct;
        if (sum != 33)
            return false;

        staticObj = obj;
        staticStr = str;

        sum = staticObj.str.a + staticStr.str.a + staticObj.ct + staticStr.ct;
        if (sum != 33)
            return false;

        WriteInt(ref str.str.a, 11);
        WriteInt(ref staticObj.str.a, 22);
        sum = ReadInt(ref str.str.a) + ReadInt(ref staticObj.str.a);
        if (sum != 33)
            return false;

        return true;
    }

    [ThreadStatic]
    public static MyObj threadStaticObj;
    [ThreadStatic]
    public static MyStruct2 threadStaticStr;

    public static bool TestSpecialFields()
    {
        threadStaticObj = new MyObj(1);
        threadStaticStr = new MyStruct2(2);

        int sum = threadStaticObj.str.a + threadStaticStr.str.a + threadStaticObj.ct + threadStaticStr.ct;
        if (sum != 33)
            return false;

        return true;
    }

    public static bool TestFloat()
    {
        float f1 = 14554.9f;
        float f2 = 12543.4f;

        float sum = f1 + f2;

        if ((sum - 27098.3) > 0.001 || (sum - 27098.3) < -0.001)
            return false;

        double d1 = 14554.9;
        double d2 = 12543.4;

        double diff = d1 - d2;

        if ((diff - 2011.5) > 0.001 || (diff - 2011.5) < -0.001)
            return false;

        return true;
    }
}
