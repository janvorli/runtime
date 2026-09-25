// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#ifndef __GCCONFIG_H__
#define __GCCONFIG_H__

// gcconfig.h - GC configuration management and retrieval.
//
// This file and the GCConfig class are designed to be the primary entry point
// for querying configuration information from within the GC.

// GCConfigStringHolder is a wrapper around a configuration string obtained
// from the EE. Such strings must be disposed using GCToEEInterface::FreeStringConfigValue,
// so this class ensures that is done correctly.
//
// The name is unfortunately a little long, but "ConfigStringHolder" is already taken by the
// EE's config mechanism.
class GCConfigStringHolder
{
private:
    const char* m_str;

public:
    // Constructs a new GCConfigStringHolder around a string obtained from
    // GCToEEInterface::GetStringConfigValue.
    explicit GCConfigStringHolder(const char* str)
      : m_str(str) {}

    // No copy operators - this type cannot be copied.
    GCConfigStringHolder(const GCConfigStringHolder&) = delete;
    GCConfigStringHolder& operator=(const GCConfigStringHolder&) = delete;

    // This type is returned by-value by string config functions, so it
    // requires a move constructor.
    GCConfigStringHolder(GCConfigStringHolder&&) = default;

    // Frees a string config value by delegating to GCToEEInterface::FreeStringConfigValue.
    ~GCConfigStringHolder()
    {
        if (m_str)
        {
            GCToEEInterface::FreeStringConfigValue(m_str);
        }

        m_str = nullptr;
    }

    // Retrieves the wrapped config string.
    const char* Get() const { return m_str; }
};

// Note that the configs starting BGCFLTuningEnabled ending BGCG2RatioStep are for BGC servo
// tuning which is currently an experimental feature which is why I'm not putting any of these
// in clrconfigvalues.h (yet).
// The only public facing configs are BGCFLTuningEnabled and BGCMemGoal.
// Currently the set point is the BGCMemGoal you specify, which indicates what physical memory
// load you'd like GC to maintain. And BGCFLTuningEnabled is to enable/disable this tuning.
//
// The value for BGCG2RatioStep, BGCFLkd and BGCFLSmoothFactor and BGCFLff will be divided by 100.
// The value for BGCMLkp and BGCMLki will be divided by 1000.
// The value for BGCFLkp and BGCFLki will be divided by 1000000.

// Each one of these keys produces a method on GCConfig with the name "Get{name}", where {name}
// is the first parameter of the *_CONFIG macros below.
#define GC_CONFIGURATION_KEYS \
    BOOL_CONFIG  (ConcurrentGC,              "gcConcurrent",              "System.GC.Concurrent",              true,               "Whether we should be using Concurrent GC")                                                \
    BOOL_CONFIG  (ConservativeGC,            "gcConservative",            NULL,                                false,              "Enables/Disables conservative GC")                                                       \
    BOOL_CONFIG  (ForceCompact,              "gcForceCompact",            NULL,                                false,              "When set to true, always do compacting GC")                                              \
    INT_CONFIG   (HeapCount,                 "GCHeapCount",               "System.GC.HeapCount",               0,                  "Specifies the number of server GC heaps")                                                 \
    INT_CONFIG   (LatencyMode,               "GCLatencyMode",             NULL,                                -1,                 "Specifies the GC latency mode - batch, interactive or low latency (note that the same "   \
                                                                                                                                           "thing can be specified via API which is the supported way")                             \
    BOOL_CONFIG  (GCNumaAware,               "GCNumaAware",               NULL,                                true,               "Enables numa allocations in the GC")                                                     \
    BOOL_CONFIG  (GCCpuGroup,                "GCCpuGroup",                "System.GC.CpuGroup",                false,              "Enables CPU groups in the GC")                                                            \
    BOOL_CONFIG  (RelocatingInGen1,          "gcRelocatingGen1",          NULL,                                true,               "Specifies whether GC can relocate objects in Gen1 GC")                                   \
    BOOL_CONFIG  (RelocatingInGen2,          "gcRelocatingGen2",          NULL,                                true,               "Specifies whether GC can relocate objects in Gen2 GC")                                   \
    INT_CONFIG   (ParallelGC,                "gcParallel",                NULL,                                -1,                 "Specifies max number of addtional GC threads. 0 - no helpers, -1 - default")             \
    BOOL_CONFIG  (Gen0GC,                    "gcGen0",                    NULL,                                true,               "Specifies whether Gen0 GC can be performed")                                             \
    BOOL_CONFIG  (Gen1GC,                    "gcGen1",                    NULL,                                true,               "Specifies whether Gen1 GC can be performed")                                             \
    BOOL_CONFIG  (UseTHP,                    "gcTHP",                     NULL,                                true,               "Specifies whether Transparent Huge Pages can be used. (Linux only)")                     \
    BOOL_CONFIG  (TrimmigGC,                 "gcTrim",                    NULL,                                true,               "Specifies whether background trimming is enabled")                                       \
    BOOL_CONFIG  (PacingGC,                  "gcPace",                    NULL,                                true,               "Specifies whether allocation pacing is enabled")                                         \
    INT_CONFIG   (GCRate,                    "gcRate",                    NULL,                                -1,                 "Specifies soft min limit for time between GCs in milliseconds. -1 - default")            \
    INT_CONFIG   (GCSpin,                    "gcSpin",                    NULL,                                -1,                 "Spin")                                                                                   \
    INT_CONFIG   (Gen2Target,                "gcGen2Target",              NULL,                                -1,                 "Specifies target for Gen2 GC (in terms of % of the last known size)")                    \
    INT_CONFIG   (Gen1Target,                "gcGen1Target",              NULL,                                -1,                 "Specifies target for Gen1 GC (in terms of % of the last known size)")                    \

// This class is responsible for retreiving configuration information
// for how the GC should operate.
class GCConfig
{
#define BOOL_CONFIG(name, unused_private_key, unused_public_key, unused_default, unused_doc) \
  public: static bool Get##name();                                \
  public: static bool Get##name(bool defaultValue);               \
  public: static void Set##name(bool value);                      \
  private: static bool s_##name;                                  \
  private: static bool s_##name##Provided;                        \
  private: static bool s_Updated##name;
  
#define INT_CONFIG(name, unused_private_key, unused_public_key, unused_default, unused_doc) \
  public: static int64_t Get##name();                            \
  public: static int64_t Get##name(int64_t defaultValue);        \
  public: static void Set##name(int64_t value);                  \
  private: static int64_t s_##name;                              \
  private: static bool s_##name##Provided;                       \
  private: static int64_t s_Updated##name;                       \

#define STRING_CONFIG(name, unused_private_key, unused_public_key, unused_doc) \
  public: static GCConfigStringHolder Get##name();

GC_CONFIGURATION_KEYS

#undef BOOL_CONFIG
#undef INT_CONFIG
#undef STRING_CONFIG

public:

  static void RefreshHeapHardLimitSettings();

  static void EnumerateConfigurationValues(void* context, ConfigurationValueFunc configurationValueFunc);

// Flags that may inhabit the number returned for the HeapVerifyLevel config option.
// Keep this in sync with vm\eeconfig.h if this ever changes.
enum HeapVerifyFlags {
    HEAPVERIFY_NONE             = 0,
    HEAPVERIFY_GC               = 1,   // Verify the heap at beginning and end of GC
    HEAPVERIFY_BARRIERCHECK     = 2,   // Verify the assignment operations correctly went through write barrier code
    HEAPVERIFY_SYNCBLK          = 4,   // Verify sync block scanning

    // the following options can be used to mitigate some of the overhead introduced
    // by heap verification.  some options might cause heap verifiction to be less
    // effective depending on the scenario.

    HEAPVERIFY_NO_RANGE_CHECKS  = 0x10,   // Excludes checking if an OBJECTREF is within the bounds of the managed heap
    HEAPVERIFY_NO_MEM_FILL      = 0x20,   // Excludes filling unused segment portions with fill pattern
    HEAPVERIFY_POST_GC_ONLY     = 0x40,   // Performs heap verification post-GCs only (instead of before and after each GC)
    HEAPVERIFY_DEEP_ON_COMPACT  = 0x80    // Performs deep object verfication only on compacting GCs.
};

enum WriteBarrierFlavor
{
    WRITE_BARRIER_DEFAULT = 0,
    WRITE_BARRIER_REGION_BIT = 1,
    WRITE_BARRIER_REGION_BYTE = 2,
    WRITE_BARRIER_SERVER = 3,
};

// Initializes the GCConfig subsystem. Must be called before accessing any
// configuration information.
static void Initialize();

};

bool ParseGCHeapAffinitizeRanges(const char* cpu_index_ranges, AffinitySet* config_affinity_set, uintptr_t& config_affinity_mask);

#endif // __GCCONFIG_H__
