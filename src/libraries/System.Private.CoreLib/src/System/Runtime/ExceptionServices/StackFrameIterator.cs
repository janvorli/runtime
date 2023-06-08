// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Runtime;
using System.Runtime.InteropServices;

namespace System.Runtime
{
    [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__REGDISPLAY)]
    internal unsafe struct REGDISPLAY
    {
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__SP)]
        internal UIntPtr SP;
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__ControlPC)]
        internal UIntPtr ControlPC;
        [FieldOffset(AsmOffsets.OFFSETOF__REGDISPLAY__m_pCurrentContext)]
        internal EH.PAL_LIMITED_CONTEXT* m_pCurrentContext;
    }

    [StructLayout(LayoutKind.Explicit, Size = AsmOffsets.SIZEOF__StackFrameIterator)]
    internal unsafe struct StackFrameIterator
    {
        [FieldOffset(AsmOffsets.OFFSETOF__StackFrameIterator__m_pRegDisplay)]
        private REGDISPLAY* _pRegDisplay;

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
