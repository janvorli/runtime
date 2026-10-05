;; Licensed to the .NET Foundation under one or more agreements.
;; The .NET Foundation licenses this file to you under the MIT license.

#include "ksarm64.h"
#include "asmconstants.h"
#include "asmmacros.h"

    TEXTAREA

    IMPORT g_gc_card_table
    IMPORT g_gc_card_bundle_table
    IMPORT g_write_barrier_state

;; Macro for loading a 64bit value of a global variable into a register
    MACRO
        PREPARE_EXTERNAL_VAR_INDIRECT $Name, $Reg

        adrp $Reg, $Name
        ldr  $Reg, [$Reg, $Name]
    MEND

;; JIT_CheckedWriteBarrier(Object** dst, Object* src)
;;
;; Write barrier for writes to objects that may reside
;; on the managed heap.
;;
;; On entry:
;;   x14 : the destination address (LHS of the assignment).
;;         May not be a heap location (hence the checked).
;;   x15 : the object reference (RHS of the assignment).
;;
;; On exit:
;;   x12  : trashed
;;   x14  : preserved (the destination address is not modified)
;;   x15  : trashed
;;   x16  : trashed (ip0)
;;   x17  : trashed (ip1)
    LEAF_ENTRY RhpCheckedAssignRef, _TEXT
    ;; See if dst is in GCHeap
    ALTERNATE_ENTRY RelocSite_CheckedAssignRef_CardBundleTable
        PREPARE_EXTERNAL_VAR_INDIRECT g_gc_card_bundle_table, x16
        lsr     x17, x14, #30                       ;; dst page index
        ldrb    w12, [x16, x17]
        cbnz    x12, RhpCheckedEntry

NotInHeap
    ALTERNATE_ENTRY RhpCheckedAssignRefAVLocation
        str  x15, [x14]
        ret  lr
    LEAF_END_MARKED RhpCheckedAssignRef

;; JIT_WriteBarrier(Object** dst, Object* src)
;;
;; Write barrier for writes to objects that are known to
;; reside on the managed heap.
;;
;; On entry:
;;   x14  : the destination address (LHS of the assignment)
;;   x15  : the object reference (RHS of the assignment)
;;
;; On exit:
;;   x12  : trashed
;;   x14  : preserved (the destination address is not modified)
;;   x15  : trashed
;;   x16  : trashed (ip0)
;;   x17  : trashed (ip1)
    LEAF_ENTRY RhpAssignRef, _TEXT
    ;; check for escaping assignment
    ;; 1) check if we own the source region
#ifdef FEATURE_SATORI_EXTERNAL_OBJECTS
        ALTERNATE_ENTRY RelocSite_AssignRef_CardBundleTable
        PREPARE_EXTERNAL_VAR_INDIRECT g_gc_card_bundle_table, x16
    ALTERNATE_ENTRY RhpCheckedEntry
        lsr     x17, x15, #30                   ;; source page index
        ldrb    w12, [x16, x17]
        cbz     x12, JustAssign                 ;; null or external (immutable) object
#else
    ALTERNATE_ENTRY RhpCheckedEntry
        cbz     x15, JustAssign                 ;; assigning null
#endif
        and     x16,  x15, #0xFFFFFFFFFFE00000  ;; source region
        ldr     x12, [x16]                      ;; region tag

        cmp     x12, x18                        ;; x18 - TEB
        bne     AssignAndMarkCards              ;; not local to this thread

    ;; 2) check if the src and dst are from the same region
        and     x12, x14, #0xFFFFFFFFFFE00000   ;; target aligned to region
        cmp     x12, x16
        bne     RecordEscape                    ;; cross region assignment. definitely escaping

    ;; 3) check if the target is exposed
        ubfx    x17, x14,#9,#12                 ;; word index = (dst >> 9) & 0x1FFFFF
        ldr     x17, [x16, x17, lsl #3]         ;; mark word = [region + index * 8]
        lsr     x12, x14, #3                    ;; bit = (dst >> 3) [& 63]
        lsr     x17, x17, x12
        tbnz    x17, #0, RecordEscape           ;; target is exposed. record an escape.

    ;; UNORDERED! assignment of unescaped, null or external (immutable) object
JustAssign
    ALTERNATE_ENTRY RhpAssignRefAVLocationNotHeap
        str      x15, [x14]
        ret      lr

AssignAndMarkCards
    ALTERNATE_ENTRY RhpAssignRefAVLocation
        stlr    x15, [x14]

    ; TUNING: barriers in different modes could be separate pieces of code, but barrier switch 
    ;         needs to suspend EE, not sure if skipping mode check would worth that much.
        ALTERNATE_ENTRY RelocSite_AssignAndMarkCards_WriteBarrierState
        PREPARE_EXTERNAL_VAR_INDIRECT g_write_barrier_state, x17
    ; check the barrier state. this must be done after the assignment (in program order
    ; if state == 2 we do not set or dirty cards.
        tbz     x17, #1, DoCards

ExitNoCards
        ret     lr

DoCards
    ; if same region, just check if barrier is not concurrent
        and     x12, x14, #0xFFFFFFFFFFE00000   ; target aligned to region
        cmp     x12, x16
        beq     CheckConcurrent    ; same region, just check if barrier is not concurrent

    ; if src is in gen2/3 and the barrier is not concurrent we do not need to mark cards
        ldr     w12, [x16, 16]                  ; source region + 16 -> generation
        tbz     x12, #1, MarkCards

CheckConcurrent
    ; if not concurrent, exit
        cbz     x17, ExitNoCards

MarkCards
    ; need couple temps. Save before using.
        stp     x2,  x3,  [sp, -16]!

    ; fetch card location for x14
        ALTERNATE_ENTRY RelocSite_MarkCards_CardTable
        PREPARE_EXTERNAL_VAR_INDIRECT g_gc_card_table, x12  ; fetch the page map
        lsr     x16, x14, #30
        ldr     x16, [x12, x16, lsl #3]              ; page
        sub     x2,  x14, x16   ; offset in page
        lsr     x15, x2,  #20   ; group index
        lsr     x2,  x2,  #9    ; card offset
        lsl     x15, x15, #1    ; group offset (index * 2)

    ; check if concurrent marking is in progress
        cbnz    x17, DirtyCard

    ; SETTING CARD FOR X14
SetCard
        ldrb    w3, [x16, x2]
        cbnz    w3, Exit
        mov     w17, #1
        strb    w17, [x16, x2]
SetGroup
        add     x12, x16, #0x80
        ldrb    w3, [x12, x15]
        cbnz    w3, CardSet
        strb    w17, [x12, x15]
SetPage
        ldrb    w3, [x16]
        cbnz    w3, CardSet
        strb    w17, [x16]

CardSet
    ; check if concurrent marking is still not in progress
        ALTERNATE_ENTRY RelocSite_CardSet_WriteBarrierState
        PREPARE_EXTERNAL_VAR_INDIRECT g_write_barrier_state, x12
        cbnz    x12, DirtyCard

Exit
        ldp  x2,  x3, [sp], 16
        ret  lr

    ; DIRTYING CARD FOR X14
DirtyCard
        mov     w17, #4
        add     x2, x2, x16
        ; must be after the field write to allow concurrent clean
        stlrb   w17, [x2]
DirtyGroup
        add     x12, x16, #0x80
        ldrb    w3, [x12, x15]
        tbnz    w3, #2, Exit
        strb    w17, [x12, x15]
DirtyPage
        ldrb    w3, [x16]
        tbnz    w3, #2, Exit
        strb    w17, [x16]
        b       Exit

    ;; this is expected to be rare.
RecordEscape

    ;; 4) check if the source is escaped (x16 has source region)
        add         x12, x15, #8                   ;; escape bit is MT + 1
        ubfx        x17, x12, #9,#12               ;; word index = (dst >> 9) & 0x1FFFFF
        ldr         x17, [x16, x17, lsl #3]        ;; mark word = [region + index * 8]
        lsr         x12, x12, #3                   ;; bit = (dst >> 3) [& 63]
        lsr         x17, x17, x12
        tbnz        x17, #0, AssignAndMarkCards        ;; source is already escaped.

        ;; because of the barrier call convention
        ;; we need to preserve caller-saved x0 through x15 and x29/x30

        stp     x29,x30, [sp, -16 * 9]!
        stp     x0, x1,  [sp, 16 * 1]
        stp     x2, x3,  [sp, 16 * 2]
        stp     x4, x5,  [sp, 16 * 3]
        stp     x6, x7,  [sp, 16 * 4]
        stp     x8, x9,  [sp, 16 * 5]
        stp     x10,x11, [sp, 16 * 6]
        stp     x12,x13, [sp, 16 * 7]
        stp     x14,x15, [sp, 16 * 8]

        ;; void SatoriRegion::EscapeFn(SatoriObject** dst, SatoriObject* src, SatoriRegion* region)
        ;; mov  x0, x14  EscapeFn does not use dst, it is just to avoid arg shuffle on x64
        mov  x1, x15
        mov  x2, x16                       ;; source region
        ldr  x12, [x16, #8]                 ;; EscapeFn address
        blr  x12

        ldp     x0, x1,  [sp, 16 * 1]
        ldp     x2, x3,  [sp, 16 * 2]
        ldp     x4, x5,  [sp, 16 * 3]
        ldp     x6, x7,  [sp, 16 * 4]
        ldp     x8, x9,  [sp, 16 * 5]
        ldp     x10,x11, [sp, 16 * 6]
        ldp     x12,x13, [sp, 16 * 7]
        ldp     x14,x15, [sp, 16 * 8]
        ldp     x29,x30, [sp], 16 * 9

        and     x16, x15, #0xFFFFFFFFFFE00000  ;; source region
        b       AssignAndMarkCards
    LEAF_END_MARKED RhpAssignRef

    end
