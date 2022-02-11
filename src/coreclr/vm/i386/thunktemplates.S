// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

PAGE_SIZE EQU 4096

#define DATA_SLOT(stub, field) stub##Code + PAGE_SIZE + stub##Data__##field

.macro SLOT_ADDRESS_PATCH_LABEL stub, field, offset=-4, index
    C_FUNC(\stub\()Code_\field\()Offset\index) EQU .\offset-_\stub\()Code
    .global C_FUNC(\stub\()Code_\field\()Offset\index)
.endm

LEAF_ENTRY StubPrecodeCode
        mov     eax, dword ptr DATA_SLOT(StubPrecode, MethodDesc)
SLOT_ADDRESS_PATCH_LABEL StubPrecode, MethodDesc
        jmp     dword ptr DATA_SLOT(StubPrecode, Target)
SLOT_ADDRESS_PATCH_LABEL StubPrecode, Target
        nop
LEAF_END_MARKED StubPrecodeCode

ThePreStubAddress:
        .dword C_FUNC(ThePreStub)

LEAF_ENTRY FixupPrecodeCode
        jmp     dword ptr DATA_SLOT(FixupPrecode, Target)
SLOT_ADDRESS_PATCH_LABEL FixupPrecode, Target
        mov     eax, dword ptr DATA_SLOT(FixupPrecode, MethodDesc)
SLOT_ADDRESS_PATCH_LABEL FixupPrecode, MethodDesc
        jmp     dword ptr ThePreStubAddress // The indirect jump uses an absolute address of the indirection slot
        ENDM
LEAF_END_MARKED FixupPrecodeCode

LEAF_ENTRY CallCountingStubCode
        mov    eax, dword ptr DATA_SLOT(CallCountingStub, RemainingCallCountCell)
SLOT_ADDRESS_PATCH_LABEL CallCountingStub, RemainingCallCountCell
        dec    WORD PTR [eax]
        je     CountReachedZero
        jmp    dword ptr  DATA_SLOT(CallCountingStub, TargetForMethod)
SLOT_ADDRESS_PATCH_LABEL CallCountingStub, TargetForMethod
CountReachedZero:
        call   dword ptr  DATA_SLOT(CallCountingStub, TargetForThresholdReached)
SLOT_ADDRESS_PATCH_LABEL CallCountingStub, TargetForThresholdReached
        int    3
LEAF_END_MARKED CallCountingStubCode

LEAF_ENTRY LookupStubCode
        push   eax
        push   dword ptr DATA_SLOT(LookupStub, DispatchToken)
SLOT_ADDRESS_PATCH_LABEL LookupStub, DispatchToken
        jmp    dword ptr DATA_SLOT(LookupStub, ResolveWorkerTarget)
SLOT_ADDRESS_PATCH_LABEL LookupStub, ResolveWorkerTarget
LEAF_END_MARKED LookupStubCode

LEAF_ENTRY DispatchStubCode
        push   eax
        mov    eax, dword ptr DATA_SLOT(DispatchStub, ExpectedMT)
SLOT_ADDRESS_PATCH_LABEL DispatchStub, ExpectedMT
PATCH_LABEL _DispatchStubCode_ThisDeref
        cmp    dword ptr [ecx],eax
        pop    eax
        jne    NoMatch
        jmp    dword ptr DATA_SLOT(DispatchStub, ImplTarget)
SLOT_ADDRESS_PATCH_LABEL DispatchStub, ImplTarget
NoMatch:
        jmp    dword ptr DATA_SLOT(DispatchStub, FailTarget)
SLOT_ADDRESS_PATCH_LABEL DispatchStub, FailTarget
LEAF_END_MARKED DispatchStubCode

LEAF_ENTRY ResolveStubCode
_ResolveStubCode_FailEntry:
PUBLIC _ResolveStubCode_FailEntry
        sub dword ptr DATA_SLOT(ResolveStub, Counter), 1
SLOT_ADDRESS_PATCH_LABEL ResolveStub, Counter, -5
        jl Backpatcher
PATCH_LABEL _ResolveStubCode_ResolveEntry
        push    eax
PATCH_LABEL _ResolveStubCode_ThisDeref
        mov     eax,dword ptr [ecx]
        push    edx
        mov     edx,eax
        shr     eax, 12
        add     eax,edx
        xor     eax,dword ptr DATA_SLOT(ResolveStub, HashedToken)
SLOT_ADDRESS_PATCH_LABEL ResolveStub, HashedToken
        and     eax,dword ptr DATA_SLOT(ResolveStub, CacheMask)
SLOT_ADDRESS_PATCH_LABEL ResolveStub, CacheMask
        add     eax,dword ptr DATA_SLOT(ResolveStub, CacheAddress)
SLOT_ADDRESS_PATCH_LABEL ResolveStub, CacheAddress
        mov     eax,dword ptr [eax]
        cmp     edx,dword ptr [eax]
        jne     Miss
        mov     edx,dword ptr DATA_SLOT(ResolveStub, Token)
SLOT_ADDRESS_PATCH_LABEL ResolveStub, Token,, 1
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
SLOT_ADDRESS_PATCH_LABEL ResolveStub, Token,, 2
        jmp     dword ptr DATA_SLOT(ResolveStub, ResolveWorkerTarget) // <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
SLOT_ADDRESS_PATCH_LABEL ResolveStub, ResolveWorkerTarget
Backpatcher:
        call    dword ptr DATA_SLOT(ResolveStub, PatcherTarget) // <<< backpatcherWorker == BackPatchWorkerAsmStub
SLOT_ADDRESS_PATCH_LABEL ResolveStub, PatcherTarget
        jmp     ResolveStubCode_ResolveEntry
LEAF_END_MARKED ResolveStubCode
