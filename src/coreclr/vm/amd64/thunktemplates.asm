; Licensed to the .NET Foundation under one or more agreements.
; The .NET Foundation licenses this file to you under the MIT license.

include <AsmMacros.inc>
include AsmConstants.inc

LEAF_ENTRY StubPrecodeCode, _TEXT
        mov    r10, QWORD PTR [StubPrecodeCode + 4096 + StubPrecodeData__MethodDesc]
        jmp    QWORD PTR [StubPrecodeCode + 4096 + StubPrecodeData__Target]
        REPEAT 11
        nop
        ENDM
LEAF_END StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
        jmp QWORD PTR [FixupPrecodeCode + 4096 + FixupPrecodeData__Target]
        mov    r10, QWORD PTR [FixupPrecodeCode + 4096 + FixupPrecodeData__MethodDesc]
        jmp    QWORD PTR [FixupPrecodeCode + 4096 + FixupPrecodeData__PrecodeFixupThunk]
        REPEAT 5
        nop
        ENDM
LEAF_END FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
        mov    rax,QWORD PTR [CallCountingStubCode + 4096 + CallCountingStubData__RemainingCallCountCell]        ; 1000 <l0+0xfee>
        dec    WORD PTR [rax]
        je     CountReachedZero
        jmp    QWORD PTR [CallCountingStubCode + 4096 + CallCountingStubData__TargetForMethod]        ; 1008 <l0+0xff6>
    CountReachedZero:
        call   QWORD PTR [CallCountingStubCode + 4096 + CallCountingStubData__TargetForThresholdReached]        ; 1010 <l0+0xffe>
LEAF_END CallCountingStubCode, _TEXT

LEAF_ENTRY LookupStubCode, _TEXT
       nop
       push   QWORD PTR [LookupStubCode + 4096 + LookupStubData__DispatchToken]        ; 1000 <_main+0x1000>
       jmp    QWORD PTR [LookupStubCode + 4096 + LookupStubData__ResolveWorkerTarget]        ; 1008 <_main+0x1008>
LEAF_END LookupStubCode, _TEXT

LEAF_ENTRY DispatchStubCode, _TEXT
       mov    rax,QWORD PTR [DispatchStubCode + 4096 + DispatchStubData__ExpectedMT]        ; 1000 <fail+0xfee> <<< _expectedMT
       cmp    QWORD PTR [rcx],rax;
       jne    Fail
       jmp    QWORD PTR [DispatchStubCode + 4096 + DispatchStubData__ImplTarget]        ;1008 <fail+0xff6> <<< _implTarget
    Fail:
       jmp    QWORD PTR [DispatchStubCode + 4096 + DispatchStubData__FailTarget]        ; 1010 <fail+0xffe> <<< _failTarget
LEAF_END DispatchStubCode, _TEXT

LEAF_ENTRY ResolveStubCode, _TEXT
        push   rdx
        mov    r10,QWORD PTR [ResolveStubCode + 4096 + ResolveStubData__CacheAddress]        ; 1000 <miss+0xfac>
        mov    rax,QWORD PTR [rcx]
        mov    rdx,rax
        shr    rax,12
        add    rax,rdx
        xor    eax,DWORD PTR [ResolveStubCode + 4096 + ResolveStubData__HashedToken]        ; 1008 <miss+0xfb4>
        and    eax,DWORD PTR [ResolveStubCode + 4096 + ResolveStubData__CacheMask]        ; 100c <miss+0xfb8>
        mov    rax,QWORD PTR [r10+rax*1]
        mov    r10,QWORD PTR [ResolveStubCode + 4096 + ResolveStubData__Token]        ; 1010 <miss+0xfbc>
        cmp    rdx,QWORD PTR [rax]
        jne    miss
        cmp    r10,QWORD PTR [rax+8]
        jne    miss
        pop    rdx
        jmp    QWORD PTR [rax+10h]
PATCH_LABEL ResolveStubCode_FailEntry
        add    DWORD PTR [ResolveStubCode + 4096 + ResolveStubData__Counter], -1 ; 1018 <miss+0xfc4>
        jge    ResolveStubCode
        or     r11, 1; SDF_ResolveBackPatch
PATCH_LABEL ResolveStubCode_SlowEntry
        push   rdx
        mov    r10,QWORD PTR [ResolveStubCode + 4096 + ResolveStubData__Token]        ; 1010 <miss+0xfcc>
miss:
        push   rax
        jmp    QWORD PTR [ResolveStubCode + 4096 + ResolveStubData__ResolveWorkerTarget]        ; 1020 <miss+0xfd4>
LEAF_END ResolveStubCode, _TEXT

        end
