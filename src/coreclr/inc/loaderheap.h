// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//*****************************************************************************
// LoaderHeap.h
//

//
// Utility functions for managing memory allocations that typically do not
// need releasing.
//
//*****************************************************************************


#ifndef __LoaderHeap_h__
#define __LoaderHeap_h__

#include "utilcode.h"
#include "ex.h"
#include <intrin.h>

class UnlockedLoaderHeap;

#define ENABLE_DOUBLE_MAPPING

template<typename T>
class DoublePtrT
{
    T* m_pRW;
    T* m_pRX;
    size_t m_headerSize;
    // TODO: this will rather be a handle or a lower lever allocator
    UnlockedLoaderHeap* m_pHeap;

public:
    DoublePtrT() : m_pRW(NULL), m_pRX(NULL), m_headerSize(0), m_pHeap(NULL)
    {
    }

    DoublePtrT(const DoublePtrT<void>& voidDoublePtr) : m_pRW((T*)voidDoublePtr.GetRW()), m_pRX((T*)voidDoublePtr.GetRX()), m_headerSize(voidDoublePtr.GetHeaderSize()), m_pHeap(voidDoublePtr.Heap())
    {
    }

    DoublePtrT(T* pRX, T* pRW, UnlockedLoaderHeap* pHeap) : m_pRW(pRW), m_pRX(pRX), m_headerSize(0), m_pHeap(pHeap)
    {
    }

    DoublePtrT(T* pRX, T* pRW, size_t headerSize, UnlockedLoaderHeap* pHeap) : m_pRW(pRW), m_pRX(pRX), m_headerSize(headerSize), m_pHeap(pHeap)
    {
    }

    static DoublePtrT Null()
    {
        return DoublePtrT();
    }

    DoublePtrT& operator=(const DoublePtrT& other)
    {
        m_pRW = other.m_pRW;
        m_pRX = other.m_pRX;
        m_headerSize = other.m_headerSize;

        return *this;
    }

    T* GetRX() const
    {
        return m_pRX;
    }

    T* GetRW() const
    {
        //_ASSERTE((m_pRX == NULL) || (m_pRW != NULL));
        if (!((m_pRX == NULL) || (m_pRW != NULL)))
        {
            __debugbreak();
        }
        return m_pRW;
    }

    size_t GetHeaderSize() const
    {
        return m_headerSize;
    }

    UnlockedLoaderHeap* Heap() const
    {
        return m_pHeap;
    }

    bool IsNull() const
    {
        return m_pRX == NULL;
    }

    void UnmapRW()
    {
#if !defined(CROSSGEN_COMPILE) && defined(ENABLE_DOUBLE_MAPPING)
        //_ASSERTE(m_pRW != m_pRX);
        if (m_pRW == m_pRX)
        {
            __debugbreak();
        }
        DoubleMappedAllocator::Instance()->UnmapRW((BYTE*)m_pRW - m_headerSize);
#endif        
        m_pRW = NULL;
    }
};

typedef DoublePtrT<void> DoublePtr;

//==============================================================================
// Interface used to back out loader heap allocations.
//==============================================================================
class ILoaderHeapBackout
{
#ifdef _DEBUG
#define BackoutMem(pMem, dwSize)  RealBackoutMem( (pMem), (dwSize), __FILE__, __LINE__, "UNKNOWN", -1 )
#else
#define BackoutMem(pMem, dwSize)  RealBackoutMem( (pMem), (dwSize) )
#endif

public:
    virtual void RealBackoutMem(void *pMem
                        , size_t dwSize
#ifdef _DEBUG
                        , __in __in_z const char *szFile
                        , int lineNum
                        , __in __in_z const char *szAllocFile
                        , int allocLineNum
#endif
                        ) = 0;
};

//==============================================================================
// This structure packages up all the data needed to back out an AllocMem.
// It's mainly a short term parking place to get the data from the AllocMem
// to the AllocMemHolder while preserving the illusion that AllocMem() still
// returns just a pointer as it did in V1.
//==============================================================================
struct TaggedMemAllocPtr
{
    // Note: For AllocAlignedMem blocks, m_pMem and m_dwRequestedSize are the actual values to pass
    // to BackoutMem. Do not add "m_dwExtra"
    void        *m_pMem;                //Pointer to AllocMem'd block (needed to pass back to BackoutMem)
    size_t       m_dwRequestedSize;     //Requested allocation size (needed to pass back to BackoutMem)

    ILoaderHeapBackout  *m_pHeap;          //The heap that alloc'd the block (needed to know who to call BackoutMem on)

    //For AllocMem'd blocks, this is always 0.
    //For AllocAlignedMem blocks, you have to add m_dwExtra to m_pMem to arrive
    //   at the actual aligned pointer.
    size_t       m_dwExtra;

#ifdef _DEBUG
    const char  *m_szFile;              //File that called AllocMem
    int          m_lineNum;             //Line # of AllocMem callsite
#endif

//! Note: this structure is copied around using bitwise copy ("=").
//! Don't get too fancy putting stuff in here. It's really just a temporary
//! holding place to get stuff from RealAllocMem() to the MemAllocHolder.


  public:

    //
    // This makes "void *ptr = pLoaderHeap->AllocMem()" work as in V1.
    //
    operator void*() const
    {
        LIMITED_METHOD_CONTRACT;
        return (void*)(m_dwExtra + (BYTE*)m_pMem);
    }

    template < typename T >
    T cast() const
    {
        LIMITED_METHOD_CONTRACT;
        return reinterpret_cast< T >( operator void *() );
    }
};

#ifdef ENABLE_DOUBLE_MAPPING


extern BYTE * s_CodeMinAddr;        // Preferred region to allocate the code in.
extern BYTE * s_CodeMaxAddr;
extern BYTE * s_CodeAllocStart;
extern BYTE * s_CodeAllocHint;      // Next address to try to allocate for code in the preferred region.

class DoubleMappedAllocator
{
    static volatile DoubleMappedAllocator* g_instance;

    struct Block
    {
        Block* next;
        void* baseRX;
        size_t size;
        size_t offset;
    };

    struct MappedBlock
    {
        MappedBlock* next;

        void* baseRW;
        void* baseRX;
        size_t size;
        size_t refCount;
    };

    struct UsersListEntry
    {
        UsersListEntry* next;
        size_t count;
        void *user;
    };

    Block* m_firstBlock = NULL;
    MappedBlock* m_firstMappedBlock = NULL;
    HANDLE m_hSharedMemoryFile = NULL;
    size_t maxSize = 2048ULL*1024*1024;
    size_t m_freeOffset = 0;
    UsersListEntry *m_mapUsers = NULL;

    CRITSEC_COOKIE m_CriticalSection;

    size_t Granularity()
    {
        return 64 * 1024;
    }
public:

    static DoubleMappedAllocator* Instance()
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

    ~DoubleMappedAllocator()
    {
        CloseHandle(m_hSharedMemoryFile);
    }

    bool Initialize()
    {
        m_hSharedMemoryFile = WszCreateFileMapping(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_EXECUTE_READWRITE |  SEC_RESERVE,  // read/write/execute access
                 HIDWORD(maxSize),                       // maximum object size (high-order DWORD)
                 LODWORD(maxSize),   // maximum object size (low-order DWORD)
                 NULL);    

        m_CriticalSection = ClrCreateCriticalSection(CrstListLock,CrstFlags(CRST_UNSAFE_ANYMODE | CRST_DEBUGGER_THREAD));

        return m_hSharedMemoryFile != NULL;    
    }

    MappedBlock* m_cachedMapping = NULL;

    void UpdateCachedMapping(MappedBlock *b)
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
            if (!RemoveMappedBlock2(m_cachedMapping->baseRW, &unmapAddress))
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

    void* FindMappedBlock(void* baseRX, size_t size)
    {
        for (MappedBlock* b = m_firstMappedBlock; b != NULL; b = b->next)
        {
            if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
            {
                b->refCount++;
                if (b->refCount > g_maxReusedRwMapsRefcount)
                {
                    g_maxReusedRwMapsRefcount = b->refCount;
                }
                UpdateCachedMapping(b);
                //printf("Reusing mapping at %p for %p\n", b->baseRW, baseRX);
                return (BYTE*)b->baseRW + ((size_t)baseRX - (size_t)b->baseRX);
            }
        }

        return NULL;
    }

    bool AddMappedBlock(void* baseRW, void* baseRX, size_t size)
    {
//        CRITSEC_Holder csh(m_CriticalSection);

        for (MappedBlock* b = m_firstMappedBlock; b != NULL; b = b->next)
        {
            if (b->baseRX <= baseRX && ((size_t)baseRX + size) <= ((size_t)b->baseRX + b->size))
            {
                InterlockedIncrement(&g_mapReusePossibility);
                break;
            }
        }

        MappedBlock* mappedBlock = (MappedBlock*)malloc(sizeof(MappedBlock));// new (nothrow) MappedBlock();
        if (mappedBlock == NULL)
        {
            return false;
        }

        mappedBlock->baseRW = baseRW;
        mappedBlock->baseRX = baseRX;
        mappedBlock->size = size;
        mappedBlock->next = m_firstMappedBlock;
        mappedBlock->refCount = 1;
        m_firstMappedBlock = mappedBlock;

        UpdateCachedMapping(mappedBlock);

        InterlockedIncrement(&g_RWMappingCount);

        return true;
    }

    bool RemoveMappedBlock(void* base)
    {
//        CRITSEC_Holder csh(m_CriticalSection);

        MappedBlock* prevMappedBlock = NULL;
        for (MappedBlock* mappedBlock = m_firstMappedBlock; mappedBlock != NULL; mappedBlock = mappedBlock->next)
        {
            if (mappedBlock->baseRW == base)
            {
                // found
                if (prevMappedBlock == NULL)
                {
                    m_firstMappedBlock = mappedBlock->next;
                }
                else
                {
                    prevMappedBlock->next = mappedBlock->next;
                }

                InterlockedDecrement(&g_RWMappingCount);
                if (g_RWMappingCount > g_maxRWMappingCount)
                {
                    g_maxRWMappingCount = g_RWMappingCount;
                }

                //delete mappedBlock;
                free(mappedBlock);
                return true;
            }

            prevMappedBlock = mappedBlock;
        }

        return false;
    }

    bool RemoveMappedBlock2(void* pRW, void** pUnmapAddress)
    {
//        CRITSEC_Holder csh(m_CriticalSection);

        MappedBlock* prevMappedBlock = NULL;
        for (MappedBlock* mappedBlock = m_firstMappedBlock; mappedBlock != NULL; mappedBlock = mappedBlock->next)
        {
            if (mappedBlock->baseRW <= pRW && (size_t)pRW <  ((size_t)mappedBlock->baseRW + mappedBlock->size))
            {
                // found
                mappedBlock->refCount--;
                if (mappedBlock->refCount != 0)
                {
                    *pUnmapAddress = NULL;
                    return true;
                }

                if (prevMappedBlock == NULL)
                {
                    m_firstMappedBlock = mappedBlock->next;
                }
                else
                {
                    prevMappedBlock->next = mappedBlock->next;
                }

                InterlockedDecrement(&g_RWMappingCount);
                if (g_RWMappingCount > g_maxRWMappingCount)
                {
                    g_maxRWMappingCount = g_RWMappingCount;
                }

                *pUnmapAddress = mappedBlock->baseRW;

                //delete mappedBlock;
                free(mappedBlock);
                return true;
            }

            prevMappedBlock = mappedBlock;
        }

        return false;
    }

    BYTE * ReserveWithinRange(const BYTE *pMinAddr,
                              const BYTE *pMaxAddr,
                              SIZE_T dwSize,
                              SIZE_T offset)
    {
        BYTE *pResult = nullptr;  // our return value;

        if (dwSize == 0)
        {
            return nullptr;
        }

        //
        // First lets normalize the pMinAddr and pMaxAddr values
        //
        // If pMinAddr is NULL then set it to BOT_MEMORY
        if ((pMinAddr == 0) || (pMinAddr < (BYTE *) BOT_MEMORY))
        {
            pMinAddr = (BYTE *) BOT_MEMORY;
        }

        // If pMaxAddr is NULL then set it to TOP_MEMORY
        if ((pMaxAddr == 0) || (pMaxAddr > (BYTE *) TOP_MEMORY))
        {
            pMaxAddr = (BYTE *) TOP_MEMORY;
        }

        // If pMaxAddr is not greater than pMinAddr we can not make an allocation
        if (pMaxAddr <= pMinAddr)
        {
            return nullptr;
        }

        // If pMinAddr is BOT_MEMORY and pMaxAddr is TOP_MEMORY
        // then we can call ClrVirtualAlloc instead
        if ((pMinAddr == (BYTE *) BOT_MEMORY) && (pMaxAddr == (BYTE *) TOP_MEMORY))
        {
            return (BYTE*)MapViewOfFile(m_hSharedMemoryFile,
                            FILE_MAP_EXECUTE | FILE_MAP_READ | FILE_MAP_WRITE, // TODO: can we add the write only for reservations that will be for both data and execution, like the loader allocator? Or should we split the initial allocation to two parts - exe and non-exe?
                            offset / ((size_t)1) >> 32,
                            offset & 0xFFFFFFFFUL,
                            dwSize);
        }

        // We will do one scan from [pMinAddr .. pMaxAddr]
        // First align the tryAddr up to next 64k base address.
        // See docs for VirtualAllocEx and lpAddress and 64k alignment for reasons.
        //
        BYTE *   tryAddr            = (BYTE *)ALIGN_UP((BYTE *)pMinAddr, Granularity());
        bool     virtualQueryFailed = false;
#ifdef DEBUG        
        bool     faultInjected      = false;
#endif        
        unsigned virtualQueryCount  = 0;

        // Now scan memory and try to find a free block of the size requested.
        while ((tryAddr + dwSize) <= (BYTE *) pMaxAddr)
        {
            MEMORY_BASIC_INFORMATION mbInfo;

            // Use VirtualQuery to find out if this address is MEM_FREE
            //
            virtualQueryCount++;
            if (!ClrVirtualQuery((LPCVOID)tryAddr, &mbInfo, sizeof(mbInfo)))
            {
                // Exit and return nullptr if the VirtualQuery call fails.
                virtualQueryFailed = true;
                break;
            }

            // Is there enough memory free from this start location?
            // Note that for most versions of UNIX the mbInfo.RegionSize returned will always be 0
            if ((mbInfo.State == MEM_FREE) &&
                (mbInfo.RegionSize >= (SIZE_T) dwSize || mbInfo.RegionSize == 0))
            {
                pResult = (BYTE*) MapViewOfFileEx(m_hSharedMemoryFile,
                        FILE_MAP_EXECUTE | FILE_MAP_READ | FILE_MAP_WRITE, // TODO: can we add the write only for reservations that will be for both data and execution, like the loader allocator? Or should we split the initial allocation to two parts - exe and non-exe?
                        offset / ((size_t)1) >> 32,
                        offset & 0xFFFFFFFFUL,
                        dwSize,
                        tryAddr);

                // Normally this will be successful
                //
                if (pResult != nullptr)
                {
                    // return pResult
                    break;
                }

    #ifdef _DEBUG
                if (ShouldInjectFaultInRange())
                {
                    // return nullptr (failure)
                    faultInjected = true;
                    break;
                }
    #endif // _DEBUG

                // On UNIX we can also fail if our request size 'dwSize' is larger than 64K and
                // and our tryAddr is pointing at a small MEM_FREE region (smaller than 'dwSize')
                // However we can't distinguish between this and the race case.

                // We might fail in a race.  So just move on to next region and continue trying
                tryAddr = tryAddr + Granularity();
            }
            else
            {
                // Try another section of memory
                tryAddr = max(tryAddr + Granularity(),
                            (BYTE*) mbInfo.BaseAddress + mbInfo.RegionSize);
            }
        }

        // STRESS_LOG7(LF_JIT, LL_INFO100,
        //             "ClrVirtualAllocWithinRange request #%u for %08x bytes in [ %p .. %p ], query count was %u - returned %s: %p\n",
        //             countOfCalls, (DWORD)dwSize, pMinAddr, pMaxAddr,
        //             virtualQueryCount, (pResult != nullptr) ? "success" : "failure", pResult);

        // // If we failed this call the process will typically be terminated
        // // so we log any additional reason for failing this call.
        // //
        // if (pResult == nullptr)
        // {
        //     if ((tryAddr + dwSize) > (BYTE *)pMaxAddr)
        //     {
        //         // Our tryAddr reached pMaxAddr
        //         STRESS_LOG0(LF_JIT, LL_INFO100, "Additional reason: Address space exhausted.\n");
        //     }

        //     if (virtualQueryFailed)
        //     {
        //         STRESS_LOG0(LF_JIT, LL_INFO100, "Additional reason: VirtualQuery operation failed.\n");
        //     }

        //     if (faultInjected)
        //     {
        //         STRESS_LOG0(LF_JIT, LL_INFO100, "Additional reason: fault injected.\n");
        //     }
        // }

        return pResult;
    }

    void* Reserve(size_t size)
    {
        _ASSERTE((size & (Granularity() - 1)) == 0);
        CRITSEC_Holder csh(m_CriticalSection);

        InterlockedIncrement(&g_reserveCalls);

        size_t offset;
        size_t newFreeOffset;

        do
        {
            offset = m_freeOffset;
            newFreeOffset = offset + size;

            if (newFreeOffset > maxSize)
            {
                __debugbreak();
                return false;
            }
        }
        while (InterlockedCompareExchangeT(&m_freeOffset, newFreeOffset, offset) != offset);

        Block* block = new (nothrow) Block();
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
            result = ReserveWithinRange(pHint, s_CodeMaxAddr, size, offset);

            if (result != NULL)
            {
                s_CodeAllocHint = result + size;
            }
            else
            {
                // Try to allocate in the preferred region before the hint
                result = ReserveWithinRange(s_CodeMinAddr, pHint + size, size, offset);

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
            result = (BYTE*)MapViewOfFile(m_hSharedMemoryFile,
                            FILE_MAP_EXECUTE | FILE_MAP_READ | FILE_MAP_WRITE, // TODO: can we add the write only for reservations that will be for both data and execution, like the loader allocator? Or should we split the initial allocation to two parts - exe and non-exe?
                            offset / ((size_t)1) >> 32,
                            offset & 0xFFFFFFFFUL,
                            size);
        }

        block->baseRX = result;
        block->offset = offset;
        block->size = size;

        // TODO: probably use lock so that we can search the list? Hmm, we will never remove blocks, will we?
        do
        {
            block->next = m_firstBlock;
        }
        while (InterlockedCompareExchangeT(&m_firstBlock, block, block->next) != block->next);

        return result;
    }

    void* ReserveAt(void* baseAddressRX, size_t size)
    {
        CRITSEC_Holder csh(m_CriticalSection);
        _ASSERTE((size & (Granularity() - 1)) == 0);

        InterlockedIncrement(&g_reserveAtCalls);

        size_t offset;
        size_t newFreeOffset;

        do
        {
            offset = m_freeOffset;
            newFreeOffset = offset + size;

            if (newFreeOffset > maxSize)
            {
                __debugbreak();
                return false;
            }
        }
        while (InterlockedCompareExchangeT(&m_freeOffset, newFreeOffset, offset) != offset);

        Block* block = new (nothrow) Block();
        if (block == NULL)
        {
            return false;
        }

        void* result = MapViewOfFileEx(m_hSharedMemoryFile,
                        FILE_MAP_EXECUTE | FILE_MAP_READ | FILE_MAP_WRITE, // TODO: can we add the write only for reservations that will be for both data and execution, like the loader allocator? Or should we split the initial allocation to two parts - exe and non-exe?
                        offset / ((size_t)1) >> 32,
                        offset & 0xFFFFFFFFUL,
                        size,
                        baseAddressRX);

        if (result == NULL)
        {
            return false;
        }

        block->baseRX = result;
        block->offset = offset;
        block->size = size;

        // TODO: probably use lock so that we can search the list? Hmm, we will never remove blocks, will we?
        do
        {
            block->next = m_firstBlock;
        }
        while (InterlockedCompareExchangeT(&m_firstBlock, block, block->next) != block->next);

        return result;
    }

    #pragma intrinsic(_ReturnAddress)

    static size_t g_allocCalls;
    static size_t g_reserveCalls;
    static size_t g_reserveAtCalls;
    static size_t g_rwMaps;
    static size_t g_reusedRwMaps;
    static size_t g_maxReusedRwMapsRefcount;
    static size_t g_rwUnmaps;
    static size_t g_failedRwUnmaps;
    static size_t g_failedRwMaps;
    static size_t g_mapReusePossibility;
    static size_t g_RWMappingCount;
    static size_t g_maxRWMappingCount;

    void ReportState();

    void UnmapRW(void* pRW)
    {
#ifndef CROSSGEN_COMPILE
        CRITSEC_Holder csh(m_CriticalSection);
        _ASSERTE(pRW != NULL);
        //_ASSERTE(pRW < (void*)0x700000000000ULL);

#ifdef _DEBUG
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T cbBytes = ClrVirtualQuery(pRW, &mbi, sizeof(mbi));
        _ASSERTE(cbBytes);
        if (cbBytes == 0)
        {
            __debugbreak();
        }
        //printf("UnmapRW: RW=%p, base=%p, size=%p, prot=%08x, state=%08x, type=%08x, caller=%p\n", pRW, (void*)mbi.BaseAddress, (void*)mbi.RegionSize, mbi.Protect, mbi.State, mbi.Type, (void*)_ReturnAddress());
#endif
        // TODO: Linux will need the full range. So we may need to store all the RW mappings in a separate list (per RX mapping)
        InterlockedIncrement(&g_rwUnmaps);

        void* unmapAddress = NULL;
        if (!RemoveMappedBlock2(pRW, &unmapAddress))
        {
            __debugbreak();            
        }

        if (unmapAddress)
        {
            //printf("Unmapping %p called from %p\n", unmapAddress, (void*)_ReturnAddress());
        }
        if (unmapAddress && !UnmapViewOfFile(unmapAddress))
        {
            InterlockedIncrement(&g_failedRwUnmaps);
            __debugbreak();
        }
#endif
    }

    void RecordUser(void* callAddress)
    {
        for (UsersListEntry* user = m_mapUsers; user != NULL; user = user->next)
        {
            if (user->user == callAddress)
            {
                user->count++;
                return;
            }
        }

        UsersListEntry* newEntry = new (nothrow) UsersListEntry();
        newEntry->count = 1;
        newEntry->user = callAddress;
        newEntry->next = m_mapUsers;
        m_mapUsers = newEntry;
    }

    void* MapRW(void* pRX, size_t size)
    {
        CRITSEC_Holder csh(m_CriticalSection);

        void* result = FindMappedBlock(pRX, size);
        if (result != NULL)
        {
            InterlockedIncrement(&g_reusedRwMaps);
            return result;
        }

        RecordUser((void*)_ReturnAddress());

        for (Block* b = m_firstBlock; b != NULL; b = b->next)
        {
            if (pRX >= b->baseRX && ((size_t)pRX + size) <= ((size_t)b->baseRX + b->size))
            {
                // Offset of the RX address in the originally allocated block
                size_t offset = (size_t)pRX - (size_t)b->baseRX;
                // Offset of the RX address that will start the newly mapped block
                size_t mapOffset = ALIGN_DOWN(offset, Granularity());
                // Size of the block we will map
                size_t mapSize = ALIGN_UP(offset - mapOffset + size, Granularity());
                void* pRW = MapViewOfFile(m_hSharedMemoryFile,
                                FILE_MAP_READ | FILE_MAP_WRITE,
                                (b->offset + mapOffset) / ((size_t)1) >> 32,
                                (b->offset + mapOffset) & 0xFFFFFFFFUL,
                                mapSize);

                //printf("MapRW: baseRX=%p, RX=%p, RW=%p, mapOffset=%p, size=%p, mapSize=%p\n", b->baseRX, pRX, pRW, (void*)mapOffset, (void*)size, (void*)mapSize);
                if (pRW == NULL)
                {
                    __debugbreak();
                    return NULL;
                }

                InterlockedIncrement(&g_rwMaps);

                //printf("Created mapping at %p for %p, size %p\n", pRW, (BYTE*)b->baseRX + mapOffset, (void*)mapSize);
                AddMappedBlock(pRW, (BYTE*)b->baseRX + mapOffset, mapSize);

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
        }
#ifdef _DEBUG
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T cbBytes = ClrVirtualQuery(pRX, &mbi, sizeof(mbi));
        _ASSERTE(cbBytes);

        if ((mbi.Protect & PAGE_EXECUTE_READ) || (mbi.Protect & PAGE_EXECUTE_READWRITE))
        {
            __debugbreak();
        }
#endif        
//        return NULL;
        // TODO: this is a hack for the preallocated pages that are part of the coreclr module
        InterlockedIncrement(&g_failedRwMaps);
        return pRX;
    }
};

#else // ENABLE_DOUBLE_MAPPING

class DoubleMappedAllocator
{
    static volatile DoubleMappedAllocator* g_instance;

    CRITSEC_COOKIE m_CriticalSection;

    size_t Granularity()
    {
        return 64 * 1024;
    }
public:

    static DoubleMappedAllocator* Instance()
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

    ~DoubleMappedAllocator()
    {
    }

    bool Initialize()
    {
        return true;
    }

    void* Reserve(size_t size)
    {
        return ClrVirtualAllocExecutable(size, MEM_RESERVE, PAGE_NOACCESS);
    }

    void* ReserveAt(void* baseAddressRX, size_t size)
    {
        return VirtualAlloc(baseAddressRX, size, MEM_RESERVE, PAGE_NOACCESS);
    }

    #pragma intrinsic(_ReturnAddress)

    static size_t g_allocCalls;
    static size_t g_reserveCalls;
    static size_t g_reserveAtCalls;
    static size_t g_rwMaps;
    static size_t g_reusedRwMaps;
    static size_t g_maxReusedRwMapsRefcount;
    static size_t g_rwUnmaps;
    static size_t g_failedRwUnmaps;
    static size_t g_failedRwMaps;
    static size_t g_mapReusePossibility;
    static size_t g_RWMappingCount;
    static size_t g_maxRWMappingCount;

    void ReportState();

    void UnmapRW(void* pRW)
    {
    }

    void* MapRW(void* pRX, size_t size)
    {
        return pRX;
    }
};

#endif // ENABLE_DOUBLE_MAPPING

// # bytes to leave between allocations in debug mode
// Set to a > 0 boundary to debug problems - I've made this zero, otherwise a 1 byte allocation becomes
// a (1 + LOADER_HEAP_DEBUG_BOUNDARY) allocation
#define LOADER_HEAP_DEBUG_BOUNDARY  0

#define VIRTUAL_ALLOC_RESERVE_GRANULARITY (64*1024)    // 0x10000  (64 KB)

typedef DPTR(struct LoaderHeapBlock) PTR_LoaderHeapBlock;

struct LoaderHeapBlock
{
    PTR_LoaderHeapBlock     pNext;
    PTR_VOID                pVirtualAddress;
    size_t                  dwVirtualSize;
    BOOL                    m_fReleaseMemory;

#ifndef DACCESS_COMPILE
    // pVirtualMemory  - the start address of the virtual memory
    // cbVirtualMemory - the length in bytes of the virtual memory
    // fReleaseMemory  - should LoaderHeap be responsible for releasing this memory
    void Init(void   *pVirtualMemory,
              size_t  cbVirtualMemory,
              BOOL    fReleaseMemory)
    {
        LIMITED_METHOD_CONTRACT;
        this->pNext = NULL;
        this->pVirtualAddress = pVirtualMemory;
        this->dwVirtualSize = cbVirtualMemory;
        this->m_fReleaseMemory = fReleaseMemory;
    }

    // Just calls LoaderHeapBlock::Init
    LoaderHeapBlock(void   *pVirtualMemory,
                    size_t  cbVirtualMemory,
                    BOOL    fReleaseMemory)
    {
        WRAPPER_NO_CONTRACT;
        Init(pVirtualMemory, cbVirtualMemory, fReleaseMemory);
    }

    LoaderHeapBlock()
    {
        WRAPPER_NO_CONTRACT;
        Init(NULL, 0, FALSE);
    }
#else
    // No ctors in DAC builds
    LoaderHeapBlock() {}
#endif
};

struct LoaderHeapFreeBlock;

// Collection of methods for helping in debugging heap corruptions
#ifdef _DEBUG
class  LoaderHeapSniffer;
struct LoaderHeapEvent;
#endif



//===============================================================================
// This is the base class for LoaderHeap and ExplicitControlLoaderHeap. Unfortunately,
// this class has become schizophrenic. Sometimes, it's used as a simple
// allocator that's semantically (but not perfwise!) equivalent to a blackbox
// alloc/free heap. Othertimes, it's used by callers who are actually aware
// of how it reserves addresses and want direct control over the range over which
// this thing allocates. These two types of allocations are handed out
// from two independent pools inside the heap.
//
// The backout strategy we use for the simple heap probably isn't
// directly applicable to the more advanced uses.
//
// We don't have time to refactor this so as a second-best measure,
// we make most of UnlockedLoaderHeap's methods protected and force everyone
// to use it them through two public derived classes that are mutual siblings.
//
// The LoaderHeap is the black-box heap and has a Backout() method but none
// of the advanced features that let you control address ranges.
//
// The ExplicitControlLoaderHeap exposes all the advanced features but
// has no Backout() feature. (If someone wants a backout feature, they need
// to design an appropriate one into this class.)
//===============================================================================
class UnlockedLoaderHeap
{
#ifdef _DEBUG
    friend class LoaderHeapSniffer;
#endif

#ifdef DACCESS_COMPILE
    friend class ClrDataAccess;
#endif

private:
    // Linked list of ClrVirtualAlloc'd pages
    PTR_LoaderHeapBlock m_pFirstBlock;

    // Allocation pointer in current block
    PTR_BYTE            m_pAllocPtr;

    // Points to the end of the committed region in the current block
    PTR_BYTE            m_pPtrToEndOfCommittedRegion;
    PTR_BYTE            m_pEndReservedRegion;

    PTR_LoaderHeapBlock m_pCurBlock;

    // When we need to ClrVirtualAlloc() MEM_RESERVE a new set of pages, number of bytes to reserve
    DWORD               m_dwReserveBlockSize;

    // When we need to commit pages from our reserved list, number of bytes to commit at a time
    DWORD               m_dwCommitBlockSize;

    // Range list to record memory ranges in
    RangeList *         m_pRangeList;

    size_t              m_dwTotalAlloc;

    DWORD                m_Options;

    LoaderHeapFreeBlock *m_pFirstFreeBlock;

    // This is used to hold on to a block of reserved memory provided to the
    // constructor. We do this instead of adding it as the first block because
    // that requires comitting the first page of the reserved block, and for
    // startup working set reasons we want to delay that as long as possible.
    LoaderHeapBlock      m_reservedBlock;

public:

#ifdef _DEBUG
    enum
    {
        kCallTracing    = 0x00000001,   // Keep a permanent log of all callers

        kEncounteredOOM = 0x80000000,   // One time flag to record that an OOM interrupted call tracing
    }
    LoaderHeapDebugFlags;

    DWORD               m_dwDebugFlags;

    LoaderHeapEvent    *m_pEventList;   // Linked list of events (in reverse time order)
#endif



#ifdef _DEBUG
    size_t              m_dwDebugWastedBytes;
    static DWORD        s_dwNumInstancesOfLoaderHeaps;
#endif

#ifdef _DEBUG
    size_t DebugGetWastedBytes()
    {
        WRAPPER_NO_CONTRACT;
        return m_dwDebugWastedBytes + GetBytesAvailCommittedRegion();
    }
#endif

#ifdef _DEBUG
    // Stubs allocated from a LoaderHeap will have unwind info registered with NT.
    // The info must be unregistered when the heap is destroyed.
    BOOL                m_fPermitStubsWithUnwindInfo;
    BOOL                m_fStubUnwindInfoUnregistered;
#endif

public:
    BOOL                m_fExplicitControl;  // Am I a LoaderHeap or an ExplicitControlLoaderHeap?

#ifdef DACCESS_COMPILE
public:
    void EnumMemoryRegions(enum CLRDataEnumMemoryFlags flags);
#endif

public:
    typedef bool EnumPageRegionsCallback (PTR_VOID pvArgs, PTR_VOID pvAllocationBase, SIZE_T cbReserved);
    void EnumPageRegions (EnumPageRegionsCallback *pCallback, PTR_VOID pvArgs);

#ifndef DACCESS_COMPILE
protected:
    // Use this version if dwReservedRegionAddress already points to a
    // blob of reserved memory.  This will set up internal data structures,
    // using the provided, reserved memory.
    UnlockedLoaderHeap(DWORD dwReserveBlockSize,
                       DWORD dwCommitBlockSize,
                       const BYTE* dwReservedRegionAddress,
                       SIZE_T dwReservedRegionSize,
                       RangeList *pRangeList = NULL,
                       BOOL fMakeExecutable = FALSE);

    ~UnlockedLoaderHeap();
#endif

private:
    size_t GetBytesAvailCommittedRegion();
    size_t GetBytesAvailReservedRegion();

protected:
    // number of bytes available in region
    size_t UnlockedGetReservedBytesFree()
    {
        LIMITED_METHOD_CONTRACT;
        return m_pEndReservedRegion - m_pAllocPtr;
    }

private:
    // Get some more committed pages - either commit some more in the current reserved region, or, if it
    // has run out, reserve another set of pages
    BOOL GetMoreCommittedPages(size_t dwMinSize);

protected:
    // Reserve some pages at any address
    BOOL UnlockedReservePages(size_t dwCommitBlockSize);

protected:
    // In debug mode, allocate an extra LOADER_HEAP_DEBUG_BOUNDARY bytes and fill it with invalid data.  The reason we
    // do this is that when we're allocating vtables out of the heap, it is very easy for code to
    // get careless, and end up reading from memory that it doesn't own - but since it will be
    // reading some other allocation's vtable, no crash will occur.  By keeping a gap between
    // allocations, it is more likely that these errors will be encountered.
    void *UnlockedAllocMem(size_t dwSize
#ifdef _DEBUG
                          ,__in __in_z const char *szFile
                          ,int  lineNum
#endif
                          );
    void *UnlockedAllocMem_NoThrow(size_t dwSize
#ifdef _DEBUG
                                   ,__in __in_z const char *szFile
                                   ,int  lineNum
#endif
                                   );





protected:
    // Allocates memory aligned on power-of-2 boundary.
    //
    // The return value is a pointer that's guaranteed to be aligned.
    //
    // FREEING THIS BLOCK: Underneath, the actual block allocated may
    // be larger and start at an address prior to the one you got back.
    // It is this adjusted size and pointer that you pass to BackoutMem.
    // The required adjustment is passed back thru the pdwExtra pointer.
    //
    // Here is how to properly backout the memory:
    //
    //   size_t dwExtra;
    //   void *pMem = UnlockedAllocAlignedMem(dwRequestedSize, alignment, &dwExtra);
    //   _ASSERTE( 0 == (pMem & (alignment - 1)) );
    //   UnlockedBackoutMem( ((BYTE*)pMem) - dExtra, dwRequestedSize + dwExtra );
    //
    // If you use the AllocMemHolder or AllocMemTracker, all this is taken care of
    // behind the scenes.
    //
    //
    void *UnlockedAllocAlignedMem(size_t  dwRequestedSize
                                 ,size_t  dwAlignment
                                 ,size_t *pdwExtra
#ifdef _DEBUG
                                 ,__in __in_z const char *szFile
                                 ,int  lineNum
#endif
                                 );

    void *UnlockedAllocAlignedMem_NoThrow(size_t  dwRequestedSize
                                         ,size_t  dwAlignment
                                         ,size_t *pdwExtra
#ifdef _DEBUG
                                         ,__in __in_z const char *szFile
                                         ,int  lineNum
#endif
                                 );

protected:
    // This frees memory allocated by UnlockAllocMem. It's given this horrible name to emphasize
    // that it's purpose is for error path leak prevention purposes. You shouldn't
    // use LoaderHeap's as general-purpose alloc-free heaps.
    void UnlockedBackoutMem(void *pMem
                          , size_t dwSize
#ifdef _DEBUG
                          , __in __in_z const char *szFile
                          , int lineNum
                          , __in __in_z const char *szAllocFile
                          , int AllocLineNum
#endif
                          );

public:
    // Perf Counter reports the size of the heap
    size_t GetSize ()
    {
        LIMITED_METHOD_CONTRACT;
        return m_dwTotalAlloc;
    }

    BOOL IsExecutable();

public:
#ifdef _DEBUG
    void DumpFreeList();
#endif

public:
// Extra CallTracing support
#ifdef _DEBUG
    void UnlockedClearEvents();     //Discard saved events
    void UnlockedCompactEvents();   //Discard matching alloc/free events
    void UnlockedPrintEvents();     //Print event list
#endif

protected:
    void *UnlockedAllocMemForCode_NoThrow(size_t dwHeaderSize, size_t dwCodeSize, DWORD dwCodeAlignment, size_t dwReserveForJumpStubs);

    void UnlockedSetReservedRegion(BYTE* dwReservedRegionAddress, SIZE_T dwReservedRegionSize, BOOL fReleaseMemory);
};

//===============================================================================
// Create the LoaderHeap lock. It's the same lock for several different Heaps.
//===============================================================================
inline CRITSEC_COOKIE CreateLoaderHeapLock()
{
    return ClrCreateCriticalSection(CrstLoaderHeap,CrstFlags(CRST_UNSAFE_ANYMODE | CRST_DEBUGGER_THREAD));
}

//===============================================================================
// The LoaderHeap is the black-box heap and has a Backout() method but none
// of the advanced features that let you control address ranges.
//===============================================================================
typedef DPTR(class LoaderHeap) PTR_LoaderHeap;
class LoaderHeap : public UnlockedLoaderHeap, public ILoaderHeapBackout
{
private:
    CRITSEC_COOKIE    m_CriticalSection;

#ifndef DACCESS_COMPILE
public:
    LoaderHeap(DWORD dwReserveBlockSize,
               DWORD dwCommitBlockSize,
               RangeList *pRangeList = NULL,
               BOOL fMakeExecutable = FALSE,
               BOOL fUnlocked = FALSE
               )
      : UnlockedLoaderHeap(dwReserveBlockSize,
                           dwCommitBlockSize,
                           NULL, 0,
                           pRangeList,
                           fMakeExecutable),
        m_CriticalSection(fUnlocked ? NULL : CreateLoaderHeapLock())
    {
        WRAPPER_NO_CONTRACT;
        m_fExplicitControl = FALSE;
    }

public:
    LoaderHeap(DWORD dwReserveBlockSize,
               DWORD dwCommitBlockSize,
               const BYTE* dwReservedRegionAddress,
               SIZE_T dwReservedRegionSize,
               RangeList *pRangeList = NULL,
               BOOL fMakeExecutable = FALSE,
               BOOL fUnlocked = FALSE
               )
      : UnlockedLoaderHeap(dwReserveBlockSize,
                           dwCommitBlockSize,
                           dwReservedRegionAddress,
                           dwReservedRegionSize,
                           pRangeList,
                           fMakeExecutable),
        m_CriticalSection(fUnlocked ? NULL : CreateLoaderHeapLock())
    {
        WRAPPER_NO_CONTRACT;
        m_fExplicitControl = FALSE;
    }

#endif // DACCESS_COMPILE

    virtual ~LoaderHeap()
    {
        WRAPPER_NO_CONTRACT;

#ifndef DACCESS_COMPILE
        if (m_CriticalSection != NULL)
        {
            ClrDeleteCriticalSection(m_CriticalSection);
        }
#endif // DACCESS_COMPILE
    }



#ifdef _DEBUG
#define AllocMem(dwSize)                  RealAllocMem( (dwSize), __FILE__, __LINE__ )
#define AllocMem_NoThrow(dwSize)          RealAllocMem_NoThrow( (dwSize), __FILE__, __LINE__ )
#else
#define AllocMem(dwSize)                  RealAllocMem( (dwSize) )
#define AllocMem_NoThrow(dwSize)          RealAllocMem_NoThrow( (dwSize) )
#endif

public:
    FORCEINLINE TaggedMemAllocPtr RealAllocMem(S_SIZE_T dwSize
#ifdef _DEBUG
                                  ,__in __in_z const char *szFile
                                  ,int  lineNum
#endif
                  )
    {
        WRAPPER_NO_CONTRACT;

        if(dwSize.IsOverflow()) ThrowOutOfMemory();

        return RealAllocMemUnsafe(dwSize.Value() COMMA_INDEBUG(szFile) COMMA_INDEBUG(lineNum));

    }

    FORCEINLINE TaggedMemAllocPtr RealAllocMem_NoThrow(S_SIZE_T  dwSize
#ifdef _DEBUG
                                           ,__in __in_z const char *szFile
                                           ,int  lineNum
#endif
                  )
    {
        WRAPPER_NO_CONTRACT;

        if(dwSize.IsOverflow()) {
            TaggedMemAllocPtr tmap;
            tmap.m_pMem             = NULL;
            tmap.m_dwRequestedSize  = dwSize.Value();
            tmap.m_pHeap            = this;
            tmap.m_dwExtra          = 0;
#ifdef _DEBUG
            tmap.m_szFile           = szFile;
            tmap.m_lineNum          = lineNum;
#endif

            return tmap;
        }

        return RealAllocMemUnsafe_NoThrow(dwSize.Value() COMMA_INDEBUG(szFile) COMMA_INDEBUG(lineNum));
    }
private:

    TaggedMemAllocPtr RealAllocMemUnsafe(size_t dwSize
#ifdef _DEBUG
                                  ,__in __in_z const char *szFile
                                  ,int  lineNum
#endif
                  )
    {
        WRAPPER_NO_CONTRACT;

#if defined(HOST_OSX) && defined(HOST_ARM64)
        auto jitWriteEnableHolder = PAL_JITWriteEnable(true);
#endif // defined(HOST_OSX) && defined(HOST_ARM64)

        void *pResult;
        TaggedMemAllocPtr tmap;

        CRITSEC_Holder csh(m_CriticalSection);
        pResult = UnlockedAllocMem(dwSize
#ifdef _DEBUG
                                 , szFile
                                 , lineNum
#endif
                                 );
        tmap.m_pMem             = pResult;
        tmap.m_dwRequestedSize  = dwSize;
        tmap.m_pHeap            = this;
        tmap.m_dwExtra          = 0;
#ifdef _DEBUG
        tmap.m_szFile           = szFile;
        tmap.m_lineNum          = lineNum;
#endif

        return tmap;
    }

    TaggedMemAllocPtr RealAllocMemUnsafe_NoThrow(size_t  dwSize
#ifdef _DEBUG
                                           ,__in __in_z const char *szFile
                                           ,int  lineNum
#endif
                  )
    {
        WRAPPER_NO_CONTRACT;

        void *pResult;
        TaggedMemAllocPtr tmap;

        CRITSEC_Holder csh(m_CriticalSection);

        pResult = UnlockedAllocMem_NoThrow(dwSize
#ifdef _DEBUG
                                           , szFile
                                           , lineNum
#endif
                                           );

        tmap.m_pMem             = pResult;
        tmap.m_dwRequestedSize  = dwSize;
        tmap.m_pHeap            = this;
        tmap.m_dwExtra          = 0;
#ifdef _DEBUG
        tmap.m_szFile           = szFile;
        tmap.m_lineNum          = lineNum;
#endif

        return tmap;
    }



#ifdef _DEBUG
#define AllocAlignedMem(dwSize, dwAlign)                RealAllocAlignedMem( (dwSize), (dwAlign), __FILE__, __LINE__)
#define AllocAlignedMem_NoThrow(dwSize, dwAlign)        RealAllocAlignedMem_NoThrow( (dwSize), (dwAlign), __FILE__, __LINE__)
#else
#define AllocAlignedMem(dwSize, dwAlign)                RealAllocAlignedMem( (dwSize), (dwAlign) )
#define AllocAlignedMem_NoThrow(dwSize, dwAlign)        RealAllocAlignedMem_NoThrow( (dwSize), (dwAlign) )
#endif

public:
    TaggedMemAllocPtr RealAllocAlignedMem(size_t  dwRequestedSize
                                         ,size_t  dwAlignment
#ifdef _DEBUG
                                         ,__in __in_z const char *szFile
                                         ,int  lineNum
#endif
                                         )
    {
        WRAPPER_NO_CONTRACT;

#if defined(HOST_OSX) && defined(HOST_ARM64)
        auto jitWriteEnableHolder = PAL_JITWriteEnable(true);
#endif // defined(HOST_OSX) && defined(HOST_ARM64)

        CRITSEC_Holder csh(m_CriticalSection);


        TaggedMemAllocPtr tmap;
        void *pResult;
        size_t dwExtra;

        pResult = UnlockedAllocAlignedMem(dwRequestedSize
                                         ,dwAlignment
                                         ,&dwExtra
#ifdef _DEBUG
                                         ,szFile
                                         ,lineNum
#endif
                                     );

        tmap.m_pMem             = (void*)(((BYTE*)pResult) - dwExtra);
        tmap.m_dwRequestedSize  = dwRequestedSize + dwExtra;
        tmap.m_pHeap            = this;
        tmap.m_dwExtra          = dwExtra;
#ifdef _DEBUG
        tmap.m_szFile           = szFile;
        tmap.m_lineNum          = lineNum;
#endif

        return tmap;
    }


    TaggedMemAllocPtr RealAllocAlignedMem_NoThrow(size_t  dwRequestedSize
                                                 ,size_t  dwAlignment
#ifdef _DEBUG
                                                 ,__in __in_z const char *szFile
                                                 ,int  lineNum
#endif
                                                 )
    {
        WRAPPER_NO_CONTRACT;

        CRITSEC_Holder csh(m_CriticalSection);


        TaggedMemAllocPtr tmap;
        void *pResult;
        size_t dwExtra;

        pResult = UnlockedAllocAlignedMem_NoThrow(dwRequestedSize
                                                 ,dwAlignment
                                                 ,&dwExtra
#ifdef _DEBUG
                                                 ,szFile
                                                 ,lineNum
#endif
                                            );

        _ASSERTE(!(pResult == NULL && dwExtra != 0));

        tmap.m_pMem             = (void*)(((BYTE*)pResult) - dwExtra);
        tmap.m_dwRequestedSize  = dwRequestedSize + dwExtra;
        tmap.m_pHeap            = this;
        tmap.m_dwExtra          = dwExtra;
#ifdef _DEBUG
        tmap.m_szFile           = szFile;
        tmap.m_lineNum          = lineNum;
#endif

        return tmap;
    }


public:
    // This frees memory allocated by AllocMem. It's given this horrible name to emphasize
    // that it's purpose is for error path leak prevention purposes. You shouldn't
    // use LoaderHeap's as general-purpose alloc-free heaps.
    void RealBackoutMem(void *pMem
                        , size_t dwSize
#ifdef _DEBUG
                        , __in __in_z const char *szFile
                        , int lineNum
                        , __in __in_z const char *szAllocFile
                        , int allocLineNum
#endif
                        )
    {
        WRAPPER_NO_CONTRACT;
        CRITSEC_Holder csh(m_CriticalSection);
        UnlockedBackoutMem(pMem
                           , dwSize
#ifdef _DEBUG
                           , szFile
                           , lineNum
                           , szAllocFile
                           , allocLineNum
#endif
                           );
    }

public:
// Extra CallTracing support
#ifdef _DEBUG
    void ClearEvents()
    {
        WRAPPER_NO_CONTRACT;
        CRITSEC_Holder csh(m_CriticalSection);
        UnlockedClearEvents();
    }

    void CompactEvents()
    {
        WRAPPER_NO_CONTRACT;
        CRITSEC_Holder csh(m_CriticalSection);
        UnlockedCompactEvents();
    }

    void PrintEvents()
    {
        WRAPPER_NO_CONTRACT;
        CRITSEC_Holder csh(m_CriticalSection);
        UnlockedPrintEvents();
    }
#endif

};





//===============================================================================
// The ExplicitControlLoaderHeap exposes all the advanced features but
// has no Backout() feature. (If someone wants a backout feature, they need
// to design an appropriate one into this class.)
//
// Caller is responsible for synchronization. ExplicitControlLoaderHeap is
// not multithread safe.
//===============================================================================
typedef DPTR(class ExplicitControlLoaderHeap) PTR_ExplicitControlLoaderHeap;
class ExplicitControlLoaderHeap : public UnlockedLoaderHeap
{
#ifndef DACCESS_COMPILE
public:
    ExplicitControlLoaderHeap(RangeList *pRangeList = NULL,
                              BOOL fMakeExecutable = FALSE
               )
      : UnlockedLoaderHeap(0, 0, NULL, 0,
                           pRangeList,
                           fMakeExecutable)
    {
        WRAPPER_NO_CONTRACT;
        m_fExplicitControl = TRUE;
    }
#endif // DACCESS_COMPILE

public:
    void *RealAllocMem(size_t dwSize
#ifdef _DEBUG
                       ,__in __in_z const char *szFile
                       ,int  lineNum
#endif
                       )
    {
        WRAPPER_NO_CONTRACT;

        void *pResult;

        pResult = UnlockedAllocMem(dwSize
#ifdef _DEBUG
                                   , szFile
                                   , lineNum
#endif
                                   );
        return pResult;
    }

    void *RealAllocMem_NoThrow(size_t dwSize
#ifdef _DEBUG
                               ,__in __in_z const char *szFile
                               ,int  lineNum
#endif
                               )
    {
        WRAPPER_NO_CONTRACT;

        void *pResult;

        pResult = UnlockedAllocMem_NoThrow(dwSize
#ifdef _DEBUG
                                           , szFile
                                           , lineNum
#endif
                                           );
        return pResult;
    }


public:
    void *AllocMemForCode_NoThrow(size_t dwHeaderSize, size_t dwCodeSize, DWORD dwCodeAlignment, size_t dwReserveForJumpStubs)
    {
        WRAPPER_NO_CONTRACT;
        return UnlockedAllocMemForCode_NoThrow(dwHeaderSize, dwCodeSize, dwCodeAlignment, dwReserveForJumpStubs);
    }

    void SetReservedRegion(BYTE* dwReservedRegionAddress, SIZE_T dwReservedRegionSize, BOOL fReleaseMemory)
    {
        WRAPPER_NO_CONTRACT;
        return UnlockedSetReservedRegion(dwReservedRegionAddress, dwReservedRegionSize, fReleaseMemory);
    }

public:
    // number of bytes available in region
    size_t GetReservedBytesFree()
    {
        WRAPPER_NO_CONTRACT;
        return UnlockedGetReservedBytesFree();
    }
};



//==============================================================================
// AllocMemHolder : Allocated memory from LoaderHeap
//
// Old:
//
//   Foo* pFoo = (Foo*)pLoaderHeap->AllocMem(size);
//   pFoo->BackoutMem(pFoo, size)
//
//
// New:
//
//  {
//      AllocMemHolder<Foo> pfoo = pLoaderHeap->AllocMem();
//  } // BackoutMem on out of scope
//
//==============================================================================
template <typename TYPE>
class AllocMemHolder
{
    private:
        TaggedMemAllocPtr m_value;
        BOOL              m_fAcquired;


    //--------------------------------------------------------------------
    // All allowed (and disallowed) ctors here.
    //--------------------------------------------------------------------
    public:
        // Allow the construction "Holder h;"
        AllocMemHolder()
        {
            LIMITED_METHOD_CONTRACT;

            m_value.m_pMem = NULL;
            m_value.m_dwRequestedSize = 0;
            m_value.m_pHeap = 0;
            m_value.m_dwExtra = 0;
#ifdef _DEBUG
            m_value.m_szFile = NULL;
            m_value.m_lineNum = 0;
#endif
            m_fAcquired    = FALSE;
        }

    public:
        // Allow the construction "Holder h = pHeap->AllocMem()"
        AllocMemHolder(const TaggedMemAllocPtr value)
        {
            LIMITED_METHOD_CONTRACT;
            m_value     = value;
            m_fAcquired = TRUE;
        }

    private:
        // Disallow "Holder holder1 = holder2"
        AllocMemHolder(const AllocMemHolder<TYPE> &);


    private:
        // Disallow "Holder holder1 = void*"
        AllocMemHolder(const LPVOID &);

    //--------------------------------------------------------------------
    // Destructor (and the whole point of AllocMemHolder)
    //--------------------------------------------------------------------
    public:
        ~AllocMemHolder()
        {
            WRAPPER_NO_CONTRACT;
            if (m_fAcquired && m_value.m_pMem)
            {
                m_value.m_pHeap->RealBackoutMem(m_value.m_pMem,
                                                m_value.m_dwRequestedSize
#ifdef _DEBUG
                                                ,__FILE__
                                                ,__LINE__
                                                ,m_value.m_szFile
                                                ,m_value.m_lineNum
#endif
                                                );
            }
        }


    //--------------------------------------------------------------------
    // All allowed (and disallowed) assignment operators here.
    //--------------------------------------------------------------------
    public:
        // Reluctantly allow "AllocMemHolder h; ... h = pheap->AllocMem()"
        void operator=(const TaggedMemAllocPtr & value)
        {
            WRAPPER_NO_CONTRACT;
            // However, prevent repeated assignments as that would leak.
            _ASSERTE(m_value.m_pMem == NULL && !m_fAcquired);
            m_value = value;
            m_fAcquired = TRUE;
        }

    private:
        // Disallow "holder == holder2"
        const AllocMemHolder<TYPE> & operator=(const AllocMemHolder<TYPE> &);

    private:
        // Disallow "holder = void*"
        const AllocMemHolder<TYPE> & operator=(const LPVOID &);


    //--------------------------------------------------------------------
    // Operations on the holder itself
    //--------------------------------------------------------------------
    public:
        // Call this when you're ready to take ownership away from the holder.
        void SuppressRelease()
        {
            LIMITED_METHOD_CONTRACT;
            m_fAcquired = FALSE;
        }



    //--------------------------------------------------------------------
    // ... And the smart-pointer stuff so we can drop holders on top
    // of former pointer variables (mostly)
    //--------------------------------------------------------------------
    public:
        // Allow holder to be treated as the underlying pointer type
        operator TYPE* ()
        {
            LIMITED_METHOD_CONTRACT;
            return (TYPE*)(void*)m_value;
        }

    public:
        // Allow holder to be treated as the underlying pointer type
        TYPE* operator->()
        {
            LIMITED_METHOD_CONTRACT;
            return (TYPE*)(void*)m_value;
        }
    public:
        int operator==(TYPE* value)
        {
            LIMITED_METHOD_CONTRACT;
            return ((void*)m_value) == ((void*)value);
        }

    public:
        int operator!=(TYPE* value)
        {
            LIMITED_METHOD_CONTRACT;
            return ((void*)m_value) != ((void*)value);
        }

    public:
        int operator!() const
        {
            LIMITED_METHOD_CONTRACT;
            return m_value.m_pMem == NULL;
        }


};



// This utility helps track loaderheap allocations. Its main purpose
// is to backout allocations in case of an exception.
class AllocMemTracker
{
    public:
        AllocMemTracker();
        ~AllocMemTracker();

        // Tells tracker to store an allocated loaderheap block.
        //
        // Returns the pointer address of block for convenience.
        //
        // Ok to call on failed loaderheap allocation (will just do nothing and propagate the OOM as apropos).
        //
        // If Track fails due to an OOM allocating node space, it will backout the loaderheap block before returning.
        void *Track(TaggedMemAllocPtr tmap);
        void *Track_NoThrow(TaggedMemAllocPtr tmap);

        void SuppressRelease();

    private:
        struct AllocMemTrackerNode
        {
            ILoaderHeapBackout *m_pHeap;
            void            *m_pMem;
            size_t           m_dwRequestedSize;
#ifdef _DEBUG
            const char      *m_szAllocFile;
            int              m_allocLineNum;
#endif
        };

        enum
        {
            kAllocMemTrackerBlockSize =
#ifdef _DEBUG
                                        3
#else
                                       20
#endif
        };

        struct AllocMemTrackerBlock
        {
            AllocMemTrackerBlock    *m_pNext;
            int                      m_nextFree;
            AllocMemTrackerNode      m_Node[kAllocMemTrackerBlockSize];
        };


        AllocMemTrackerBlock        *m_pFirstBlock;
        AllocMemTrackerBlock        m_FirstBlock; // Stack-allocate the first block - "new" the rest.

    protected:
        BOOL                        m_fReleased;

};

#endif // __LoaderHeap_h__

