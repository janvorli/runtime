// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// precode.cpp
//

//
// Stub that runs before the actual native code
//


#include "common.h"

#ifdef FEATURE_PERFMAP
#include "perfmap.h"
#endif

//==========================================================================================
// class Precode
//==========================================================================================
BOOL Precode::IsValidType(PrecodeType t)
{
    LIMITED_METHOD_CONTRACT;
    SUPPORTS_DAC;

    switch (t) {
    case PRECODE_STUB:
#ifdef HAS_NDIRECT_IMPORT_PRECODE
    case PRECODE_NDIRECT_IMPORT:
#endif // HAS_NDIRECT_IMPORT_PRECODE
#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
#endif // HAS_THISPTR_RETBUF_PRECODE
        return TRUE;
    default:
        return FALSE;
    }
}

SIZE_T Precode::SizeOf(PrecodeType t)
{
    LIMITED_METHOD_CONTRACT;
    SUPPORTS_DAC;

    switch (t)
    {
    case PRECODE_STUB:
        return sizeof(StubPrecode);
#ifdef HAS_NDIRECT_IMPORT_PRECODE
    case PRECODE_NDIRECT_IMPORT:
        return sizeof(NDirectImportPrecode);
#endif // HAS_NDIRECT_IMPORT_PRECODE
#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
        return sizeof(FixupPrecode);
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
        return sizeof(ThisPtrRetBufPrecode);
#endif // HAS_THISPTR_RETBUF_PRECODE

    default:
        UnexpectedPrecodeType("Precode::SizeOf", t);
        break;
    }
    return 0;
}

// Note: This is immediate target of the precode. It does not follow jump stub if there is one.
PCODE Precode::GetTarget()
{
    LIMITED_METHOD_CONTRACT;
    SUPPORTS_DAC;

    PCODE target = NULL;

    PrecodeType precodeType = GetType();
    switch (precodeType)
    {
    case PRECODE_STUB:
        target = AsStubPrecode()->GetTarget();
        break;
#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
        target = AsFixupPrecode()->GetTarget();
        break;
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
        target = AsThisPtrRetBufPrecode()->GetTarget();
        break;
#endif // HAS_THISPTR_RETBUF_PRECODE

    default:
        UnexpectedPrecodeType("Precode::GetTarget", precodeType);
        break;
    }
    return target;
}

MethodDesc* Precode::GetMethodDesc(BOOL fSpeculative /*= FALSE*/)
{
    CONTRACTL {
        NOTHROW;
        GC_NOTRIGGER;
        SUPPORTS_DAC;
    } CONTRACTL_END;

    TADDR pMD = NULL;

    PrecodeType precodeType = GetType();
    switch (precodeType)
    {
    case PRECODE_STUB:
        pMD = AsStubPrecode()->GetMethodDesc();
        break;
#ifdef HAS_NDIRECT_IMPORT_PRECODE
    case PRECODE_NDIRECT_IMPORT:
        pMD = AsNDirectImportPrecode()->GetMethodDesc();
        break;
#endif // HAS_NDIRECT_IMPORT_PRECODE
#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
        pMD = AsFixupPrecode()->GetMethodDesc();
        break;
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
        pMD = AsThisPtrRetBufPrecode()->GetMethodDesc();
        break;
#endif // HAS_THISPTR_RETBUF_PRECODE

    default:
        break;
    }

    if (pMD == NULL)
    {
        if (fSpeculative)
            return NULL;
        else
            UnexpectedPrecodeType("Precode::GetMethodDesc", precodeType);
    }

    // GetMethodDesc() on platform specific precode types returns TADDR. It should return
    // PTR_MethodDesc instead. It is a workaround to resolve cyclic dependency between headers.
    // Once we headers factoring of headers cleaned up, we should be able to get rid of it.

    // For speculative calls, pMD can be garbage that causes IBC logging to crash
    if (!fSpeculative)
        g_IBCLogger.LogMethodPrecodeAccess((PTR_MethodDesc)pMD);

    return (PTR_MethodDesc)pMD;
}

BOOL Precode::IsCorrectMethodDesc(MethodDesc *  pMD)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;
    MethodDesc * pMDfromPrecode = GetMethodDesc(TRUE);

    if (pMDfromPrecode == pMD)
        return TRUE;

    return FALSE;
}

BOOL Precode::IsPointingToPrestub(PCODE target)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    if (IsPointingTo(target, GetPreStubEntryPoint()))
        return TRUE;

#ifdef HAS_FIXUP_PRECODE
#ifdef TARGET_ARM64
    if (IsPointingTo(target, ((PCODE)this + 8)))
#elif defined(TARGET_AMD64)
    if (IsPointingTo(target, ((PCODE)this + 6)))
#elif defined(TARGET_X86)
    if (IsPointingTo(target, ((PCODE)this + 6)))
#else
#error Implement this!    
#endif
        return TRUE;
#endif

    return FALSE;
}

// If addr is patched fixup precode, returns address that it points to. Otherwise returns NULL.
PCODE Precode::TryToSkipFixupPrecode(PCODE addr)
{
    CONTRACTL {
        NOTHROW;
        GC_NOTRIGGER;
    } CONTRACTL_END;

    PCODE pTarget = NULL;

    return pTarget;
}

Precode* Precode::GetPrecodeForTemporaryEntryPoint(TADDR temporaryEntryPoints, int index)
{
    WRAPPER_NO_CONTRACT;
    PrecodeType t = PTR_Precode(temporaryEntryPoints)->GetType();
    SIZE_T oneSize = SizeOfTemporaryEntryPoint(t);
    return PTR_Precode(temporaryEntryPoints + index * oneSize);
}

SIZE_T Precode::SizeOfTemporaryEntryPoints(PrecodeType t, int count)
{
    WRAPPER_NO_CONTRACT;
    SUPPORTS_DAC;

    SIZE_T oneSize = SizeOfTemporaryEntryPoint(t);
    return count * oneSize;
}

SIZE_T Precode::SizeOfTemporaryEntryPoints(TADDR temporaryEntryPoints, int count)
{
    WRAPPER_NO_CONTRACT;
    SUPPORTS_DAC;

    PrecodeType precodeType = PTR_Precode(temporaryEntryPoints)->GetType();
    return SizeOfTemporaryEntryPoints(precodeType, count);
}

#ifndef DACCESS_COMPILE

Precode* Precode::Allocate(PrecodeType t, MethodDesc* pMD,
                           LoaderAllocator *  pLoaderAllocator,
                           AllocMemTracker *  pamTracker)
{
    CONTRACTL
    {
        THROWS;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    SIZE_T size = Precode::SizeOf(t);
    Precode* pPrecode;

    if (t == PRECODE_FIXUP)
    {
        pPrecode = (Precode*)pLoaderAllocator->GetFixupPrecodeHeap()->Allocate();
        pPrecode->Init(pPrecode, t, pMD, pLoaderAllocator);
    }
    else if (t == PRECODE_STUB || t == PRECODE_NDIRECT_IMPORT)
    {
        pPrecode = (Precode*)pLoaderAllocator->GetNewStubPrecodeHeap()->Allocate();
        pPrecode->Init(pPrecode, t, pMD, pLoaderAllocator);
    }
    else
    {
        pPrecode = (Precode*)pamTracker->Track(pLoaderAllocator->GetPrecodeHeap()->AllocAlignedMem(size, AlignOf(t)));
        ExecutableWriterHolder<Precode> precodeWriterHolder(pPrecode, size);
        precodeWriterHolder.GetRW()->Init(pPrecode, t, pMD, pLoaderAllocator);
        ClrFlushInstructionCache(pPrecode, size);

    }

//    ClrFlushInstructionCache(pPrecode, size);

    return pPrecode;
}

void Precode::Init(Precode* pPrecodeRX, PrecodeType t, MethodDesc* pMD, LoaderAllocator *pLoaderAllocator)
{
    LIMITED_METHOD_CONTRACT;

    switch (t) {
    case PRECODE_STUB:
        ((StubPrecode*)this)->Init((StubPrecode*)pPrecodeRX, pMD, pLoaderAllocator);
        break;
#ifdef HAS_NDIRECT_IMPORT_PRECODE
    case PRECODE_NDIRECT_IMPORT:
        ((NDirectImportPrecode*)this)->Init((NDirectImportPrecode*)pPrecodeRX, pMD, pLoaderAllocator);
        break;
#endif // HAS_NDIRECT_IMPORT_PRECODE
#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
        ((FixupPrecode*)this)->Init((FixupPrecode*)pPrecodeRX, pMD, pLoaderAllocator);
        break;
#endif // HAS_FIXUP_PRECODE
#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
        ((ThisPtrRetBufPrecode*)this)->Init(pMD, pLoaderAllocator);
        break;
#endif // HAS_THISPTR_RETBUF_PRECODE
    default:
        UnexpectedPrecodeType("Precode::Init", t);
        break;
    }

    _ASSERTE(IsValidType(GetType()));
}

void Precode::ResetTargetInterlocked()
{
    WRAPPER_NO_CONTRACT;

    PrecodeType precodeType = GetType();
    switch (precodeType)
    {
        case PRECODE_STUB:
            AsStubPrecode()->ResetTargetInterlocked();
            break;

#ifdef HAS_FIXUP_PRECODE
        case PRECODE_FIXUP:
            AsFixupPrecode()->ResetTargetInterlocked();
            break;
#endif // HAS_FIXUP_PRECODE

        default:
            UnexpectedPrecodeType("Precode::ResetTargetInterlocked", precodeType);
            break;
    }

    // Although executable code is modified on x86/x64, a FlushInstructionCache() is not necessary on those platforms due to the
    // interlocked operation above (see ClrFlushInstructionCache())
}

BOOL Precode::SetTargetInterlocked(PCODE target, BOOL fOnlyRedirectFromPrestub)
{
    WRAPPER_NO_CONTRACT;
    _ASSERTE(!IsPointingToPrestub(target));

    PCODE expected = GetTarget();
    BOOL ret = FALSE;

    if (fOnlyRedirectFromPrestub && !IsPointingToPrestub(expected))
        return FALSE;

    g_IBCLogger.LogMethodPrecodeWriteAccess(GetMethodDesc());

    PrecodeType precodeType = GetType();
    switch (precodeType)
    {
    case PRECODE_STUB:
        ret = AsStubPrecode()->SetTargetInterlocked(target, expected);
        break;

#ifdef HAS_FIXUP_PRECODE
    case PRECODE_FIXUP:
        ret = AsFixupPrecode()->SetTargetInterlocked(target, expected);
        break;
#endif // HAS_FIXUP_PRECODE

#ifdef HAS_THISPTR_RETBUF_PRECODE
    case PRECODE_THISPTR_RETBUF:
        ret = AsThisPtrRetBufPrecode()->SetTargetInterlocked(target, expected);
        break;
#endif // HAS_THISPTR_RETBUF_PRECODE

    default:
        UnexpectedPrecodeType("Precode::SetTargetInterlocked", precodeType);
        break;
    }

    // Although executable code is modified on x86/x64, a FlushInstructionCache() is not necessary on those platforms due to the
    // interlocked operation above (see ClrFlushInstructionCache())

    return ret;
}

void Precode::Reset()
{
    WRAPPER_NO_CONTRACT;

    MethodDesc* pMD = GetMethodDesc();

    PrecodeType t = GetType();
    SIZE_T size = Precode::SizeOf(t);

    if (t == PRECODE_FIXUP)
    {
        Init(this, t, pMD, pMD->GetLoaderAllocator());
    }
    else
    {
        ExecutableWriterHolder<Precode> precodeWriterHolder(this, size);
        precodeWriterHolder.GetRW()->Init(this, t, pMD, pMD->GetLoaderAllocator());
        ClrFlushInstructionCache(this, SizeOf());
    }
}

/* static */
TADDR Precode::AllocateTemporaryEntryPoints(MethodDescChunk *  pChunk,
                                            LoaderAllocator *  pLoaderAllocator,
                                            AllocMemTracker *  pamTracker)
{
    WRAPPER_NO_CONTRACT;

    MethodDesc* pFirstMD = pChunk->GetFirstMethodDesc();

    int count = pChunk->GetCount();

    // Determine eligibility for tiered compilation
#ifdef HAS_COMPACT_ENTRYPOINTS
    bool hasMethodDescVersionableWithPrecode = false;
#endif
    {
        MethodDesc *pMD = pChunk->GetFirstMethodDesc();
        for (int i = 0; i < count; ++i)
        {
            if (pMD->DetermineAndSetIsEligibleForTieredCompilation())
            {
                _ASSERTE(pMD->IsEligibleForTieredCompilation());
                _ASSERTE(!pMD->IsVersionableWithPrecode() || pMD->RequiresStableEntryPoint());
            }

#ifdef HAS_COMPACT_ENTRYPOINTS
            if (pMD->IsVersionableWithPrecode())
            {
                _ASSERTE(pMD->RequiresStableEntryPoint());
                hasMethodDescVersionableWithPrecode = true;
            }
#endif

            pMD = (MethodDesc *)(dac_cast<TADDR>(pMD) + pMD->SizeOf());
        }
    }

    PrecodeType t = PRECODE_STUB;
    bool preallocateJumpStubs = false;

#ifdef HAS_FIXUP_PRECODE
    // Default to faster fixup precode if possible
    if (!pFirstMD->RequiresMethodDescCallingConvention(count > 1))
    {
        t = PRECODE_FIXUP;

    }
    else
    {
        _ASSERTE(!pFirstMD->IsLCGMethod());
    }
#endif // HAS_FIXUP_PRECODE

    SIZE_T totalSize = SizeOfTemporaryEntryPoints(t, count);

#ifdef HAS_COMPACT_ENTRYPOINTS
    // Note that these are just best guesses to save memory. If we guessed wrong,
    // we will allocate a new exact type of precode in GetOrCreatePrecode.
    BOOL fForcedPrecode = hasMethodDescVersionableWithPrecode || pFirstMD->RequiresStableEntryPoint(count > 1);

#ifdef TARGET_ARM
    if (pFirstMD->RequiresMethodDescCallingConvention(count > 1)
        || count >= MethodDescChunk::GetCompactEntryPointMaxCount ())
    {
        // We do not pass method desc on scratch register
        fForcedPrecode = TRUE;
    }
#endif // TARGET_ARM

    if (!fForcedPrecode && (totalSize > MethodDescChunk::SizeOfCompactEntryPoints(count)))
        return NULL;
#endif

    TADDR temporaryEntryPoints;
    SIZE_T oneSize = SizeOfTemporaryEntryPoint(t);
    MethodDesc * pMD = pChunk->GetFirstMethodDesc();

    if (t == PRECODE_FIXUP)
    {
        // This is unfortunate, as the entrypoints needs to be sequential.
        temporaryEntryPoints = (TADDR)pLoaderAllocator->GetFixupPrecodeHeap()->Allocate(count);
        TADDR entryPoint = temporaryEntryPoints;
        for (int i = 0; i < count; i++)
        {
            ((Precode *)entryPoint)->Init((Precode *)entryPoint, t, pMD, pLoaderAllocator);

            _ASSERTE((Precode *)entryPoint == GetPrecodeForTemporaryEntryPoint(temporaryEntryPoints, i));
            entryPoint += oneSize;

            pMD = (MethodDesc *)(dac_cast<TADDR>(pMD) + pMD->SizeOf());
        }
    }
    else if (t == PRECODE_STUB || t == PRECODE_NDIRECT_IMPORT)
    {
        temporaryEntryPoints = (TADDR)pLoaderAllocator->GetNewStubPrecodeHeap()->Allocate(count);
        TADDR entryPoint = temporaryEntryPoints;
        for (int i = 0; i < count; i++)
        {
            ((Precode*)entryPoint)->Init((Precode*)entryPoint, t, pMD, pLoaderAllocator);

            _ASSERTE((Precode*)entryPoint == GetPrecodeForTemporaryEntryPoint(temporaryEntryPoints, i));
            entryPoint += oneSize;

            pMD = (MethodDesc*)(dac_cast<TADDR>(pMD) + pMD->SizeOf());
        }
    }
    else
    {
        temporaryEntryPoints = (TADDR)pamTracker->Track(pLoaderAllocator->GetPrecodeHeap()->AllocAlignedMem(totalSize, AlignOf(t)));
        ExecutableWriterHolder<void> entryPointsWriterHolder((void*)temporaryEntryPoints, totalSize);

        TADDR entryPoint = temporaryEntryPoints;
        TADDR entryPointRW = (TADDR)entryPointsWriterHolder.GetRW();
        for (int i = 0; i < count; i++)
        {
            ((Precode *)entryPointRW)->Init((Precode *)entryPoint, t, pMD, pLoaderAllocator);

            _ASSERTE((Precode *)entryPoint == GetPrecodeForTemporaryEntryPoint(temporaryEntryPoints, i));
            entryPoint += oneSize;
            entryPointRW += oneSize;

            pMD = (MethodDesc *)(dac_cast<TADDR>(pMD) + pMD->SizeOf());
        }
    }

#ifdef FEATURE_PERFMAP
    PerfMap::LogStubs(__FUNCTION__, "PRECODE_STUB", (PCODE)temporaryEntryPoints, count * oneSize);
#endif

    ClrFlushInstructionCache((LPVOID)temporaryEntryPoints, count * oneSize);

    return temporaryEntryPoints;
}

#endif // !DACCESS_COMPILE

#ifdef DACCESS_COMPILE
void Precode::EnumMemoryRegions(CLRDataEnumMemoryFlags flags)
{
    SUPPORTS_DAC;
    PrecodeType t = GetType();

    DacEnumMemoryRegion(GetStart(), SizeOf(t));
}
#endif

