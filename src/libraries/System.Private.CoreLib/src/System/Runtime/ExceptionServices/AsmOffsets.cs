// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
internal static class AsmOffsets
{
#if DEBUG

#if TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__REGDISPLAY = 0x1a90;
    public const int OFFSETOF__REGDISPLAY__SP = 0x1a78;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x1a80;
#elif TARGET_ARM64
    public const int SIZEOF__REGDISPLAY = 0x940;
    public const int OFFSETOF__REGDISPLAY__SP = 0x898;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x8a0;
#elif TARGET_ARM
    public const int SIZEOF__REGDISPLAY = 0x410;
    public const int OFFSETOF__REGDISPLAY__SP = 0x3ec;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x3f0;
#endif

#else // TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__REGDISPLAY = 0xbf0;
    public const int OFFSETOF__REGDISPLAY__SP = 0xbd8;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0xbe0;
#elif TARGET_ARM64
#elif TARGET_ARM
#endif

#endif // TARGET_UNIX

#if HOST_64BIT
    public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x8;
    public const int SIZEOF__StackFrameIterator = 0x370;
#else // HOST_64BIT
    public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x4;
    public const int SIZEOF__StackFrameIterator = 0x2d0;
#endif // HOST_64BIT

#else // DEBUG

#if TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__REGDISPLAY = 0x1a90;
    public const int OFFSETOF__REGDISPLAY__SP = 0x1a78;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x1a80;
#elif TARGET_ARM64
    public const int SIZEOF__REGDISPLAY = 0x940;
    public const int OFFSETOF__REGDISPLAY__SP = 0x898;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x8a0;
#elif TARGET_ARM
    public const int SIZEOF__REGDISPLAY = 0x408;
    public const int OFFSETOF__REGDISPLAY__SP = 0x3e8;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0x3ec;
#endif

#else // TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__REGDISPLAY = 0xbe0;
    public const int OFFSETOF__REGDISPLAY__SP = 0xbd0;
    public const int OFFSETOF__REGDISPLAY__ControlPC = 0xbd8;
#elif TARGET_ARM64
#elif TARGET_ARM
#endif

#endif // TARGET_UNIX

#if HOST_64BIT
    public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x8;
    public const int SIZEOF__StackFrameIterator = 0x360;
#else // HOST_64BIT
    public const int OFFSETOF__REGDISPLAY__m_pCurrentContext = 0x4;
    public const int SIZEOF__StackFrameIterator = 0x2c8;
#endif // HOST_64BIT

#endif // DEBUG

#if HOST_64BIT
    public const int SIZEOF__EHEnum = 0x20;
    public const int OFFSETOF__StackFrameIterator__m_pRegDisplay = 0x228;
#else // HOST_64BIT
    public const int SIZEOF__EHEnum = 0x10;
    public const int OFFSETOF__StackFrameIterator__m_pRegDisplay = 0x218;
#endif // HOST_64BIT

#if TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0xc20;
#elif TARGET_ARM64
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x390;
#elif TARGET_ARM
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x1a0;
#endif

#else // TARGET_UNIX

#if TARGET_AMD64
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x4d0;
#elif TARGET_ARM64
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x4d0;
#elif TARGET_ARM
    public const int SIZEOF__PAL_LIMITED_CONTEXT = 0x4d0;
#endif

#endif // TARGET_UNIx

#if TARGET_AMD64
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__IP = 0xf8;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__SP = 0x98;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__FP = 0xa0;
#elif TARGET_ARM64
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__IP = 0x108;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__SP = 0x100;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__FP = 0xf0;
#elif TARGET_ARM
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__IP = 0x40;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__SP = 0x56;
    public const int OFFSETOF__PAL_LIMITED_CONTEXT__FP = 0x30;
#endif


#if HOST_64BIT
    public const int OFFSETOF__ExInfo__m_pPrevExInfo = 0;
    public const int OFFSETOF__ExInfo__m_pExContext = 8;
    public const int OFFSETOF__ExInfo__m_exception = 0x10;

    public const int OFFSETOF__ExInfo__m_kind = 0x18;
    public const int OFFSETOF__ExInfo__m_passNumber = 0x19;

    // BEWARE: This field is used by the stackwalker to know if the dispatch code has reached the
    //         point at which a handler is called.  In other words, it serves as an "is a handler
    //         active" state where '_idxCurClause == MaxTryRegionIdx' means 'no'.
    public const int OFFSETOF__ExInfo__m_idxCurClause = 0x1c;
    public const int OFFSETOF__ExInfo__m_frameIter = 0x20;

    public const int OFFSETOF__ExInfo__m_notifyDebuggerSP = OFFSETOF__ExInfo__m_frameIter + SIZEOF__StackFrameIterator;
#else // HOST_64BIT
    public const int OFFSETOF__ExInfo__m_pPrevExInfo = 0;
    public const int OFFSETOF__ExInfo__m_pExContext = 4;
    public const int OFFSETOF__ExInfo__m_exception = 8;

    public const int OFFSETOF__ExInfo__m_kind = 0xC;
    public const int OFFSETOF__ExInfo__m_passNumber = 0xD;

    // BEWARE: This field is used by the stackwalker to know if the dispatch code has reached the
    //         point at which a handler is called.  In other words, it serves as an "is a handler
    //         active" state where '_idxCurClause == MaxTryRegionIdx' means 'no'.
    public const int OFFSETOF__ExInfo__m_idxCurClause = 0x10;
    public const int OFFSETOF__ExInfo__m_frameIter = 0x18;

    public const int OFFSETOF__ExInfo__m_notifyDebuggerSP = OFFSETOF__ExInfo__m_frameIter + SIZEOF__StackFrameIterator;
#endif // HOST_64BIT
}
