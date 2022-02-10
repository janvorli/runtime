// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"
#include "asmconstants.h"

.syntax unified
.thumb

    .align 4

    #define DATA_SLOT(stub, field) stub##Code + 4096 + stub##Data__##field

    LEAF_ENTRY StubPrecodeCode
        ldr r12, DATA_SLOT(StubPrecode, MethodDesc)
        ldr pc, DATA_SLOT(StubPrecode, Target)
    LEAF_END StubPrecodeCode

    .align 4

    LEAF_ENTRY FixupPrecodeCode
        ldr pc, DATA_SLOT(FixupPrecode, Target)
        ldr r12, DATA_SLOT(FixupPrecode, MethodDesc)
        ldr pc, DATA_SLOT(FixupPrecode, PrecodeFixupThunk)
    LEAF_END_MARKED FixupPrecodeCode

    .align 4

    LEAF_ENTRY CallCountingStubCode
        push {r0}
        ldr r12, DATA_SLOT(CallCountingStub, RemainingCallCountCell)
        ldrh r0, [r12]
        subs r0, r0, #1
        strh r0, [r12]
        pop {r0}
        beq LCountReachedZero
        ldr pc, DATA_SLOT(CallCountingStub, TargetForMethod)
LCountReachedZero:
        adr r12, CallCountingStubCode
        ldr pc, DATA_SLOT(CallCountingStub, TargetForThresholdReached)
    LEAF_END_MARKED CallCountingStubCode

    .align 4

    LEAF_ENTRY LookupStubCode
        ldr r12, LookupStubCode + 4096 + LookupStubData__DispatchToken
        ldr pc, LookupStubCode + 4096 + LookupStubData__ResolveWorkerTarget
    LEAF_END_MARKED LookupStubCode

    .align 4

    LEAF_ENTRY DispatchStubCode
        ldr r12, [r0]
        push {r5}
        ldr r5, DispatchStubCode + 4096 + DispatchStubData__ExpectedMT
        cmp r5, r12
        pop {r5}
        bne LFailTarget
        ldr pc, DispatchStubCode + 4096 + DispatchStubData__ImplTarget
LFailTarget:
        ldr pc, DispatchStubCode + 4096 + DispatchStubData__FailTarget
    LEAF_END_MARKED DispatchStubCode

    .align 4

    LEAF_ENTRY ResolveStubCode
        ldr r12, [r0]
        push {r5, r6}
        add r6, r12, r12,    lsr #12
        ldr r5, ResolveStubCode + 4096 + ResolveStubData__HashedToken
        eor r6, r6, r5
        ldr r5, ResolveStubCode + 4096 + ResolveStubData__CacheMask
        and r6, r6, r5
        ldr r5, ResolveStubCode + 4096 + ResolveStubData__CacheAddress
        ldr r6, [r5, r6]
loop:
        ldr r5, [r6]
        cmp r12, r5
        bne nextEntry
        ldr r5, ResolveStubCode + 4096 + ResolveStubData__Token
        ldr r12, [r6, #4]
        cmp r12, r5
        bne nextEntry
        ldr r12, [r6, #8]
        pop {r5, r6}
        bx r12
nextEntry:
        ldr r6, [r6, #12]
        cbz r6, slowEntryPoint2
        ldr r12, [r0]
        b loop
slowEntryPoint2:
        pop {r5, r6}
        nop
slowEntryPoint:
        ldr r12, ResolveStubCode + 4096 + ResolveStubData__Token
        ldr pc, ResolveStubCode + 4096 + ResolveStubData__ResolveWorkerTarget
PATCH_LABEL ResolveStubCode_FailEntry
        push {r5}
        adr r5, ResolveStubCode + 4096 + ResolveStubData__Counter
        ldr r12, [r5]
        subs r12, r12, #1
        str r12, [r5]
        pop {r5}
        //str r12, ResolveStubCode + 4096 + ResolveStubData__Counter
        bge ResolveStubCode
        orr r4, r4, #1 // SDF_ResolveBackPatch
        b ResolveStubCode
    LEAF_END_MARKED ResolveStubCode


