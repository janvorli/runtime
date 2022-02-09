// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// precode.h
//

//
// Stub that runs before the actual native code

#ifndef __PRECODE_H__
#define __PRECODE_H__

// TODO: is it target or host???

#if defined(HOST_AMD64)

#define OFFSETOF_PRECODE_TYPE              0
#define OFFSETOF_PRECODE_TYPE_CALL_OR_JMP  5
#define OFFSETOF_PRECODE_TYPE_MOV_R10     10

#define SIZEOF_PRECODE_BASE               16

#elif defined(HOST_X86)

EXTERN_C VOID STDCALL PrecodeRemotingThunk();

#define OFFSETOF_PRECODE_TYPE              6
#define OFFSETOF_PRECODE_TYPE_CALL_OR_JMP  5
#define OFFSETOF_PRECODE_TYPE_MOV_RM_R     6

#define SIZEOF_PRECODE_BASE                8

#elif defined(HOST_ARM64)

#define PRECODE_ALIGNMENT           CODE_SIZE_ALIGN
#define SIZEOF_PRECODE_BASE         CODE_SIZE_ALIGN
#define OFFSETOF_PRECODE_TYPE       0

#elif defined(HOST_ARM)

#define PRECODE_ALIGNMENT           sizeof(void*)
#define SIZEOF_PRECODE_BASE         CODE_SIZE_ALIGN
#define OFFSETOF_PRECODE_TYPE       3

#endif // HOST_AMD64

#include <pshpack1.h>

// Invalid precode type
struct InvalidPrecode
{
#if defined(HOST_AMD64) || defined(HOST_X86)
    // int3
    static const int Type = 0xCC;
#elif defined(HOST_ARM64) || defined(HOST_ARM)
    static const int Type = 0;
#endif
};

struct StubPrecodeData
{
    PCODE Target;
    PTR_MethodDesc MethodDesc;
    BYTE Type;
};

typedef DPTR(StubPrecodeData) PTR_StubPrecodeData;

// Regular precode
struct StubPrecode
{

#if defined(HOST_AMD64)
    static const BYTE Type = 0xF8;
    static const int CodeSize = 24;
#elif defined(HOST_X86)
    static const BYTE Type = 0xED;
    static const int CodeSize = 24;
#elif defined(HOST_ARM64)
    static const int Type = 0x0A;
    static const int CodeSize = 24;
#elif defined(HOST_ARM)
    static const int Type = 0xcf;
    static const int CodeSize = 12;
#endif // HOST_AMD64

    BYTE m_code[CodeSize];

    void Init(StubPrecode* pPrecodeRX, MethodDesc* pMD, LoaderAllocator *pLoaderAllocator = NULL, BYTE type = StubPrecode::Type, TADDR target = NULL);

    PTR_StubPrecodeData GetData() const
    {
        LIMITED_METHOD_CONTRACT;
        return dac_cast<PTR_StubPrecodeData>((BYTE*)this + GetOsPageSize());
    }

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_DAC_CONTRACT;

        return dac_cast<TADDR>(GetData()->MethodDesc);
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;

        return GetData()->Target;
    }

    BYTE GetType()
    {
        return GetData()->Type;
    }

    // PCODE GetEntryPoint()
    // {
    //     LIMITED_METHOD_CONTRACT;

    //     return PINSTRToPCODE(dac_cast<TADDR>(this));
    // }


#ifndef DACCESS_COMPILE
    void ResetTargetInterlocked()
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        StubPrecodeData *pData = GetData();
        InterlockedExchangeT<PCODE>(&pData->Target, GetPreStubEntryPoint());
    }

    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        StubPrecodeData *pData = GetData();
        return InterlockedCompareExchangeT<PCODE>(&pData->Target, (PCODE)target, (PCODE)expected) == expected;
  }

    static size_t GenerateCodePage(uint8_t* pageBase);

#endif // !DACCESS_COMPILE
};

typedef DPTR(StubPrecode) PTR_StubPrecode;


#ifdef HAS_NDIRECT_IMPORT_PRECODE

// NDirect import precode
// (This is fake precode. VTable slot does not point to it.)
struct NDirectImportPrecode : StubPrecode
{
#if defined(HOST_AMD64)
    static const int Type = 0xF9;
#elif defined(HOST_X86)
    static const int Type = 0xC0;
#elif defined(HOST_ARM64)
    static const int Type = 0x8B; // Any random value would do as long as it doesn't collide with other stubs
#elif defined(HOST_ARM)
    static const int Type = 0xe0;
#endif // HOST_AMD64

    void Init(NDirectImportPrecode* pPrecodeRX, MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

    LPVOID GetEntrypoint()
    {
        LIMITED_METHOD_CONTRACT;
        return (LPVOID)PINSTRToPCODE(dac_cast<TADDR>(this));
    }
};
typedef DPTR(NDirectImportPrecode) PTR_NDirectImportPrecode;

#endif // HAS_NDIRECT_IMPORT_PRECODE


#ifdef HAS_FIXUP_PRECODE

struct FixupPrecodeData
{
    PCODE Target;
    MethodDesc *MethodDesc;
    PCODE PrecodeFixupThunk;
};

typedef DPTR(FixupPrecodeData) PTR_FixupPrecodeData;

// Fixup precode is used in ngen images when the prestub does just one time fixup.
// The fixup precode is simple jump once patched. It does not have the two instruction overhead of regular precode.
struct FixupPrecode
{
#if defined(HOST_AMD64)
    static const int Type = 0x5F;
    static const int CodeSize = 24;
    static const int FixupCodeOffset = 6;
#elif defined(HOST_X86)
    static const int Type = 0x5F;
    static const int CodeSize = 24;
    static const int FixupCodeOffset = 6;
#elif defined(HOST_ARM64)
    static const int Type = 0x0b;
    static const int CodeSize = 24;
    static const int FixupCodeOffset = 8;
#elif defined(HOST_ARM)
    static const int Type = 0xff;
    static const int CodeSize = 12;
    static const int FixupCodeOffset = 4 + THUMB_CODE;
#endif // HOST_AMD64

    BYTE m_code[CodeSize];

    void Init(FixupPrecode* pPrecodeRX, MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

    static size_t GenerateCodePage(uint8_t* pageBase);

    PTR_FixupPrecodeData GetData() const
    {
        LIMITED_METHOD_CONTRACT;
        return dac_cast<PTR_FixupPrecodeData>((BYTE*)this + 4096);
    }

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_CONTRACT;
        return (TADDR)GetData()->MethodDesc;
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return GetData()->Target;
    }

#if defined(HOST_ARM64)
    static BOOL IsFixupPrecodeByASM(PCODE addr)
    {
        PTR_DWORD pInstr = dac_cast<PTR_DWORD>(PCODEToPINSTR(addr));
        return
            (pInstr[0] == 0x5800800b) &&
            (pInstr[1] == 0xd61f0160) &&
            (pInstr[2] == 0x5800800c) &&
            (pInstr[3] == 0x5800802b) &&
            (pInstr[4] == 0xd61f0160);
    }
#endif // HOST_ARM64

    void ResetTargetInterlocked()
    {
        CONTRACTL
        {
            NOTHROW;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        PCODE target = (PCODE)this + FixupCodeOffset;
        _ASSERTE(IS_ALIGNED(this, sizeof(INT64)));

        InterlockedExchangeT<PCODE>(&GetData()->Target, target);
    }

    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            NOTHROW;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        MethodDesc * pMD = (MethodDesc*)GetMethodDesc();
        g_IBCLogger.LogMethodPrecodeWriteAccess(pMD);

        PCODE oldTarget = (PCODE)GetData()->Target;
        if (oldTarget != ((PCODE)this + FixupCodeOffset))
        {
#ifdef FEATURE_CODE_VERSIONING
            // No change needed, jmp is already in place
#else
            // Setting the target more than once is unexpected
            return FALSE;
#endif
        }

        _ASSERTE(IS_ALIGNED(this, sizeof(INT64)));

    //    return FastInterlockCompareExchangeLong((INT64*)&GetData()->Target, (INT64)target, oldTarget) == oldTarget;
        return InterlockedCompareExchangeT<PCODE>(&GetData()->Target, (PCODE)target, (PCODE)oldTarget) == (PCODE)oldTarget;
    }

#ifdef DACCESS_COMPILE
    void EnumMemoryRegions(CLRDataEnumMemoryFlags flags);
#endif
};
// IN_TARGET_32BIT(static_assert_no_msg(offsetof(FixupPrecode, m_type) == OFFSETOF_PRECODE_TYPE));
// IN_TARGET_64BIT(static_assert_no_msg(offsetof(FixupPrecode, m_op)   == OFFSETOF_PRECODE_TYPE);)
// IN_TARGET_64BIT(static_assert_no_msg(offsetof(FixupPrecode, m_type) == OFFSETOF_PRECODE_TYPE_CALL_OR_JMP);)

typedef DPTR(FixupPrecode) PTR_FixupPrecode;

#endif // HAS_FIXUP_PRECODE

#include <poppack.h>

typedef DPTR(class Precode) PTR_Precode;

#ifndef PRECODE_ALIGNMENT
#define PRECODE_ALIGNMENT sizeof(void*)
#endif

enum PrecodeType {
    PRECODE_INVALID         = InvalidPrecode::Type,
    PRECODE_STUB            = StubPrecode::Type,
#ifdef HAS_NDIRECT_IMPORT_PRECODE
    PRECODE_NDIRECT_IMPORT  = NDirectImportPrecode::Type,
#endif // HAS_NDIRECT_IMPORT_PRECODE
#ifdef HAS_FIXUP_PRECODE
    PRECODE_FIXUP           = FixupPrecode::Type,
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    PRECODE_THISPTR_RETBUF  = ThisPtrRetBufPrecode::Type,
#endif // HAS_THISPTR_RETBUF_PRECODE
};

// For more details see. file:../../doc/BookOfTheRuntime/ClassLoader/MethodDescDesign.doc
class Precode {

    BYTE m_data[SIZEOF_PRECODE_BASE];

    StubPrecode* AsStubPrecode()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;

        return dac_cast<PTR_StubPrecode>(this);
    }

#ifdef HAS_NDIRECT_IMPORT_PRECODE
public:
    // Fake precodes has to be exposed
    NDirectImportPrecode* AsNDirectImportPrecode()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;

        return dac_cast<PTR_NDirectImportPrecode>(this);
    }

private:
#endif // HAS_NDIRECT_IMPORT_PRECODE

#ifdef HAS_FIXUP_PRECODE
    FixupPrecode* AsFixupPrecode()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;

        return dac_cast<PTR_FixupPrecode>(this);
    }
#endif // HAS_FIXUP_PRECODE

#ifdef HAS_THISPTR_RETBUF_PRECODE
    ThisPtrRetBufPrecode* AsThisPtrRetBufPrecode()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;
        return dac_cast<PTR_ThisPtrRetBufPrecode>(this);
    }
#endif // HAS_THISPTR_RETBUF_PRECODE

    TADDR GetStart()
    {
        SUPPORTS_DAC;
        LIMITED_METHOD_CONTRACT;
        return dac_cast<TADDR>(this);
    }

    static void UnexpectedPrecodeType(const char * originator, PrecodeType precodeType)

    {
        SUPPORTS_DAC;
#ifdef DACCESS_COMPILE
        DacError(E_UNEXPECTED);
#else
#ifdef _PREFIX_
        // We only use __UNREACHABLE here since otherwise it would be a hint
        // for the compiler to fold this case with the other cases in a switch
        // statement. However, we would rather have this case be a separate
        // code path so that we will get a clean crash sooner.
        __UNREACHABLE("Unexpected precode type");
#endif
        CONSISTENCY_CHECK_MSGF(false, ("%s: Unexpected precode type: 0x%02x.", originator, precodeType));
#endif
    }

public:
    PrecodeType GetType()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;

#ifdef OFFSETOF_PRECODE_TYPE

        BYTE type = m_data[OFFSETOF_PRECODE_TYPE];
#ifdef TARGET_X86
        // if (type == X86_INSTR_MOV_RM_R)
        //     type = m_data[OFFSETOF_PRECODE_TYPE_MOV_RM_R];
        if (type == 0x25)
        {
            type = AsStubPrecode()->GetType();
        }
        else if (type == 0xa1)
        {
            type = FixupPrecode::Type;
        }
#endif //  TARGET_X86

#ifdef TARGET_AMD64
        // TODO: make sure this doesn't collide with FixupPrecode if we change it back!
        if (type == 0x4c && m_data[13] != 0xcc)
        {
            type = AsStubPrecode()->GetType();
        }
/*
        if (type == (X86_INSTR_MOV_R10_IMM64 & 0xFF))
            type = m_data[OFFSETOF_PRECODE_TYPE_MOV_R10];
*/
        else if ((type == (X86_INSTR_CALL_REL32 & 0xFF)) || (type == (X86_INSTR_JMP_REL32  & 0xFF)))
            type = m_data[OFFSETOF_PRECODE_TYPE_CALL_OR_JMP];
#endif // _AMD64

#if defined(HAS_FIXUP_PRECODE) && (defined(TARGET_X86) || defined(TARGET_AMD64))
        if (type == 0xff) //FixupPrecode::TypePrestub)
            type = FixupPrecode::Type;
#endif

#ifdef TARGET_ARM
        //static_assert_no_msg(offsetof(StubPrecode, m_pTarget) == offsetof(NDirectImportPrecode, m_pMethodDesc));
        // If the precode does not have thumb bit on target, it must be NDirectImportPrecode.
        if (type == StubPrecode::Type && ((AsStubPrecode()->GetTarget() & THUMB_CODE) == 0))
            type = NDirectImportPrecode::Type;
#endif

#ifdef TARGET_ARM64
        if (type == StubPrecode::Type)
            type = AsStubPrecode()->GetType();
#endif
        return (PrecodeType)type;

#else // OFFSETOF_PRECODE_TYPE
        return PRECODE_STUB;
#endif // OFFSETOF_PRECODE_TYPE
    }

    static BOOL IsValidType(PrecodeType t);

    static int AlignOf(PrecodeType t)
    {
        SUPPORTS_DAC;
        unsigned int align = PRECODE_ALIGNMENT;

#if defined(TARGET_X86) && defined(HAS_FIXUP_PRECODE)
        // Fixup precodes has to be aligned to allow atomic patching
        if (t == PRECODE_FIXUP)
            align = 8;
#endif // TARGET_X86 && HAS_FIXUP_PRECODE

#if defined(TARGET_ARM) && defined(HAS_COMPACT_ENTRYPOINTS)
        // Precodes have to be aligned to allow fast compact entry points check
        _ASSERTE (align >= sizeof(void*));
#endif // TARGET_ARM && HAS_COMPACT_ENTRYPOINTS

        return align;
    }

    static SIZE_T SizeOf(PrecodeType t);

    SIZE_T SizeOf()
    {
        WRAPPER_NO_CONTRACT;
        return SizeOf(GetType());
    }

    // Note: This is immediate target of the precode. It does not follow jump stub if there is one.
    PCODE GetTarget();

    BOOL IsPointingTo(PCODE target, PCODE addr)
    {
        WRAPPER_NO_CONTRACT;
        SUPPORTS_DAC;

        if (target == addr)
            return TRUE;

#ifdef TARGET_AMD64
        // Handle jump stubs
        if (isJumpRel64(target)) {
            target = decodeJump64(target);
            if (target == addr)
                return TRUE;
        }
#endif // TARGET_AMD64

        return FALSE;
    }

    BOOL IsPointingToNativeCode(PCODE pNativeCode)
    {
        WRAPPER_NO_CONTRACT;
        SUPPORTS_DAC;

        return IsPointingTo(GetTarget(), pNativeCode);
    }

    BOOL IsPointingToPrestub(PCODE target);

    BOOL IsPointingToPrestub()
    {
        WRAPPER_NO_CONTRACT;
        return IsPointingToPrestub(GetTarget());
    }

    PCODE GetEntryPoint()
    {
        LIMITED_METHOD_CONTRACT;
        return dac_cast<TADDR>(this) + GetEntryPointOffset();
    }

    static SIZE_T GetEntryPointOffset()
    {
        LIMITED_METHOD_CONTRACT;
#ifdef TARGET_ARM
        return THUMB_CODE;
#else
        return 0;
#endif
    }

    MethodDesc *  GetMethodDesc(BOOL fSpeculative = FALSE);
    BOOL          IsCorrectMethodDesc(MethodDesc *  pMD);

    static Precode* Allocate(PrecodeType t, MethodDesc* pMD,
        LoaderAllocator *pLoaderAllocator, AllocMemTracker *pamTracker);
    void Init(Precode* pPrecodeRX, PrecodeType t, MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

#ifndef DACCESS_COMPILE
    void ResetTargetInterlocked();
    BOOL SetTargetInterlocked(PCODE target, BOOL fOnlyRedirectFromPrestub = TRUE);

    // Reset precode to point to prestub
    void Reset();
#endif // DACCESS_COMPILE

    static Precode* GetPrecodeFromEntryPoint(PCODE addr, BOOL fSpeculative = FALSE)
    {
        LIMITED_METHOD_DAC_CONTRACT;

#ifdef DACCESS_COMPILE
        // Always use speculative checks with DAC
        fSpeculative = TRUE;
#endif

        TADDR pInstr = PCODEToPINSTR(addr);

        // Always do consistency check in debug
        if (fSpeculative INDEBUG(|| TRUE))
        {
            if (!IS_ALIGNED(pInstr, PRECODE_ALIGNMENT) || !IsValidType(PTR_Precode(pInstr)->GetType()))
            {
                if (fSpeculative) return NULL;
                _ASSERTE(!"Precode::GetPrecodeFromEntryPoint: Unexpected code in precode");
            }
        }

        Precode* pPrecode = PTR_Precode(pInstr);

        if (!fSpeculative)
        {
            g_IBCLogger.LogMethodPrecodeAccess(pPrecode->GetMethodDesc());
        }

        return pPrecode;
    }

    // If addr is patched fixup precode, returns address that it points to. Otherwise returns NULL.
    static PCODE TryToSkipFixupPrecode(PCODE addr);

    //
    // Precode as temporary entrypoint
    //

    static SIZE_T SizeOfTemporaryEntryPoint(PrecodeType t)
    {
        LIMITED_METHOD_DAC_CONTRACT;

        return ALIGN_UP(SizeOf(t), AlignOf(t));
    }

    static Precode * GetPrecodeForTemporaryEntryPoint(TADDR temporaryEntryPoints, int index);

    static SIZE_T SizeOfTemporaryEntryPoints(PrecodeType t, int count);
    static SIZE_T SizeOfTemporaryEntryPoints(TADDR temporaryEntryPoints, int count);

    static TADDR AllocateTemporaryEntryPoints(MethodDescChunk* pChunk,
        LoaderAllocator *pLoaderAllocator, AllocMemTracker *pamTracker);

#ifdef DACCESS_COMPILE
    void EnumMemoryRegions(CLRDataEnumMemoryFlags flags);
#endif
};

#endif // __PRECODE_H__
