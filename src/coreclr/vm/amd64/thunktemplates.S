// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

.intel_syntax noprefix
#include "unixasmmacros.inc"
#include "asmconstants.h"

LEAF_ENTRY StubPrecodeCode, _TEXT
        mov    r10, [rip + StubPrecodeCode + 4096 + StubPrecodeData__MethodDesc]
        jmp    [rip + StubPrecodeCode + 4096 + StubPrecodeData__Target]
        .rept 11
        nop
        .endr
LEAF_END_MARKED StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
        jmp    [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__Target]
        mov    r10, [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__MethodDesc]
        jmp    [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__PrecodeFixupThunk]
        .rept 5
        nop
        .endr
LEAF_END_MARKED FixupPrecodeCode, _TEXT

LEAF_ENTRY CallCountingStubCode, _TEXT
        mov    rax,QWORD PTR [rip + CallCountingStubCode + 4096 + CallCountingStubData__RemainingCallCountCell]
        dec    WORD PTR [rax]
        je     CountReachedZero
        jmp    QWORD PTR [rip + CallCountingStubCode + 4096 + CallCountingStubData__TargetForMethod]
    CountReachedZero:
        call   QWORD PTR [rip + CallCountingStubCode + 4096 + CallCountingStubData__TargetForThresholdReached]
LEAF_END_MARKED CallCountingStubCode, _TEXT

LEAF_ENTRY LookupStubCode, _TEXT
       nop
       push   QWORD PTR [rip + LookupStubCode + 4096 + LookupStubData__DispatchToken]
       jmp    QWORD PTR [rip + LookupStubCode + 4096 + LookupStubData__ResolveWorkerTarget]
LEAF_END_MARKED LookupStubCode, _TEXT

LEAF_ENTRY DispatchStubCode, _TEXT
       mov    rax,QWORD PTR [rip + DispatchStubCode + 4096 + DispatchStubData__ExpectedMT]
PATCH_LABEL DispatchStubCode_ThisDeref
       cmp    QWORD PTR [rdi],rax;
       jne    Fail
       jmp    QWORD PTR [rip + DispatchStubCode + 4096 + DispatchStubData__ImplTarget]
    Fail:
       jmp    QWORD PTR [rip + DispatchStubCode + 4096 + DispatchStubData__FailTarget]
LEAF_END_MARKED DispatchStubCode, _TEXT

LEAF_ENTRY ResolveStubCode, _TEXT
PATCH_LABEL ResolveStubCode_ResolveEntry
        push   rdx
        mov    r10,QWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__CacheAddress]
PATCH_LABEL ResolveStubCode_ThisDeref
        mov    rax,QWORD PTR [rdi]
        mov    rdx,rax
        shr    rax,12
        add    rax,rdx
        xor    eax,DWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__HashedToken]
        and    eax,DWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__CacheMask]
        mov    rax,QWORD PTR [r10+rax*1]
        mov    r10,QWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__Token]
        cmp    rdx,QWORD PTR [rax]
        jne    miss
        cmp    r10,QWORD PTR [rax+8]
        jne    miss
        pop    rdx
        jmp    QWORD PTR [rax+0x10]
PATCH_LABEL ResolveStubCode_FailEntry
        add    DWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__Counter], -1
        //jge    ResolveStubCode
        .byte 0x7d, 0xbc
        or     r11, 1 // SDF_ResolveBackPatch
PATCH_LABEL ResolveStubCode_SlowEntry
        push   rdx
        mov    r10,QWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__Token]
miss:
        push   rax
        jmp    QWORD PTR [rip + ResolveStubCode + 4096 + ResolveStubData__ResolveWorkerTarget]
LEAF_END_MARKED ResolveStubCode, _TEXT
