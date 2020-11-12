// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*++



Module Name:

    context.c

Abstract:

    Implementation of GetThreadContext/SetThreadContext/DebugBreak.
    There are a lot of architecture specifics here.



--*/

#include "pal/dbgmsg.h"
SET_DEFAULT_DEBUG_CHANNEL(THREAD); // some headers have code with asserts, so do this first

#include "pal/palinternal.h"
#include "pal/context.h"
#include "pal/debug.h"
#include "pal/thread.hpp"
#include "pal/utils.h"
#include "pal/virtual.h"

#if HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>
#endif
#include <errno.h>
#include <unistd.h>

extern PGET_GCMARKER_EXCEPTION_CODE g_getGcMarkerExceptionCode;

#define CONTEXT_AREA_MASK 0xffff
#ifdef HOST_X86
#define CONTEXT_ALL_FLOATING (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS)
#elif defined(HOST_AMD64)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#elif defined(HOST_ARM)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#elif defined(HOST_ARM64)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#else
#error Unexpected architecture.
#endif

#ifndef __GLIBC__
typedef int __ptrace_request;
#endif

#if HAVE_MACHINE_REG_H
#include <machine/reg.h>
#endif  // HAVE_MACHINE_REG_H
#if HAVE_MACHINE_NPX_H
#include <machine/npx.h>
#endif  // HAVE_MACHINE_NPX_H

#if HAVE_PT_REGS
#include <asm/ptrace.h>
#endif  // HAVE_PT_REGS

#ifdef HOST_AMD64
#define ASSIGN_CONTROL_REGS \
        ASSIGN_REG(Rbp)     \
        ASSIGN_REG(Rip)     \
        ASSIGN_REG(SegCs)   \
        ASSIGN_REG(EFlags)  \
        ASSIGN_REG(Rsp)     \

#define ASSIGN_INTEGER_REGS \
        ASSIGN_REG(Rdi)     \
        ASSIGN_REG(Rsi)     \
        ASSIGN_REG(Rbx)     \
        ASSIGN_REG(Rdx)     \
        ASSIGN_REG(Rcx)     \
        ASSIGN_REG(Rax)     \
        ASSIGN_REG(R8)     \
        ASSIGN_REG(R9)     \
        ASSIGN_REG(R10)     \
        ASSIGN_REG(R11)     \
        ASSIGN_REG(R12)     \
        ASSIGN_REG(R13)     \
        ASSIGN_REG(R14)     \
        ASSIGN_REG(R15)     \

#elif defined(HOST_X86)
#define ASSIGN_CONTROL_REGS \
        ASSIGN_REG(Ebp)     \
        ASSIGN_REG(Eip)     \
        ASSIGN_REG(SegCs)   \
        ASSIGN_REG(EFlags)  \
        ASSIGN_REG(Esp)     \
        ASSIGN_REG(SegSs)   \

#define ASSIGN_INTEGER_REGS \
        ASSIGN_REG(Edi)     \
        ASSIGN_REG(Esi)     \
        ASSIGN_REG(Ebx)     \
        ASSIGN_REG(Edx)     \
        ASSIGN_REG(Ecx)     \
        ASSIGN_REG(Eax)     \

#elif defined(HOST_ARM)
#define ASSIGN_CONTROL_REGS \
        ASSIGN_REG(Sp)     \
        ASSIGN_REG(Lr)     \
        ASSIGN_REG(Pc)   \
        ASSIGN_REG(Cpsr)  \

#define ASSIGN_INTEGER_REGS \
        ASSIGN_REG(R0)     \
        ASSIGN_REG(R1)     \
        ASSIGN_REG(R2)     \
        ASSIGN_REG(R3)     \
        ASSIGN_REG(R4)     \
        ASSIGN_REG(R5)     \
        ASSIGN_REG(R6)     \
        ASSIGN_REG(R7)     \
        ASSIGN_REG(R8)     \
        ASSIGN_REG(R9)     \
        ASSIGN_REG(R10)     \
        ASSIGN_REG(R11)     \
        ASSIGN_REG(R12)
#elif defined(HOST_ARM64)
#define ASSIGN_CONTROL_REGS \
        ASSIGN_REG(Cpsr)    \
        ASSIGN_REG(Fp)      \
        ASSIGN_REG(Sp)      \
        ASSIGN_REG(Lr)      \
        ASSIGN_REG(Pc)

#define ASSIGN_INTEGER_REGS \
	ASSIGN_REG(X0)      \
	ASSIGN_REG(X1)      \
	ASSIGN_REG(X2)      \
	ASSIGN_REG(X3)      \
	ASSIGN_REG(X4)      \
	ASSIGN_REG(X5)      \
	ASSIGN_REG(X6)      \
	ASSIGN_REG(X7)      \
	ASSIGN_REG(X8)      \
	ASSIGN_REG(X9)      \
	ASSIGN_REG(X10)     \
	ASSIGN_REG(X11)     \
	ASSIGN_REG(X12)     \
	ASSIGN_REG(X13)     \
	ASSIGN_REG(X14)     \
	ASSIGN_REG(X15)     \
	ASSIGN_REG(X16)     \
	ASSIGN_REG(X17)     \
	ASSIGN_REG(X18)     \
	ASSIGN_REG(X19)     \
	ASSIGN_REG(X20)     \
	ASSIGN_REG(X21)     \
	ASSIGN_REG(X22)     \
	ASSIGN_REG(X23)     \
	ASSIGN_REG(X24)     \
	ASSIGN_REG(X25)     \
	ASSIGN_REG(X26)     \
	ASSIGN_REG(X27)     \
	ASSIGN_REG(X28)

#else
#error "Don't know how to assign registers on this architecture"
#endif

#define ASSIGN_ALL_REGS     \
        ASSIGN_CONTROL_REGS \
        ASSIGN_INTEGER_REGS \

/*++
Function:
  CONTEXT_GetRegisters

Abstract
  retrieve the machine registers value of the indicated process.

Parameter
  processId: process ID
  lpContext: context structure in which the machine registers value will be returned.
Return
 returns TRUE if it succeeds, FALSE otherwise
--*/
BOOL CONTEXT_GetRegisters(DWORD processId, LPCONTEXT lpContext)
{
#if HAVE_BSD_REGS_T
    int regFd = -1;
#endif  // HAVE_BSD_REGS_T
    BOOL bRet = FALSE;

    if (processId == GetCurrentProcessId())
    {
        CONTEXT_CaptureContext(lpContext);
    }
    else
    {
        ucontext_t registers;
#if HAVE_PT_REGS
        struct pt_regs ptrace_registers;
        if (ptrace((__ptrace_request)PTRACE_GETREGS, processId, (caddr_t) &ptrace_registers, 0) == -1)
#elif HAVE_BSD_REGS_T
        struct reg ptrace_registers;
        if (PAL_PTRACE(PT_GETREGS, processId, &ptrace_registers, 0) == -1)
#endif
        {
            ASSERT("Failed ptrace(PT_GETREGS, processId:%d) errno:%d (%s)\n",
                   processId, errno, strerror(errno));
        }

#if HAVE_PT_REGS
#define ASSIGN_REG(reg) MCREG_##reg(registers.uc_mcontext) = PTREG_##reg(ptrace_registers);
#elif HAVE_BSD_REGS_T
#define ASSIGN_REG(reg) MCREG_##reg(registers.uc_mcontext) = BSDREG_##reg(ptrace_registers);
#else
#define ASSIGN_REG(reg)
	ASSERT("Don't know how to get the context of another process on this platform!");
	return bRet;
#endif
        ASSIGN_ALL_REGS
#undef ASSIGN_REG

        CONTEXTFromNativeContext(&registers, lpContext, lpContext->ContextFlags);
    }

    bRet = TRUE;
#if HAVE_BSD_REGS_T
    if (regFd != -1)
    {
        close(regFd);
    }
#endif  // HAVE_BSD_REGS_T
    return bRet;
}

/*++
Function:
  GetThreadContext

See MSDN doc.
--*/
BOOL
CONTEXT_GetThreadContext(
         DWORD dwProcessId,
         pthread_t self,
         LPCONTEXT lpContext)
{
    BOOL ret = FALSE;

    if (lpContext == NULL)
    {
        ERROR("Invalid lpContext parameter value\n");
        SetLastError(ERROR_NOACCESS);
        goto EXIT;
    }

    /* How to consider the case when self is different from the current
       thread of its owner process. Machine registers values could be retreived
       by a ptrace(pid, ...) call or from the "/proc/%pid/reg" file content.
       Unfortunately, these two methods only depend on process ID, not on
       thread ID. */

    if (dwProcessId == GetCurrentProcessId())
    {
        if (self != pthread_self())
        {
            DWORD flags;
            // There aren't any APIs for this. We can potentially get the
            // context of another thread by using per-thread signals, but
            // on FreeBSD signal handlers that are called as a result
            // of signals raised via pthread_kill don't get a valid
            // sigcontext or ucontext_t. But we need this to return TRUE
            // to avoid an assertion in the CLR in code that manages to
            // cope reasonably well without a valid thread context.
            // Given that, we'll zero out our structure and return TRUE.
            ERROR("GetThreadContext on a thread other than the current "
                  "thread is returning TRUE\n");
            flags = lpContext->ContextFlags;
            memset(lpContext, 0, sizeof(*lpContext));
            lpContext->ContextFlags = flags;
            ret = TRUE;
            goto EXIT;
        }

    }

    if (lpContext->ContextFlags &
        (CONTEXT_CONTROL | CONTEXT_INTEGER) & CONTEXT_AREA_MASK)
    {
        if (CONTEXT_GetRegisters(dwProcessId, lpContext) == FALSE)
        {
            SetLastError(ERROR_INTERNAL_ERROR);
            goto EXIT;
        }
    }

    ret = TRUE;

EXIT:
    return ret;
}

/*++
Function:
  SetThreadContext

See MSDN doc.
--*/
BOOL
CONTEXT_SetThreadContext(
           DWORD dwProcessId,
           pthread_t self,
           CONST CONTEXT *lpContext)
{
    BOOL ret = FALSE;

#if HAVE_PT_REGS
    struct pt_regs ptrace_registers;
#elif HAVE_BSD_REGS_T
    struct reg ptrace_registers;
#endif

    if (lpContext == NULL)
    {
        ERROR("Invalid lpContext parameter value\n");
        SetLastError(ERROR_NOACCESS);
        goto EXIT;
    }

    /* How to consider the case when self is different from the current
       thread of its owner process. Machine registers values could be retreived
       by a ptrace(pid, ...) call or from the "/proc/%pid/reg" file content.
       Unfortunately, these two methods only depend on process ID, not on
       thread ID. */

    if (dwProcessId == GetCurrentProcessId())
    {
        // Need to implement SetThreadContext(current thread) for the IX architecture; look at common_signal_handler.
        _ASSERT(FALSE);

        ASSERT("SetThreadContext should be called for cross-process only.\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        goto EXIT;
    }

    if (lpContext->ContextFlags  &
        (CONTEXT_CONTROL | CONTEXT_INTEGER) & CONTEXT_AREA_MASK)
    {
#if HAVE_PT_REGS
        if (ptrace((__ptrace_request)PTRACE_GETREGS, dwProcessId, (caddr_t)&ptrace_registers, 0) == -1)
#elif HAVE_BSD_REGS_T
        if (PAL_PTRACE(PT_GETREGS, dwProcessId, &ptrace_registers, 0) == -1)
#endif
        {
            ASSERT("Failed ptrace(PT_GETREGS, processId:%d) errno:%d (%s)\n",
                   dwProcessId, errno, strerror(errno));
             SetLastError(ERROR_INTERNAL_ERROR);
             goto EXIT;
        }

#if HAVE_PT_REGS
#define ASSIGN_REG(reg) PTREG_##reg(ptrace_registers) = lpContext->reg;
#elif HAVE_BSD_REGS_T
#define ASSIGN_REG(reg) BSDREG_##reg(ptrace_registers) = lpContext->reg;
#else
#define ASSIGN_REG(reg)
	ASSERT("Don't know how to set the context of another process on this platform!");
	return FALSE;
#endif
        if (lpContext->ContextFlags & CONTEXT_CONTROL & CONTEXT_AREA_MASK)
        {
            ASSIGN_CONTROL_REGS
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER & CONTEXT_AREA_MASK)
        {
            ASSIGN_INTEGER_REGS
        }
#undef ASSIGN_REG

#if HAVE_PT_REGS
        if (ptrace((__ptrace_request)PTRACE_SETREGS, dwProcessId, (caddr_t)&ptrace_registers, 0) == -1)
#elif HAVE_BSD_REGS_T
        if (PAL_PTRACE(PT_SETREGS, dwProcessId, &ptrace_registers, 0) == -1)
#endif
        {
            ASSERT("Failed ptrace(PT_SETREGS, processId:%d) errno:%d (%s)\n",
                   dwProcessId, errno, strerror(errno));
            SetLastError(ERROR_INTERNAL_ERROR);
            goto EXIT;
        }
    }

    ret = TRUE;
   EXIT:
     return ret;
}

/*++
Function :
    CONTEXTToNativeContext

    Converts a CONTEXT record to a native context.

Parameters :
    CONST CONTEXT *lpContext : CONTEXT to convert
    native_context_t *native : native context to fill in

Return value :
    None

--*/
void CONTEXTToNativeContext(CONST CONTEXT *lpContext, native_context_t *native)
{
#define ASSIGN_REG(reg) MCREG_##reg(native->uc_mcontext) = lpContext->reg;
    if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        ASSIGN_CONTROL_REGS
    }

    if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        ASSIGN_INTEGER_REGS
    }
#undef ASSIGN_REG

#if !HAVE_FPREGS_WITH_CW
#if HAVE_GREGSET_T || HAVE_GREGSET_T
#if HAVE_GREGSET_T
    if (native->uc_mcontext.fpregs == nullptr)
#elif HAVE___GREGSET_T
    if (native->uc_mcontext.__fpregs == nullptr)
#endif // HAVE_GREGSET_T
    {
        // If the pointer to the floating point state in the native context
        // is not valid, we can't copy floating point registers regardless of
        // whether CONTEXT_FLOATING_POINT is set in the CONTEXT's flags.
        return;
    }
#endif // HAVE_GREGSET_T || HAVE_GREGSET_T
#endif // !HAVE_FPREGS_WITH_CW

    if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
#ifdef HOST_AMD64
        FPREG_ControlWord(native) = lpContext->FltSave.ControlWord;
        FPREG_StatusWord(native) = lpContext->FltSave.StatusWord;
#if HAVE_FPREGS_WITH_CW
        FPREG_TagWord1(native) = lpContext->FltSave.TagWord >> 8;
        FPREG_TagWord2(native) = lpContext->FltSave.TagWord & 0xff;
#else
        FPREG_TagWord(native) = lpContext->FltSave.TagWord;
#endif
        FPREG_ErrorOffset(native) = lpContext->FltSave.ErrorOffset;
        FPREG_ErrorSelector(native) = lpContext->FltSave.ErrorSelector;
        FPREG_DataOffset(native) = lpContext->FltSave.DataOffset;
        FPREG_DataSelector(native) = lpContext->FltSave.DataSelector;
        FPREG_MxCsr(native) = lpContext->FltSave.MxCsr;
        FPREG_MxCsr_Mask(native) = lpContext->FltSave.MxCsr_Mask;

        for (int i = 0; i < 8; i++)
        {
            FPREG_St(native, i) = lpContext->FltSave.FloatRegisters[i];
        }

        for (int i = 0; i < 16; i++)
        {
            FPREG_Xmm(native, i) = lpContext->FltSave.XmmRegisters[i];
        }
#elif defined(HOST_ARM64)
        fpsimd_context* fp = GetNativeSigSimdContext(native);
        if (fp)
        {
            fp->fpsr = lpContext->Fpsr;
            fp->fpcr = lpContext->Fpcr;
            for (int i = 0; i < 32; i++)
            {
                *(NEON128*) &fp->vregs[i] = lpContext->V[i];
            }
        }
#elif defined(HOST_ARM)
        VfpSigFrame* fp = GetNativeSigSimdContext(native);
        if (fp)
        {
            fp->Fpscr = lpContext->Fpscr;
            for (int i = 0; i < 32; i++)
            {
                fp->D[i] = lpContext->D[i];
            }
        }
#endif
    }

    // TODO: Enable for all Unix systems
#if defined(HOST_AMD64) && defined(XSTATE_SUPPORTED)
    if ((lpContext->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
    {
        _ASSERTE(FPREG_HasYmmRegisters(native));
        memcpy_s(FPREG_Xstate_Ymmh(native), sizeof(M128A) * 16, lpContext->VectorRegister, sizeof(M128A) * 16);
    }
#endif //HOST_AMD64 && XSTATE_SUPPORTED
}

/*++
Function :
    CONTEXTFromNativeContext

    Converts a native context to a CONTEXT record.

Parameters :
    const native_context_t *native : native context to convert
    LPCONTEXT lpContext : CONTEXT to fill in
    ULONG contextFlags : flags that determine which registers are valid in
                         native and which ones to set in lpContext

Return value :
    None

--*/
void CONTEXTFromNativeContext(const native_context_t *native, LPCONTEXT lpContext,
                              ULONG contextFlags)
{
    lpContext->ContextFlags = contextFlags;

#define ASSIGN_REG(reg) lpContext->reg = MCREG_##reg(native->uc_mcontext);
    if ((contextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        ASSIGN_CONTROL_REGS
#if defined(HOST_ARM)
        // WinContext assumes that the least bit of Pc is always 1 (denoting thumb)
        // although the pc value retrived from native context might not have set the least bit.
        // This becomes especially problematic if the context is on the JIT_WRITEBARRIER.
        lpContext->Pc |= 0x1;
#endif
    }

    if ((contextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        ASSIGN_INTEGER_REGS
    }
#undef ASSIGN_REG

#if !HAVE_FPREGS_WITH_CW
#if HAVE_GREGSET_T || HAVE___GREGSET_T
#if HAVE_GREGSET_T
    if (native->uc_mcontext.fpregs == nullptr)
#elif HAVE___GREGSET_T
    if (native->uc_mcontext.__fpregs == nullptr)
#endif // HAVE_GREGSET_T
    {
        // Reset the CONTEXT_FLOATING_POINT bit(s) and the CONTEXT_XSTATE bit(s) so it's
        // clear that the floating point and extended state data in the CONTEXT is not
        // valid. Since these flags are defined as the architecture bit(s) OR'd with one
        // or more other bits, we first get the bits that are unique to each by resetting
        // the architecture bits. We determine what those are by inverting the union of
        // CONTEXT_CONTROL and CONTEXT_INTEGER, both of which should also have the
        // architecture bit(s) set.
        const ULONG floatingPointFlags = CONTEXT_FLOATING_POINT & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);
        const ULONG xstateFlags = CONTEXT_XSTATE & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);

        lpContext->ContextFlags &= ~(floatingPointFlags | xstateFlags);

        // Bail out regardless of whether the caller wanted CONTEXT_FLOATING_POINT or CONTEXT_XSTATE
        return;
    }
#endif // HAVE_GREGSET_T || HAVE___GREGSET_T
#endif // !HAVE_FPREGS_WITH_CW

    if ((contextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
#ifdef HOST_AMD64
        lpContext->FltSave.ControlWord = FPREG_ControlWord(native);
        lpContext->FltSave.StatusWord = FPREG_StatusWord(native);
#if HAVE_FPREGS_WITH_CW
        lpContext->FltSave.TagWord = ((DWORD)FPREG_TagWord1(native) << 8) | FPREG_TagWord2(native);
#else
        lpContext->FltSave.TagWord = FPREG_TagWord(native);
#endif
        lpContext->FltSave.ErrorOffset = FPREG_ErrorOffset(native);
        lpContext->FltSave.ErrorSelector = FPREG_ErrorSelector(native);
        lpContext->FltSave.DataOffset = FPREG_DataOffset(native);
        lpContext->FltSave.DataSelector = FPREG_DataSelector(native);
        lpContext->FltSave.MxCsr = FPREG_MxCsr(native);
        lpContext->FltSave.MxCsr_Mask = FPREG_MxCsr_Mask(native);

        for (int i = 0; i < 8; i++)
        {
            lpContext->FltSave.FloatRegisters[i] = FPREG_St(native, i);
        }

        for (int i = 0; i < 16; i++)
        {
            lpContext->FltSave.XmmRegisters[i] = FPREG_Xmm(native, i);
        }
#elif defined(HOST_ARM64)
        const fpsimd_context* fp = GetConstNativeSigSimdContext(native);
        if (fp)
        {
            lpContext->Fpsr = fp->fpsr;
            lpContext->Fpcr = fp->fpcr;
            for (int i = 0; i < 32; i++)
            {
                lpContext->V[i] = *(NEON128*) &fp->vregs[i];
            }
        }
#elif defined(HOST_ARM)
        const VfpSigFrame* fp = GetConstNativeSigSimdContext(native);
        if (fp)
        {
            lpContext->Fpscr = fp->Fpscr;
            for (int i = 0; i < 32; i++)
            {
                lpContext->D[i] = fp->D[i];
            }
        }
        else
        {
            // Floating point state is not valid
            // Mark the context correctly
            lpContext->ContextFlags &= ~(ULONG)CONTEXT_FLOATING_POINT;
        }
#endif
    }

#ifdef HOST_AMD64
    if ((contextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
    {
    // TODO: Enable for all Unix systems
#if XSTATE_SUPPORTED
        if (FPREG_HasYmmRegisters(native))
        {
            memcpy_s(lpContext->VectorRegister, sizeof(M128A) * 16, FPREG_Xstate_Ymmh(native), sizeof(M128A) * 16);
        }
        else
#endif // XSTATE_SUPPORTED
        {
            // Reset the CONTEXT_XSTATE bit(s) so it's clear that the extended state data in
            // the CONTEXT is not valid.
            const ULONG xstateFlags = CONTEXT_XSTATE & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);
            lpContext->ContextFlags &= ~xstateFlags;
        }
    }
#endif // HOST_AMD64
}

/*++
Function :
    GetNativeContextPC

    Returns the program counter from the native context.

Parameters :
    const native_context_t *native : native context

Return value :
    The program counter from the native context.

--*/
LPVOID GetNativeContextPC(const native_context_t *context)
{
#ifdef HOST_AMD64
    return (LPVOID)MCREG_Rip(context->uc_mcontext);
#elif defined(HOST_X86)
    return (LPVOID) MCREG_Eip(context->uc_mcontext);
#elif defined(HOST_ARM)
    return (LPVOID) MCREG_Pc(context->uc_mcontext);
#elif defined(HOST_ARM64)
    return (LPVOID) MCREG_Pc(context->uc_mcontext);
#else
#   error implement me for this architecture
#endif
}

/*++
Function :
    GetNativeContextSP

    Returns the stack pointer from the native context.

Parameters :
    const native_context_t *native : native context

Return value :
    The stack pointer from the native context.

--*/
LPVOID GetNativeContextSP(const native_context_t *context)
{
#ifdef HOST_AMD64
    return (LPVOID)MCREG_Rsp(context->uc_mcontext);
#elif defined(HOST_X86)
    return (LPVOID) MCREG_Esp(context->uc_mcontext);
#elif defined(HOST_ARM)
    return (LPVOID) MCREG_Sp(context->uc_mcontext);
#elif defined(HOST_ARM64)
    return (LPVOID) MCREG_Sp(context->uc_mcontext);
#else
#   error implement me for this architecture
#endif
}


/*++
Function :
    CONTEXTGetExceptionCodeForSignal

    Translates signal and context information to a Win32 exception code.

Parameters :
    const siginfo_t *siginfo : signal information from a signal handler
    const native_context_t *context : context information

Return value :
    The Win32 exception code that corresponds to the signal and context
    information.

--*/
#ifdef ILL_ILLOPC
// If si_code values are available for all signals, use those.
DWORD CONTEXTGetExceptionCodeForSignal(const siginfo_t *siginfo,
                                       const native_context_t *context)
{
    // IMPORTANT NOTE: This function must not call any signal unsafe functions
    // since it is called from signal handlers.
    // That includes ASSERT and TRACE macros.

    switch (siginfo->si_signo)
    {
        case SIGILL:
            switch (siginfo->si_code)
            {
                case ILL_ILLOPC:    // Illegal opcode
                case ILL_ILLOPN:    // Illegal operand
                case ILL_ILLADR:    // Illegal addressing mode
                case ILL_ILLTRP:    // Illegal trap
                case ILL_COPROC:    // Co-processor error
                    return EXCEPTION_ILLEGAL_INSTRUCTION;
                case ILL_PRVOPC:    // Privileged opcode
                case ILL_PRVREG:    // Privileged register
                    return EXCEPTION_PRIV_INSTRUCTION;
                case ILL_BADSTK:    // Internal stack error
                    return EXCEPTION_STACK_OVERFLOW;
                default:
                    break;
            }
            break;
        case SIGFPE:
            switch (siginfo->si_code)
            {
                case FPE_INTDIV:
                    return EXCEPTION_INT_DIVIDE_BY_ZERO;
                case FPE_INTOVF:
                    return EXCEPTION_INT_OVERFLOW;
                case FPE_FLTDIV:
                    return EXCEPTION_FLT_DIVIDE_BY_ZERO;
                case FPE_FLTOVF:
                    return EXCEPTION_FLT_OVERFLOW;
                case FPE_FLTUND:
                    return EXCEPTION_FLT_UNDERFLOW;
                case FPE_FLTRES:
                    return EXCEPTION_FLT_INEXACT_RESULT;
                case FPE_FLTINV:
                    return EXCEPTION_FLT_INVALID_OPERATION;
                case FPE_FLTSUB:
                    return EXCEPTION_FLT_INVALID_OPERATION;
                default:
                    break;
            }
            break;
        case SIGSEGV:
            switch (siginfo->si_code)
            {
                case SI_USER:       // User-generated signal, sometimes sent
                                    // for SIGSEGV under normal circumstances
                case SEGV_MAPERR:   // Address not mapped to object
                case SEGV_ACCERR:   // Invalid permissions for mapped object
                    return EXCEPTION_ACCESS_VIOLATION;

#ifdef SI_KERNEL
                case SI_KERNEL:
                {
                    // Identify privileged instructions that are not identified as such by the system
                    if (g_getGcMarkerExceptionCode != nullptr)
                    {
                        DWORD exceptionCode = g_getGcMarkerExceptionCode(GetNativeContextPC(context));
                        if (exceptionCode != 0)
                        {
                            return exceptionCode;
                        }
                    }
                    return EXCEPTION_ACCESS_VIOLATION;
                }
#endif
                default:
                    break;
            }
            break;
        case SIGBUS:
            switch (siginfo->si_code)
            {
                case BUS_ADRALN:    // Invalid address alignment
                    return EXCEPTION_DATATYPE_MISALIGNMENT;
                case BUS_ADRERR:    // Non-existent physical address
                    return EXCEPTION_ACCESS_VIOLATION;
                case BUS_OBJERR:    // Object-specific hardware error
                default:
                    break;
            }
            break;
        case SIGTRAP:
            switch (siginfo->si_code)
            {
#ifdef SI_KERNEL
                case SI_KERNEL:
#endif
                case SI_USER:
                case TRAP_BRKPT:    // Process breakpoint
                    return EXCEPTION_BREAKPOINT;
                case TRAP_TRACE:    // Process trace trap
                    return EXCEPTION_SINGLE_STEP;
                default:
                    // Got unknown SIGTRAP signal with code siginfo->si_code;
                    return EXCEPTION_ILLEGAL_INSTRUCTION;
            }
        default:
            break;
    }

    // Got unknown signal number siginfo->si_signo with code siginfo->si_code;
    return EXCEPTION_ILLEGAL_INSTRUCTION;
}
#else   // ILL_ILLOPC
DWORD CONTEXTGetExceptionCodeForSignal(const siginfo_t *siginfo,
                                       const native_context_t *context)
{
    // IMPORTANT NOTE: This function must not call any signal unsafe functions
    // since it is called from signal handlers.
    // That includes ASSERT and TRACE macros.

    int trap;

    if (siginfo->si_signo == SIGFPE)
    {
        // Floating point exceptions are mapped by their si_code.
        switch (siginfo->si_code)
        {
            case FPE_INTDIV :
                return EXCEPTION_INT_DIVIDE_BY_ZERO;
            case FPE_INTOVF :
                return EXCEPTION_INT_OVERFLOW;
            case FPE_FLTDIV :
                return EXCEPTION_FLT_DIVIDE_BY_ZERO;
            case FPE_FLTOVF :
                return EXCEPTION_FLT_OVERFLOW;
            case FPE_FLTUND :
                return EXCEPTION_FLT_UNDERFLOW;
            case FPE_FLTRES :
                return EXCEPTION_FLT_INEXACT_RESULT;
            case FPE_FLTINV :
                return EXCEPTION_FLT_INVALID_OPERATION;
            case FPE_FLTSUB :/* subscript out of range */
                return EXCEPTION_FLT_INVALID_OPERATION;
            default:
                // Got unknown signal code siginfo->si_code;
                return 0;
        }
    }

    trap = context->uc_mcontext.mc_trapno;
    switch (trap)
    {
        case T_PRIVINFLT : /* privileged instruction */
            return EXCEPTION_PRIV_INSTRUCTION;
        case T_BPTFLT :    /* breakpoint instruction */
            return EXCEPTION_BREAKPOINT;
        case T_ARITHTRAP : /* arithmetic trap */
            return 0;      /* let the caller pick an exception code */
#ifdef T_ASTFLT
        case T_ASTFLT :    /* system forced exception : ^C, ^\. SIGINT signal
                              handler shouldn't be calling this function, since
                              it doesn't need an exception code */
            // Trap code T_ASTFLT received, shouldn't get here;
            return 0;
#endif  // T_ASTFLT
        case T_PROTFLT :   /* protection fault */
            return EXCEPTION_ACCESS_VIOLATION;
        case T_TRCTRAP :   /* debug exception (sic) */
            return EXCEPTION_SINGLE_STEP;
        case T_PAGEFLT :   /* page fault */
            return EXCEPTION_ACCESS_VIOLATION;
        case T_ALIGNFLT :  /* alignment fault */
            return EXCEPTION_DATATYPE_MISALIGNMENT;
        case T_DIVIDE :
            return EXCEPTION_INT_DIVIDE_BY_ZERO;
        case T_NMI :       /* non-maskable trap */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        case T_OFLOW :
            return EXCEPTION_INT_OVERFLOW;
        case T_BOUND :     /* bound instruction fault */
            return EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
        case T_DNA :       /* device not available fault */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        case T_DOUBLEFLT : /* double fault */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        case T_FPOPFLT :   /* fp coprocessor operand fetch fault */
            return EXCEPTION_FLT_INVALID_OPERATION;
        case T_TSSFLT :    /* invalid tss fault */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        case T_SEGNPFLT :  /* segment not present fault */
            return EXCEPTION_ACCESS_VIOLATION;
        case T_STKFLT :    /* stack fault */
            return EXCEPTION_STACK_OVERFLOW;
        case T_MCHK :      /* machine check trap */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        case T_RESERVED :  /* reserved (unknown) */
            return EXCEPTION_ILLEGAL_INSTRUCTION;
        default:
            // Got unknown trap code trap;
            break;
    }
    return EXCEPTION_ILLEGAL_INSTRUCTION;
}
#endif  // ILL_ILLOPC


/*++
Function:
  DBG_FlushInstructionCache: processor-specific portion of
  FlushInstructionCache

See MSDN doc.
--*/
BOOL
DBG_FlushInstructionCache(
                          IN LPCVOID lpBaseAddress,
                          IN SIZE_T dwSize)
{
#ifndef HOST_ARM
    // Intrinsic should do the right thing across all platforms (except Linux arm)
    __builtin___clear_cache((char *)lpBaseAddress, (char *)((INT_PTR)lpBaseAddress + dwSize));
#else // HOST_ARM
    // On Linux/arm (at least on 3.10) we found that there is a problem with __do_cache_op (arch/arm/kernel/traps.c)
    // implementing cacheflush syscall. cacheflush flushes only the first page in range [lpBaseAddress, lpBaseAddress + dwSize)
    // and leaves other pages in undefined state which causes random tests failures (often due to SIGSEGV) with no particular pattern.
    //
    // As a workaround, we call __builtin___clear_cache on each page separately.

    const SIZE_T pageSize = GetVirtualPageSize();
    INT_PTR begin = (INT_PTR)lpBaseAddress;
    const INT_PTR end = begin + dwSize;

    while (begin < end)
    {
        INT_PTR endOrNextPageBegin = ALIGN_UP(begin + 1, pageSize);
        if (endOrNextPageBegin > end)
            endOrNextPageBegin = end;

        __builtin___clear_cache((char *)begin, (char *)endOrNextPageBegin);
        begin = endOrNextPageBegin;
    }
#endif // HOST_ARM
    return TRUE;
}
