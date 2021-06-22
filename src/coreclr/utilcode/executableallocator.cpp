// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "pedecoder.h"
#include "executableallocator.h"
#ifndef DACCESS_COMPILE

#if USE_UPPER_ADDRESS
BYTE * ExecutableAllocator::s_CodeMinAddr;        // Preferred region to allocate the code in.
BYTE * ExecutableAllocator::s_CodeMaxAddr;
BYTE * ExecutableAllocator::s_CodeAllocStart;
BYTE * ExecutableAllocator::s_CodeAllocHint;      // Next address to try to allocate for code in the preferred region.
#endif // USE_UPPER_ADDRESS

bool ExecutableAllocator::g_isWXorXEnabled = false;

ExecutableAllocator* ExecutableAllocator::g_instance = NULL;
bool ExecutableAllocator::IsDoubleMappingEnabled()
{
    LIMITED_METHOD_CONTRACT;

#if defined(HOST_OSX) && defined(HOST_ARM64)
    return false;
#else
    return g_isWXorXEnabled;
#endif
}

bool ExecutableAllocator::IsWXORXEnabled()
{
    LIMITED_METHOD_CONTRACT;

#if defined(HOST_OSX) && defined(HOST_ARM64)
    return true;
#else
    return g_isWXorXEnabled;
#endif
}

extern SYSTEM_INFO g_SystemInfo;

size_t ExecutableAllocator::Granularity()
{
    LIMITED_METHOD_CONTRACT;

    return g_SystemInfo.dwAllocationGranularity;
}

//
// Use this function to initialize the s_CodeAllocHint
// during startup. base is runtime .dll base address,
// size is runtime .dll virtual size.
//
void ExecutableAllocator::InitCodeAllocHint(size_t base, size_t size, int randomPageOffset)
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
void ExecutableAllocator::ResetCodeAllocHint()
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
bool ExecutableAllocator::IsPreferredExecutableRange(void * p)
{
    LIMITED_METHOD_CONTRACT;
#if USE_UPPER_ADDRESS
    if (s_CodeMinAddr <= (BYTE *)p && (BYTE *)p < s_CodeMaxAddr)
        return TRUE;
#endif
    return FALSE;
}

ExecutableAllocator* ExecutableAllocator::Instance()
{
    LIMITED_METHOD_CONTRACT;
    return g_instance;
}

ExecutableAllocator::~ExecutableAllocator()
{
    if (IsDoubleMappingEnabled())
    {
        VMToOSInterface::DestroyDoubleMemoryMapper(m_doubleMemoryMapperHandle);
    }
}

HRESULT ExecutableAllocator::StaticInitialize()
{
    g_isWXorXEnabled = CLRConfig::GetConfigValue(CLRConfig::EXTERNAL_EnableWXORX) != 0;
    g_instance = new (nothrow) ExecutableAllocator();
    if (g_instance == NULL)
    {
        return E_OUTOFMEMORY;
    }

    if (!g_instance->Initialize())
    {
        return E_FAIL;
    }

    return S_OK;
}

bool ExecutableAllocator::Initialize()
{
    if (IsDoubleMappingEnabled())
    {
        if (!VMToOSInterface::CreateDoubleMemoryMapper(&m_doubleMemoryMapperHandle, &m_maxExecutableCodeSize))
        {
            return false;
        }

        m_CriticalSection = ClrCreateCriticalSection(CrstExecutableAllocatorLock,CrstFlags(CRST_UNSAFE_ANYMODE | CRST_DEBUGGER_THREAD));
    }

    return true;
}

//#define ENABLE_CACHED_MAPPINGS

void ExecutableAllocator::UpdateCachedMapping(BlockRW *b)
{
    LIMITED_METHOD_CONTRACT;
#ifdef ENABLE_CACHED_MAPPINGS
    if (m_cachedMapping == NULL)
    {
        m_cachedMapping = b;
        b->refCount++;
    }
    else if (m_cachedMapping != b)
    {
        void* unmapAddress = NULL;
        size_t unmapSize;

        if (!RemoveRWBlock(m_cachedMapping->baseRW, &unmapAddress, &unmapSize))
        {
            __debugbreak();
        }
        if (unmapAddress && !VMToOSInterface::ReleaseRWMapping(unmapAddress, unmapSize))
        {
            __debugbreak();
        }
        m_cachedMapping = b;
        b->refCount++;
    }
#endif // ENABLE_CACHED_MAPPINGS    
}

void* ExecutableAllocator::FindRWBlock(void* baseRX, size_t size)
{
    for (BlockRW* b = m_pFirstBlockRW; b != NULL; b = b->next)
    {
        if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
        {
            b->refCount++;
            UpdateCachedMapping(b);

            return (BYTE*)b->baseRW + ((size_t)baseRX - (size_t)b->baseRX);
        }
    }

    return NULL;
}

bool ExecutableAllocator::AddRWBlock(void* baseRW, void* baseRX, size_t size)
{
    for (BlockRW* b = m_pFirstBlockRW; b != NULL; b = b->next)
    {
        if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
        {
            break;
        }
    }

    //BlockRW* pBlockRW = (BlockRW*)malloc(sizeof(BlockRW));
    BlockRW* pBlockRW = new (nothrow) BlockRW();
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

    return true;
}

bool ExecutableAllocator::RemoveRWBlock(void* pRW, void** pUnmapAddress, size_t* pUnmapSize)
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

            *pUnmapAddress = pBlockRW->baseRW;
            *pUnmapSize = pBlockRW->size;

            delete pBlockRW;
            //free(pBlockRW);
            return true;
        }

        pPrevBlockRW = pBlockRW;
    }

    return false;
}

bool ExecutableAllocator::AllocateOffset(size_t *pOffset, size_t size)
{
    LIMITED_METHOD_CONTRACT;

    size_t offset = m_freeOffset;
    size_t newFreeOffset = offset + size;

    if (newFreeOffset > m_maxExecutableCodeSize)
    {
        return false;
    }

    m_freeOffset = newFreeOffset;

    *pOffset = offset;

    return true;
}

void ExecutableAllocator::AddBlockToList(BlockRX* pBlock)
{
    LIMITED_METHOD_CONTRACT;

    pBlock->next = m_pFirstBlockRX;
    m_pFirstBlockRX = pBlock;
}

void* ExecutableAllocator::Commit(void* pStart, size_t size, bool isExecutable)
{
    if (IsDoubleMappingEnabled())
    {
        return VMToOSInterface::CommitDoubleMappedMemory(pStart, size, isExecutable);
    }
    else
    {
        return ClrVirtualAlloc(pStart, size, MEM_COMMIT, isExecutable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    }
}

void ExecutableAllocator::Release(void* pRX)
{
    if (IsDoubleMappingEnabled())
    {
        CRITSEC_Holder csh(m_CriticalSection);

        BlockRX* b;
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

        if (b != NULL)
        {
            VMToOSInterface::ReleaseDoubleMappedMemory(m_doubleMemoryMapperHandle, pRX, b->offset, b->size);
            b->baseRX = NULL;
            b->next = m_pFirstFreeBlockRX;
            m_pFirstFreeBlockRX = b;
        }
        else
        {
            __debugbreak();
        }
    }
    else
    {
        ClrVirtualFree(pRX, 0, MEM_RELEASE);
    }
}

ExecutableAllocator::BlockRX* ExecutableAllocator::FindBestFreeBlock(size_t size)
{
    BlockRX* pPrevBlockRX = NULL;
    BlockRX* pPrevBestBlockRX = NULL;
    BlockRX* pBestBlockRX = NULL;
    BlockRX* bBlockRX = m_pFirstFreeBlockRX;
    int count = 0;
    while (bBlockRX != NULL)
    {
        count++;
        if (bBlockRX->size >= size)
        {
            if (pBestBlockRX != NULL)
            {
                if (bBlockRX->size < pBestBlockRX->size)
                {
                    pPrevBestBlockRX = pPrevBlockRX;
                    pBestBlockRX = bBlockRX;
                }
            }
            else
            {
                pPrevBestBlockRX = pPrevBlockRX;
                pBestBlockRX = bBlockRX;
            }
        }
        pPrevBlockRX = bBlockRX;
        bBlockRX = bBlockRX->next;
    }

    if (pBestBlockRX != NULL)
    {
        if (pPrevBestBlockRX != NULL)
        {
            pPrevBestBlockRX->next = pBestBlockRX->next;
        }
        else
        {
            m_pFirstFreeBlockRX = pBestBlockRX->next;
        }

        pBestBlockRX->next = NULL;

        printf("@@@ Found best free block in %d blocks - requested size 0x%llx, found size 0x%llx\n", count, (uint64_t)size, (uint64_t)pBestBlockRX->size);
    }

    return pBestBlockRX;
}

ExecutableAllocator::BlockRX* ExecutableAllocator::AllocateBlock(size_t size, bool* pIsFreeBlock)
{
    size_t offset;
    BlockRX* block = FindBestFreeBlock(size);
    *pIsFreeBlock = (block != NULL);

    if (block == NULL)
    {
        if (!AllocateOffset(&offset, size))
        {
            __debugbreak();
            return NULL;
        }

        block = new (nothrow) BlockRX();
        if (block == NULL)
        {
            return NULL;
        }

        block->offset = offset;
        block->size = size;
    }

    return block;
}

void ExecutableAllocator::BackoutBlock(BlockRX* pBlock, bool isFreeBlock)
{
    if (!isFreeBlock)
    {
        m_freeOffset -= pBlock->size;
        delete pBlock;
    }
    else
    {
        pBlock->next = m_pFirstFreeBlockRX;
        m_pFirstFreeBlockRX = pBlock;
    }
}

void* ExecutableAllocator::ReserveWithinRange(size_t size, const void* loAddress, const void* hiAddress)
{
    _ASSERTE((size & (Granularity() - 1)) == 0);
    if (IsDoubleMappingEnabled())
    {
        CRITSEC_Holder csh(m_CriticalSection);

        bool isFreeBlock;
        BlockRX* block = AllocateBlock(size, &isFreeBlock);
        if (block == NULL)
        {
            return NULL;
        }

        void *result = VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, block->offset, size, loAddress, hiAddress);

        if (result != NULL)
        {
            block->baseRX = result;
            AddBlockToList(block);
        }
        else 
        {
            BackoutBlock(block, isFreeBlock);
        }

        return result;
    }
    else
    {
        DWORD allocationType = MEM_RESERVE;
#ifdef HOST_UNIX
        // Tell PAL to use the executable memory allocator to satisfy this request for virtual memory.
        // This will allow us to place JIT'ed code close to the coreclr library
        // and thus improve performance by avoiding jump stubs in managed code.
        allocationType |= MEM_RESERVE_EXECUTABLE;
#endif
        return ClrVirtualAllocWithinRange((const BYTE*)loAddress, (const BYTE*)hiAddress, size, allocationType, PAGE_NOACCESS);
    }
}

void* ExecutableAllocator::Reserve(size_t size)
{
    _ASSERTE((size & (Granularity() - 1)) == 0);

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
        result = (BYTE*)ReserveWithinRange(size, pHint, s_CodeMaxAddr);
        if (result != NULL)
        {
            s_CodeAllocHint = result + size;
        }
        else
        {
            // Try to allocate in the preferred region before the hint
            result = (BYTE*)ReserveWithinRange(size, s_CodeMinAddr, pHint + size);

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
        if (IsDoubleMappingEnabled())
        {
            CRITSEC_Holder csh(m_CriticalSection);

            bool isFreeBlock;
            BlockRX* block = AllocateBlock(size, &isFreeBlock);
            if (block == NULL)
            {
                return NULL;
            }

            result = (BYTE*)VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, block->offset, size, 0, 0);

            if (result != NULL)
            {
                block->baseRX = result;
                AddBlockToList(block);
            }
            else 
            {
                BackoutBlock(block, isFreeBlock);
            }
        }
        else
        {
            DWORD allocationType = MEM_RESERVE;
#ifdef HOST_UNIX
            // Tell PAL to use the executable memory allocator to satisfy this request for virtual memory.
            // This will allow us to place JIT'ed code close to the coreclr library
            // and thus improve performance by avoiding jump stubs in managed code.
            allocationType |= MEM_RESERVE_EXECUTABLE;
#endif
            result = (BYTE*)ClrVirtualAlloc(NULL, size, allocationType, PAGE_NOACCESS);
        }
    }

    return result;
}

void* ExecutableAllocator::ReserveAt(void* baseAddressRX, size_t size)
{
    _ASSERTE((size & (Granularity() - 1)) == 0);

    if (IsDoubleMappingEnabled())
    {
        CRITSEC_Holder csh(m_CriticalSection);

        bool isFreeBlock;
        BlockRX* block = AllocateBlock(size, &isFreeBlock);
        if (block == NULL)
        {
            return NULL;
        }

        void* result = VMToOSInterface::ReserveDoubleMappedMemory(m_doubleMemoryMapperHandle, block->offset, size, baseAddressRX, baseAddressRX);

        if (result != NULL)
        {
            block->baseRX = result;
            AddBlockToList(block);
        }
        else 
        {
            BackoutBlock(block, isFreeBlock);
        }

        return result;
    }
    else
    {
        return VirtualAlloc(baseAddressRX, size, MEM_RESERVE, PAGE_NOACCESS);
    }
}

void ExecutableAllocator::UnmapRW(void* pRW)
{
    if (!IsDoubleMappingEnabled())
    {
        return;
    }

#ifndef CROSSGEN_COMPILE
    CRITSEC_Holder csh(m_CriticalSection);
    _ASSERTE(pRW != NULL);

    void* unmapAddress = NULL;
    size_t unmapSize;
    
    if (!RemoveRWBlock(pRW, &unmapAddress, &unmapSize))
    {
        __debugbreak();            
    }

    if (unmapAddress && !VMToOSInterface::ReleaseRWMapping(unmapAddress, unmapSize))
    {
        __debugbreak();
    }
#endif // CROSSGEN_COMPILE
}

void* ExecutableAllocator::MapRW(void* pRX, size_t size)
{
    if (!IsDoubleMappingEnabled())
    {
        return pRX;
    }

#ifndef CROSSGEN_COMPILE
    CRITSEC_Holder csh(m_CriticalSection);

//#define DISABLE_MAP_REUSE

#ifndef DISABLE_MAP_REUSE
    void* result = FindRWBlock(pRX, size);
    if (result != NULL)
    {
        return result;
    }
#endif

    size_t searchLength = 0;
    for (BlockRX* b = m_pFirstBlockRX; b != NULL; b = b->next)
    {
        if (pRX >= b->baseRX && ((size_t)pRX + size) <= ((size_t)b->baseRX + b->size))
        {
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
//        return NULL;
    __debugbreak();
    // TODO: this is a hack for the preallocated pages that are part of the coreclr module
#endif // !CROSSGEN_COMPILE        
    return pRX;
}

#endif