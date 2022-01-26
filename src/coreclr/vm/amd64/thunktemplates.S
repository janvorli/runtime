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
LEAF_END StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT
        jmp    [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__Target]
        mov    r10, [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__MethodDesc]
        jmp    [rip + FixupPrecodeCode + 4096 + FixupPrecodeData__PrecodeFixupThunk]
        .rept 5
        nop
        .endr
LEAF_END FixupPrecodeCode, _TEXT
