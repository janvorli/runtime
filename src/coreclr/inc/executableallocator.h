// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//

//
// Allocator and holders for double mapped executable memory
//

#pragma once

#include "utilcode.h"
#include "ex.h"

// Holder class to map read-execute memory as read-write so that it can be modified without using read-write-execute mapping.
// At the moment the implementation is dummy, returning the same addresses for both cases and expecting them to be read-write-execute.
// The class uses the move semantics to ensure proper unmapping in case of re-assigning of the holder value.
#include <intrin.h>

#define ENABLE_DOUBLE_MAPPING

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
        size_t reuseCount;
        void *user;
    };

    Block* m_firstBlock = NULL;
    MappedBlock* m_firstMappedBlock = NULL;
    HANDLE m_hSharedMemoryFile = NULL;
    uint64_t maxSize = 2048ULL*1024*1024;
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
                // char msg[256];
                // sprintf(msg, "Reusing mapping at %p (maps RX %p, len %zx), for %p\n", b->baseRW, b->baseRX, b->size, baseRX);
                // OutputDebugStringA(msg);

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
                            HIDWORD((int64_t)offset),
                            LODWORD((int64_t)offset),
                            dwSize);
        }

        // We will do one scan from [pMinAddr .. pMaxAddr]
        // First align the tryAddr up to next 64k base address.
        // See docs for VirtualAllocEx and lpAddress and 64k alignment for reasons.
        //
        BYTE *   tryAddr            = (BYTE *)ALIGN_UP((BYTE *)pMinAddr, Granularity());
        bool     virtualQueryFailed = false;
#ifdef DEBUG        
        //bool     faultInjected      = false;
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
                        HIDWORD((int64_t)offset),
                        LODWORD((int64_t)offset),
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
                // if (ShouldInjectFaultInRange())
                // {
                //     // return nullptr (failure)
                //     faultInjected = true;
                //     break;
                // }
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

    void* Commit(void* pStart, size_t size, bool isExecutable)
    {
        return ClrVirtualAlloc(pStart, size, MEM_COMMIT, isExecutable ? PAGE_EXECUTE_READ : PAGE_READWRITE);
    }

//#define DISABLE_UNMAPS

    void Release(void* pRX)
    {
        //ClrVirtualFree(pStart, 0, MEM_RELEASE);
        Block* b = NULL;

        {
            CRITSEC_Holder csh(m_CriticalSection);
            // TODO: remove the mapping, remove it from the allocated list and put the file index and size on a free list so that it can be reused
            //UnmapViewOfFile(pStart);
            Block* pPrevBlock = NULL;
            for (b = m_firstBlock; b != NULL; b = b->next)
            {
                if (pRX == b->baseRX)
                {
                    if (pPrevBlock == NULL)
                    {
                        m_firstBlock = b->next;
                    }
                    else
                    {
                        pPrevBlock->next = b->next;
                    }

                    break;
                }
                pPrevBlock = b;
            }
#ifdef DISABLE_UNMAPS
            // This is likely not correct, but disable unmaps is experimental only, so this prevents reusing mappings that were released
            if (b != NULL)
            {
                MappedBlock* pPrevMappedBlock = NULL;
                MappedBlock* mb = NULL;
                MappedBlock* mbNext = NULL;
                for (mb = m_firstMappedBlock; mb != NULL; mb = mbNext)
                {
                    if (mb->baseRX >= b->baseRX && (BYTE*)mb->baseRX < ((BYTE*)b->baseRX + b->size))
                    {
                        if (pPrevMappedBlock == NULL)
                        {
                            m_firstMappedBlock = mb->next;
                        }
                        else
                        {
                            pPrevMappedBlock->next = mb->next;
                        }
                        mbNext = mb->next;
                        free(mb);

                        // pPrevMappedBlock stays untouched
                    }
                    else
                    {
                        pPrevMappedBlock = mb;
                        mbNext = mb->next;
                    }
                }
            }
#endif            
        }

        if (b != NULL)
        {
            // char msg[256];
            // sprintf(msg, "Releasing %p (length %zx, offset %zx)\n", pRX, b->size, b->offset);
            // OutputDebugStringA(msg);
        
            UnmapViewOfFile(pRX);
        }
        else
        {
            __debugbreak();
        }
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
                return NULL;
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
                            HIDWORD((int64_t)offset),
                            LODWORD((int64_t)offset),
                            size);
        }

        block->baseRX = result;
        block->offset = offset;
        block->size = size;

        // char msg[256];
        // sprintf(msg, "Reserved %p (length %zx, offset %zx)\n", result, size, offset);
        // OutputDebugStringA(msg);


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
                return NULL;
            }
        }
        while (InterlockedCompareExchangeT(&m_freeOffset, newFreeOffset, offset) != offset);

        Block* block = new (nothrow) Block();
        if (block == NULL)
        {
            return NULL;
        }

        void* result = MapViewOfFileEx(m_hSharedMemoryFile,
                        FILE_MAP_EXECUTE | FILE_MAP_READ | FILE_MAP_WRITE, // TODO: can we add the write only for reservations that will be for both data and execution, like the loader allocator? Or should we split the initial allocation to two parts - exe and non-exe?
                        HIDWORD((int64_t)offset),
                        LODWORD((int64_t)offset),
                        size,
                        baseAddressRX);

        if (result == NULL)
        {
            return NULL;
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
    static size_t g_maxRXSearchLength;
    static size_t g_rxSearchLengthSum;
    static size_t g_rxSearchLengthCount;

    void ReportState();


    void UnmapRW(void* pRW)
    {
#ifndef DISABLE_UNMAPS
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
#endif // CROSSGEN_COMPILE
#endif // DISABLE_UNMAPS
    }

    void RecordUser(void* callAddress, bool reused)
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

    void* MapRW(void* pRX, size_t size, void* returnAddress)
    {
// TODO: this was not needed before, check why is it needed now
#ifndef CROSSGEN_COMPILE
        CRITSEC_Holder csh(m_CriticalSection);

//#define DISABLE_MAP_REUSE

#ifndef DISABLE_MAP_REUSE
        void* result = FindMappedBlock(pRX, size);
        if (result != NULL)
        {
            InterlockedIncrement(&g_reusedRwMaps);
            RecordUser(returnAddress, true);
            return result;
        }
#endif
        RecordUser(returnAddress, false);

        size_t searchLength = 0;
        for (Block* b = m_firstBlock; b != NULL; b = b->next)
        {
            if (pRX >= b->baseRX && ((size_t)pRX + size) <= ((size_t)b->baseRX + b->size))
            {
                MEMORY_BASIC_INFORMATION mbi2;
                ClrVirtualQuery(pRX, &mbi2, sizeof(mbi2));
                
                if (mbi2.Protect & PAGE_READWRITE)
                {
                    __debugbreak();
                    return pRX;
                }

                if (searchLength > g_maxRXSearchLength)
                {
                    g_maxRXSearchLength = searchLength;
                }

                g_rxSearchLengthSum += searchLength;
                g_rxSearchLengthCount++;

                // Offset of the RX address in the originally allocated block
                size_t offset = (size_t)pRX - (size_t)b->baseRX;
                // Offset of the RX address that will start the newly mapped block
                size_t mapOffset = ALIGN_DOWN(offset, Granularity());
                // Size of the block we will map
                size_t mapSize = ALIGN_UP(offset - mapOffset + size, Granularity());
                void* pRW = MapViewOfFile(m_hSharedMemoryFile,
                                FILE_MAP_READ | FILE_MAP_WRITE,
                                HIDWORD((int64_t)(b->offset + mapOffset)),
                                LODWORD((int64_t)(b->offset + mapOffset)),
                                mapSize);

                //printf("MapRW: baseRX=%p, RX=%p, RW=%p, mapOffset=%p, size=%p, mapSize=%p\n", b->baseRX, pRX, pRW, (void*)mapOffset, (void*)size, (void*)mapSize);
                // char msg[256];
                // sprintf(msg, "MapRW: baseRX=%p, RX=%p, RW=%p, mapOffset=%p, size=%p, mapSize=%p\n", b->baseRX, pRX, pRW, (void*)mapOffset, (void*)size, (void*)mapSize);
                // OutputDebugStringA(msg);

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
            searchLength++;
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
        __debugbreak();
        // TODO: this is a hack for the preallocated pages that are part of the coreclr module
        InterlockedIncrement(&g_failedRwMaps);
#endif // !CROSSGEN_COMPILE        
        return pRX;
    }

    void* MapRW(void* pRX, size_t size)
    {
        return MapRW(pRX, size, (void*)_ReturnAddress());
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

    void* Commit(void* pStart, size_t size, bool isExecutable)
    {
        return ClrVirtualAlloc(pStart, size, MEM_COMMIT, isExecutable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    }

    void Release(void* pStart)
    {
        ClrVirtualFree(pStart, 0, MEM_RELEASE);
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

template<typename T>
class ExecutableWriterHolder
{
    T *m_addressRX;
    T *m_addressRW;

    void Move(ExecutableWriterHolder& other)
    {
        m_addressRX = other.m_addressRX;
        m_addressRW = other.m_addressRW;
        other.m_addressRX = NULL;
        other.m_addressRW = NULL;
    }

    void Unmap()
    {
#if defined(HOST_OSX) && defined(HOST_ARM64)
        if (m_addressRX != NULL)
        {
            PAL_JitWriteProtect(false);
        }
#else            
        if (m_addressRX != m_addressRW)
        {
            DoubleMappedAllocator::Instance()->UnmapRW(m_addressRW);
        }
#endif
    }

public:
    ExecutableWriterHolder(const ExecutableWriterHolder& other) = delete;
    ExecutableWriterHolder& operator=(const ExecutableWriterHolder& other) = delete;

    ExecutableWriterHolder(ExecutableWriterHolder&& other)
    {
        Move(other);
    }

    ExecutableWriterHolder& operator=(ExecutableWriterHolder&& other)
    {
        Unmap();
        Move(other);
        return *this;
    }

    ExecutableWriterHolder() : m_addressRX(nullptr), m_addressRW(nullptr)
    {
    }

    ExecutableWriterHolder(T* addressRX, size_t size)
    {
        m_addressRX = addressRX;
#if defined(HOST_OSX) && defined(HOST_ARM64)
        m_addressRW = addressRX;
        PAL_JitWriteProtect(true);
#else        
        m_addressRW = (T *)DoubleMappedAllocator::Instance()->MapRW(addressRX, size, (void *)_ReturnAddress());
#endif
    }

    ~ExecutableWriterHolder()
    {
        Unmap();
    }

    // Get the writeable address
    inline T *GetRW() const
    {
        return m_addressRW;
    }
};
