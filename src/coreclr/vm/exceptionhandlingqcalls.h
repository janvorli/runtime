// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//

#ifndef EXCEPTION_HANDLING_QCALLS_H
#define EXCEPTION_HANDLING_QCALLS_H

enum RhEHClauseKind
{
    RH_EH_CLAUSE_TYPED = 0,
    RH_EH_CLAUSE_FAULT = 1,
    RH_EH_CLAUSE_FILTER = 2,
    RH_EH_CLAUSE_UNUSED = 3,
};

struct RhEHClause
{
    RhEHClauseKind _clauseKind;
    unsigned _tryStartOffset;
    unsigned _tryEndOffset;
    BYTE *_filterAddress;
    BYTE *_handlerAddress;
    TypeHandle _pTargetType;
    BOOL _isSameTry;
};

enum class ExKind : uint8_t
{
    None = 0,
    Throw = 1,
    HardwareFault = 2,
    KindMask = 3,

    RethrowFlag = 4,

    SupersededFlag = 8,

    InstructionFaultFlag = 0x10
};

struct ExInfo
{
    ExInfo* _pPrevExInfo;

    void* _pExContext;

    OBJECTREF _exception;  // actual object reference, specially reported by GcScanRootsWorker

    ExKind _kind;

    uint8_t _passNumber;
    uint8_t _stackBoundsPassNumber;

    // BEWARE: This field is used by the stackwalker to know if the dispatch code has reached the
    //         point at which a handler is called.  In other words, it serves as an "is a handler
    //         active" state where '_idxCurClause == MaxTryRegionIdx' means 'no'.
    uint32_t _idxCurClause;

    StackFrameIterator _frameIter;

    volatile size_t _notifyDebuggerSP;

    REGDISPLAY *_pRD;

    StackTraceInfo _stackTraceInfo;

    Frame* _pFrame;

    StackFrame          _sfLowBound;
    StackFrame          _sfHighBound;
    CallerStackFrame    _csfEHClause;
    CallerStackFrame    _csfEnclosingClause;
    StackFrame          _sfCallerOfActualHandlerFrame;
    EE_ILEXCEPTION_CLAUSE _ClauseForCatch;

    // These are for profiler / debugger use only
    OBJECTHANDLE    _hThrowable;       // thrown exception handle
    EE_ILEXCEPTION_CLAUSE _CurrentClause;
    DebuggerExState _DebuggerExState;
    ExceptionFlags _ExceptionFlags;
};

//  class StackFrameIterator;
//struct EH_CLAUSE_ENUMERATOR;
#ifndef DACCESS_COMPILE

extern "C" void * QCALLTYPE  RhpCallCatchFunclet(QCall::ObjectHandleOnStack exceptionObj, BYTE* pHandlerIP, REGDISPLAY* pvRegDisplay, ExInfo* exInfo);
extern "C" void QCALLTYPE  RhpCallFinallyFunclet(BYTE* pHandlerIP, REGDISPLAY* pvRegDisplay, ExInfo* exInfo);
extern "C" BOOL QCALLTYPE  RhpCallFilterFunclet(QCall::ObjectHandleOnStack exceptionObj, BYTE* pFilterP, REGDISPLAY* pvRegDisplay);
extern "C" void QCALLTYPE RhpAppendExceptionStackFrame(QCall::ObjectHandleOnStack exceptionObj, SIZE_T ip, SIZE_T sp, int flags, ExInfo *pExInfo);
extern "C" BOOL QCALLTYPE RhpEHEnumInitFromStackFrameIterator(StackFrameIterator *pFrameIter, BYTE** pMethodStartAddress, EH_CLAUSE_ENUMERATOR * pEHEnum);
extern "C" BOOL QCALLTYPE RhpEHEnumNext(StackFrameIterator *pFrameIter, EH_CLAUSE_ENUMERATOR* pEHEnum, RhEHClause* pEHClause);
extern "C" void QCALLTYPE RhpCaptureCallerContext(CONTEXT* pStackwalkCtx);
extern "C" bool QCALLTYPE RhpSfiInit(StackFrameIterator* pThis, CONTEXT* pStackwalkCtx, REGDISPLAY* pRD, bool instructionFault);
extern "C" bool QCALLTYPE RhpSfiNext(StackFrameIterator* pThis, unsigned int* uExCollideClauseIdx, bool* fUnwoundReversePInvoke);
#endif // DACCESS_COMPILE

#endif // EXCEPTION_HANDLING_QCALLS_H