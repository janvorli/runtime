;; Licensed to the .NET Foundation under one or more agreements.
;; The .NET Foundation licenses this file to you under the MIT license.

include AsmConstants.inc
include AsmMacros.inc

EXTERN g_gc_card_table         : QWORD
EXTERN g_gc_card_bundle_table  : QWORD
EXTERN g_write_barrier_state  : QWORD

;
;   rcx - dest address 
;   rdx - object
;
LEAF_ENTRY RhpCheckedAssignRef, _TEXT

    ; See if dst is in GCHeap
        mov     rax, [g_gc_card_bundle_table] ; fetch the page byte map
        mov     r8,  rcx
        shr     r8,  30                    ; dst page index
        cmp     byte ptr [rax + r8], 0
        jne     RhpCheckedEntry

NotInHeap:
ALTERNATE_ENTRY RhpCheckedAssignRefAVLocation
    mov     [rcx], rdx
    ret
LEAF_END RhpCheckedAssignRef, _TEXT

;
;   rcx - dest address 
;   rdx - object
;
LEAF_ENTRY RhpAssignRef, _TEXT

ifdef FEATURE_SATORI_EXTERNAL_OBJECTS
    ; check if src is in heap
        mov     rax, [g_gc_card_bundle_table] ; fetch the page byte map
    ALTERNATE_ENTRY RhpCheckedEntry
        mov     r8,  rdx
        shr     r8,  30                    ; dst page index
        cmp     byte ptr [rax + r8], 0
        je      JustAssign              ; src not in heap
else
    ALTERNATE_ENTRY RhpCheckedEntry
endif

    ; check for escaping assignment
    ; 1) check if we own the source region
        mov     r8, rdx
        and     r8, 0FFFFFFFFFFE00000h  ; source region

ifndef FEATURE_SATORI_EXTERNAL_OBJECTS
        jz      JustAssign              ; assigning null
endif

        mov     rax,  gs:[30h]          ; thread tag, TEB on NT
        cmp     qword ptr [r8], rax     
        jne     AssignAndMarkCards      ; not local to this thread

    ; 2) check if the src and dst are from the same region
        mov     rax, rcx
        and     rax, 0FFFFFFFFFFE00000h ; target aligned to region
        cmp     rax, r8
        jnz     RecordEscape            ; cross region assignment. definitely escaping

    ; 3) check if the target is exposed
        mov     rax, rcx
        and     rax, 01FFFFFh
        shr     rax, 3
        bt      qword ptr [r8], rax
        jb      RecordEscape            ; target is exposed. record an escape.

    JustAssign:
ALTERNATE_ENTRY RhpAssignRefAVLocationNotHeap
        mov     [rcx], rdx              ; threadlocal assignment of unescaped object
        ret

    AssignAndMarkCards:
ALTERNATE_ENTRY RhpAssignRefAVLocation
        mov     [rcx], rdx

    ; TUNING: barriers in different modes could be separate pieces of code, but barrier switch 
    ;         needs to suspend EE, not sure if skipping mode check would worth that much.
        mov     r11, qword ptr [g_write_barrier_state]

    ; check the barrier state. this must be done after the assignment (in program order)
    ; if state == 2 we do not set or dirty cards.
        cmp     r11, 2h
        jne     DoCards
    Exit:
        ret

    DoCards:
    ; if same region, just check if barrier is not concurrent
        xor     rdx, rcx
        shr     rdx, 21
        jz      CheckConcurrent

    ; if src is in gen2/3 and the barrier is not concurrent we do not need to mark cards
        cmp     dword ptr [r8 + 16], 2
        jl      MarkCards

    CheckConcurrent:
        cmp     r11, 0h
        je      Exit

    MarkCards:
    ; fetch card location for rcx
        mov     r9 , [g_gc_card_table]     ; fetch the page map
        mov     r8,  rcx
        shr     rcx, 30
        mov     rax, qword ptr [r9 + rcx * 8] ; page
        sub     r8, rax   ; offset in page
        mov     rdx,r8
        shr     r8, 9     ; card offset
        shr     rdx, 20   ; group index
        lea     rdx, [rax + rdx * 2 + 80h] ; group offset

    ; check if concurrent marking is in progress
        cmp     r11, 0h
        jne     DirtyCard

    ; SETTING CARD FOR RCX
     SetCard:
        cmp     byte ptr [rax + r8], 0
        jne     Exit
        mov     byte ptr [rax + r8], 1
     SetGroup:
        cmp     byte ptr [rdx], 0
        jne     CardSet
        mov     byte ptr [rdx], 1
     SetPage:
        cmp     byte ptr [rax], 0
        jne     CardSet
        mov     byte ptr [rax], 1

     CardSet:
    ; check if concurrent marking is still not in progress
        cmp     qword ptr [g_write_barrier_state], 0h
        jne     DirtyCard
        ret

    ; DIRTYING CARD FOR RCX
     DirtyCard:
        mov     byte ptr [rax + r8], 4
     DirtyGroup:
        cmp     byte ptr [rdx], 4
        je      Exit
        mov     byte ptr [rdx], 4
     DirtyPage:
        cmp     byte ptr [rax], 4
        je      Exit
        mov     byte ptr [rax], 4
        ret

    ; this is expected to be rare.
    RecordEscape:

        ; 4) check if the source is escaped
        mov     rax, rdx
        add     rax, 8                        ; escape bit is MT + 1
        and     rax, 01FFFFFh
        shr     rax, 3
        bt      qword ptr [r8], rax
        jb      AssignAndMarkCards            ; source is already escaped.

        ; Align rsp
        mov  r9, rsp
        and  rsp, -16

        ; save rsp, rcx, rdx, r8
        push r9
        push rcx
        push rdx
        push r8

        ; also save xmm0, in case it is used for stack clearing, as JIT_ByRefWriteBarrier should not trash xmm0
        ; Hopefully EscapeFn cannot corrupt other xmm regs, since there is no float math or vectorizable code in there.
        sub     rsp, 16
        movdqa  [rsp], xmm0

        ; shadow space
        sub  rsp, 20h

        ; void SatoriRegion::EscapeFn(SatoriObject** dst, SatoriObject* src, SatoriRegion* region)
        call    qword ptr [r8 + 8]

        add     rsp, 20h

        movdqa  xmm0, [rsp]
        add     rsp, 16

        pop     r8
        pop     rdx
        pop     rcx
        pop     rsp
        jmp     AssignAndMarkCards
LEAF_END RhpAssignRef, _TEXT

    end
