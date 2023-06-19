// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

//
// This is where we group together all the internal calls.
//

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace System.Runtime
{
    internal static partial class InternalCalls
    {
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpSfiInit")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpSfiInit(ref StackFrameIterator pThis, void* pStackwalkCtx, [MarshalAs(UnmanagedType.Bool)] bool instructionFault);

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpSfiNext")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpSfiNext(ref StackFrameIterator pThis, uint* uExCollideClauseIdx, bool* fUnwoundReversePInvoke);

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallCatchFunclet")]
        internal static unsafe partial IntPtr RhpCallCatchFunclet(
            ObjectHandleOnStack exceptionObj, byte* pHandlerIP, void* pvRegDisplay, EH.ExInfo* exInfo);
#pragma warning restore CS8500

        internal static unsafe IntPtr RhpCallCatchFunclet(
            object exception, byte* pHandlerIP, void* pvRegDisplay, ref EH.ExInfo exInfo)
        {
#pragma warning disable CS8500
            fixed (EH.ExInfo* pExInfo = &exInfo)
            {
                return InternalCalls.RhpCallCatchFunclet(
                    ObjectHandleOnStack.Create(ref exception), pHandlerIP, pvRegDisplay, pExInfo);
            }
#pragma warning restore CS8500
        }

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallFinallyFunclet")]
        internal static unsafe partial void RhpCallFinallyFunclet(byte* pHandlerIP, void* pvRegDisplay, EH.ExInfo* exInfo);
#pragma warning restore CS8500

        internal static unsafe void RhpCallFinallyFunclet(byte* pHandlerIP, void* pvRegDisplay, ref EH.ExInfo exInfo)
        {
#pragma warning disable CS8500
            fixed (EH.ExInfo* pExInfo = &exInfo)
            {
                InternalCalls.RhpCallFinallyFunclet(pHandlerIP, pvRegDisplay, pExInfo);
            }
#pragma warning restore CS8500
        }

#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpCallFilterFunclet")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpCallFilterFunclet(
            ObjectHandleOnStack exceptionObj, byte* pFilterIP, void* pvRegDisplay);
#pragma warning restore CS8500

        internal static unsafe bool RhpCallFilterFunclet(object exception, byte* pFilterIP, void* pvRegDisplay)
        {
            return InternalCalls.RhpCallFilterFunclet(ObjectHandleOnStack.Create(ref exception), pFilterIP, pvRegDisplay);
        }

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpEHEnumInitFromStackFrameIterator")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpEHEnumInitFromStackFrameIterator(ref StackFrameIterator pFrameIter, byte** pMethodStartAddress, void* pEHEnum);

        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpEHEnumNext")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static unsafe partial bool RhpEHEnumNext(void* pEHEnum, void* pEHClause);

        // TODO: rename?
#pragma warning disable CS8500
        [LibraryImport(RuntimeHelpers.QCall, EntryPoint = "RhpAppendExceptionStackFrame")]
        internal static unsafe partial void RhpAppendExceptionStackFrame(ObjectHandleOnStack exceptionObj, IntPtr ip, UIntPtr sp, int flags, EH.ExInfo* exInfo);
#pragma warning restore CS8500

        internal static unsafe void RhpAppendExceptionStackFrame(object exception, IntPtr ip, UIntPtr sp, int flags, ref EH.ExInfo exInfo)
        {
#pragma warning disable CS8500
            fixed (EH.ExInfo* pExInfo = &exInfo)
            {
                InternalCalls.RhpAppendExceptionStackFrame(ObjectHandleOnStack.Create(ref exception), ip, sp, flags, pExInfo);
            }
#pragma warning restore CS8500
        }
    }
}
