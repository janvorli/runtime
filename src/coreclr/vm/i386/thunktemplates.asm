; Licensed to the .NET Foundation under one or more agreements.
; The .NET Foundation licenses this file to you under the MIT license.

        .586
        .model  flat

include <AsmMacros.inc>
include AsmConstants.inc

        option  casemap:none
        .code

.686P
.XMM

PAGE_SIZE EQU 4096

DATA_SLOT macro stub, field
    exitm @CatStr(<_>, stub, <Code@0 + 4096 + >, stub, <Data__>, field)
endm

SLOT_ADDRESS_PATCH_LABEL macro stub, field
    <_>&stub&_&field&_Offset EQU $-4-<_>&stub&<@0>
    PUBLIC <_>&stub&_&field&_Offset
endm

LEAF_ENTRY _StubPrecodeCode@0
        mov     eax, dword ptr DATA_SLOT(StubPrecode, MethodDesc)
        jmp     dword ptr DATA_SLOT(StubPrecode, Target)
        nop
LEAF_END_MARKED _StubPrecodeCode@0

EXTERN _ThePreStub@0:PROC

ThePreStubAddress:
        dd _ThePreStub@0

LEAF_ENTRY _FixupPrecodeCode@0
        jmp     dword ptr DATA_SLOT(FixupPrecode, Target)
        mov     eax, dword ptr DATA_SLOT(FixupPrecode, MethodDesc)
        jmp     dword ptr ThePreStubAddress; The indirect jump uses an absolute address of the indirection slot
        REPEAT  7
        nop
        ENDM
LEAF_END_MARKED _FixupPrecodeCode@0

LEAF_ENTRY _CallCountingStubCode@0
        mov    eax, dword ptr DATA_SLOT(CallCountingStub, RemainingCallCountCell)
SLOT_ADDRESS_PATCH_LABEL CallCountingStub, RemainingCallCountCell
;_CallCountingStubCode_RemainingCallCountCellOffset EQU $-4-_CallCountingStubCode@0
;PUBLIC _CallCountingStubCode_RemainingCallCountCellOffset
        dec    WORD PTR [eax]
        je     CountReachedZero
        jmp    dword ptr  DATA_SLOT(CallCountingStub, TargetForMethod)
_CallCountingStubCode_TargetForMethodOffset EQU $-4-_CallCountingStubCode@0
PUBLIC _CallCountingStubCode_TargetForMethodOffset
CountReachedZero:
        call   dword ptr  DATA_SLOT(CallCountingStub, TargetForThresholdReached)
_CallCountingStubCode_TargetForThresholdReachedOffset EQU $-4-_CallCountingStubCode@0
PUBLIC _CallCountingStubCode_TargetForThresholdReachedOffset
        int    3
LEAF_END_MARKED _CallCountingStubCode@0

LEAF_ENTRY _LookupStubCode@0
        push   eax
        push   dword ptr DATA_SLOT(LookupStub, DispatchToken)
        jmp    dword ptr DATA_SLOT(LookupStub, ResolveWorkerTarget)
LEAF_END_MARKED _LookupStubCode@0

LEAF_ENTRY _DispatchStubCode@0
        push   eax
        mov    eax, dword ptr DATA_SLOT(DispatchStub, ExpectedMT)
        cmp    dword ptr [ecx],eax
        pop    eax
        jne    NoMatch
        jmp    dword ptr DATA_SLOT(DispatchStub, ImplTarget)
NoMatch:
        jmp    dword ptr DATA_SLOT(DispatchStub, FailTarget)
LEAF_END_MARKED _DispatchStubCode@0

LEAF_ENTRY _ResolveStubCode@0
_ResolveStubCode_FailEntry@0:
PUBLIC _ResolveStubCode_FailEntry@0
        sub dword ptr _ResolveStubCode@0 + 1010h, 1
        jl Backpatcher
Resolve:
        push    eax
        mov     eax,dword ptr [ecx]
        push    edx
        mov     edx,eax
        shr     eax, 12
        add     eax,edx
        xor     eax,dword ptr DATA_SLOT(ResolveStub, HashedToken)
HashedTokenAddr EQU .-4
        and     eax,dword ptr DATA_SLOT(ResolveStub, CacheMask)
CacheMaskAddr EQU .-4
        add     eax,dword ptr DATA_SLOT(ResolveStub, CacheAddress)
LookupCacheAddr EQU .-4
        mov     eax,dword ptr [eax]
        cmp     edx,dword ptr [eax]
        jne     Miss
        mov     edx,dword ptr DATA_SLOT(ResolveStub, Token)
TokenAddr1 EQU .-4
        cmp     edx,dword ptr [eax + 4]
        jne     Miss
        mov     eax,dword ptr [eax + 8]
        pop     edx
        add     esp, 4
        jmp     eax
Miss:
        pop     edx
Slow:
        push    dword ptr DATA_SLOT(ResolveStub, Token)
TokenAddr2 EQU .-4
        jmp     dword ptr DATA_SLOT(ResolveStub, ResolveWorkerTarget); <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
ResolveWorkerAddr EQU .-4
Backpatcher:
        call    dword ptr DATA_SLOT(ResolveStub, PatcherTarget); <<< backpatcherWorker == BackPatchWorkerAsmStub
BackpatcherWorkerAddr EQU .-4
        jmp     Resolve
LEAF_END_MARKED _ResolveStubCode@0

        end