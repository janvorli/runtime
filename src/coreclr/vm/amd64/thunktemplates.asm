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

        end
