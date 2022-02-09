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

_StubPrecodeCode@0 PROC PUBLIC
        mov     eax, dword ptr _StubPrecodeCode@0 + 4096 + StubPrecodeData__MethodDesc
        jmp     dword ptr ds:_StubPrecodeCode@0 + 4096 + StubPrecodeData__Target
        nop
_StubPrecodeCode@0 ENDP

EXTERN _ThePreStub@0:PROC

ThePreStubAddress:
        dd _ThePreStub@0

_FixupPrecodeCode@0 PROC PUBLIC
        jmp     dword ptr _FixupPrecodeCode@0 + 4096 + FixupPrecodeData__Target
        mov     eax, dword ptr _FixupPrecodeCode@0 + 4096 + FixupPrecodeData__MethodDesc
        jmp     dword ptr ThePreStubAddress; The indirect jump uses an absolute address of the indirection slot
        REPEAT  7
        nop
        ENDM
_FixupPrecodeCode@0 ENDP

_CallCountingStubCode@0 PROC PUBLIC
        mov    eax, dword ptr _CallCountingStubCode@0 + 4096 +  CallCountingStubData__RemainingCallCountCell
_RemainingCallCountCellOffset EQU $-4-_CallCountingStubCode@0
        dec    WORD PTR [eax]
        je     CountReachedZero
        jmp    dword ptr  _CallCountingStubCode@0 + 4096 +  CallCountingStubData__TargetForMethod
_TargetForMethodOffset EQU $-4-_CallCountingStubCode@0
CountReachedZero:
        call   dword ptr  _CallCountingStubCode@0 + 4096 +  CallCountingStubData__TargetForThresholdReached
_TargetForThresholdReachedOffset EQU $-4-_CallCountingStubCode@0
        int    3
_CallCountingStubCode@0 ENDP

PUBLIC _RemainingCallCountCellOffset
PUBLIC _TargetForMethodOffset
PUBLIC _TargetForThresholdReachedOffset

_LookupStubCode@0 PROC PUBLIC
        push   eax
        push   dword ptr _LookupStubCode@0 + 4096 + LookupStubData__DispatchToken
        jmp    dword ptr _LookupStubCode@0 + 4096 + LookupStubData__ResolveWorkerTarget
_LookupStubCode@0 ENDP

_DispatchStubCode@0 PROC PUBLIC
        push   eax
        mov    eax, dword ptr _DispatchStubCode@0 + 4096 + DispatchStubData__ExpectedMT
        cmp    dword ptr [ecx],eax
        pop    eax
        jne    NoMatch
        jmp    dword ptr _DispatchStubCode@0 + 4096 + DispatchStubData__ImplTarget
NoMatch:
        jmp    dword ptr _DispatchStubCode@0 + 4096 + DispatchStubData__FailTarget
_DispatchStubCode@0 ENDP

_ResolveStubCode@0 PROC PUBLIC
        sub dword ptr _ResolveStubCode@0 + 1010h, 1
        jl Backpatcher
Resolve:
        push    eax
        mov     eax,dword ptr [ecx]
        push    edx
        mov     edx,eax
        shr     eax, 12
        add     eax,edx
        xor     eax,dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__HashedToken
HashedTokenAddr EQU .-4
        and     eax,dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__CacheMask
CacheMaskAddr EQU .-4
        add     eax,dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__CacheAddress
LookupCacheAddr EQU .-4
        mov     eax,dword ptr [eax]
        cmp     edx,dword ptr [eax]
        jne     Miss
        mov     edx,dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__Token
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
        push    dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__Token
TokenAddr2 EQU .-4
        jmp     dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__ResolveWorkerTarget; <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
ResolveWorkerAddr EQU .-4
Backpatcher:
        call    dword ptr _ResolveStubCode@0 + 4096 + ResolveStubData__PatcherTarget; <<< backpatcherWorker == BackPatchWorkerAsmStub
BackpatcherWorkerAddr EQU .-4
        jmp     Resolve
_ResolveStubCode@0 ENDP

        end