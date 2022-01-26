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

EXTERN _PrecodeFixupThunk@0:PROC

PrecodeFixupThunkAddress:
     dd _PrecodeFixupThunk@0

_FixupPrecodeCode@0 PROC PUBLIC
        jmp     dword ptr _FixupPrecodeCode@0 + 4096 + FixupPrecodeData__Target
        mov     eax, dword ptr _FixupPrecodeCode@0 + 4096 + FixupPrecodeData__MethodDesc
        jmp     dword ptr PrecodeFixupThunkAddress; The indirect jump uses an absolute address of the indirection slot
        REPEAT  7
        nop
        ENDM

_FixupPrecodeCode@0 ENDP

_VSDLookupStubCode@0 PROC PUBLIC

        push   eax
        push   dword ptr _VSDLookupStubCode@0 + 1000h
        jmp    dword ptr _VSDLookupStubCode@0 + 1004h

_VSDLookupStubCode@0 ENDP

_VSDDispatchStubCode@0 PROC PUBLIC

        push   eax
        mov    eax, dword ptr _VSDLookupStubCode@0 + 1000h
        cmp    dword ptr [ecx],eax
        pop    eax
        jne    NoMatch
        jmp    dword ptr _VSDDispatchStubCode@0 + 1004h
NoMatch:
        jmp    dword ptr _VSDDispatchStubCode@0 + 1008h

_VSDDispatchStubCode@0 ENDP

_VSDResolveStubCode@0 PROC PUBLIC

        sub dword ptr _VSDResolveStubCode@0 + 1010h, 1
        jl Backpatcher
Resolve:
        push    eax
        mov     eax,dword ptr [ecx]
        push    edx
        mov     edx,eax
        shr     eax, 12
        add     eax,edx
        xor     eax,dword ptr _VSDResolveStubCode@0 + 1004h; <<< hashedToken
HashedTokenAddr EQU .-4
        and     eax,dword ptr _VSDResolveStubCode@0 + 1008h; <<< cacheMask
CacheMaskAddr EQU .-4
        add     eax,dword ptr _VSDResolveStubCode@0 + 1000h; <<< lookupCache
LookupCacheAddr EQU .-4
        mov     eax,dword ptr [eax]
        cmp     edx,dword ptr [eax]
        jne     Miss
        mov     edx,dword ptr _VSDResolveStubCode@0 + 100ch; <<< token
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
        push    dword ptr _VSDResolveStubCode@0 + 100ch; <<< token
TokenAddr2 EQU .-4
        jmp     dword ptr _VSDResolveStubCode@0 + 1014h; <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
ResolveWorkerAddr EQU .-4
Backpatcher:
        call    dword ptr _VSDResolveStubCode@0 + 1018h; <<< backpatcherWorker == BackPatchWorkerAsmStub
BackpatcherWorkerAddr EQU .-4
        jmp     Resolve

_VSDResolveStubCode@0 ENDP

        end