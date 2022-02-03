// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
// VirtualCallStubCpu.hpp
//
#ifndef _VIRTUAL_CALL_STUB_ARM_H
#define _VIRTUAL_CALL_STUB_ARM_H

#define DISPATCH_STUB_FIRST_DWORD 0xf940000d
#define RESOLVE_STUB_FIRST_DWORD 0xF940000C
#define VTABLECALL_STUB_FIRST_DWORD 0xF9400009

#define USES_LOOKUP_STUBS   1

struct LookupStubData
{
    size_t DispatchToken;
    PCODE  ResolveWorkerTarget;
};

typedef DPTR(LookupStubData) PTR_LookupStubData;

struct LookupStub
{
    inline PCODE entryPoint() { LIMITED_METHOD_CONTRACT; return (PCODE)this; }
    inline size_t token()           { LIMITED_METHOD_CONTRACT; return GetData()->DispatchToken; }
    inline size_t  size() { LIMITED_METHOD_CONTRACT; return sizeof(LookupStub); }
private :
    friend struct LookupHolder;

    PTR_LookupStubData GetData() const
    {
        return dac_cast<PTR_LookupStubData>((uint8_t*)this + 4096);
    }

    DWORD _entryPoint[4];
};

struct LookupHolder
{
private:
    LookupStub _stub;
public:
    void  Initialize(LookupHolder* pLookupHolderRX, PCODE resolveWorkerTarget, size_t dispatchToken)
    {
        LookupStubData *pData = stub()->GetData();
        pData->DispatchToken = dispatchToken;
        pData->ResolveWorkerTarget = resolveWorkerTarget;
    }

    static size_t GenerateCodePage(uint8_t* pageBase);

    LookupStub*    stub()        { LIMITED_METHOD_CONTRACT; return &_stub; }
    static LookupHolder*  FromLookupEntry(PCODE lookupEntry)
    {
        return (LookupHolder*)lookupEntry;
    }
};

struct DispatchStubData
{
    size_t ExpectedMT;
    PCODE ImplTarget;
    PCODE FailTarget;
};

typedef DPTR(DispatchStubData) PTR_DispatchStubData;

struct DispatchStub
{
    inline PCODE entryPoint()         { LIMITED_METHOD_CONTRACT; return (PCODE)&_entryPoint[0]; }
    inline size_t expectedMT() const { LIMITED_METHOD_CONTRACT;  return GetData()->ExpectedMT; }
    inline size_t size()        { LIMITED_METHOD_CONTRACT; return sizeof(DispatchStub); }


    inline PCODE implTarget() const
    {
        LIMITED_METHOD_CONTRACT;
        return GetData()->ImplTarget;
    }

    inline TADDR implTargetSlot(EntryPointSlots::SlotType *slotTypeRef) const
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(slotTypeRef != nullptr);

        *slotTypeRef = EntryPointSlots::SlotType_Normal;
        return (TADDR)&GetData()->ImplTarget;
    }

    inline PCODE failTarget() const
    {
        return GetData()->FailTarget;
    }

private:
    friend struct DispatchHolder;

    DWORD _entryPoint[8];

    PTR_DispatchStubData GetData() const
    {
        return dac_cast<PTR_DispatchStubData>((uint8_t*)this + 4096);
    }
};

struct DispatchHolder
{
    void  Initialize(DispatchHolder* pDispatchHolderRX, PCODE implTarget, PCODE failTarget, size_t expectedMT)
    {
        DispatchStubData *pData = stub()->GetData();
        pData->ExpectedMT = expectedMT;
        pData->ImplTarget = implTarget;
        pData->FailTarget = failTarget;
    }

    DispatchStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static size_t GetHolderSize()
    {
        STATIC_CONTRACT_WRAPPER; return sizeof(DispatchStub);
    }

    static size_t GenerateCodePage(uint8_t* pageBase);

    static DispatchHolder*  FromDispatchEntry(PCODE dispatchEntry)
    {
        LIMITED_METHOD_CONTRACT;
        DispatchHolder* dispatchHolder = (DispatchHolder*)dispatchEntry;
        return dispatchHolder;
    }

private:
    DispatchStub _stub;
};

struct ResolveStubData
{
    size_t CacheAddress;
    UINT32 HashedToken;
    UINT32 CacheMask;
    size_t Token;
    INT32  Counter;
    INT32  _dummy;
    PCODE  ResolveWorkerTarget;
    PCODE  PatcherTarget;
};

typedef DPTR(ResolveStubData) PTR_ResolveStubData;

extern "C" void ResolveStubCode();
extern "C" void ResolveStubCode_FailEntry();
extern "C" void ResolveStubCode_SlowEntry();

struct ResolveStub
{
    inline PCODE failEntryPoint()       { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this + ((BYTE*)ResolveStubCode_FailEntry - (BYTE*)ResolveStubCode)); }
    inline PCODE resolveEntryPoint()    { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this); }
    inline PCODE slowEntryPoint()       { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this + ((BYTE*)ResolveStubCode_SlowEntry - (BYTE*)ResolveStubCode)); }

    inline INT32* pCounter()            { LIMITED_METHOD_CONTRACT; return &GetData()->Counter; }
    inline UINT32 hashedToken()         { LIMITED_METHOD_CONTRACT; return GetData()->HashedToken >> LOG2_PTRSIZE; }
    inline size_t cacheAddress()        { LIMITED_METHOD_CONTRACT; return GetData()->CacheAddress; }
    inline size_t token()               { LIMITED_METHOD_CONTRACT; return GetData()->Token; }
    inline size_t size()                { LIMITED_METHOD_CONTRACT; return sizeof(ResolveStub); }

private:
    friend struct ResolveHolder;
    const static int resolveEntryPointLen = 17;
    const static int slowEntryPointLen = 4;
    const static int failEntryPointLen = 8;

    PTR_ResolveStubData GetData() const
    {
        return dac_cast<PTR_ResolveStubData>((uint8_t*)this + 4096);
    }

    DWORD code[32];
};

struct ResolveHolder
{
    void Initialize(ResolveHolder* pResolveHolderRX,
                    PCODE resolveWorkerTarget, PCODE patcherTarget,
                    size_t dispatchToken, UINT32 hashedToken,
                    void * cacheAddr, INT32 counterValue)
    {
        ResolveStubData *pData = stub()->GetData();

        pData->CacheAddress = (size_t)cacheAddr;
        pData->HashedToken = hashedToken << LOG2_PTRSIZE;
        pData->CacheMask = CALL_STUB_CACHE_MASK * sizeof(void*);
        pData->Token = dispatchToken;
        pData->Counter = counterValue;
        pData->ResolveWorkerTarget = resolveWorkerTarget;
        pData->PatcherTarget = patcherTarget;
    }

    ResolveStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static size_t GenerateCodePage(uint8_t* pageBase);

    static ResolveHolder*  FromFailEntry(PCODE failEntry);
    static ResolveHolder*  FromResolveEntry(PCODE resolveEntry);
private:
    ResolveStub _stub;
};

/*VTableCallStub**************************************************************************************
These are jump stubs that perform a vtable-base virtual call. These stubs assume that an object is placed
in the first argument register (this pointer). From there, the stub extracts the MethodTable pointer, followed by the
vtable pointer, and finally jumps to the target method at a given slot in the vtable.
*/
struct VTableCallStub
{
    friend struct VTableCallHolder;

    inline size_t size()
    {
        LIMITED_METHOD_CONTRACT;

        BYTE* pStubCode = (BYTE *)this;

        int numDataSlots = 0;

        size_t cbSize = 4;              // First ldr instruction

        for (int i = 0; i < 2; i++)
        {
            if (((*(DWORD*)(&pStubCode[cbSize])) & 0xFFC003FF) == 0xF9400129)
            {
                // ldr x9, [x9, #offsetOfIndirection]
                cbSize += 4;
            }
            else
            {
                // These 2 instructions used when the indirection offset is >= 0x8000
                // ldr w10, [PC, #dataOffset]
                // ldr x9, [x9, x10]
                numDataSlots++;
                cbSize += 8;
            }
        }
        return cbSize +
                4 +                     // Last 'br x9' instruction
                (numDataSlots * 4) +    // Data slots containing indirection offset values
                4;                      // Slot value (data storage, not a real instruction)
    }

    inline PCODE        entryPoint()        const { LIMITED_METHOD_CONTRACT;  return (PCODE)&_entryPoint[0]; }

    inline size_t token()
    {
        LIMITED_METHOD_CONTRACT;
        DWORD slot = *(DWORD*)(reinterpret_cast<BYTE*>(this) + size() - 4);
        return DispatchToken::CreateDispatchToken(slot).To_SIZE_T();
    }

private:
    BYTE    _entryPoint[0];         // Dynamically sized stub. See Initialize() for more details.
};

/* VTableCallHolders are the containers for VTableCallStubs, they provide for any alignment of
stubs as necessary.  */
struct VTableCallHolder
{
    void  Initialize(unsigned slot);

    VTableCallStub* stub() { LIMITED_METHOD_CONTRACT;  return reinterpret_cast<VTableCallStub *>(this); }

    static size_t GetHolderSize(unsigned slot)
    {
        STATIC_CONTRACT_WRAPPER;
        unsigned offsetOfIndirection = MethodTable::GetVtableOffset() + MethodTable::GetIndexOfVtableIndirection(slot) * TARGET_POINTER_SIZE;
        unsigned offsetAfterIndirection = MethodTable::GetIndexAfterVtableIndirection(slot) * TARGET_POINTER_SIZE;
        int indirectionsCodeSize = (offsetOfIndirection >= 0x8000 ? 8 : 4) + (offsetAfterIndirection >= 0x8000 ? 8 : 4);
        int indirectionsDataSize = (offsetOfIndirection >= 0x8000 ? 4 : 0) + (offsetAfterIndirection >= 0x8000 ? 4 : 0);
        return 8 + indirectionsCodeSize + indirectionsDataSize + 4;
    }

    static VTableCallHolder* FromVTableCallEntry(PCODE entry) { LIMITED_METHOD_CONTRACT; return (VTableCallHolder*)entry; }

private:
    // VTableCallStub follows here. It is dynamically sized on allocation because it could
    // use short/long instruction sizes for LDR, depending on the slot value.
};


#ifdef DECLARE_DATA

#ifndef DACCESS_COMPILE
ResolveHolder* ResolveHolder::FromFailEntry(PCODE failEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( failEntry - ((BYTE*)ResolveStubCode_FailEntry - (BYTE*)ResolveStubCode));
    return resolveHolder;
}

ResolveHolder* ResolveHolder::FromResolveEntry(PCODE resolveEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*)resolveEntry;
    return resolveHolder;
}

void VTableCallHolder::Initialize(unsigned slot)
{
    unsigned offsetOfIndirection = MethodTable::GetVtableOffset() + MethodTable::GetIndexOfVtableIndirection(slot) * TARGET_POINTER_SIZE;
    unsigned offsetAfterIndirection = MethodTable::GetIndexAfterVtableIndirection(slot) * TARGET_POINTER_SIZE;

    int indirectionsCodeSize = (offsetOfIndirection >= 0x8000 ? 8 : 4) + (offsetAfterIndirection >= 0x8000 ? 8 : 4);
    int indirectionsDataSize = (offsetOfIndirection >= 0x8000 ? 4 : 0) + (offsetAfterIndirection >= 0x8000 ? 4 : 0);
    int codeSize = 8 + indirectionsCodeSize + indirectionsDataSize;

    VTableCallStub* pStub = stub();
    BYTE* p = (BYTE*)pStub->entryPoint();

    // ldr x9,[x0] : x9 = MethodTable pointer
    *(UINT32*)p = 0xF9400009; p += 4;

    // moving offset value wrt PC. Currently points to first indirection offset data.
    uint dataOffset = codeSize - indirectionsDataSize - 4;

    if (offsetOfIndirection >= 0x8000)
    {
        // ldr w10, [PC, #dataOffset]
        *(DWORD*)p = 0x1800000a | ((dataOffset >> 2) << 5); p += 4;
        // ldr x9, [x9, x10]
        *(DWORD*)p = 0xf86a6929; p += 4;

        // move to next indirection offset data
        dataOffset = dataOffset - 8 + 4; // subtract 8 as we have moved PC by 8 and add 4 as next data is at 4 bytes from previous data
    }
    else
    {
        // ldr x9, [x9, #offsetOfIndirection]
        *(DWORD*)p = 0xf9400129 | (((UINT32)offsetOfIndirection >> 3) << 10);
        p += 4;
    }

    if (offsetAfterIndirection >= 0x8000)
    {
        // ldr w10, [PC, #dataOffset]
        *(DWORD*)p = 0x1800000a | ((dataOffset >> 2) << 5); p += 4;
        // ldr x9, [x9, x10]
        *(DWORD*)p = 0xf86a6929; p += 4;
    }
    else
    {
        // ldr x9, [x9, #offsetAfterIndirection]
        *(DWORD*)p = 0xf9400129 | (((UINT32)offsetAfterIndirection >> 3) << 10);
        p += 4;
    }

    // br x9
    *(UINT32*)p = 0xd61f0120; p += 4;

    // data labels:
    if (offsetOfIndirection >= 0x8000)
    {
        *(UINT32*)p = (UINT32)offsetOfIndirection;
        p += 4;
    }
    if (offsetAfterIndirection >= 0x8000)
    {
        *(UINT32*)p = (UINT32)offsetAfterIndirection;
        p += 4;
    }

    // Store the slot value here for convenience. Not a real instruction (unreachable anyways)
    // NOTE: Not counted in codeSize above.
    *(UINT32*)p = slot; p += 4;

    _ASSERT(p == (BYTE*)stub()->entryPoint() + VTableCallHolder::GetHolderSize(slot));
    _ASSERT(stub()->size() == VTableCallHolder::GetHolderSize(slot));
}

#endif // DACCESS_COMPILE

VirtualCallStubManager::StubKind VirtualCallStubManager::predictStubKind(PCODE stubStartAddress)
{

    SUPPORTS_DAC;
#ifdef DACCESS_COMPILE

    return SK_BREAKPOINT;  // Dac always uses the slower lookup

#else

    StubKind stubKind = SK_UNKNOWN;
    TADDR pInstr = PCODEToPINSTR(stubStartAddress);

    EX_TRY
    {
        // If stubStartAddress is completely bogus, then this might AV,
        // so we protect it with SEH. An AV here is OK.
        AVInRuntimeImplOkayHolder AVOkay;

        DWORD firstDword = *((DWORD*) pInstr);

        if (firstDword == DISPATCH_STUB_FIRST_DWORD) // assembly of first instruction of DispatchStub : ldr x13, [x0]
        {
            stubKind = SK_DISPATCH;
        }
        else if (firstDword == RESOLVE_STUB_FIRST_DWORD) // assembly of first instruction of ResolveStub : ldr x12, [x0,#Object.m_pMethTab ]
        {
            stubKind = SK_RESOLVE;
        }
        else if (firstDword == VTABLECALL_STUB_FIRST_DWORD) // assembly of first instruction of VTableCallStub : ldr x9, [x0]
        {
            stubKind = SK_VTABLECALL;
        }
        else if (firstDword == 0x5800800C) // assembly of first instruction of LookupStub : xxxxxxxxxxxxxxxx
        {
            stubKind = SK_LOOKUP;
        }
    }
    EX_CATCH
    {
        stubKind = SK_UNKNOWN;
    }
    EX_END_CATCH(SwallowAllExceptions);

    return stubKind;

#endif // DACCESS_COMPILE
}

#endif //DECLARE_DATA

#endif // _VIRTUAL_CALL_STUB_ARM_H
