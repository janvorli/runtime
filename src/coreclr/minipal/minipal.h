// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
#include <stddef.h>

class VMToOSInterface
{
private:
    ~VMToOSInterface() {}
public:
    static bool CreateDoubleMemoryMapper(void **pHandle, size_t *pMaxExecutableCodeSize);
    static void DestroyDoubleMemoryMapper(void *mapperHandle);
    static void* ReserveDoubleMappedMemory(void *mapperHandle, size_t offset, size_t size, const void *rangeStart, const void* rangeEnd);
    static void* CommitDoubleMappedMemory(void* pStart, size_t size, bool isExecutable);
    static bool ReleaseDoubleMappedMemory(void *mapperHandle, void* pStart, size_t offset, size_t size);
    static void* GetRWMapping(void *mapperHandle, void* pStart, size_t offset, size_t size);
    static bool ReleaseRWMapping(void* pStart, size_t size);
};
