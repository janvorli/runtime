// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

.intel_syntax noprefix
#include "unixasmmacros.inc"
#include "asmconstants.h"

NESTED_ENTRY ThePreStub, _TEXT, NoHandler
        PROLOG_WITH_TRANSITION_BLOCK 0, 0, 0, 0, 0

        //
        // call PreStubWorker
        //
        lea             rdi, [rsp + __PWTB_TransitionBlock]     // pTransitionBlock*
        mov             rsi, METHODDESC_REGISTER
        call            C_FUNC(PreStubWorker)

        EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
        TAILJMP_RAX

NESTED_END ThePreStub, _TEXT

LEAF_ENTRY ThePreStubPatch, _TEXT
        // make sure that the basic block is unique
        test            eax,34
PATCH_LABEL ThePreStubPatchLabel
        ret
LEAF_END ThePreStubPatch, _TEXT

LEAF_ENTRY StubPrecodeCode, _TEXT

__TargetSlot = StubPrecodeCode + 0x1000
__MethodDescSlot = StubPrecodeCode + 0x1008

        mov    r10, [MethodDescSlot]
        jmp    [__TargetSlot]
        REPEAT 11
        nop
        ENDM
LEAF_END StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT

__TargetSlot = FixupPrecodeCode + 0x1000
__MethodDescSlot = FixupPrecodeCode + 0x1008
__PrecodeFixupThunkSlot = FixupPrecodeCode + 0x1010

        jmp [rip + __TargetSlot]
        mov    r10, [rip + __MethodDescSlot]
        jmp    [rip + __PrecodeFixupThunkSlot]
        REPEAT 5
        nop
        ENDM
LEAF_END FixupPrecodeCode, _TEXT


