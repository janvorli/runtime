// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//

#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include "minipal.h"

#if defined(TARGET_OSX) && defined(TARGET_AMD64)
#include <mach/mach.h>
#include <sys/mman.h>
#endif // TARGET_OSX && TARGET_AMD64

#ifndef TARGET_OSX
static const off_t MaxDoubleMappedSize = 2048ULL*1024*1024*1024;
#endif // TARGET_OSX

void* VMToOSInterface::CreateDoubleMemoryMapper()
{
#ifndef TARGET_OSX
    char name[256];
    sprintf(name, "/doublemapper_%d", getpid());
    shm_unlink(name);
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd == -1)
    {
        return (void*)-1;
    }

    shm_unlink(name);

    if (ftruncate(fd, MaxDoubleMappedSize) == -1)
    {
        close(fd);
        return (void*)-1;
    }

    return (void*)(size_t)fd;
#else
    return NULL;
#endif    
}

void VMToOSInterface::DestroyDoubleMemoryMapper(void *mapperHandle)
{
#ifndef TARGET_OSX
    close((int)(size_t)mapperHandle);
#endif
}

void PAL_VirtualReserveFromExecutableMemoryAllocatorWithinRange(
    const void* lpBeginAddress,
    const void* lpEndAddress,
    size_t dwSize);

void* VMToOSInterface::ReserveDoubleMappedMemory(void *mapperHandle, size_t offset, size_t size, const void *rangeStart, const void* rangeEnd)
{
    int fd = (int)(size_t)mapperHandle;

    if (rangeStart != NULL || rangeEnd != NULL)
    {
        void* result = PAL_VirtualReserveFromExecutableMemoryAllocatorWithinRange(rangeStart, rangeEnd, size);
#ifndef TARGET_OSX
        // Create double mappable mapping over the range reserved from the executable memory allocator.
        result = mmap(result, size, PROT_NONE, MAP_SHARED | MAP_FIXED, fd, offset);
#endif // TARGET_OSX
        return result;
    }

#ifndef TARGET_OSX
    void* result = mmap(NULL, size, PROT_NONE, MAP_SHARED, fd, offset);
#else
    void* result = mmap(NULL, size, PROT_NONE, MAP_JIT | MAP_ANON | MAP_PRIVATE, -1, 0);
#endif    
    assert(result != MAP_FAILED);

    return result;
}

void *VMToOSInterface::CommitDoubleMappedMemory(void* pStart, size_t size, bool isExecutable)
{
    if (mprotect(pStart, size, isExecutable ? PROT_EXEC : (PROT_READ | PROT_WRITE)) == -1)
    {
        return NULL;
    }

    return pStart;
}

bool VMToOSInterface::ReleaseDoubleMappedMemory(void* pStart, size_t size)
{
    return munmap(pStart, size) != -1;
}

void* VMToOSInterface::GetRWMapping(void *mapperHandle, void* pStart, size_t offset, size_t size)
{
#ifndef TARGET_OSX    
    int fd = (int)(size_t)mapperHandle;
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
#else // TARGET_OSX
#ifdef TARGET_AMD64
    vm_address_t startRW;
    vm_prot_t curProtection, maxProtection;
    kern_return_t kr = vm_remap(mach_task_self(), &startRW, size, 0, VM_FLAGS_ANYWHERE | VM_FLAGS_RANDOM_ADDR,
                                mach_task_self(), (vm_address_t)pStart, FALSE, &curProtection, &maxProtection, VM_INHERIT_SHARE); // TODO: VM_INHERIT_NONE?

    if (kr != KERN_SUCCESS)
    {
        return NULL;
    }

    int st = mprotect((void*)startRW, size, PROT_READ | PROT_WRITE);
    if (st == -1)
    {
        munmap((void*)startRW, size);
        return NULL;
    }

    return (void*)startRW;
#else // TARGET_AMD64
    // This method should not be called on OSX ARM64
    assert(false);
    return NULL;
#endif // TARGET_AMD64
#endif // TARGET_OSX
}
