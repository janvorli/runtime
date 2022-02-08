// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

LEAF_ENTRY StubPrecodeCode, _TEXT
        mov     eax, dword ptr StubPrecodeCode + 4096 + StubPrecodeData__MethodDesc
        jmp     dword ptr ds:StubPrecodeCode + 4096 + StubPrecodeData__Target
        nop
LEAF_END StubPrecodeCode, _TEXT

ThePreStubAddress:
     dd C_FUNC(ThePreStub)

LEAF_ENTRY FixupPrecodeCode, _TEXT1
        jmp     dword ptr FixupPrecodeCode + 4096 + FixupPrecodeData__Target
        mov     eax, dword ptr FixupPrecodeCode + 4096 + FixupPrecodeData__MethodDesc
        jmp     dword ptr ThePreStubAddress; The indirect jump uses an absolute address of the indirection slot
        .rept  7
        nop
        .endr
LEAF_END FixupPrecodeCode, _TEXT

LEAF_ENTRY LookupStubCode, _TEXT
        push   eax
        push   dword ptr LookupStubCode + 1000h
        jmp    dword ptr LookupStubCode + 1004h
LEAF_END LookupStubCode, _TEXT

LEAF_ENTRY DispatchStubCode, _TEXT
        push   eax
        mov    eax, dword ptr LookupStubCode + 1000h
        cmp    dword ptr [ecx],eax
        pop    eax
        jne    NoMatch
        jmp    dword ptr DispatchStubCode + 1004h
NoMatch:
        jmp    dword ptr DispatchStubCode + 1008h
LEAF_END DispatchStubCode, _TEXT

LEAF_ENTRY ResolveStubCode, _TEXT
        sub dword ptr ResolveStubCode + 1010h, 1
        jl Backpatcher
Resolve:
        push    eax
        mov     eax,dword ptr [ecx]
        push    edx
        mov     edx,eax
        shr     eax, 12
        add     eax,edx
        xor     eax,dword ptr ResolveStubCode + 1004h; <<< hashedToken
HashedTokenAddr EQU .-4
        and     eax,dword ptr ResolveStubCode + 1008h; <<< cacheMask
CacheMaskAddr EQU .-4
        add     eax,dword ptr ResolveStubCode + 1000h; <<< lookupCache
LookupCacheAddr EQU .-4
        mov     eax,dword ptr [eax]
        cmp     edx,dword ptr [eax]
        jne     Miss
        mov     edx,dword ptr ResolveStubCode + 100ch; <<< token
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
        push    dword ptr ResolveStubCode + 100ch; <<< token
TokenAddr2 EQU .-4
        jmp     dword ptr ResolveStubCode + 1014h; <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
ResolveWorkerAddr EQU .-4
Backpatcher:
        call    dword ptr ResolveStubCode + 1018h; <<< backpatcherWorker == BackPatchWorkerAsmStub
BackpatcherWorkerAddr EQU .-4
        jmp     Resolve
LEAF_END ResolveStubCode, _TEXT