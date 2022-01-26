// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
// File: virtualcallstubcpu.hpp
//


//

//
// ============================================================================

#ifndef _VIRTUAL_CALL_STUB_X86_H
#define _VIRTUAL_CALL_STUB_X86_H

#ifdef DECLARE_DATA
#include "asmconstants.h"
#endif

#include <pshpack1.h>  // Since we are placing code, we want byte packing of the structs

#define USES_LOOKUP_STUBS	1

/*********************************************************************************************
Stubs that contain code are all part of larger structs called Holders.  There is a
Holder for each kind of stub, i.e XXXStub is contained with XXXHolder.  Holders are
essentially an implementation trick that allowed rearranging the code sequences more
easily while trying out different alternatives, and for dealing with any alignment
issues in a way that was mostly immune to the actually code sequences.  These Holders
should be revisited when the stub code sequences are fixed, since in many cases they
add extra space to a stub that is not really needed.

Stubs are placed in cache and hash tables.  Since unaligned access of data in memory
is very slow, the keys used in those tables should be aligned.  The things used as keys
typically also occur in the generated code, e.g. a token as an immediate part of an instruction.
For now, to avoid alignment computations as different code strategies are tried out, the key
fields are all in the Holders.  Eventually, many of these fields should be dropped, and the instruction
streams aligned so that the immediate fields fall on aligned boundaries.
*/

#if USES_LOOKUP_STUBS

struct LookupStub;
struct LookupHolder;

/*LookupStub**************************************************************************************
Virtual and interface call sites are initially setup to point at LookupStubs.
This is because the runtime type of the <this> pointer is not yet known,
so the target cannot be resolved.  Note: if the jit is able to determine the runtime type
of the <this> pointer, it should be generating a direct call not a virtual or interface call.
This stub pushes a lookup token onto the stack to identify the sought after method, and then
jumps into the EE (VirtualCallStubManager::ResolveWorkerStub) to effectuate the lookup and
transfer of control to the appropriate target method implementation, perhaps patching of the call site
along the way to point to a more appropriate stub.  Hence callsites that point to LookupStubs
get quickly changed to point to another kind of stub.
*/

struct LookupStubData
{
    size_t DispatchToken;
    PCODE  ResolveWorkerTarget;
};

typedef DPTR(LookupStubData) PTR_LookupStubData;

struct LookupStub
{
    inline PCODE entryPoint() const { LIMITED_METHOD_CONTRACT;  return (PCODE)((BYTE*)this); }
    inline size_t token()           { LIMITED_METHOD_CONTRACT; return GetData()->DispatchToken; }
    inline size_t size()            { LIMITED_METHOD_CONTRACT; return sizeof(LookupStub); }

private:
    friend struct LookupHolder;

    PTR_LookupStubData GetData() const
    {
        return dac_cast<PTR_LookupStubData>((uint8_t*)this + 4096);
    }

    BYTE code[16];    
};

/* LookupHolders are the containers for LookupStubs, they provide for any alignment of
stubs as necessary.  In the case of LookupStubs, alignment is necessary since
LookupStubs are placed in a hash table keyed by token. */
struct LookupHolder
{
    static void InitializeStatic();

    void  Initialize(LookupHolder* pLookupHolderRX, PCODE resolveWorkerTarget, size_t dispatchToken);

    LookupStub*    stub()               { LIMITED_METHOD_CONTRACT;  return &_stub;    }

    static LookupHolder*  FromLookupEntry(PCODE lookupEntry);

    static size_t GenerateCodePage(uint8_t* pageBase);

private:
    friend struct LookupStub;

    LookupStub _stub;
};

#endif // USES_LOOKUP_STUBS

struct DispatchStub;
struct DispatchHolder;

/*DispatchStub**************************************************************************************
Monomorphic and mostly monomorphic call sites eventually point to DispatchStubs.
A dispatch stub has an expected type (expectedMT), target address (target) and fail address (failure).
If the calling frame does in fact have the <this> type be of the expected type, then
control is transfered to the target address, the method implementation.  If not,
then control is transfered to the fail address, a fail stub (see below) where a polymorphic
lookup is done to find the correct address to go to.

implementation note: Order, choice of instructions, and branch directions
should be carefully tuned since it can have an inordinate effect on performance.  Particular
attention needs to be paid to the effects on the BTB and branch prediction, both in the small
and in the large, i.e. it needs to run well in the face of BTB overflow--using static predictions.
Note that since this stub is only used for mostly monomorphic callsites (ones that are not, get patched
to something else), therefore the conditional jump "jne failure" is mostly not taken, and hence it is important
that the branch prediction staticly predict this, which means it must be a forward jump.  The alternative
is to reverse the order of the jumps and make sure that the resulting conditional jump "je implTarget"
is statically predicted as taken, i.e a backward jump. The current choice was taken since it was easier
to control the placement of the stubs than control the placement of the jitted code and the stubs. */

struct DispatchStubData
{
    size_t ExpectedMT;
    PCODE ImplTarget;
    PCODE FailTarget;
};

typedef DPTR(DispatchStubData) PTR_DispatchStubData;

struct DispatchStub
{
    inline PCODE        entryPoint() const { LIMITED_METHOD_CONTRACT;  return (PCODE)((BYTE*)this); }
    inline size_t       expectedMT() const { LIMITED_METHOD_CONTRACT;  return GetData()->ExpectedMT; }
    inline size_t       size()       const { WRAPPER_NO_CONTRACT; return sizeof(DispatchStub); }

#ifndef UNIX_X86_ABI
    inline static size_t offsetOfThisDeref(){ LIMITED_METHOD_CONTRACT; return 0x06; }
#endif

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

    PTR_DispatchStubData GetData() const
    {
        return dac_cast<PTR_DispatchStubData>((uint8_t*)this + 4096);
    }

    BYTE code[24];
};

/* DispatchHolders are the containers for DispatchStubs, they provide for any alignment of
stubs as necessary.  DispatchStubs are placed in a hashtable and in a cache.  The keys for both
are the pair expectedMT and token.  Efficiency of the of the hash table is not a big issue,
since lookups in it are fairly rare.  Efficiency of the cache is paramount since it is accessed frequently
o(see ResolveStub below).  Currently we are storing both of these fields in the DispatchHolder to simplify
alignment issues.  If inlineMT in the stub itself was aligned, then it could be the expectedMT field.
While the token field can be logically gotten by following the failure target to the failEntryPoint
of the ResolveStub and then to the token over there, for perf reasons of cache access, it is duplicated here.
This allows us to use DispatchStubs in the cache.  The alternative is to provide some other immutable struct
for the cache composed of the triplet (expectedMT, token, target) and some sort of reclaimation scheme when
they are thrown out of the cache via overwrites (since concurrency will make the obvious approaches invalid).
*/

/* @workaround for ee resolution - Since the EE does not currently have a resolver function that
does what we want, see notes in implementation of VirtualCallStubManager::Resolver, we are
using dispatch stubs to siumulate what we want.  That means that inlineTarget, which should be immutable
is in fact written.  Hence we have moved target out into the holder and aligned it so we can
atomically update it.  When we get a resolver function that does what we want, we can drop this field,
and live with just the inlineTarget field in the stub itself, since immutability will hold.*/
struct DispatchHolder
{
    static void InitializeStatic();

    void  Initialize(DispatchHolder* pDispatchHolderRX, PCODE implTarget, PCODE failTarget, size_t expectedMT);

    static size_t GetHolderSize()
        { STATIC_CONTRACT_WRAPPER; return sizeof(DispatchStub); }

    DispatchStub* stub()      { LIMITED_METHOD_CONTRACT;  return reinterpret_cast<DispatchStub *>(this); }

    static DispatchHolder*  FromDispatchEntry(PCODE dispatchEntry);

    static size_t GenerateCodePage(uint8_t* pageBase);
};

struct ResolveStub;
struct ResolveHolder;

/*ResolveStub**************************************************************************************
Polymorphic call sites and monomorphic calls that fail end up in a ResolverStub.  There is only
one resolver stub built for any given token, even though there may be many call sites that
use that token and many distinct <this> types that are used in the calling call frames.  A resolver stub
actually has two entry points, one for polymorphic call sites and one for dispatch stubs that fail on their
expectedMT test.  There is a third part of the resolver stub that enters the ee when a decision should
be made about changing the callsite.  Therefore, we have defined the resolver stub as three distinct pieces,
even though they are actually allocated as a single contiguous block of memory.  These pieces are:

A ResolveStub has two entry points:

FailEntry - where the dispatch stub goes if the expected MT test fails.  This piece of the stub does
a check to see how often we are actually failing. If failures are frequent, control transfers to the
patch piece to cause the call site to be changed from a mostly monomorphic callsite
(calls dispatch stub) to a polymorphic callsize (calls resolve stub).  If failures are rare, control
transfers to the resolve piece (see ResolveStub).  The failEntryPoint decrements a counter
every time it is entered.  The ee at various times will add a large chunk to the counter.

ResolveEntry - does a lookup via in a cache by hashing the actual type of the calling frame s
<this> and the token identifying the (contract,method) pair desired.  If found, control is transfered
to the method implementation.  If not found in the cache, the token is pushed and the ee is entered via
the ResolveWorkerStub to do a full lookup and eventual transfer to the correct method implementation.  Since
there is a different resolve stub for every token, the token can be inlined and the token can be pre-hashed.
The effectiveness of this approach is highly sensitive to the effectiveness of the hashing algorithm used,
as well as its speed.  It turns out it is very important to make the hash function sensitive to all
of the bits of the method table, as method tables are laid out in memory in a very non-random way.  Before
making any changes to the code sequences here, it is very important to measure and tune them as perf
can vary greatly, in unexpected ways, with seeming minor changes.

Implementation note - Order, choice of instructions, and branch directions
should be carefully tuned since it can have an inordinate effect on performance.  Particular
attention needs to be paid to the effects on the BTB and branch prediction, both in the small
and in the large, i.e. it needs to run well in the face of BTB overflow--using static predictions.
Note that this stub is called in highly polymorphic cases, but the cache should have been sized
and the hash function chosen to maximize the cache hit case.  Hence the cmp/jcc instructions should
mostly be going down the cache hit route, and it is important that this be statically predicted as so.
Hence the 3 jcc instrs need to be forward jumps.  As structured, there is only one jmp/jcc that typically
gets put in the BTB since all the others typically fall straight thru.  Minimizing potential BTB entries
is important. */

struct ResolveStubData
{
    size_t CacheAddress;
    UINT32 HashedToken;
    UINT32 CacheMask;
    size_t Token;
    INT32  Counter;
    PCODE  ResolveWorkerTarget;
    PCODE  PatcherTarget;
#ifndef UNIX_X86_ABI
    size_t StackArgumentsSize;
#endif
};

typedef DPTR(ResolveStubData) PTR_ResolveStubData;

struct ResolveStub
{
    inline PCODE failEntryPoint()       { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this); }
    inline PCODE resolveEntryPoint()    { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this + 9); }
    inline PCODE slowEntryPoint()       { LIMITED_METHOD_CONTRACT; return (PCODE)((BYTE*)this + 0x41); }

    inline INT32* pCounter()            { LIMITED_METHOD_CONTRACT; return &GetData()->Counter; }// ((BYTE*)this + 4096 + 16); }
    inline UINT32 hashedToken()         { LIMITED_METHOD_CONTRACT; return GetData()->HashedToken >> LOG2_PTRSIZE; } //(*(INT32*)((BYTE*)this + 4096 + 4)) >> LOG2_PTRSIZE;    }
    inline size_t cacheAddress()        { LIMITED_METHOD_CONTRACT; return GetData()->CacheAddress; } // *(size_t*)((BYTE*)this + 4096);   }
    inline size_t token()               { LIMITED_METHOD_CONTRACT; return GetData()->Token; } // *(size_t*)((BYTE*)this + 4096 + 12);          }
    inline size_t size()                { LIMITED_METHOD_CONTRACT; return sizeof(ResolveStub); }

#ifndef UNIX_X86_ABI
    inline static size_t offsetOfThisDeref(){ LIMITED_METHOD_CONTRACT; return 0x01; }
    inline size_t stackArgumentsSize()      { LIMITED_METHOD_CONTRACT; return GetData()->StackArgumentsSize; } //*(size_t*)((BYTE*)this + 4096 + 28); }
#endif

private:

    PTR_ResolveStubData GetData() const
    {
        return dac_cast<PTR_ResolveStubData>((uint8_t*)this + 4096);
    }

    friend struct ResolveHolder;

    BYTE code[88];
};

/* ResolveHolders are the containers for ResolveStubs,  They provide
for any alignment of the stubs as necessary. The stubs are placed in a hash table keyed by
the token for which they are built.  Efficiency of access requires that this token be aligned.
For now, we have copied that field into the ResolveHolder itself, if the resolve stub is arranged such that
any of its inlined tokens (non-prehashed) is aligned, then the token field in the ResolveHolder
is not needed. */
struct ResolveHolder
{
    static void  InitializeStatic();

    void  Initialize(ResolveHolder* pResolveHolderRX,
                     PCODE resolveWorkerTarget, PCODE patcherTarget,
                     size_t dispatchToken, UINT32 hashedToken,
                     void * cacheAddr, INT32 counterValue
#ifndef UNIX_X86_ABI
                     , size_t stackArgumentsSize
#endif
                     );

    ResolveStub* stub()      { LIMITED_METHOD_CONTRACT;  return &_stub; }

    static ResolveHolder*  FromFailEntry(PCODE failEntry);
    static ResolveHolder*  FromResolveEntry(PCODE resolveEntry);

    static size_t GenerateCodePage(uint8_t* pageBase);

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

        size_t cbSize = 2;                                      // First mov instruction
        cbSize += (pStubCode[cbSize + 1] == 0x80 ? 6 : 3);      // Either 8B 80 or 8B 40: mov eax,[eax+offset]
        cbSize += (pStubCode[cbSize + 1] == 0xa0 ? 6 : 3);      // Either FF A0 or FF 60: jmp dword ptr [eax+slot]
        cbSize += 4;                                            // Slot value (data storage, not a real instruction)

        return cbSize;
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
        return 2 + (offsetOfIndirection >= 0x80 ? 6 : 3) + (offsetAfterIndirection >= 0x80 ? 6 : 3) + 4;
    }

    static VTableCallHolder* FromVTableCallEntry(PCODE entry) { LIMITED_METHOD_CONTRACT; return (VTableCallHolder*)entry; }

private:
    // VTableCallStub follows here. It is dynamically sized on allocation because it could
    // use short/long instruction sizes for the mov/jmp, depending on the slot value.
};

#include <poppack.h>


#ifdef DECLARE_DATA

#ifndef DACCESS_COMPILE

#ifdef _MSC_VER

#ifdef CHAIN_LOOKUP
/* This will perform a chained lookup of the entry if the initial cache lookup fails

   Entry stack:
            dispatch token
            siteAddrForRegisterIndirect (used only if this is a RegisterIndirect dispatch call)
            return address of caller to stub
        Also, EAX contains the pointer to the first ResolveCacheElem pointer for the calculated
        bucket in the cache table.
*/
__declspec (naked) void ResolveWorkerChainLookupAsmStub()
{
    enum
    {
        e_token_size                = 4,
        e_indirect_addr_size        = 4,
        e_caller_ret_addr_size      = 4,
    };
    enum
    {
        // this is the part of the stack that is present as we enter this function:
        e_token                     = 0,
        e_indirect_addr             = e_token + e_token_size,
        e_caller_ret_addr           = e_indirect_addr + e_indirect_addr_size,
        e_ret_esp                   = e_caller_ret_addr + e_caller_ret_addr_size,
    };
    enum
    {
        e_spilled_reg_size          = 8,
    };

    // main loop setup
    __asm {
#ifdef STUB_LOGGING
        inc     g_chained_lookup_call_counter
#endif
        // spill regs
        push    edx
        push    ecx
        // move the token into edx
        mov     edx,[esp+e_spilled_reg_size+e_token]
        // move the MT into ecx
        mov     ecx,[ecx]
    }
    main_loop:
    __asm {
        // get the next entry in the chain (don't bother checking the first entry again)
        mov     eax,[eax+e_resolveCacheElem_offset_next]
        // test if we hit a terminating NULL
        test    eax,eax
        jz      fail
        // compare the MT of the ResolveCacheElem
        cmp     ecx,[eax+e_resolveCacheElem_offset_mt]
        jne     main_loop
        // compare the token of the ResolveCacheElem
        cmp     edx,[eax+e_resolveCacheElem_offset_token]
        jne     main_loop
        // success
        // decrement success counter and move entry to start if necessary
        sub     g_dispatch_cache_chain_success_counter,1
        //@TODO: Perhaps this should be a jl for better branch prediction?
        jge     nopromote
        // be quick to reset the counter so we don't get a bunch of contending threads
        add     g_dispatch_cache_chain_success_counter,CALL_STUB_CACHE_INITIAL_SUCCESS_COUNT
        // promote the entry to the beginning of the chain
        mov     ecx,eax
        call    VirtualCallStubManager::PromoteChainEntry
    }
    nopromote:
    __asm {
        // clean up the stack and jump to the target
        pop     ecx
        pop     edx
        add     esp,(e_caller_ret_addr - e_token)
        mov     eax,[eax+e_resolveCacheElem_offset_target]
        jmp     eax
    }
    fail:
    __asm {
#ifdef STUB_LOGGING
        inc     g_chained_lookup_miss_counter
#endif
        // restore registers
        pop     ecx
        pop     edx
        jmp     ResolveWorkerAsmStub
    }
}
#endif

/* Call the resolver, it will return where we are supposed to go.
   There is a little stack magic here, in that we are entered with one
   of the arguments for the resolver (the token) on the stack already.
   We just push the other arguments, <this> in the call frame and the call site pointer,
   and call the resolver.

   On return we have the stack frame restored to the way it was when the ResolveStub
   was called, i.e. as it was at the actual call site.  The return value from
   the resolver is the address we need to transfer control to, simulating a direct
   call from the original call site.  If we get passed back NULL, it means that the
   resolution failed, an unimpelemented method is being called.

   Entry stack:
            dispatch token
            siteAddrForRegisterIndirect (used only if this is a RegisterIndirect dispatch call)
            return address of caller to stub

   Call stack:
            pointer to TransitionBlock
            call site
            dispatch token
            TransitionBlock
                ArgumentRegisters (ecx, edx)
                CalleeSavedRegisters (ebp, ebx, esi, edi)
            return address of caller to stub
   */
__declspec (naked) void ResolveWorkerAsmStub()
{
    CANNOT_HAVE_CONTRACT;

    __asm {
        //
        // The stub arguments are where we want to setup the TransitionBlock. We will
        // setup the TransitionBlock later once we can trash them
        //
        // push ebp-frame
        // push      ebp
        // mov       ebp,esp

        // save CalleeSavedRegisters
        // push      ebx

        push        esi
        push        edi

        // push ArgumentRegisters
        push        ecx
        push        edx

        mov         esi, esp

        push        [esi + 4*4]     // dispatch token
        push        [esi + 5*4]     // siteAddrForRegisterIndirect
        push        esi             // pTransitionBlock

        // Setup up proper EBP frame now that the stub arguments can be trashed
        mov         [esi + 4*4],ebx
        mov         [esi + 5*4],ebp
        lea         ebp, [esi + 5*4]

        // Make the call
        call        VSD_ResolveWorker

        // From here on, mustn't trash eax

        // pop ArgumentRegisters
        pop     edx
        pop     ecx

        // pop CalleeSavedRegisters
        pop edi
        pop esi
        pop ebx
        pop ebp

        // Now jump to the target
        jmp     eax             // continue on into the method
    }
}


/* Call the callsite back patcher.  The fail stub piece of the resolver is being
call too often, i.e. dispatch stubs are failing the expect MT test too often.
In this stub wraps the call to the BackPatchWorker to take care of any stack magic
needed.
*/
__declspec (naked) void BackPatchWorkerAsmStub()
{
    CANNOT_HAVE_CONTRACT;

    __asm {
        push EBP
        mov ebp,esp
        push EAX        // it may contain siteAddrForRegisterIndirect
        push ECX
        push EDX
        push EAX        //  push any indirect call address as the second arg to BackPatchWorker
        push [EBP+8]    //  and push return address as the first arg to BackPatchWorker
        call VirtualCallStubManager::BackPatchWorkerStatic
        pop EDX
        pop ECX
        pop EAX
        mov esp,ebp
        pop ebp
        ret
    }
}

#endif // _MSC_VER

#ifdef _DEBUG
//
// This function verifies that a pointer to an indirection cell lives inside a delegate object.
// In the delegate case the indirection cell is held by the delegate itself in _methodPtrAux, when the delegate Invoke is
// called the shuffle thunk is first invoked and that will call into the virtual dispatch stub.
// Before control is given to the virtual dispatch stub a pointer to the indirection cell (thus an interior pointer to the delegate)
// is pushed in EAX
//
BOOL isDelegateCall(BYTE *interiorPtr)
{
    LIMITED_METHOD_CONTRACT;

    if (GCHeapUtilities::GetGCHeap()->IsHeapPointer((void*)interiorPtr))
    {
        Object *delegate = (Object*)(interiorPtr - DelegateObject::GetOffsetOfMethodPtrAux());
        VALIDATEOBJECTREF(ObjectToOBJECTREF(delegate));
        _ASSERTE(delegate->GetMethodTable()->IsDelegate());

        return TRUE;
    }
    return FALSE;
}
#endif

StubCallSite::StubCallSite(TADDR siteAddrForRegisterIndirect, PCODE returnAddr)
{
    LIMITED_METHOD_CONTRACT;

    // Not used
    // if (isCallRelative(returnAddr))
    // {
    //      m_siteAddr = returnAddr - sizeof(DISPL);
    // }
    // else
    if (isCallRelativeIndirect((BYTE *)returnAddr))
    {
        m_siteAddr = *dac_cast<PTR_PTR_PCODE>(returnAddr - sizeof(PCODE));
    }
    else
    {
        _ASSERTE(isCallRegisterIndirect((BYTE *)returnAddr) || isDelegateCall((BYTE *)siteAddrForRegisterIndirect));
        m_siteAddr = dac_cast<PTR_PCODE>(siteAddrForRegisterIndirect);
    }
}

// the special return address for VSD tailcalls
extern "C" void STDCALL JIT_TailCallReturnFromVSD();

PCODE StubCallSite::GetCallerAddress()
{
    LIMITED_METHOD_CONTRACT;

#ifdef UNIX_X86_ABI
    return m_returnAddr;
#else // UNIX_X86_ABI
    if (m_returnAddr != (PCODE)JIT_TailCallReturnFromVSD)
        return m_returnAddr;

    // Find the tailcallframe in the frame chain and get the actual caller from the first TailCallFrame
    return TailCallFrame::FindTailCallFrame(GetThread()->GetFrame())->GetCallerAddress();
#endif // UNIX_X86_ABI
}

#ifdef STUB_LOGGING
extern size_t g_lookup_inline_counter;
extern size_t g_mono_call_counter;
extern size_t g_mono_miss_counter;
extern size_t g_poly_call_counter;
extern size_t g_poly_miss_counter;
#endif

/* Template used to generate the stub.  We generate a stub by allocating a block of
   memory and copy the template over it and just update the specific fields that need
   to be changed.
*/
// LookupStub lookupInit;

void LookupHolder::InitializeStatic()
{
}

void  LookupHolder::Initialize(LookupHolder* pLookupHolderRX, PCODE resolveWorkerTarget, size_t dispatchToken)
{
    LookupStubData *pData = stub()->GetData();
    pData->DispatchToken = dispatchToken;
    pData->ResolveWorkerTarget = resolveWorkerTarget;
}

LookupHolder* LookupHolder::FromLookupEntry(PCODE lookupEntry)
{
    LIMITED_METHOD_CONTRACT;
    LookupHolder* lookupHolder = (LookupHolder*)lookupEntry;
    //    _ASSERTE(lookupHolder->_stub._entryPoint[0] == lookupInit._entryPoint[0]);
    return lookupHolder;
}

#ifndef DACCESS_COMPILE
size_t LookupHolder::GenerateCodePage(uint8_t* pageBaseRX)
{
    /*
    0:  50                      push   eax
    1:  ff 35 f9 0f 00 00       push   DWORD PTR ds:0xff9
    7:  ff 25 f7 0f 00 00       jmp    DWORD PTR ds:0xff7    
    */
    ExecutableWriterHolder<uint8_t> codePageWriterHolder(pageBaseRX, 4096);
    uint8_t* pageBase = codePageWriterHolder.GetRW();

    for (int i = 0; i < 4096; i += 16)
    {
        pageBase[i + 0] = 0x50;
        pageBase[i + 1] = 0xff;
        pageBase[i + 2] = 0x35;
        uint8_t* pDispatchTokenSlot = pageBase + i + 4096 + offsetof(LookupStubData, DispatchToken);
        *(uint8_t**)(pageBase + i + 3) = pDispatchTokenSlot;

        pageBase[i + 7] = 0xFF;
        pageBase[i + 8] = 0x25;
        uint8_t* pResolveWorkerTargetSlot = pageBase + i + 4096 + offsetof(LookupStubData, ResolveWorkerTarget);
        *(uint8_t**)(pageBase + i + 9) = pResolveWorkerTargetSlot;

        pageBase[i + 13] = 0x90;
        pageBase[i + 14] = 0x90;
        pageBase[i + 15] = 0x90;
    }

    ClrFlushInstructionCache(pageBaseRX, 4096);

    return 0;
}

size_t DispatchHolder::GenerateCodePage(uint8_t* pageBaseRX)
{
    /*
        0:  50                      push   eax
        1:  a1 00 10 00 00          mov    eax,ds:0x1000
        6:  39 01                   cmp    DWORD PTR [ecx],eax
        8:  58                      pop    eax
        9:  75 06                   jne    11 <f>
        b:  ff 25 00 10 00 00       jmp    DWORD PTR ds:0x1000
        00000011 <f>:
        11: ff 25 00 10 00 00       jmp    DWORD PTR ds:0x1000
    */
    ExecutableWriterHolder<uint8_t> codePageWriterHolder(pageBaseRX, 4096);
    uint8_t* pageBase = codePageWriterHolder.GetRW();

    for (int i = 0; i <= 4096 - 24; i += 24)
    {
        pageBase[i + 0] = 0x50;
        pageBase[i + 1] = 0xa1;

        uint8_t* pExpectedMTSlot = pageBase + i + 4096 + offsetof(DispatchStubData, ExpectedMT);
        *(uint8_t**)(pageBase + i + 2) = pExpectedMTSlot;

        pageBase[i + 6] = 0x39;
        pageBase[i + 7] = 0x01;
        pageBase[i + 8] = 0x58;
        pageBase[i + 9] = 0x75;
        pageBase[i + 10] = 0x06;
        pageBase[i + 11] = 0xff;
        pageBase[i + 12] = 0x25;

        uint8_t* pImplTargetSlot = pageBase + i + 4096 + offsetof(DispatchStubData, ImplTarget);
        *(uint8_t**)(pageBase + i + 13) = pImplTargetSlot;

        pageBase[i + 17] = 0xff;
        pageBase[i + 18] = 0x25;

        uint8_t* pFailTargetSlot = pageBase + i + 4096 + offsetof(DispatchStubData, FailTarget);
        *(uint8_t**)(pageBase + i + 19) = pFailTargetSlot;

        pageBase[i + 23] = 0x90;
    }

    for (int i = 0; i < 16; i++)
    {
        pageBase[4080 + i] = 0xcc;
    }

    ClrFlushInstructionCache(pageBaseRX, 4096);
    return 0;
}

size_t ResolveHolder::GenerateCodePage(uint8_t* pageBaseRX)
{
/*
 BYTE    _resolveEntryPoint;     // 50           push    eax             ;save siteAddrForRegisterIndirect - this may be an indirect call
    BYTE    part1 [11];             // 8b 01        mov     eax,[ecx]       ;get the method table from the "this" pointer. This is the place
                                    //                                      ;    where we are going to fault on null this. If you change it,
                                    //                                      ;    change also AdjustContextForVirtualStub in excep.cpp!!!
                                    // 52           push    edx
                                    // 8b d0        mov     edx, eax
                                    // c1 e8 0C     shr     eax,12          ;we are adding upper bits into lower bits of mt
                                    // 03 c2        add     eax,edx
                                    // 35           xor     eax,
    UINT32  _hashedToken;           // xx xx xx xx              hashedToken ;along with pre-hashed token
    BYTE    part2 [1];              // 25           and     eax,
    size_t mask;                    // xx xx xx xx              cache_mask
    BYTE part3 [2];                 // 8b 80        mov     eax, [eax+
    size_t  _cacheAddress;          // xx xx xx xx                lookupCache]
#ifdef STUB_LOGGING
    BYTE cntr1[2];                  // ff 05        inc
    size_t* c_call;                 // xx xx xx xx          [call_cache_counter]
#endif //STUB_LOGGING
    BYTE part4 [2];                 // 3b 10        cmp     edx,[eax+
    // BYTE mtOffset;               //                          ResolverCacheElem.pMT]
    BYTE part5 [1];                 // 75           jne
    BYTE toMiss1;                   // xx                   miss            ;must be forward jump, for perf reasons
    BYTE part6 [2];                 // 81 78        cmp     [eax+
    BYTE tokenOffset;               // xx                        ResolverCacheElem.token],
    size_t  _token;                 // xx xx xx xx              token
    BYTE part7 [1];                 // 75           jne
    BYTE toMiss2;                   // xx                   miss            ;must be forward jump, for perf reasons
    BYTE part8 [2];                 // 8B 40 xx     mov     eax,[eax+
    BYTE targetOffset;              //                          ResolverCacheElem.target]
    BYTE part9 [6];                 // 5a           pop     edx
                                    // 83 c4 04     add     esp,4           ;throw away siteAddrForRegisterIndirect - we don't need it now
                                    // ff e0        jmp     eax
                                    //         miss:
    BYTE    miss [1];               // 5a           pop     edx             ; don't pop siteAddrForRegisterIndirect - leave it on the stack for use by ResolveWorkerChainLookupAsmStub and/or ResolveWorkerAsmStub
    BYTE    _slowEntryPoint[1];     // 68           push
    size_t  _tokenPush;             // xx xx xx xx          token
#ifdef STUB_LOGGING
    BYTE cntr2[2];                  // ff 05        inc
    size_t* c_miss;                 // xx xx xx xx          [miss_cache_counter]
#endif //STUB_LOGGING
    BYTE part10 [1];                // e9           jmp
    DISPL   _resolveWorkerDispl;    // xx xx xx xx          resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
    BYTE  patch[1];                 // e8           call
    DISPL _backpatcherDispl;        // xx xx xx xx          backpatcherWorker  == BackPatchWorkerAsmStub
    BYTE  part11 [1];               // eb           jmp
    BYTE toResolveStub;             // xx                   resolveStub, i.e. go back to _resolveEntryPoint
#ifndef UNIX_X86_ABI
    size_t _stackArgumentsSize;     // xx xx xx xx
#endif

    *(void**)((BYTE*)this + 4096) = cacheAddr;
    *(UINT32*)((BYTE*)this + 4096 + 4) = hashedToken << LOG2_PTRSIZE;
    *(UINT32*)((BYTE*)this + 4096 + 8) = CALL_STUB_CACHE_MASK * sizeof(void*); // TODO: Move this to code
    *(size_t*)((BYTE*)this + 4096 + 12) = dispatchToken;
    *(INT32**)((BYTE*)this + 4096 + 16) = counter
    *(size_t*)((BYTE*)this + 4096 + 20) = (size_t)resolveWorkerTarget;
    *(size_t*)((BYTE*)this + 4096 + 24) = (size_t)patcherTarget;
#ifndef UNIX_X86_ABI
    *(size_t*)((BYTE*)this + 4096 + 28) = stackArgumentsSize;
#endif

fail:
0:  83 2d 09 10 00 00 01    sub    DWORD PTR ds:0x1009,0x1
7:  7c 44                   jl     <backpatcher>
resolveStub:
9:  50                      push   eax
a:  8b 01                   mov    eax,DWORD PTR [ecx]
c:  52                      push   edx
d:  89 c2                   mov    edx,eax
f:  c1 e8 0c                shr    eax,0xc <<< CALL_STUB_CACHE_NUM_BITS
12: 01 d0                   add    eax,edx
14: 33 05 ea 0f 00 00       xor    eax,DWORD PTR ds:0xfea; 0x1004 <<< hashedToken
// CALL_STUB_CACHE_MASK * sizeof(void*); 0:  25 78 56 34 12          and    eax,0x12345678
1a: 23 05 e8 0f 00 00       and    eax,DWORD PTR ds:0xfe8; 0x1008 <<< cacheMask
20: 03 05 da 0f 00 00       add    eax,DWORD PTR ds:0xfda; 0x1000 <<< lookupCache
26: 8b 00                   mov    eax,DWORD PTR [eax]
28: 3b 10                   cmp    edx,DWORD PTR [eax]; ResolverCacheElem.pMT
2a: 75 14                   jne    40 <miss>
2c: 8b 15 da 0f 00 00       mov    edx,DWORD PTR ds:0xfda; 0x100c <<< token
32: 3b 50 04                cmp    edx,DWORD PTR [eax+0x4]; ResolverCacheElem.token
35: 75 09                   jne    40 <miss>    
37: 8b 40 08                mov    eax,DWORD PTR [eax+0x8]; ResolverCacheElem.target
3a: 5a                      pop    edx
3b: 83 c4 04                add    esp,0x4
3e: ff e0                   jmp    eax; We can use jmp DWORD PTR [eax+0x8] and remove the mov eax, DWORD PTR [eax+0x8] above
<miss>:
40: 5a                      pop    edx
<slow>:
41: ff 35 c5 0f 00 00       push   DWORD PTR ds:0x0fc5; 0x100c <<< token
47: ff 25 c9 0f 00 00       jmp    DWORD PTR ds:0x0fc9; 0x1014 <<< resolveWorker == ResolveWorkerChainLookupAsmStub or ResolveWorkerAsmStub
<backpatcher>:
4d: ff 15 c5 0f 00 00       call   DWORD PTR ds:0x0fc5; 0x1018 <<< backpatcherWorker == BackPatchWorkerAsmStub
53: eb b4                   jmp    9
*/    
    ExecutableWriterHolder<uint8_t> codePageWriterHolder(pageBaseRX, 4096);
    uint8_t* pageBase = codePageWriterHolder.GetRW();

    for (int i = 0; i <= 4096 - 88; i += 88)
    {
        pageBase[i + 0] = 0x83;
        pageBase[i + 1] = 0x2d; 

        uint8_t* pCounterSlot = pageBase + i + 4096 + offsetof(ResolveStubData, Counter);
        *(uint8_t**)(pageBase + i + 2) = pCounterSlot;

        pageBase[i + 6] = 0x01;
        pageBase[i + 7] = 0x7c;
        pageBase[i + 8] = 0x44;
        pageBase[i + 9] = 0x50;
        pageBase[i + 10] = 0x8b;
        pageBase[i + 11] = 0x01;
        pageBase[i + 12] = 0x52;
        pageBase[i + 13] = 0x89;
        pageBase[i + 14] = 0xc2;
        pageBase[i + 15] = 0xc1;
        pageBase[i + 16] = 0xe8;
        pageBase[i + 17] = CALL_STUB_CACHE_NUM_BITS;
        pageBase[i + 18] = 0x01;
        pageBase[i + 19] = 0xd0;
        pageBase[i + 20] = 0x33;
        pageBase[i + 21] = 0x05;

        uint8_t* pHashedTokenSlot = pageBase + i + 4096 + offsetof(ResolveStubData, HashedToken);
        *(uint8_t**)(pageBase + i + 22) = pHashedTokenSlot;

        pageBase[i + 26] = 0x23;
        pageBase[i + 27] = 0x05;

        uint8_t* pCacheMaskSlot = pageBase + i + 4096 + offsetof(ResolveStubData, CacheMask);
        *(uint8_t**)(pageBase + i + 28) = pCacheMaskSlot;

        pageBase[i + 32] = 0x03;
        pageBase[i + 33] = 0x05;

        uint8_t* pLookupCacheSlot = pageBase + i + 4096 + offsetof(ResolveStubData, CacheAddress);
        *(uint8_t**)(pageBase + i + 34) = pLookupCacheSlot;

        pageBase[i + 38] = 0x8b;
        pageBase[i + 39] = 0x00;
        static_assert_no_msg(offsetof(ResolveCacheElem, pMT) == 0);
        pageBase[i + 40] = 0x3b;
        pageBase[i + 41] = 0x10;
        pageBase[i + 42] = 0x75;
        pageBase[i + 43] = 0x14;
        pageBase[i + 44] = 0x8b;
        pageBase[i + 45] = 0x15;

        uint8_t* pTokenSlot = pageBase + i + 4096 + offsetof(ResolveStubData, Token);
        *(uint8_t**)(pageBase + i + 46) = pTokenSlot;

        pageBase[i + 50] = 0x3b;
        pageBase[i + 51] = 0x50;
        pageBase[i + 52] = offsetof(ResolveCacheElem, token);
        pageBase[i + 53] = 0x75;
        pageBase[i + 54] = 0x09;
        pageBase[i + 55] = 0x8b;
        pageBase[i + 56] = 0x40;
        pageBase[i + 57] = offsetof(ResolveCacheElem, target);
        pageBase[i + 58] = 0x5a;
        pageBase[i + 59] = 0x83;
        pageBase[i + 60] = 0xc4;
        pageBase[i + 61] = 0x04;
        pageBase[i + 62] = 0xff;
        pageBase[i + 63] = 0xe0;
        pageBase[i + 64] = 0x5a;

        pageBase[i + 65] = 0xff;
        pageBase[i + 66] = 0x35;

        *(uint8_t**)(pageBase + i + 67) = pTokenSlot;

        pageBase[i + 71] = 0xff;
        pageBase[i + 72] = 0x25;

        uint8_t* pResolveWorkerSlot = pageBase + i + 4096 + offsetof(ResolveStubData, ResolveWorkerTarget);
        *(uint8_t**)(pageBase + i + 73) = pResolveWorkerSlot;

        pageBase[i + 77] = 0xff;
        pageBase[i + 78] = 0x15;

        uint8_t* pBackpatcherSlot = pageBase + i + 4096 + offsetof(ResolveStubData, PatcherTarget);
        *(uint8_t**)(pageBase + i + 79) = pBackpatcherSlot;

        pageBase[i + 83] = 0xeb;
        pageBase[i + 84] = 0xb4;
        pageBase[i + 85] = 0x90;
        pageBase[i + 86] = 0x90;
        pageBase[i + 87] = 0x90;
    }

    for (int i = 0; i < 48; i++)
    {
        pageBase[4048 + i] = 0xcc;
    }

    // TODO: do we correctly support crossing the page boundary in the allocator?

    ClrFlushInstructionCache(pageBaseRX, 4096);
    return 0;
}

#endif

/* Template used to generate the stub.  We generate a stub by allocating a block of
   memory and copy the template over it and just update the specific fields that need
   to be changed.
*/
//DispatchStub dispatchInit;

void DispatchHolder::InitializeStatic()
{
};

void  DispatchHolder::Initialize(DispatchHolder* pDispatchHolderRX, PCODE implTarget, PCODE failTarget, size_t expectedMT)
{
    DispatchStubData *pData = stub()->GetData();
    pData->ExpectedMT = expectedMT;
    pData->ImplTarget = implTarget;
    pData->FailTarget = failTarget;
}

DispatchHolder* DispatchHolder::FromDispatchEntry(PCODE dispatchEntry)
{
    LIMITED_METHOD_CONTRACT;
    DispatchHolder* dispatchHolder = (DispatchHolder*)dispatchEntry;
    //    _ASSERTE(dispatchHolder->_stub._entryPoint[0] == dispatchInit._entryPoint[0]);
    return dispatchHolder;
}


/* Template used to generate the stub.  We generate a stub by allocating a block of
   memory and copy the template over it and just update the specific fields that need
   to be changed.
*/

//ResolveStub resolveInit;

void ResolveHolder::InitializeStatic()
{
};

void  ResolveHolder::Initialize(ResolveHolder* pResolveHolderRX, 
                                PCODE resolveWorkerTarget, PCODE patcherTarget,
                                size_t dispatchToken, UINT32 hashedToken,
                                void * cacheAddr, INT32 counterValue
#ifndef UNIX_X86_ABI
                                , size_t stackArgumentsSize
#endif
                                )
{
    ResolveStubData *pData = stub()->GetData();

    pData->CacheAddress = (size_t)cacheAddr;
    pData->HashedToken = hashedToken << LOG2_PTRSIZE;
    pData->CacheMask = CALL_STUB_CACHE_MASK * sizeof(void*);
    pData->Token = dispatchToken;
    pData->Counter = counterValue;
    pData->ResolveWorkerTarget = resolveWorkerTarget;
    pData->PatcherTarget = patcherTarget;
#ifndef UNIX_X86_ABI
    pData->StackArgumentsSize = stackArgumentsSize;
#endif
}

ResolveHolder* ResolveHolder::FromFailEntry(PCODE failEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*)failEntry;
    return resolveHolder;
}

ResolveHolder* ResolveHolder::FromResolveEntry(PCODE resolveEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( resolveEntry - 9 );
    return resolveHolder;
}

void VTableCallHolder::Initialize(unsigned slot)
{
    unsigned offsetOfIndirection = MethodTable::GetVtableOffset() + MethodTable::GetIndexOfVtableIndirection(slot) * TARGET_POINTER_SIZE;
    unsigned offsetAfterIndirection = MethodTable::GetIndexAfterVtableIndirection(slot) * TARGET_POINTER_SIZE;

    VTableCallStub* pStub = stub();
    BYTE* p = (BYTE*)pStub->entryPoint();

    // mov eax,[ecx] : eax = MethodTable pointer
    *(UINT16*)p = 0x018b; p += 2;

    // mov eax,[eax+vtable offset] : eax = vtable pointer
    if (offsetOfIndirection >= 0x80)
    {
        *(UINT16*)p = 0x808b; p += 2;
        *(UINT32*)p = offsetOfIndirection; p += 4;
    }
    else
    {
        *(UINT16*)p = 0x408b; p += 2;
        *p++ = (BYTE)offsetOfIndirection;
    }

    // jmp dword ptr [eax+slot]
    if (offsetAfterIndirection >= 0x80)
    {
        *(UINT16*)p = 0xa0ff; p += 2;
        *(UINT32*)p = offsetAfterIndirection; p += 4;
    }
    else
    {
        *(UINT16*)p = 0x60ff; p += 2;
        *p++ = (BYTE)offsetAfterIndirection;
    }

    // Store the slot value here for convenience. Not a real instruction (unreachable anyways)
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

    EX_TRY
    {
        // If stubStartAddress is completely bogus, then this might AV,
        // so we protect it with SEH. An AV here is OK.
        AVInRuntimeImplOkayHolder AVOkay;

        WORD firstWord = *((WORD*) stubStartAddress);

#ifndef STUB_LOGGING
        if (firstWord == 0x50a1)
#else //STUB_LOGGING
#error
        if (firstWord == 0x05ff)
#endif
        {
            stubKind = SK_DISPATCH;
        }
        else if (firstWord == 0xFF50)
        {
            stubKind = SK_LOOKUP;
        }
        else if (firstWord == 0x2d83)
        {
            stubKind = SK_RESOLVE;
        }
        else if (firstWord == 0x018b)
        {
            stubKind = SK_VTABLECALL;
        }
        else
        {
            BYTE firstByte  = ((BYTE*) stubStartAddress)[0];
            BYTE secondByte = ((BYTE*) stubStartAddress)[1];

            if ((firstByte  == X86_INSTR_INT3) ||
                (secondByte == X86_INSTR_INT3))
            {
                stubKind = SK_BREAKPOINT;
            }
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

#endif // _VIRTUAL_CALL_STUB_X86_H
