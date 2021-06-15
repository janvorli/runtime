// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "pedecoder.h"
#include "executableallocator.h"

BYTE * DoubleMappedAllocator::s_CodeMinAddr;        // Preferred region to allocate the code in.
BYTE * DoubleMappedAllocator::s_CodeMaxAddr;
BYTE * DoubleMappedAllocator::s_CodeAllocStart;
BYTE * DoubleMappedAllocator::s_CodeAllocHint;      // Next address to try to allocate for code in the preferred region.

volatile DoubleMappedAllocator* DoubleMappedAllocator::g_instance = NULL;
LONG DoubleMappedAllocator::g_rwMaps = 0;
LONG DoubleMappedAllocator::g_reusedRwMaps = 0;
LONG DoubleMappedAllocator::g_rwUnmaps = 0;
LONG DoubleMappedAllocator::g_failedRwUnmaps = 0;
LONG DoubleMappedAllocator::g_failedRwMaps = 0;
LONG DoubleMappedAllocator::g_reserveCalls = 0;
LONG DoubleMappedAllocator::g_reserveAtCalls = 0;
LONG DoubleMappedAllocator::g_mapReusePossibility = 0;
LONG DoubleMappedAllocator::g_maxRWMappingCount = 0;
LONG DoubleMappedAllocator::g_RWMappingCount = 0;
LONG DoubleMappedAllocator::g_maxReusedRwMapsRefcount = 0;
LONG DoubleMappedAllocator::g_maxRXSearchLength = 0;
LONG DoubleMappedAllocator::g_rxSearchLengthSum = 0;
LONG DoubleMappedAllocator::g_rxSearchLengthCount = 0;

void FillSymbolInfo
(
SYM_INFO *psi,
DWORD_PTR dwAddr
);

void MagicInit();

void DoubleMappedAllocator::ReportState()
{
// #ifdef ENABLE_DOUBLE_MAPPING
// #ifndef CROSSGEN_COMPILE
// #ifndef DACCESS_COMPILE
// #ifdef TARGET_WINDOWS
//     printf("Reserve calls: %zd\n", g_reserveCalls);
//     printf("Reserve-at calls: %zd\n", g_reserveAtCalls);
//     printf("RW Maps: %zd\n", g_rwMaps);
//     printf("Reused RW Maps: %zd\n", g_reusedRwMaps);
//     printf("Max reused RW Maps refcount: %zd\n", g_maxReusedRwMapsRefcount);
//     printf("RW Unmaps: %zd\n", g_rwUnmaps);
//     printf("Failed RW Maps: %zd\n", g_failedRwMaps);
//     printf("Failed RW Unmaps: %zd\n", g_failedRwUnmaps);
//     printf("Map reuse possibility: %zd\n", g_mapReusePossibility);
//     printf("RW mappings count: %zd\n", g_RWMappingCount);
//     printf("Max RW mappings count: %zd\n", g_maxRWMappingCount);
//     printf("Max RX search length: %zd\n", g_maxRXSearchLength);
//     printf("Average RX search length: %zd\n", g_rxSearchLengthSum / g_rxSearchLengthCount);

//     printf("\n");
//     printf("MapRW users:\n");

//     MagicInit();
//     for (UsersListEntry* user = m_mapUsers; user != NULL; user = user->next)
//     {
//         SYM_INFO psi;
//         FillSymbolInfo(&psi, (DWORD_PTR)user->user);
//         printf(" %p - count %zd reuseCount %zd  %s\n", user->user, user->count, user->reuseCount, psi.achSymbol);
//     }
// #endif
// #endif
// #endif
// #endif
}

//
// Use this function to initialize the s_CodeAllocHint
// during startup. base is runtime .dll base address,
// size is runtime .dll virtual size.
//
void DoubleMappedAllocator::InitCodeAllocHint(size_t base, size_t size, int randomPageOffset)
{
#if USE_UPPER_ADDRESS

#ifdef _DEBUG
    // If GetForceRelocs is enabled we don't constrain the pMinAddr
    if (PEDecoder::GetForceRelocs())
        return;
#endif

//
    // If we are using the UPPER_ADDRESS space (on Win64)
    // then for any code heap that doesn't specify an address
    // range using [pMinAddr..pMaxAddr] we place it in the
    // upper address space
    // This enables us to avoid having to use long JumpStubs
    // to reach the code for our ngen-ed images.
    // Which are also placed in the UPPER_ADDRESS space.
    //
    SIZE_T reach = 0x7FFF0000u;

    // We will choose the preferred code region based on the address of clr.dll. The JIT helpers
    // in clr.dll are the most heavily called functions.
    s_CodeMinAddr = (base + size > reach) ? (BYTE *)(base + size - reach) : (BYTE *)0;
    s_CodeMaxAddr = (base + reach > base) ? (BYTE *)(base + reach) : (BYTE *)-1;

    BYTE * pStart;

    if (s_CodeMinAddr <= (BYTE *)CODEHEAP_START_ADDRESS &&
        (BYTE *)CODEHEAP_START_ADDRESS < s_CodeMaxAddr)
    {
        // clr.dll got loaded at its preferred base address? (OS without ASLR - pre-Vista)
        // Use the code head start address that does not cause collisions with NGen images.
        // This logic is coupled with scripts that we use to assign base addresses.
        pStart = (BYTE *)CODEHEAP_START_ADDRESS;
    }
    else
    if (base > UINT32_MAX)
    {
        // clr.dll got address assigned by ASLR?
        // Try to occupy the space as far as possible to minimize collisions with other ASLR assigned
        // addresses. Do not start at s_CodeMinAddr exactly so that we can also reach common native images
        // that can be placed at higher addresses than clr.dll.
        pStart = s_CodeMinAddr + (s_CodeMaxAddr - s_CodeMinAddr) / 8;
    }
    else
    {
        // clr.dll missed the base address?
        // Try to occupy the space right after it.
        pStart = (BYTE *)(base + size);
    }

    // Randomize the address space
    pStart += GetOsPageSize() * randomPageOffset;

    s_CodeAllocStart = pStart;
    s_CodeAllocHint = pStart;
#endif
}

//
// Use this function to reset the s_CodeAllocHint
// after unloading an AppDomain
//
void DoubleMappedAllocator::ResetCodeAllocHint()
{
    LIMITED_METHOD_CONTRACT;
#if USE_UPPER_ADDRESS
    s_CodeAllocHint = s_CodeAllocStart;
#endif
}

//
// Returns TRUE if p is located in near clr.dll that allows us
// to use rel32 IP-relative addressing modes.
//
bool DoubleMappedAllocator::IsPreferredExecutableRange(void * p)
{
    LIMITED_METHOD_CONTRACT;
#if USE_UPPER_ADDRESS
    if (s_CodeMinAddr <= (BYTE *)p && (BYTE *)p < s_CodeMaxAddr)
        return TRUE;
#endif
    return FALSE;
}

DoubleMappedAllocator* DoubleMappedAllocator::Instance()
{
    if (g_instance == NULL)
    {
        DoubleMappedAllocator *instance = new (nothrow) DoubleMappedAllocator();
        instance->Initialize();

        if (InterlockedCompareExchangeT(const_cast<DoubleMappedAllocator**>(&g_instance), instance, NULL) != NULL)
        {
            delete instance;
        }
    }

    return const_cast<DoubleMappedAllocator*>(g_instance);    
}

DoubleMappedAllocator::~DoubleMappedAllocator()
{
    VMToOSInterface::DestroyDoubleMemoryMapper(m_doubleMemoryMapperHandle);
}

bool DoubleMappedAllocator::Initialize()
{
    m_doubleMemoryMapperHandle = VMToOSInterface::CreateDoubleMemoryMapper();

    m_CriticalSection = ClrCreateCriticalSection(CrstListLock,CrstFlags(CRST_UNSAFE_ANYMODE | CRST_DEBUGGER_THREAD));

    return m_doubleMemoryMapperHandle != NULL;    
}

void DoubleMappedAllocator::UpdateCachedMapping(BlockRW *b)
{
    /*
    if (m_cachedMapping == NULL)
    {
        m_cachedMapping = b;
        b->refCount++;
    }
    else if (m_cachedMapping != b)
    {
        void* unmapAddress = NULL;
        if (!RemoveRWBlock(m_cachedMapping->baseRW, &unmapAddress))
        {
            __debugbreak();
        }
        if (unmapAddress && !UnmapViewOfFile(unmapAddress))
        {
            InterlockedIncrement(&g_failedRwUnmaps);
            __debugbreak();
        }
        m_cachedMapping = b;
        b->refCount++;
    }
    */
}

void* DoubleMappedAllocator::FindRWBlock(void* baseRX, size_t size)
{
    for (BlockRW* b = m_pFirstBlockRW; b != NULL; b = b->next)
    {
        if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
        {
            b->refCount++;
            if ((LONG)b->refCount > g_maxReusedRwMapsRefcount)
            {
                g_maxReusedRwMapsRefcount = (LONG)b->refCount;
            }
            UpdateCachedMapping(b);

            return (BYTE*)b->baseRW + ((size_t)baseRX - (size_t)b->baseRX);
        }
    }

    return NULL;
}

bool DoubleMappedAllocator::AddRWBlock(void* baseRW, void* baseRX, size_t size)
{
    for (BlockRW* b = m_pFirstBlockRW; b != NULL; b = b->next)
    {
        if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
        {
            InterlockedIncrement(&g_mapReusePossibility);
            break;
        }
    }

    BlockRW* pBlockRW = (BlockRW*)malloc(sizeof(BlockRW));// new (nothrow) BlockRW();
    if (pBlockRW == NULL)
    {
        return false;
    }

    pBlockRW->baseRW = baseRW;
    pBlockRW->baseRX = baseRX;
    pBlockRW->size = size;
    pBlockRW->next = m_pFirstBlockRW;
    pBlockRW->refCount = 1;
    m_pFirstBlockRW = pBlockRW;

    UpdateCachedMapping(pBlockRW);

    InterlockedIncrement(&g_RWMappingCount);

    return true;
}

bool DoubleMappedAllocator::RemoveRWBlock(void* pRW, void** pUnmapAddress, size_t* pUnmapSize)
{
    BlockRW* pPrevBlockRW = NULL;
    for (BlockRW* pBlockRW = m_pFirstBlockRW; pBlockRW != NULL; pBlockRW = pBlockRW->next)
    {
        if (pBlockRW->baseRW <= pRW && (size_t)pRW < ((size_t)pBlockRW->baseRW + pBlockRW->size))
        {
            // found
            pBlockRW->refCount--;
            if (pBlockRW->refCount != 0)
            {
                *pUnmapAddress = NULL;
                return true;
            }

            if (pPrevBlockRW == NULL)
            {
                m_pFirstBlockRW = pBlockRW->next;
            }
            else
            {
                pPrevBlockRW->next = pBlockRW->next;
            }

            InterlockedDecrement(&g_RWMappingCount);
            if (g_RWMappingCount > g_maxRWMappingCount)
            {
                g_maxRWMappingCount = g_RWMappingCount;
            }

            *pUnmapAddress = pBlockRW->baseRW;
            *pUnmapSize = pBlockRW->size;

            //delete pBlockRW;
            free(pBlockRW);
            return true;
        }

        pPrevBlockRW = pBlockRW;
    }

    return false;
}

void* DoubleMappedAllocator::Commit(void* pStart, size_t size, bool isExecutable)
{
    return VMToOSInterface::CommitDoubleMappedMemory(pStart, size, isExecutable);
}

//#define DISABLE_UNMAPS

void DoubleMappedAllocator::Release(void* pRX)
{
    BlockRX* b = NULL;

    {
        CRITSEC_Holder csh(m_CriticalSection);
        BlockRX* pPrevBlockRX = NULL;
        for (b = m_pFirstBlockRX; b != NULL; b = b->next)
        {
            if (pRX == b->baseRX)
            {
                if (pPrevBlockRX == NULL)
                {
                    m_pFirstBlockRX = b->next;
                }
                else
                {
                    pPrevBlockRX->next = b->next;
                }

                break;
            }
            pPrevBlockRX = b;
        }
#ifdef DISABLE_UNMAPS
        // This is likely not correct, but disable unmaps is experimental only, so this prevents reusing mappings that were released
        if (b != NULL)
        {
            BlockRW* pPrevBlockRW = NULL;
            BlockRW* mb = NULL;
            BlockRW* mbNext = NULL;
            for (mb = m_pFirstBlockRW; mb != NULL; mb = mbNext)
            {
                if (mb->baseRX >= b->baseRX && (BYTE*)mb->baseRX < ((BYTE*)b->baseRX + b->size))
                {
                    if (pPrevBlockRW == NULL)
                    {
                        m_pFirstBlockRW = mb->next;
                    }
                    else
                    {
                        pPrevBlockRW->next = mb->next;
                    }
                    mbNext = mb->next;
                    free(mb);

                    // pPrevBlockRW stays untouched
                }
                else
                {
                    pPrevBlockRW = mb;
                    mbNext = mb->next;
                }
            }
        }
#endif
    }

    if (b != NULL)
    {
        VMToOSInterface::ReleaseDoubleMappedMemory(pRX, b->size);
    }
    else
    {
        __debugbreak();
    }
}

bool DoubleMappedAllocator::AllocateOffset(size_t *pOffset, size_t size)
{
    size_t offset = m_freeOffset;
    size_t newFreeOffset = offset + size;

    if (newFreeOffset > maxSize)
    {
        __debugbreak();
        return false;
    }

    m_freeOffset = newFreeOffset;

    *pOffset = offset;

    return true;
}

void DoubleMappedAllocator::AddBlockToList(BlockRX* pBlock)
{
    pBlock->next = m_pFirstBlockRX;
    m_pFirstBlockRX = pBlock;
}

void* DoubleMappedAllocator::ReserveWithinRange(size_t size, const void* loAddress, const void* hiAddress)
{
    _ASSERTE((size & (Granularity() - 1)) == 0);
    CRITSEC_Holder csh(m_CriticalSection);

    InterlockedIncrement(&g_reserveCalls);

    size_t offset;
    if (!AllocateOffset(&offset, size))
    {
        __debugbreak();
        return NULL;
    }

    BlockRX* block = new (nothrow) BlockRX();
    if (block == NULL)
    {
        return NULL;
    }

    void *result = VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, offset, size, loAddress, hiAddress);

    if (result != NULL)
    {
        block->baseRX = result;
        block->offset = offset;
        block->size = size;
        AddBlockToList(block);
    }

    return result;
}

void* DoubleMappedAllocator::Reserve(size_t size)
{
    _ASSERTE((size & (Granularity() - 1)) == 0);
    CRITSEC_Holder csh(m_CriticalSection);

    InterlockedIncrement(&g_reserveCalls);

    size_t offset;
    if (!AllocateOffset(&offset, size))
    {
        __debugbreak();
        return NULL;
    }

    BlockRX* block = new (nothrow) BlockRX();
    if (block == NULL)
    {
        return NULL;
    }

    BYTE *result = NULL;

#if USE_UPPER_ADDRESS
    //
    // If we are using the UPPER_ADDRESS space (on Win64)
    // then for any heap that will contain executable code
    // we will place it in the upper address space
    //
    // This enables us to avoid having to use JumpStubs
    // to reach the code for our ngen-ed images on x64,
    // since they are also placed in the UPPER_ADDRESS space.
    //
    BYTE * pHint = s_CodeAllocHint;

    if (size <= (SIZE_T)(s_CodeMaxAddr - s_CodeMinAddr) && pHint != NULL)
    {
        // Try to allocate in the preferred region after the hint
        result = (BYTE*)VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, offset, size, pHint, s_CodeMaxAddr);
        if (result != NULL)
        {
            s_CodeAllocHint = result + size;
        }
        else
        {
            // Try to allocate in the preferred region before the hint
            result = (BYTE*)VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, offset, size, s_CodeMinAddr, pHint + size);

            if (result != NULL)
            {
                s_CodeAllocHint = result + size;
            }

            s_CodeAllocHint = NULL;
        }
    }

    // Fall through to
#endif // USE_UPPER_ADDRESS

    if (result == NULL)
    {
        result = (BYTE*)VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, offset, size, 0, 0);
    }

    block->baseRX = result;
    block->offset = offset;
    block->size = size;
    AddBlockToList(block);

    return result;
}

void* DoubleMappedAllocator::ReserveAt(void* baseAddressRX, size_t size)
{
    CRITSEC_Holder csh(m_CriticalSection);
    _ASSERTE((size & (Granularity() - 1)) == 0);

    InterlockedIncrement(&g_reserveAtCalls);

    size_t offset;
    if (!AllocateOffset(&offset, size))
    {
        __debugbreak();
        return NULL;
    }

    BlockRX* block = new (nothrow) BlockRX();
    if (block == NULL)
    {
        return NULL;
    }

    void* result = VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, offset, size, baseAddressRX, baseAddressRX);

    if (result == NULL)
    {
        return NULL;
    }

    block->baseRX = result;
    block->offset = offset;
    block->size = size;
    AddBlockToList(block);

    return result;
}

#pragma intrinsic(_ReturnAddress)

void DoubleMappedAllocator::UnmapRW(void* pRW)
{
#ifndef DISABLE_UNMAPS
#ifndef CROSSGEN_COMPILE
    CRITSEC_Holder csh(m_CriticalSection);
    _ASSERTE(pRW != NULL);

#ifdef _DEBUG
#ifndef TARGET_UNIX
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T cbBytes = ClrVirtualQuery(pRW, &mbi, sizeof(mbi));
    _ASSERTE(cbBytes);
    if (cbBytes == 0)
    {
        __debugbreak();
    }
#endif
#endif
    // TODO: Linux will need the full range. So we may need to store all the RW mappings in a separate list (per RX mapping)
    InterlockedIncrement(&g_rwUnmaps);

    void* unmapAddress = NULL;
    size_t unmapSize;
    
    if (!RemoveRWBlock(pRW, &unmapAddress, &unmapSize))
    {
        __debugbreak();            
    }

    if (unmapAddress && !VMToOSInterface::ReleaseDoubleMappedMemory(unmapAddress, unmapSize))
    {
        InterlockedIncrement(&g_failedRwUnmaps);
        __debugbreak();
    }
#endif // CROSSGEN_COMPILE
#endif // DISABLE_UNMAPS
}

void DoubleMappedAllocator::RecordUser(void* callAddress, bool reused)
{
    for (UsersListEntry* user = m_mapUsers; user != NULL; user = user->next)
    {
        if (user->user == callAddress)
        {
            if (reused)
            {
                user->reuseCount++;
            }
            else
            {
                user->count++;
            }
            return;
        }
    }

    UsersListEntry* newEntry = (UsersListEntry*)malloc(sizeof(UsersListEntry));
    if (reused)
    {
        newEntry->reuseCount = 1;
        newEntry->count = 0;
    }
    else
    {
        newEntry->reuseCount = 0;
        newEntry->count = 1;
    }
    newEntry->user = callAddress;
    newEntry->next = m_mapUsers;
    m_mapUsers = newEntry;
}

void* DoubleMappedAllocator::MapRW(void* pRX, size_t size, void* returnAddress)
{
#ifndef CROSSGEN_COMPILE
    CRITSEC_Holder csh(m_CriticalSection);

//#define DISABLE_MAP_REUSE

#ifndef DISABLE_MAP_REUSE
    void* result = FindRWBlock(pRX, size);
    if (result != NULL)
    {
        InterlockedIncrement(&g_reusedRwMaps);
        RecordUser(returnAddress, true);
        return result;
    }
#endif
    RecordUser(returnAddress, false);

    size_t searchLength = 0;
    for (BlockRX* b = m_pFirstBlockRX; b != NULL; b = b->next)
    {
        if (pRX >= b->baseRX && ((size_t)pRX + size) <= ((size_t)b->baseRX + b->size))
        {
#ifndef TARGET_UNIX
            MEMORY_BASIC_INFORMATION mbi2;
            ClrVirtualQuery(pRX, &mbi2, sizeof(mbi2));
            
            if (mbi2.Protect & PAGE_READWRITE)
            {
                __debugbreak();
                return pRX;
            }
#endif
            if ((LONG)searchLength > g_maxRXSearchLength)
            {
                g_maxRXSearchLength = (LONG)searchLength;
            }

            g_rxSearchLengthSum += (LONG)searchLength;
            g_rxSearchLengthCount++;

            // Offset of the RX address in the originally allocated block
            size_t offset = (size_t)pRX - (size_t)b->baseRX;
            // Offset of the RX address that will start the newly mapped block
            size_t mapOffset = ALIGN_DOWN(offset, Granularity());
            // Size of the block we will map
            size_t mapSize = ALIGN_UP(offset - mapOffset + size, Granularity());
            void* pRW = VMToOSInterface::GetRWMapping(m_doubleMemoryMapperHandle, (BYTE*)b->baseRX + mapOffset, b->offset + mapOffset, mapSize);

            if (pRW == NULL)
            {
                __debugbreak();
                return NULL;
            }

            InterlockedIncrement(&g_rwMaps);

            AddRWBlock(pRW, (BYTE*)b->baseRX + mapOffset, mapSize);

            return (void*)((size_t)pRW + (offset - mapOffset));
        }
        else if (pRX >= b->baseRX && pRX < (void*)((size_t)b->baseRX + b->size))
        {
            // Error - attempting to map a block that crosses the end of the allocated range
            __debugbreak();
        }
        else if (pRX < b->baseRX && (void*)((size_t)pRX + size) > b->baseRX)
        {
            // Error - attempting to map a block that crosses the beginning of the allocated range
            __debugbreak();
        }
        searchLength++;
    }
#ifdef _DEBUG
#ifndef TARGET_UNIX
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T cbBytes = ClrVirtualQuery(pRX, &mbi, sizeof(mbi));
    _ASSERTE(cbBytes);

    if ((mbi.Protect & PAGE_EXECUTE_READ) || (mbi.Protect & PAGE_EXECUTE_READWRITE))
    {
        __debugbreak();
    }
#endif
#endif
//        return NULL;
    __debugbreak();
    // TODO: this is a hack for the preallocated pages that are part of the coreclr module
    InterlockedIncrement(&g_failedRwMaps);
#endif // !CROSSGEN_COMPILE        
    return pRX;
}

void* DoubleMappedAllocator::MapRW(void* pRX, size_t size)
{
    return MapRW(pRX, size, (void*)_ReturnAddress());
}
