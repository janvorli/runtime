; Licensed to the .NET Foundation under one or more agreements.
; The .NET Foundation licenses this file to you under the MIT license.

include <AsmMacros.inc>
include AsmConstants.inc

        extern  PreStubWorker:proc
        extern  ProcessCLRException:proc

NESTED_ENTRY ThePreStub, _TEXT, ProcessCLRException

        PROLOG_WITH_TRANSITION_BLOCK

        ;
        ; call PreStubWorker
        ;
        lea             rcx, [rsp + __PWTB_TransitionBlock]     ; pTransitionBlock*
        mov             rdx, METHODDESC_REGISTER
        call            PreStubWorker

        EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
        TAILJMP_RAX

NESTED_END ThePreStub, _TEXT

LEAF_ENTRY ThePreStubPatch, _TEXT
        ; make sure that the basic block is unique
        test            eax,34
PATCH_LABEL ThePreStubPatchLabel
        ret
LEAF_END ThePreStubPatch, _TEXT

LEAF_ENTRY StubPrecodeCode, _TEXT

TargetSlot EQU StubPrecodeCode + 1000h
MethodDescSlot EQU StubPrecodeCode + 1008h

        mov    r10, QWORD PTR [MethodDescSlot]
        jmp    QWORD PTR [TargetSlot]
        REPEAT 11
        nop
        ENDM
LEAF_END StubPrecodeCode, _TEXT

LEAF_ENTRY FixupPrecodeCode, _TEXT

FixupPrecodeCode_TargetSlot EQU FixupPrecodeCode + 1000h
FixupPrecodeCode_MethodDescSlot EQU FixupPrecodeCode + 1008h
PrecodeFixupThunkSlot EQU FixupPrecodeCode + 1010h

        jmp QWORD PTR [FixupPrecodeCode_TargetSlot]
        mov    r10, QWORD PTR [FixupPrecodeCode_MethodDescSlot]       
        jmp    QWORD PTR [PrecodeFixupThunkSlot]
        REPEAT 5
        nop
        ENDM
LEAF_END FixupPrecodeCode, _TEXT

end
