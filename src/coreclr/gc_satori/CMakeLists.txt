set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(FEATURE_STANDALONE_GC)
  add_definitions(-DFEATURE_STANDALONE_GC)
  remove_definitions(-DSTRESS_HEAP)
  remove_definitions(-DWRITE_BARRIER_CHECK)
endif(FEATURE_STANDALONE_GC)

set(GC_SOURCES
  gceventstatus.cpp
  gcconfig.cpp
  gccommon.cpp
  gcscan.cpp
  handletable.cpp
  handletablecore.cpp
  handletablescan.cpp
  objecthandle.cpp
  # softwarewritewatch.cpp
  gchandletable.cpp
  gcload.cpp
  gcbridge.cpp
  handletablecache.cpp
  SatoriGC.cpp
  SatoriHandlePartitioner.cpp
  SatoriHeap.cpp
  SatoriPage.cpp
  SatoriRegion.cpp
  SatoriObject.cpp
  SatoriRegionQueue.cpp
  SatoriAllocator.cpp
  SatoriRecycler.cpp
  SatoriTrimmer.cpp
  SatoriFinalizationQueue.cpp
  SatoriAllocationContext.cpp
  SatoriUtil.cpp
  SatoriLock.cpp
  SatoriWorkList.cpp
  SatoriGate.cpp
)

set(GC_WRITE_BARRIER_SOURCES
  amd64/writebarrier.asm
)

if(CLR_CMAKE_TARGET_ARCH_WASM)
  add_subdirectory(wasm)
elseif(CLR_CMAKE_HOST_UNIX)
  add_subdirectory(unix)
  include(unix/configure.cmake)
else()
  add_subdirectory(windows)
  set (GC_SOURCES
    ${GC_SOURCES}
    windows/Native.rc) # TODO: have a copy
endif()

if (CLR_CMAKE_TARGET_ARCH_ARM64 OR CLR_CMAKE_TARGET_ARCH_AMD64)
#  add_subdirectory(vxsort  ${CMAKE_CURRENT_BINARY_DIR}/vxsort)
endif (CLR_CMAKE_TARGET_ARCH_ARM64 OR CLR_CMAKE_TARGET_ARCH_AMD64)

if (CLR_CMAKE_TARGET_WIN32)
  set(GC_HEADERS
    env/common.h
    env/etmdummy.h
    env/gcenv.base.h
    env/gcenv.ee.h
    env/gcenv.h
    env/gcenv.interlocked.h
    env/gcenv.interlocked.inl
    env/gcenv.object.h
    env/gcenv.os.h
    env/gcenv.structs.h
    env/gcenv.sync.h
    env/gcenv.windows.inl
    env/volatile.h
    # gc.h
    # gcinternal.h
    # gcconfig.h
    # gcbridge.h
    # gcdesc.h
    # gcenv.ee.standalone.inl
    # gcenv.inl
    # gcevent_serializers.h
    # gcevents.h
    # gceventstatus.h
    # gchandletableimpl.h
    # gcimpl.h
    # gcinterface.dac.h
    # gcinterface.ee.h
    # gcinterface.h
    # gcpriv.h
    # gcrecord.h
    # gcscan.h
    # handletable.h
    # handletable.inl
    # handletablepriv.h
    # objecthandle.h
    # softwarewritewatch.h
    SatoriGC.h
    SatoriHandlePartitioner.h
    SatoriHeap.h
    SatoriPage.h
    SatoriRegion.h
    SatoriRegion.inl
    SatoriObject.h
    SatoriObject.inl
    SatoriQueue.h
    SatoriRegionQueue.h
    SatoriAllocator.h
    SatoriRecycler.h
    SatoriTrimmer.h
    SatoriFinalizationQueue.h
    SatoriWorkChunk.h
    SatoriWorkList.h
    SatoriAllocationContext.h
    SatoriUtil.h
    SatoriLock.h
    SatoriGate.h
  )
endif(CLR_CMAKE_TARGET_WIN32)

if(CLR_CMAKE_HOST_WIN32)
  set (GC_LINK_LIBRARIES
    ${STATIC_MT_CRT_LIB}
    ${STATIC_MT_VCRT_LIB}
    kernel32.lib
    Synchronization.lib
    advapi32.lib
    coreclrminipal)
endif(CLR_CMAKE_HOST_WIN32)

set (GC_LINK_LIBRARIES
    ${GC_LINK_LIBRARIES}
    gc_pal_satori
    minipal)

# if(CLR_CMAKE_TARGET_ARCH_ARM64 OR CLR_CMAKE_TARGET_ARCH_AMD64)
#     list(APPEND GC_LINK_LIBRARIES
#         gc_vxsort
#     )
# endif(CLR_CMAKE_TARGET_ARCH_ARM64 OR CLR_CMAKE_TARGET_ARCH_AMD64)


list(APPEND GC_SOURCES ${GC_HEADERS})

convert_to_absolute_path(GC_SOURCES ${GC_SOURCES})

add_library_clr(satori_writebarrier OBJECT ${GC_WRITE_BARRIER_SOURCES})
target_include_directories(satori_writebarrier PRIVATE
  ${ARCH_SOURCES_DIR}
#  ../pal/inc
  ../vm/${ARCH_SOURCES_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../vm/wks)

if(FEATURE_STANDALONE_GC)
  add_library_clr(satorigc SHARED ${GC_SOURCES} $<TARGET_OBJECTS:satori_writebarrier>)
  add_dependencies(satorigc eventing_headers)
  target_link_libraries(satorigc PRIVATE ${GC_LINK_LIBRARIES})
  # target_link_libraries(satorigc PRIVATE gc_dll_wks_descriptor)
  # if(FEATURE_SVR_GC)
  #   target_link_libraries(satorigc PRIVATE gc_dll_svr_descriptor)
  # endif()
  install_clr(TARGETS satorigc DESTINATIONS . COMPONENT runtime)

  add_definitions(-DBUILD_AS_STANDALONE)

  add_definitions(-DFX_VER_INTERNALNAME_STR=satorigc.dll)
  add_definitions(-DVERIFY_HEAP)
  add_definitions(-DGC_CONFIG_DRIVEN)
  if(CLR_CMAKE_HOST_APPLE)
    # The implementation of GCToOSInterface on Apple platforms makes use of non-POSIX
    # pthreads APIs, which by default are not included in the pthreads header
    # unless we define this macro.
    add_definitions(-D_DARWIN_C_SOURCE)
  endif(CLR_CMAKE_HOST_APPLE)

  include_directories(${COMMON_GC})
  include_directories(env)

  # add_definitions(-DGC_DESCRIPTOR)
  # add_subdirectory(datadescriptor)

  install_clr(TARGETS satorigc DESTINATIONS . sharedFramework COMPONENT runtime)
endif(FEATURE_STANDALONE_GC)
