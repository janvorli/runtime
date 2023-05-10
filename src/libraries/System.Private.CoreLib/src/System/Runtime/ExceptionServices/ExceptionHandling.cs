// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

//#define DEBUGEH
#if !MONO
using System;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Runtime;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Tasks;

#pragma warning disable CS7095
// just temporary
#pragma warning disable IDE0060

namespace System.Runtime
{
    // TODO: move to ExceptionIDs.cs like in AOT
    internal enum ExceptionIDs
    {
        OutOfMemory = 1,
        Arithmetic = 2,
        ArrayTypeMismatch = 3,
        DivideByZero = 4,
        IndexOutOfRange = 5,
        InvalidCast = 6,
        Overflow = 7,
        NullReference = 8,
        AccessViolation = 9,
        DataMisaligned = 10,
        EntrypointNotFound = 11,
        AmbiguousImplementation = 12,
    }
    internal enum RhFailFastReason
    {
        Unknown = 0,
        InternalError = 1,                                   // "Runtime internal error"
        UnhandledException_ExceptionDispatchNotAllowed = 2,  // "Unhandled exception: no handler found before escaping a finally clause or other fail-fast scope."
        UnhandledException_CallerDidNotHandle = 3,           // "Unhandled exception: no handler found in calling method."
        ClassLibDidNotTranslateExceptionID = 4,              // "Unable to translate failure into a classlib-specific exception object."
        UnhandledException = 5,                              // "unhandled exception"
        UnhandledExceptionFromPInvoke = 6,                   // "Unhandled exception: an unmanaged exception was thrown out of a managed-to-native transition."
    }
    internal static class AsmOffsets
    {
#if DEBUG
        public const int SIZEOF__REGDISPLAY = 0xbf0;
        public const int OFFSETOF__REGDISPLAY__SP = 0xbd8;
        public const int OFFSETOF__REGDISPLAY__ControlPC = 0xbe0;
        public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x8;
        public const int SIZEOF__StackFrameIterator = 0x360+0x10; // +8 for the m_pNextExInfo, +8 for alignment
#else
        public const int SIZEOF__REGDISPLAY = 0xbe0;
        public const int OFFSETOF__REGDISPLAY__SP = 0xbd0;
        public const int OFFSETOF__REGDISPLAY__ControlPC = 0xbd8;
        public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x8;
        public const int SIZEOF__StackFrameIterator = 0x360;
#endif
        public const int SIZEOF__EHEnum = 0x18; // TODO: on 32 bit systems, isn't it 8?

        //public const int OFFSETOF__StackFrameIterator__m_FramePointer = 0;
        //public const int OFFSETOF__StackFrameIterator__m_ControlPC = 0;
        public const int OFFSETOF__StackFrameIterator__m_pRegDisplay = 0x228;
        //public const int OFFSETOF__StackFrameIterator__m_OriginalControlPC = ;

        public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x4d0;
        public const int OFFSETOF__PAL_LIMITED_CONTEXT__IP = 0xf8;
        public const int OFFSETOF__PAL_LIMITED_CONTEXT__SP = 0x98;
        public const int OFFSETOF__PAL_LIMITED_CONTEXT__FP = 0xa0;


        public const int OFFSETOF__ExInfo__m_pPrevExInfo = 0;
        public const int OFFSETOF__ExInfo__m_pExContext = 8;
        public const int OFFSETOF__ExInfo__m_exception = 0x10;

        public const int OFFSETOF__ExInfo__m_kind = 0x18;
        public const int OFFSETOF__ExInfo__m_passNumber = 0x19;

        // BEWARE: This field is used by the stackwalker to know if the dispatch code has reached the
        //         point at which a handler is called.  In other words, it serves as an "is a handler
        //         active" state where '_idxCurClause == MaxTryRegionIdx' means 'no'.
        public const int OFFSETOF__ExInfo__m_idxCurClause = 0x1c;
        public const int OFFSETOF__ExInfo__m_frameIter = 0x20;

        // ???
        public const int OFFSETOF__ExInfo__m_notifyDebuggerSP = OFFSETOF__ExInfo__m_frameIter + SIZEOF__StackFrameIterator;
        // ???
        public const int OFFSETOF__ExInfo__m_pRD = OFFSETOF__ExInfo__m_frameIter + SIZEOF__StackFrameIterator + 8;
        public const int OFFSETOF__ExInfo__m_StackTraceInfo = OFFSETOF__ExInfo__m_frameIter + SIZEOF__StackFrameIterator + 8 + 8;
        public const int OFFSETOF__ExInfo__m_InitialSP = OFFSETOF__ExInfo__m_StackTraceInfo + 8;
        public const int SIZEOF__StackTraceInfo = 0x18;
    }

    internal static partial class InternalCalls
    {
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpSfiInit")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpSfiInit(ref StackFrameIterator pThis, void* pStackwalkCtx, void* pRegDisplay, [MarshalAs(UnmanagedType.Bool)] bool instructionFault);

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpSfiNext")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpSfiNext(ref StackFrameIterator pThis, uint* uExCollideClauseIdx, bool* fUnwoundReversePInvoke);

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallCatchFunclet")]
        internal static unsafe partial IntPtr RhpCallCatchFunclet(
            ObjectHandleOnStack exceptionObj, byte* pHandlerIP, void* pvRegDisplay, EH.ExInfo* exInfo);
#pragma warning restore CS8500

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallFinallyFunclet")]
        internal static unsafe partial void RhpCallFinallyFunclet(byte* pHandlerIP, void* pvRegDisplay, EH.ExInfo* exInfo);
#pragma warning restore CS8500

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallFilterFunclet")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpCallFilterFunclet(
            ObjectHandleOnStack exceptionObj, byte* pFilterIP, void* pvRegDisplay);
#pragma warning restore CS8500

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpEHEnumInitFromStackFrameIterator")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpEHEnumInitFromStackFrameIterator(ref StackFrameIterator pFrameIter, byte** pMethodStartAddress, void* pEHEnum);

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpEHEnumNext")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpEHEnumNext(ref StackFrameIterator pFrameIter, void* pEHEnum, void* pEHClause);

        // TODO: rename?
#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpAppendExceptionStackFrame")]
        internal static unsafe partial void RhpAppendExceptionStackFrame(ObjectHandleOnStack exceptionObj, IntPtr ip, IntPtr sp, int flags, EH.ExInfo* exInfo);
#pragma warning restore CS8500
    }

    // internal static unsafe class InternalCalls
    // {
    //     internal static delegate* unmanaged<void*, void> RhpCaptureCallerContext;
    //     internal static delegate* unmanaged<ref StackFrameIterator, void*, void*, bool, bool> RhpSfiInit;
    //     internal static delegate* unmanaged<ref StackFrameIterator, uint*, bool*, bool> RhpSfiNext;
    //     internal static delegate* unmanaged<void*, byte*, void*, ref EH.ExInfo, IntPtr> RhpCallCatchFunclet;
    //     internal static delegate* unmanaged<byte*, void*, void> RhpCallFinallyFunclet;
    //     internal static delegate* unmanaged<object, byte*, void*, bool> RhpCallFilterFunclet;
    //     internal static delegate* unmanaged<ref StackFrameIterator, byte**, void*, bool> RhpEHEnumInitFromStackFrameIterator;
    //     internal static delegate* unmanaged<ref StackFrameIterator, void*, void*, bool> RhpEHEnumNext;
    // }

    [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__REGDISPLAY)]
    internal unsafe struct REGDISPLAY
    {
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__SP)]
        internal UIntPtr SP;
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__ControlPC)]
        internal UIntPtr ControlPC;
//        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__FP)]
//        internal UIntPtr FP;
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__m_pCurrentContext)]
        internal EH.PAL_LIMITED_CONTEXT* m_pCurrentContext;
    }

    // internal class EH
    // {
    //     [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__PAL_LIMITED_CONTEXT)]
    //     public struct PAL_LIMITED_CONTEXT
    //     {
    //         [FieldOffset(AsmOffsets.OFFSETOF__PAL_LIMITED_CONTEXT__IP)]
    //         internal IntPtr IP;
    //         [FieldOffset(AsmOffsets.OFFSETOF__PAL_LIMITED_CONTEXT__SP)]
    //         internal IntPtr SP;
    //         // the rest of the struct is left unspecified.
    //     }
    // }

    internal static unsafe partial class EH
    {
        internal static UIntPtr MaxSP
        {
            get
            {
                return (UIntPtr)(void*)(-1);
            }
        }

        private enum RhEHClauseKind
        {
            RH_EH_CLAUSE_TYPED = 0,
            RH_EH_CLAUSE_FAULT = 1,
            RH_EH_CLAUSE_FILTER = 2,
            RH_EH_CLAUSE_UNUSED = 3,
        }

        private struct RhEHClause
        {
            internal RhEHClauseKind _clauseKind;
            internal uint _tryStartOffset;
            internal uint _tryEndOffset;
            internal byte* _filterAddress;
            internal byte* _handlerAddress;
            //internal void* _pTargetType;
            internal TypeHandle _pTargetType;
            internal bool _isSameTry;

            ///<summary>
            /// We expect the stackwalker to adjust return addresses to point at 'return address - 1' so that we
            /// can use an interval here that is closed at the start and open at the end.  When a hardware fault
            /// occurs, the IP is pointing at the start of the instruction and will not be adjusted by the
            /// stackwalker.  Therefore, it will naturally work with an interval that has a closed start and open
            /// end.
            ///</summary>
            public bool ContainsCodeOffset(uint codeOffset)
            {
                return ((codeOffset >= _tryStartOffset) &&
                        (codeOffset < _tryEndOffset));
            }
        }

        [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__EHEnum)]
        private struct EHEnum
        {
            [FieldOffset(0)]
            private IntPtr _dummy; // For alignment
        }

        // This is a fail-fast function used by the runtime as a last resort that will terminate the process with
        // as little effort as possible. No guarantee is made about the semantics of this fail-fast.
        [DoesNotReturn]
        internal static void FallbackFailFast(RhFailFastReason reason, object? unhandledException)
        {
            //InternalCalls.RhpFallbackFailFast();
            System.Diagnostics.Debugger.Break();
            Environment.FailFast(reason.ToString());
        }

        // Given an address pointing somewhere into a managed module, get the classlib-defined fail-fast
        // function and invoke it.  Any failure to find and invoke the function, or if it returns, results in
        // MRT-defined fail-fast behavior.
        [DoesNotReturn]
        internal static void FailFastViaClasslib(RhFailFastReason reason, object? unhandledException,
            IntPtr classlibAddress)
        {
             FallbackFailFast(reason, unhandledException);

            // // Find the classlib function that will fail fast. This is a RuntimeExport function from the
            // // classlib module, and is therefore managed-callable.
            // IntPtr pFailFastFunction = (IntPtr)InternalCalls.RhpGetClasslibFunctionFromCodeAddress(classlibAddress,
            //                                                                ClassLibFunctionId.FailFast);

            // if (pFailFastFunction == IntPtr.Zero)
            // {
            //     // The classlib didn't provide a function, so we fail our way...
            //     FallbackFailFast(reason, unhandledException);
            // }

            // try
            // {
            //     // Invoke the classlib fail fast function.
            //     ((delegate*<RhFailFastReason, object, IntPtr, IntPtr, void>)pFailFastFunction)
            //         (reason, unhandledException, IntPtr.Zero, IntPtr.Zero);
            // }
            // catch when (true)
            // {
            //     // disallow all exceptions leaking out of callbacks
            // }

            // // The classlib's function should never return and should not throw. If it does, then we fail our way...
            // FallbackFailFast(reason, unhandledException);
        }

#if TARGET_AMD64
        [StructLayout(LayoutKind.Explicit, Size = 0x4d0)]
#elif TARGET_ARM
        [StructLayout(LayoutKind.Explicit, Size = 0x1a0)]
#elif TARGET_X86
        [StructLayout(LayoutKind.Explicit, Size = 0x2cc)]
#elif TARGET_ARM64
        [StructLayout(LayoutKind.Explicit, Size = 0x390)]
#else
        [StructLayout(LayoutKind.Explicit, Size = 0x10)] // this is small enough that it should trip an assert in RhpCopyContextFromExInfo
#endif
        private struct OSCONTEXT
        {
        }

        internal static unsafe void* PointerAlign(void* ptr, int alignmentInBytes)
        {
            int alignMask = alignmentInBytes - 1;
#if TARGET_64BIT
            return (void*)((((long)ptr) + alignMask) & ~alignMask);
#else
            return (void*)((((int)ptr) + alignMask) & ~alignMask);
#endif
        }

        private static void OnFirstChanceExceptionViaClassLib(object exception)
        {
            // IntPtr pOnFirstChanceFunction =
            //     (IntPtr)InternalCalls.RhpGetClasslibFunctionFromEEType(exception.GetMethodTable(), ClassLibFunctionId.OnFirstChance);

            // if (pOnFirstChanceFunction == IntPtr.Zero)
            // {
            //     return;
            // }

            // try
            // {
            //     ((delegate*<object, void>)pOnFirstChanceFunction)(exception);
            // }
            // catch when (true)
            // {
            //     // disallow all exceptions leaking out of callbacks
            // }
        }

        private static void OnUnhandledExceptionViaClassLib(object exception)
        {
            // IntPtr pOnUnhandledExceptionFunction =
            //     (IntPtr)InternalCalls.RhpGetClasslibFunctionFromEEType(exception.GetMethodTable(), ClassLibFunctionId.OnUnhandledException);

            // if (pOnUnhandledExceptionFunction == IntPtr.Zero)
            // {
            //     return;
            // }

            // try
            // {
            //     ((delegate*<object, void>)pOnUnhandledExceptionFunction)(exception);
            // }
            // catch when (true)
            // {
            //     // disallow all exceptions leaking out of callbacks
            // }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        internal static unsafe void UnhandledExceptionFailFastViaClasslib(
            RhFailFastReason reason, object unhandledException, IntPtr classlibAddress, ref ExInfo exInfo)
        {
            FailFastViaClasslib(
                reason,
                unhandledException,
                classlibAddress);

            // IntPtr pFailFastFunction =
            //     (IntPtr)InternalCalls.RhpGetClasslibFunctionFromCodeAddress(classlibAddress, ClassLibFunctionId.FailFast);

            // if (pFailFastFunction == IntPtr.Zero)
            // {
            //     FailFastViaClasslib(
            //         reason,
            //         unhandledException,
            //         classlibAddress);
            // }

            // // 16-byte align the context.  This is overkill on x86 and ARM, but simplifies things slightly.
            // const int contextAlignment = 16;
            // byte* pbBuffer = stackalloc byte[sizeof(OSCONTEXT) + contextAlignment];
            // void* pContext = PointerAlign(pbBuffer, contextAlignment);

            // InternalCalls.RhpCopyContextFromExInfo(pContext, sizeof(OSCONTEXT), exInfo._pExContext);

            // try
            // {
            //     ((delegate*<RhFailFastReason, object, IntPtr, void*, void>)pFailFastFunction)
            //         (reason, unhandledException, exInfo._pExContext->IP, pContext);
            // }
            // catch when (true)
            // {
            //     // disallow all exceptions leaking out of callbacks
            // }

            // // The classlib's function should never return and should not throw. If it does, then we fail our way...
            // FallbackFailFast(reason, unhandledException);
        }

        private enum RhEHFrameType
        {
            RH_EH_FIRST_FRAME = 1,
            RH_EH_FIRST_RETHROW_FRAME = 2,
        }

        private static void AppendExceptionStackFrameViaClasslib(object exception, IntPtr ip, IntPtr sp, ref ExInfo exInfo,
            ref bool isFirstRethrowFrame, ref bool isFirstFrame)
        {
//            System.Diagnostics.Debug.WriteLine($"AppendExceptionStackFrameViaClasslib ip={ip:X} sp={sp:X}");
            int flags = (isFirstFrame ? (int)RhEHFrameType.RH_EH_FIRST_FRAME : 0) |
                                        (isFirstRethrowFrame ? (int)RhEHFrameType.RH_EH_FIRST_RETHROW_FRAME : 0);
#pragma warning disable CS8500
            fixed (EH.ExInfo* pExInfo = &exInfo)
            {
                InternalCalls.RhpAppendExceptionStackFrame(ObjectHandleOnStack.Create(ref exception), ip, sp, flags, pExInfo);
            }
#pragma warning restore CS8500

            // From coreclr EH:
            // (would need to add SP, MethodDesc, crawl frame etc.)
            // Maybe we can add a function that would get more higher level data struct and extract the details
            // Alternatively, maybe the args above with added SP would be sufficient, but it would be slower - we would need to get MethodDesc for IP
            // Seems we will want to add the m_StackTraceInfo to the ExInfo struct
            // {
            //     GCX_COOP();

            //     m_StackTraceInfo.AppendElement(CanAllocateMemory(), uControlPC, sf.SP, pMD, pcfThisFrame);
            //     m_StackTraceInfo.SaveStackTrace(CanAllocateMemory(), m_hThrowable, bReplaceStack, bSkipLastElement);
            // }
            /*
            IntPtr pAppendStackFrame = (IntPtr)InternalCalls.RhpGetClasslibFunctionFromCodeAddress(ip,
                ClassLibFunctionId.AppendExceptionStackFrame);

            if (pAppendStackFrame != IntPtr.Zero)
            {
                int flags = (isFirstFrame ? (int)RhEHFrameType.RH_EH_FIRST_FRAME : 0) |
                            (isFirstRethrowFrame ? (int)RhEHFrameType.RH_EH_FIRST_RETHROW_FRAME : 0);

                try
                {
                    ((delegate*<object, IntPtr, int, void>)pAppendStackFrame)(exception, ip, flags);
                }
                catch when (true)
                {
                    // disallow all exceptions leaking out of callbacks
                }

                // Clear flags only if we called the function
                isFirstRethrowFrame = false;
                isFirstFrame = false;
            }
            */
        }

        // Given an ExceptionID and an address pointing somewhere into a managed module, get
        // an exception object of a type that the module containing the given address will understand.
        // This finds the classlib-defined GetRuntimeException function and asks it for the exception object.
        internal static Exception GetClasslibException(ExceptionIDs id, IntPtr address)
        {
            Exception? e = id switch
            {
                ExceptionIDs.AccessViolation => new AccessViolationException(),
                ExceptionIDs.Arithmetic => new ArithmeticException(),
                ExceptionIDs.AmbiguousImplementation => new AmbiguousImplementationException(),
                ExceptionIDs.ArrayTypeMismatch => new ArrayTypeMismatchException(),
                ExceptionIDs.DataMisaligned => new DataMisalignedException(),
                ExceptionIDs.DivideByZero => new DivideByZeroException(),
                ExceptionIDs.EntrypointNotFound => new EntryPointNotFoundException(),
                ExceptionIDs.IndexOutOfRange => new IndexOutOfRangeException(),
                ExceptionIDs.InvalidCast => new InvalidCastException(),
                ExceptionIDs.NullReference => new NullReferenceException(),
                ExceptionIDs.OutOfMemory => new OutOfMemoryException(),
                ExceptionIDs.Overflow => new OverflowException(),
                _ => null
            };

            if (e == null)
            {
                FailFastViaClasslib(
                        RhFailFastReason.ClassLibDidNotTranslateExceptionID,
                        null,
                    address);
            }
            // // Find the classlib function that will give us the exception object we want to throw. This
            // // is a RuntimeExport function from the classlib module, and is therefore managed-callable.
            // IntPtr pGetRuntimeExceptionFunction =
            //     (IntPtr)InternalCalls.RhpGetClasslibFunctionFromCodeAddress(address, ClassLibFunctionId.GetRuntimeException);

            // // Return the exception object we get from the classlib.
            // Exception? e = null;
            // try
            // {
            //     e = ((delegate*<ExceptionIDs, Exception>)pGetRuntimeExceptionFunction)(id);
            // }
            // catch when (true)
            // {
            //     // disallow all exceptions leaking out of callbacks
            // }

            // // If the helper fails to yield an object, then we fail-fast.
            // if (e == null)
            // {
            //     FailFastViaClasslib(
            //         RhFailFastReason.ClassLibDidNotTranslateExceptionID,
            //         null,
            //         address);
            // }

            // return e;
            return e;
        }

        // Given an ExceptionID and an MethodTable address, get an exception object of a type that the module containing
        // the given address will understand. This finds the classlib-defined GetRuntimeException function and asks
        // it for the exception object.
        // internal static Exception GetClasslibExceptionFromEEType(ExceptionIDs id, MethodTable* pEEType)
        // {
        //     // Find the classlib function that will give us the exception object we want to throw. This
        //     // is a RuntimeExport function from the classlib module, and is therefore managed-callable.
        //     IntPtr pGetRuntimeExceptionFunction = IntPtr.Zero;
        //     if (pEEType != null)
        //     {
        //         pGetRuntimeExceptionFunction = (IntPtr)InternalCalls.RhpGetClasslibFunctionFromEEType(pEEType, ClassLibFunctionId.GetRuntimeException);
        //     }

        //     // Return the exception object we get from the classlib.
        //     Exception? e = null;
        //     try
        //     {
        //         e = ((delegate*<ExceptionIDs, Exception>)pGetRuntimeExceptionFunction)(id);
        //     }
        //     catch when (true)
        //     {
        //         // disallow all exceptions leaking out of callbacks
        //     }

        //     // If the helper fails to yield an object, then we fail-fast.
        //     if (e == null)
        //     {
        //         FailFastViaClasslib(
        //             RhFailFastReason.ClassLibDidNotTranslateExceptionID,
        //             null,
        //             (IntPtr)pEEType);
        //     }

        //     return e;
        // }

        // RhExceptionHandling_ functions are used to throw exceptions out of our asm helpers. We tail-call from
        // the asm helpers to these functions, which performs the throw. The tail-call is important: it ensures that
        // the stack is crawlable from within these functions.
        // [RuntimeExport("RhExceptionHandling_ThrowClasslibOverflowException")]
        // public static void ThrowClasslibOverflowException(IntPtr address)
        // {
        //     // Throw the overflow exception defined by the classlib, using the return address of the asm helper
        //     // to find the correct classlib.

        //     throw GetClasslibException(ExceptionIDs.Overflow, address);
        // }

        // [RuntimeExport("RhExceptionHandling_ThrowClasslibDivideByZeroException")]
        // public static void ThrowClasslibDivideByZeroException(IntPtr address)
        // {
        //     // Throw the divide by zero exception defined by the classlib, using the return address of the asm helper
        //     // to find the correct classlib.

        //     throw GetClasslibException(ExceptionIDs.DivideByZero, address);
        // }

        // [RuntimeExport("RhExceptionHandling_FailedAllocation")]
        // public static void FailedAllocation(EETypePtr pEEType, bool fIsOverflow)
        // {
        //     ExceptionIDs exID = fIsOverflow ? ExceptionIDs.Overflow : ExceptionIDs.OutOfMemory;

        //     // Throw the out of memory exception defined by the classlib, using the input MethodTable*
        //     // to find the correct classlib.

        //     throw pEEType.ToPointer()->GetClasslibException(exID);
        // }

// #if !INPLACE_RUNTIME
//         private static OutOfMemoryException s_theOOMException = new OutOfMemoryException();

//         // MRT exports GetRuntimeException for the few cases where we have a helper that throws an exception
//         // and may be called by either MRT or other classlibs and that helper needs to throw an exception.
//         // There are only a few cases where this happens now (the fast allocation helpers), so we limit the
//         // exception types that MRT will return.
//         [RuntimeExport("GetRuntimeException")]
//         public static Exception GetRuntimeException(ExceptionIDs id)
//         {
//             switch (id)
//             {
//                 case ExceptionIDs.OutOfMemory:
//                     // Throw a preallocated exception to avoid infinite recursion.
//                     return s_theOOMException;

//                 case ExceptionIDs.Overflow:
//                     return new OverflowException();

//                 case ExceptionIDs.InvalidCast:
//                     return new InvalidCastException();

//                 default:
//                     Debug.Assert(false, "unexpected ExceptionID");
//                     FallbackFailFast(RhFailFastReason.InternalError, null);
//                     return null;
//             }
//         }
// #endif

        private enum HwExceptionCode : uint
        {
            STATUS_REDHAWK_NULL_REFERENCE = 0x00000000u,
            STATUS_REDHAWK_UNMANAGED_HELPER_NULL_REFERENCE = 0x00000042u,
            STATUS_REDHAWK_THREAD_ABORT = 0x00000043u,

            STATUS_DATATYPE_MISALIGNMENT = 0x80000002u,
            STATUS_ACCESS_VIOLATION = 0xC0000005u,
            STATUS_INTEGER_DIVIDE_BY_ZERO = 0xC0000094u,
            STATUS_INTEGER_OVERFLOW = 0xC0000095u,
        }

        [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__PAL_LIMITED_CONTEXT)]
        public struct PAL_LIMITED_CONTEXT
        {
            [FieldOffset(AsmOffsets.OFFSETOF__PAL_LIMITED_CONTEXT__IP)]
            internal IntPtr IP;
            [FieldOffset(AsmOffsets.OFFSETOF__PAL_LIMITED_CONTEXT__SP)]
            internal IntPtr SP;
            [FieldOffset(AsmOffsets.OFFSETOF__PAL_LIMITED_CONTEXT__FP)]
            internal UIntPtr FP;
            // the rest of the struct is left unspecified.
        }

        [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__StackTraceInfo)]
        public struct StackTraceInfo
        {

        }

        // N.B. -- These values are burned into the throw helper assembly code and are also known the the
        //         StackFrameIterator code.
        [Flags]
        internal enum ExKind : byte
        {
            None = 0,
            Throw = 1,
            HardwareFault = 2,
            KindMask = 3,

            RethrowFlag = 4,

            SupersededFlag = 8,

            InstructionFaultFlag = 0x10
        }

        [StructLayout(LayoutKind.Explicit)]
        public ref struct ExInfo
        {
            internal void Init(object exceptionObj, bool instructionFault = false)
            {
                // _pPrevExInfo    -- set by asm helper
                // _pExContext     -- set by asm helper
                // _passNumber     -- set by asm helper
                // _kind           -- set by asm helper
                // _idxCurClause   -- set by asm helper
                // _frameIter      -- initialized explicitly during dispatch

                _exception = exceptionObj;
                if (instructionFault)
                    _kind |= ExKind.InstructionFaultFlag;
                _notifyDebuggerSP = UIntPtr.Zero;
            }

            internal void Init(object exceptionObj, ref ExInfo rethrownExInfo)
            {
                // _pPrevExInfo    -- set by asm helper
                // _pExContext     -- set by asm helper
                // _passNumber     -- set by asm helper
                // _idxCurClause   -- set by asm helper
                // _frameIter      -- initialized explicitly during dispatch

                _exception = exceptionObj;
                _kind = rethrownExInfo._kind | ExKind.RethrowFlag;
                _notifyDebuggerSP = UIntPtr.Zero;
            }

            internal object ThrownException
            {
                get
                {
                    return _exception;
                }
            }

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_pPrevExInfo)]
            internal void* _pPrevExInfo;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_pExContext)]
            internal PAL_LIMITED_CONTEXT* _pExContext;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_exception)]
            private object _exception;  // actual object reference, specially reported by GcScanRootsWorker

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_kind)]
            internal ExKind _kind;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_passNumber)]
            internal byte _passNumber;

            // BEWARE: This field is used by the stackwalker to know if the dispatch code has reached the
            //         point at which a handler is called.  In other words, it serves as an "is a handler
            //         active" state where '_idxCurClause == MaxTryRegionIdx' means 'no'.
            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_idxCurClause)]
            internal uint _idxCurClause;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_frameIter)]
            internal StackFrameIterator _frameIter;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_notifyDebuggerSP)]
            internal volatile UIntPtr _notifyDebuggerSP;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_pRD)]
            internal REGDISPLAY *_pRD;

            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_StackTraceInfo)]
            internal StackTraceInfo _stackTraceInfo;
/*
            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_sfLowBound)]
            internal UIntPtr _sfLowBound;
            [FieldOffset(AsmOffsets.OFFSETOF__ExInfo__m_sfHighBound)]
            internal UIntPtr _sfHighBound;
*/
        }

        public static void RhThrowInternalEx(uint exceptionId, ref ExInfo exInfo)
        {
            Exception? exceptionToThrow = null;

            switch (exceptionId)
            {
                case 4:
                    exceptionToThrow = new ArgumentOutOfRangeException();
                    break;
                case 6:
                    exceptionToThrow = new BadImageFormatException();
                    break;
                case 12:
                    exceptionToThrow = new DivideByZeroException();
                    break;
                case 23:
                    exceptionToThrow = new IndexOutOfRangeException();
                    break;
                case 25:
                    exceptionToThrow = new InvalidCastException();
                    break;
                case 40:
                    exceptionToThrow = new AmbiguousImplementationException();
                    break;
                case 43:
                    exceptionToThrow = new NullReferenceException();
                    break;
                case 47:
                    exceptionToThrow = new OverflowException();
                    break;
                case 77:
                    exceptionToThrow = new OutOfMemoryException();
                    break;
                case 78:
                    exceptionToThrow = new ArgumentNullException();
                    break;
                default:
                    FallbackFailFast(RhFailFastReason.InternalError, null);
                    break;
            }

            exInfo.Init(exceptionToThrow!);
            DispatchEx(ref exInfo._frameIter, ref exInfo, MaxTryRegionIdx);
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }

        //
        // Called by RhpThrowHwEx
        //
//        [RuntimeExport("RhThrowHwEx")]
        public static void RhThrowHwEx(uint exceptionCode, ref ExInfo exInfo)
        {
            // trigger a GC (only if gcstress) to ensure we can stackwalk at this point
//            GCStress.TriggerGC();

  //          InternalCalls.RhpValidateExInfoStack();

            IntPtr faultingCodeAddress = exInfo._pExContext->IP;
            bool instructionFault = true;
            ExceptionIDs exceptionId = default(ExceptionIDs);
            Exception? exceptionToThrow = null;

            switch (exceptionCode)
            {
                case (uint)HwExceptionCode.STATUS_REDHAWK_NULL_REFERENCE:
                    exceptionId = ExceptionIDs.NullReference;
                    break;

                case (uint)HwExceptionCode.STATUS_REDHAWK_UNMANAGED_HELPER_NULL_REFERENCE:
                    // The write barrier where the actual fault happened has been unwound already.
                    // The IP of this fault needs to be treated as return address, not as IP of
                    // faulting instruction.
                    instructionFault = false;
                    exceptionId = ExceptionIDs.NullReference;
                    break;

                // case (uint)HwExceptionCode.STATUS_REDHAWK_THREAD_ABORT:
                //     exceptionToThrow = InternalCalls.RhpGetThreadAbortException();
                //     break;

                case (uint)HwExceptionCode.STATUS_DATATYPE_MISALIGNMENT:
                    exceptionId = ExceptionIDs.DataMisaligned;
                    break;

                // N.B. -- AVs that have a read/write address lower than 64k are already transformed to
                //         HwExceptionCode.REDHAWK_NULL_REFERENCE prior to calling this routine.
                case (uint)HwExceptionCode.STATUS_ACCESS_VIOLATION:
                    exceptionId = ExceptionIDs.AccessViolation;
                    break;

                case (uint)HwExceptionCode.STATUS_INTEGER_DIVIDE_BY_ZERO:
                    exceptionId = ExceptionIDs.DivideByZero;
                    break;

                case (uint)HwExceptionCode.STATUS_INTEGER_OVERFLOW:
                    exceptionId = ExceptionIDs.Overflow;
                    break;

                default:
                    // We don't wrap SEH exceptions from foreign code like CLR does, so we believe that we
                    // know the complete set of HW faults generated by managed code and do not need to handle
                    // this case.
                    FailFastViaClasslib(RhFailFastReason.InternalError, null, faultingCodeAddress);
                    break;
            }

            if (exceptionId != default(ExceptionIDs))
            {
                exceptionToThrow = GetClasslibException(exceptionId, faultingCodeAddress);
            }

            exInfo.Init(exceptionToThrow!, instructionFault);
            DispatchEx(ref exInfo._frameIter, ref exInfo, MaxTryRegionIdx);
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }

        private const uint MaxTryRegionIdx = 0xFFFFFFFFu;

        //[RuntimeExport("RhThrowEx")]
        public static void RhThrowEx(object exceptionObj, ref ExInfo exInfo)
        {
            // trigger a GC (only if gcstress) to ensure we can stackwalk at this point
//            GCStress.TriggerGC();

//            InternalCalls.RhpValidateExInfoStack();

            // Transform attempted throws of null to a throw of NullReferenceException.
            if (exceptionObj == null)
            {
                IntPtr faultingCodeAddress = exInfo._pExContext->IP;
                exceptionObj = GetClasslibException(ExceptionIDs.NullReference, faultingCodeAddress);
            }

            exInfo.Init(exceptionObj);
            DispatchEx(ref exInfo._frameIter, ref exInfo, MaxTryRegionIdx);
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }

/*
#pragma warning disable CS8500
        [UnmanagedCallersOnly]
        public static unsafe void RhThrowEx(object* exceptionObjPtr, ExInfo *exInfo)
        {
            // trigger a GC (only if gcstress) to ensure we can stackwalk at this point
//            GCStress.TriggerGC();

//            InternalCalls.RhpValidateExInfoStack();

            object exceptionObj = null; //GetThis(exceptionObjPtr);

            // Transform attempted throws of null to a throw of NullReferenceException.
            if (exceptionObj == null)
            {
                IntPtr faultingCodeAddress = exInfo->_pExContext->IP;
                exceptionObj = GetClasslibException(ExceptionIDs.NullReference, faultingCodeAddress);
            }

            exInfo->Init(exceptionObj);
            DispatchEx(ref exInfo->_frameIter, ref *exInfo, MaxTryRegionIdx);
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }
#pragma warning restore CS8500
*/
        //[RuntimeExport("RhRethrow")]
        public static void RhRethrow(ref ExInfo activeExInfo, ref ExInfo exInfo)
        {
            // trigger a GC (only if gcstress) to ensure we can stackwalk at this point
//            GCStress.TriggerGC();

//            InternalCalls.RhpValidateExInfoStack();

            // We need to copy the exception object to this stack location because collided unwinds
            // will cause the original stack location to go dead.
            object rethrownException = activeExInfo.ThrownException;

            exInfo.Init(rethrownException, ref activeExInfo);
            DispatchEx(ref exInfo._frameIter, ref exInfo, MaxTryRegionIdx);//activeExInfo._idxCurClause);
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }

        private static void DispatchEx(scoped ref StackFrameIterator frameIter, ref ExInfo exInfo, uint startIdx)
        {
            Debug.Assert(exInfo._passNumber == 1, "expected asm throw routine to set the pass");
            object exceptionObj = exInfo.ThrownException;

            // ------------------------------------------------
            //
            // First pass
            //
            // ------------------------------------------------
            UIntPtr handlingFrameSP = MaxSP;
            byte* pCatchHandler = null;
            uint catchingTryRegionIdx = MaxTryRegionIdx;

            bool isFirstRethrowFrame = (startIdx != MaxTryRegionIdx);
            bool isFirstFrame = true;

            byte* prevControlPC = null;
            byte* prevOriginalPC = null;
            UIntPtr prevFramePtr = UIntPtr.Zero;
            bool unwoundReversePInvoke = false;

            bool isValid = frameIter.Init(exInfo._pExContext, exInfo._pRD, (exInfo._kind & ExKind.InstructionFaultFlag) != 0);
            Debug.Assert(isValid, "RhThrowEx called with an unexpected context");

            OnFirstChanceExceptionViaClassLib(exceptionObj);

            for (; isValid; isValid = frameIter.Next(&startIdx, &unwoundReversePInvoke))
            {
                // For GC stackwalking, we'll happily walk across native code blocks, but for EH dispatch, we
                // disallow dispatching exceptions across native code.
                if (unwoundReversePInvoke)
                {
                    handlingFrameSP = frameIter.SP;
//                    System.Diagnostics.Debug.WriteLine($"Found reverse pinvoke frame at SP={handlingFrameSP:X}");
                    pCatchHandler = (byte*)1;
                    break;
                }

                // TODO: move above ^^^
                prevControlPC = frameIter.ControlPC;
                prevOriginalPC = frameIter.OriginalControlPC;

                DebugScanCallFrame(exInfo._passNumber, frameIter.ControlPC, frameIter.SP);

                UpdateStackTrace(exceptionObj, exInfo._frameIter.FramePointer, (IntPtr)frameIter.OriginalControlPC, (IntPtr)frameIter.SP, ref isFirstRethrowFrame, ref prevFramePtr, ref isFirstFrame, ref exInfo);

                byte* pHandler;
                if (FindFirstPassHandler(exceptionObj, startIdx, ref frameIter,
                                         out catchingTryRegionIdx, out pHandler))
                {
                    handlingFrameSP = frameIter.SP;
#if DEBUGEH
                    System.Diagnostics.Debug.WriteLine($"Found first pass handler at {new IntPtr(pHandler):X}, handlingFrameSP={handlingFrameSP:X}");
#endif
                    pCatchHandler = pHandler;

                    DebugVerifyHandlingFrame(handlingFrameSP);
                    break;
                }
            }

            if (pCatchHandler == null)
            {
                OnUnhandledExceptionViaClassLib(exceptionObj);

                UnhandledExceptionFailFastViaClasslib(
                    RhFailFastReason.UnhandledException,
                    exceptionObj,
                    (IntPtr)prevOriginalPC, // IP of the last frame that did not handle the exception
                    ref exInfo);
            }

            // We FailFast above if the exception goes unhandled.  Therefore, we cannot run the second pass
            // without a catch handler.
            Debug.Assert(pCatchHandler != null, "We should have a handler if we're starting the second pass");

            // ------------------------------------------------
            //
            // Second pass
            //
            // ------------------------------------------------

            // Due to the stackwalker logic, we cannot tolerate triggering a GC from the dispatch code once we
            // are in the 2nd pass.  This is because the stackwalker applies a particular unwind semantic to
            // 'collapse' funclets which gets confused when we walk out of the dispatch code and encounter the
            // 'main body' without first encountering the funclet.  The thunks used to invoke 2nd-pass
            // funclets will always toggle this mode off before invoking them.

            // TODO: do we need this?
            //InternalCalls.RhpSetThreadDoNotTriggerGC();

            // TODO: we have a race here. If GC kicks in before we reset the scanned stack range in the exInfo, the stack walked would think that the frames covered
            // by the scanned range were already unwound. This needs to be fixed, as it causes crashes.
            exInfo._passNumber = 2;
            startIdx = MaxTryRegionIdx;
            isValid = frameIter.Init(exInfo._pExContext, exInfo._pRD, (exInfo._kind & ExKind.InstructionFaultFlag) != 0);
            for (; isValid && ((byte*)frameIter.SP <= (byte*)handlingFrameSP); isValid = frameIter.Next(&startIdx))
            {
                Debug.Assert(isValid, "second-pass EH unwind failed unexpectedly");
                DebugScanCallFrame(exInfo._passNumber, frameIter.ControlPC, frameIter.SP);

                if ((frameIter.SP == handlingFrameSP)
#if TARGET_ARM64
                    && (frameIter.ControlPC == prevControlPC)
#endif
                    )
                {
                    if (!unwoundReversePInvoke)
                    {
                        // invoke only a partial second-pass here...
                        InvokeSecondPass(ref exInfo, startIdx, catchingTryRegionIdx);
                    }
//                    System.Diagnostics.Debug.WriteLine($"Second pass terminating at SP={frameIter.SP:X}");
                    break;
                }

                InvokeSecondPass(ref exInfo, startIdx);
            }

            // ------------------------------------------------
            //
            // Call the handler and resume execution
            //
            // ------------------------------------------------
            exInfo._idxCurClause = catchingTryRegionIdx;

#pragma warning disable CS8500
            fixed (EH.ExInfo* pExInfo = &exInfo)
            {
                InternalCalls.RhpCallCatchFunclet(
                    ObjectHandleOnStack.Create(ref exceptionObj), pCatchHandler, frameIter.RegisterSet, pExInfo);
            }
#pragma warning restore CS8500
            // currently, RhpCallCatchFunclet will resume after the catch
            Debug.Assert(false, "unreachable");
            FallbackFailFast(RhFailFastReason.InternalError, null);
        }

        [System.Diagnostics.Conditional("DEBUG")]
        private static void DebugScanCallFrame(int passNumber, byte* ip, UIntPtr sp)
        {
            Debug.Assert(ip != null, "IP address must not be null");
        }

        [System.Diagnostics.Conditional("DEBUG")]
        private static void DebugVerifyHandlingFrame(UIntPtr handlingFrameSP)
        {
            Debug.Assert(handlingFrameSP != MaxSP, "Handling frame must have an SP value");
            Debug.Assert(((UIntPtr*)handlingFrameSP) > &handlingFrameSP,
                "Handling frame must have a valid stack frame pointer");
        }

        private static void UpdateStackTrace(object exceptionObj, UIntPtr curFramePtr, IntPtr ip, IntPtr sp,
            ref bool isFirstRethrowFrame, ref UIntPtr prevFramePtr, ref bool isFirstFrame, ref ExInfo exInfo)
        {
            // We use the fact that all funclet stack frames belonging to the same logical method activation
            // will have the same FramePointer value.  Additionally, the stackwalker will return a sequence of
            // callbacks for all the funclet stack frames, one right after the other.  The classlib doesn't
            // want to know about funclets, so we strip them out by only reporting the first frame of a
            // sequence of funclets.  This is correct because the leafmost funclet is first in the sequence
            // and corresponds to the current 'IP state' of the method.
            if ((prevFramePtr == UIntPtr.Zero) || (curFramePtr != prevFramePtr))
            {
                AppendExceptionStackFrameViaClasslib(exceptionObj, ip, sp, ref exInfo,
                    ref isFirstRethrowFrame, ref isFirstFrame);
            }
            prevFramePtr = curFramePtr;
        }

        private static bool FindFirstPassHandler(object exception, uint idxStart,
            ref StackFrameIterator frameIter, out uint tryRegionIdx, out byte* pHandler)
        {
            pHandler = null;
            tryRegionIdx = MaxTryRegionIdx;

            EHEnum ehEnum;
            byte* pbMethodStartAddress;
            if (!InternalCalls.RhpEHEnumInitFromStackFrameIterator(ref frameIter, &pbMethodStartAddress, &ehEnum))
                return false;

            byte* pbControlPC = frameIter.ControlPC;
#if DEBUGEH
            System.Diagnostics.Debug.WriteLine($"FindFirstPassHandler controlPC={new IntPtr(pbControlPC):X}, idxStart={idxStart}");
#endif
            uint codeOffset = (uint)(pbControlPC - pbMethodStartAddress);

            uint lastTryStart = 0, lastTryEnd = 0;

            // Search the clauses for one that contains the current offset.
            RhEHClause ehClause;
            for (uint curIdx = 0; InternalCalls.RhpEHEnumNext(ref frameIter, &ehEnum, &ehClause); curIdx++)
            {
#if DEBUGEH
                System.Diagnostics.Debug.WriteLine($"Considering clause {curIdx}, _tryStartOffset={ehClause._tryStartOffset:X}, _tryEndOffset={ehClause._tryEndOffset:X}, _clauseKind={ehClause._clauseKind}, _isSameTry={ehClause._isSameTry} for codeOffset={codeOffset:X}");
#endif
                RhEHClauseKind clauseKind = ehClause._clauseKind;

                //
                // Skip to the starting try region.  This is used by collided unwinds and rethrows to pickup where
                // the previous dispatch left off.
                //
                if (idxStart != MaxTryRegionIdx)
                {
                    if (curIdx <= idxStart)
                    {
                        // Q: why is this needed? the last one should be the clause that resulted in invocation of the handler,
                        // so it should be ignored, no?
                        // I've made this change to fix JIT\jit64\localloc\ehverify\eh11_small\eh11_small.dll
                        // if ((clauseKind == RhEHClauseKind.RH_EH_CLAUSE_TYPED) ||
                        //     (clauseKind == RhEHClauseKind.RH_EH_CLAUSE_FILTER))
                        {
                            lastTryStart = ehClause._tryStartOffset; lastTryEnd = ehClause._tryEndOffset;
                        }
#if DEBUGEH
                        System.Diagnostics.Debug.WriteLine($"Dismissing clause, curIdx <= idxStart)");
#endif
                        continue;
                    }

                    // Now, we continue skipping while the try region is identical to the one that invoked the
                    // previous dispatch.
                    if ((ehClause._isSameTry) && (ehClause._tryStartOffset == lastTryStart) && (ehClause._tryEndOffset == lastTryEnd))
                    {
#if DEBUGEH
                        System.Diagnostics.Debug.WriteLine($"Dismissing clause, previously invoked try region");
#endif
                        continue;
                    }

                    // We are done skipping. This is required to handle empty finally block markers that are used
                    // to separate runs of different try blocks with same native code offsets.
                    idxStart = MaxTryRegionIdx;
                }

                if (((clauseKind != RhEHClauseKind.RH_EH_CLAUSE_TYPED) &&
                     (clauseKind != RhEHClauseKind.RH_EH_CLAUSE_FILTER))
                    || !ehClause.ContainsCodeOffset(codeOffset))
                {
#if DEBUGEH
                    System.Diagnostics.Debug.WriteLine($"Dismissing clause, not typed or filter clause or not covering the code offset");
#endif
                    continue;
                }

                // Found a containing clause. Because of the order of the clauses, we know this is the
                // most containing.
                if (clauseKind == RhEHClauseKind.RH_EH_CLAUSE_TYPED)
                {
                    if (ShouldTypedClauseCatchThisException(exception, /*(MethodTable*)*/ehClause._pTargetType))
                    {
#if DEBUGEH
                        System.Diagnostics.Debug.WriteLine($"Accepting typed clause");
#endif
                        pHandler = ehClause._handlerAddress;
                        tryRegionIdx = curIdx;
                        return true;
                    }
                }
                else
                {
                    byte* pFilterFunclet = ehClause._filterAddress;
                    bool shouldInvokeHandler = false;
                    try
                    {
                        shouldInvokeHandler =
                            InternalCalls.RhpCallFilterFunclet(ObjectHandleOnStack.Create(ref exception), pFilterFunclet, frameIter.RegisterSet);
                    }
                    catch when (true)
                    {
                        // Prevent leaking any exception from the filter funclet
                    }

#if DEBUGEH
                    System.Diagnostics.Debug.WriteLine($"Filter returned {shouldInvokeHandler}");
#endif

                    if (shouldInvokeHandler)
                    {
                        pHandler = ehClause._handlerAddress;
                        tryRegionIdx = curIdx;
                        return true;
                    }
                }
            }

            return false;
        }

#if DEBUG && !INPLACE_RUNTIME
        // private static MethodTable* s_pLowLevelObjectType;
        // private static void AssertNotRuntimeObject(MethodTable* pClauseType)
        // {
        //     //
        //     // The C# try { } catch { } clause expands into a typed catch of System.Object.
        //     // Since runtime has its own definition of System.Object, try { } catch { } might not do what
        //     // was intended (catch all exceptions).
        //     //
        //     // This assertion is making sure we don't use try { } catch { } within the runtime.
        //     // The runtime codebase should either use try { } catch (Exception) { } for exception types
        //     // from the runtime or a try { } catch when (true) { } to catch all exceptions.
        //     //

        //     if (s_pLowLevelObjectType == null)
        //     {
        //         // Allocating might fail, but since this is just a debug assert, it's probably fine.
        //         s_pLowLevelObjectType = new System.Object().MethodTable;
        //     }

        //     Debug.Assert(!pClauseType->IsEquivalentTo(s_pLowLevelObjectType));
        // }
#endif // DEBUG && !INPLACE_RUNTIME

        private static bool ShouldTypedClauseCatchThisException(object exception, /*MethodTable**/TypeHandle pClauseType)
        {
#if DEBUG && !INPLACE_RUNTIME
            // AssertNotRuntimeObject(pClauseType);
#endif

#if DEBUGEH
            System.Diagnostics.Debug.WriteLine($"Checking exception {exception}");
#endif
            MethodTable* clauseMT = pClauseType.AsMethodTable();

            bool retry = false;
            do
            {
                MethodTable* mt = RuntimeHelpers.GetMethodTable(exception);
                while (mt != null)
                {
                    if (clauseMT == mt)
                    {
                        return true;
                    }

                    mt = mt->ParentMethodTable;
                }

                if (exception is RuntimeWrappedException ex)
                {
                    exception = ex.WrappedException;
                    retry = true;
                }
            }
            while (retry);

            return false;
            //return pClauseType.AsMethodTable() == RuntimeHelpers.GetMethodTable(exception); // TODO: is this correct and optimal? No, we need to also catch derived classes
            //return IsInstanceOfException(pClauseType.AsMethodTable(), exception);
            //return RuntimeHelpers.AreTypesEquivalent(pClauseType.AsMethodTable(), RuntimeHelpers.GetMethodTable(exception));
        }

        private static void InvokeSecondPass(ref ExInfo exInfo, uint idxStart)
        {
            InvokeSecondPass(ref exInfo, idxStart, MaxTryRegionIdx);
        }
        private static void InvokeSecondPass(ref ExInfo exInfo, uint idxStart, uint idxLimit)
        {
#if DEBUGEH
            System.Diagnostics.Debug.WriteLine($"InvokeSecondPass, idxStart={idxStart}, idxLimit={idxLimit}");
#endif
            EHEnum ehEnum;
            byte* pbMethodStartAddress;
            if (!InternalCalls.RhpEHEnumInitFromStackFrameIterator(ref exInfo._frameIter, &pbMethodStartAddress, &ehEnum))
                return;

            byte* pbControlPC = exInfo._frameIter.ControlPC;

            uint codeOffset = (uint)(pbControlPC - pbMethodStartAddress);

            uint lastTryStart = 0, lastTryEnd = 0;

            // Search the clauses for one that contains the current offset.
            RhEHClause ehClause;
            for (uint curIdx = 0; InternalCalls.RhpEHEnumNext(ref exInfo._frameIter, &ehEnum, &ehClause) && curIdx < idxLimit; curIdx++)
            {
#if DEBUGEH
                System.Diagnostics.Debug.WriteLine($"2nd pass Considering clause {curIdx}, _tryStartOffset={ehClause._tryStartOffset:X}, _tryEndOffset={ehClause._tryEndOffset:X}, _clauseKind={ehClause._clauseKind} for codeOffset={codeOffset:X}");
#endif
                //
                // Skip to the starting try region.  This is used by collided unwinds and rethrows to pickup where
                // the previous dispatch left off.
                //
                if (idxStart != MaxTryRegionIdx)
                {
                    if (curIdx <= idxStart)
                    {
#if DEBUGEH
                        System.Diagnostics.Debug.WriteLine($"2nd pass Dismissing clause, curIdx <= idxStart)");
#endif
                        lastTryStart = ehClause._tryStartOffset; lastTryEnd = ehClause._tryEndOffset;
                        continue;
                    }

                    // Now, we continue skipping while the try region is identical to the one that invoked the
                    // previous dispatch.
                    if ((ehClause._isSameTry) && (ehClause._tryStartOffset == lastTryStart) && (ehClause._tryEndOffset == lastTryEnd))
                    {
#if DEBUGEH
                        System.Diagnostics.Debug.WriteLine($"2nd pass Dismissing clause, try region skipped in pass 1");
#endif
                        continue;
                    }

                    // We are done skipping. This is required to handle empty finally block markers that are used
                    // to separate runs of different try blocks with same native code offsets.
                    idxStart = MaxTryRegionIdx;
                }

                RhEHClauseKind clauseKind = ehClause._clauseKind;

                if ((clauseKind != RhEHClauseKind.RH_EH_CLAUSE_FAULT)
                    || !ehClause.ContainsCodeOffset(codeOffset))
                {
#if DEBUGEH
                    System.Diagnostics.Debug.WriteLine($"2nd pass Dismissing clause, not a FAULT clause or not covering the code offset");
#endif
                    continue;
                }

                // Found a containing clause. Because of the order of the clauses, we know this is the
                // most containing.

                // N.B. -- We need to suppress GC "in-between" calls to finallys in this loop because we do
                // not have the correct next-execution point live on the stack and, therefore, may cause a GC
                // hole if we allow a GC between invocation of finally funclets (i.e. after one has returned
                // here to the dispatcher, but before the next one is invoked).  Once they are running, it's
                // fine for them to trigger a GC, obviously.
                //
                // As a result, RhpCallFinallyFunclet will set this state in the runtime upon return from the
                // funclet, and we need to reset it if/when we fall out of the loop and we know that the
                // method will no longer get any more GC callbacks.

                byte* pFinallyHandler = ehClause._handlerAddress;
                exInfo._idxCurClause = curIdx;

#if DEBUGEH
                System.Diagnostics.Debug.WriteLine($"2nd pass Calling finally funclet");
#endif

#pragma warning disable CS8500
                fixed (EH.ExInfo* pExInfo = &exInfo)
                {
                    InternalCalls.RhpCallFinallyFunclet(pFinallyHandler, exInfo._frameIter.RegisterSet, pExInfo);
                }
#pragma warning restore CS8500
                exInfo._idxCurClause = MaxTryRegionIdx;
            }
        }

        // [UnmanagedCallersOnly(EntryPoint = "RhpFailFastForPInvokeExceptionPreemp", CallConvs = new Type[] { typeof(CallConvCdecl) })]
        // public static void RhpFailFastForPInvokeExceptionPreemp(IntPtr PInvokeCallsiteReturnAddr, void* pExceptionRecord, void* pContextRecord)
        // {
        //     FailFastViaClasslib(RhFailFastReason.UnhandledExceptionFromPInvoke, null, PInvokeCallsiteReturnAddr);
        // }
        // [RuntimeExport("RhpFailFastForPInvokeExceptionCoop")]
        // public static void RhpFailFastForPInvokeExceptionCoop(IntPtr classlibBreadcrumb, void* pExceptionRecord, void* pContextRecord)
        // {
        //     FailFastViaClasslib(RhFailFastReason.UnhandledExceptionFromPInvoke, null, classlibBreadcrumb);
        // }
    } // static class EH


    [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__StackFrameIterator)]
    internal unsafe struct StackFrameIterator
    {
        // [FieldOffset(AsmOffsets.OFFSETOF__StackFrameIterator__m_FramePointer)]
        // private UIntPtr _framePointer;
        // [FieldOffset(AsmOffsets.OFFSETOF__StackFrameIterator__m_ControlPC)]
        // private IntPtr _controlPC;
        [FieldOffset(AsmOffsets.OFFSETOF__StackFrameIterator__m_pRegDisplay)]
        private REGDISPLAY* _pRegDisplay;
        //[FieldOffset(AsmOffsets.OFFSETOF__StackFrameIterator__m_OriginalControlPC)]
        //private IntPtr _originalControlPC;

        internal byte* ControlPC { get { return (byte*)_pRegDisplay->ControlPC; } }
        internal byte* OriginalControlPC { get { return (byte*)_pRegDisplay->ControlPC; } } // TODO: fix this
        internal void* RegisterSet { get { return _pRegDisplay;  } }
        internal UIntPtr SP { get { return _pRegDisplay->SP; } }
        internal UIntPtr FramePointer { get { return _pRegDisplay->m_pCurrentContext->FP; } } // FAKE! TODO: is this always FP register or is it SP or FP?

        internal bool Init(EH.PAL_LIMITED_CONTEXT* pStackwalkCtx, REGDISPLAY *pRD, bool instructionFault = false)
        {
            return InternalCalls.RhpSfiInit(ref this, pStackwalkCtx, pRD, instructionFault);
        }

        internal bool Next()
        {
            return Next(null, null);
        }

        internal bool Next(uint* uExCollideClauseIdx)
        {
            return Next(uExCollideClauseIdx, null);
        }

        internal bool Next(uint* uExCollideClauseIdx, bool* fUnwoundReversePInvoke)
        {
            return InternalCalls.RhpSfiNext(ref this, uExCollideClauseIdx, fUnwoundReversePInvoke);
        }
    }
}
#endif
