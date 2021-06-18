// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//

//
// Allocator and holders for double mapped executable memory
//

#pragma once

#include "utilcode.h"
#include "ex.h"

#include "minipal.h"

#ifndef DACCESS_COMPILE
class ExecutableAllocator
{
    static ExecutableAllocator* g_instance;

    struct BlockRX
    {
        BlockRX* next;
        void* baseRX;
        size_t size;
        size_t offset;
    };

    struct BlockRW
    {
        BlockRW* next;
        void* baseRW;
        void* baseRX;
        size_t size;
        size_t refCount;
    };

    BlockRX* m_pFirstBlockRX = NULL;
    BlockRX* m_pFirstFreeBlockRX = NULL;

    BlockRW* m_pFirstBlockRW = NULL;

    void *m_doubleMemoryMapperHandle = NULL;
    uint64_t maxSize = 2048ULL*1024*1024;
    size_t m_freeOffset = 0;

    CRITSEC_COOKIE m_CriticalSection;

#if USE_UPPER_ADDRESS
    static BYTE * s_CodeMinAddr;        // Preferred region to allocate the code in.
    static BYTE * s_CodeMaxAddr;
    static BYTE * s_CodeAllocStart;
    static BYTE * s_CodeAllocHint;      // Next address to try to allocate for code in the preferred region.
#endif // USE_UPPER_ADDRESS

    BlockRW* m_cachedMapping = NULL;

    static bool g_isWXorXEnabled;

    void UpdateCachedMapping(BlockRW *b);

    void* FindRWBlock(void* baseRX, size_t size);

    bool AddRWBlock(void* baseRW, void* baseRX, size_t size);

    bool RemoveRWBlock(void* pRW, void** pUnmapAddress, size_t* pUnmapSize);

    BlockRX* FindBestFreeBlock(size_t size);

    size_t Granularity()
    {
        return 64 * 1024;
    }

    BlockRX* AllocateBlock(size_t size, bool* pIsFreeBlock);
    void BackoutBlock(BlockRX* pBlock, bool isFreeBlock);

    bool AllocateOffset(size_t *pOffset, size_t size);
    void AddBlockToList(BlockRX *pBlock);

    //
    // Return true if double mapping is enabled
    //
    static bool IsDoubleMappingEnabled();

public:

    static ExecutableAllocator* Instance();
    static void StaticInitialize();

    //
    // Return true if W^X is enabled
    //
    static bool IsWXORXEnabled();

    //
    // Use this function to initialize the s_CodeAllocHint
    // during startup. base is runtime .dll base address,
    // size is runtime .dll virtual size.
    //
    static void InitCodeAllocHint(size_t base, size_t size, int randomPageOffset);

    //
    // Use this function to reset the s_CodeAllocHint
    // after unloading an AppDomain
    //
    static void ResetCodeAllocHint();

    //
    // Returns TRUE if p is located in near clr.dll that allows us
    // to use rel32 IP-relative addressing modes.
    //
    static bool IsPreferredExecutableRange(void * p);

    bool Initialize();

    ~ExecutableAllocator();

    // Reserve the specified amount of virtual address space for executable mapping.
    void* Reserve(size_t size);

    // Reserve the specified amount of virtual address space for executable mapping.
    // The reserved range must be within the loAddress and hiAddress. If it is not
    // possible to reserve memory in such range, the method returns NULL.
    void* ReserveWithinRange(size_t size, const void* loAddress, const void* hiAddress);

    // Reserve the specified amount of virtual address space for executable mapping
    // exactly at the given address.
    void* ReserveAt(void* baseAddressRX, size_t size);

    void* Commit(void* pStart, size_t size, bool isExecutable);

    void Release(void* pRX);

    void* MapRW(void* pRX, size_t size);
    void UnmapRW(void* pRW);
};

// Holder class to map read-execute memory as read-write so that it can be modified without using read-write-execute mapping.
// At the moment the implementation is dummy, returning the same addresses for both cases and expecting them to be read-write-execute.
// The class uses the move semantics to ensure proper unmapping in case of re-assigning of the holder value.
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
#if defined(HOST_OSX) && defined(HOST_ARM64) && !defined(DACCESS_COMPILE)
        if (m_addressRX != NULL)
        {
            PAL_JitWriteProtect(false);
        }
#else
        if (m_addressRX != m_addressRW)
        {
            ExecutableAllocator::Instance()->UnmapRW((void*)m_addressRW);
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
        m_addressRW = (T *)ExecutableAllocator::Instance()->MapRW((void*)addressRX, size);
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

#endif // !DACCESS_COMPILE
