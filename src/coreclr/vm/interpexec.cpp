// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include <math.h>

#ifdef FEATURE_INTERPRETER

#include "interpexec.h"

#ifdef _DEBUG
#undef assert
#define assert(p) _ASSERTE(p)
#endif // _DEBUG

extern "C" void Load_Stack();

#ifdef TARGET_AMD64

#ifdef TARGET_WINDOWS
extern "C" void Load_RCX();
extern "C" void Load_RCX_RDX();
extern "C" void Load_RCX_RDX_R8();
extern "C" void Load_RCX_RDX_R8_R9();
extern "C" void Load_RDX();
extern "C" void Load_RDX_R8();
extern "C" void Load_RDX_R8_R9();
extern "C" void Load_R8();
extern "C" void Load_R8_R9();
extern "C" void Load_R9();
extern "C" void Load_XMM0();
extern "C" void Load_XMM0_XMM1();
extern "C" void Load_XMM0_XMM1_XMM2();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3();
extern "C" void Load_XMM1();
extern "C" void Load_XMM1_XMM2();
extern "C" void Load_XMM1_XMM2_XMM3();
extern "C" void Load_XMM2();
extern "C" void Load_XMM2_XMM3();
extern "C" void Load_XMM3();
extern "C" void Load_Ref_RCX();
extern "C" void Load_Ref_RDX();
extern "C" void Load_Ref_R8();
extern "C" void Load_Ref_R9();

PCODE GPRegsRoutines[] =
{
    (PCODE)Load_RCX,            // 00
    (PCODE)Load_RCX_RDX,        // 01
    (PCODE)Load_RCX_RDX_R8,     // 02
    (PCODE)Load_RCX_RDX_R8_R9,  // 03
    (PCODE)0,                   // 10    
    (PCODE)Load_RDX,            // 11
    (PCODE)Load_RDX_R8,         // 12
    (PCODE)Load_RDX_R8_R9,      // 13
    (PCODE)0,                   // 20
    (PCODE)0,                   // 21
    (PCODE)Load_R8,             // 22
    (PCODE)Load_R8_R9,          // 23
    (PCODE)0,                   // 30
    (PCODE)0,                   // 31
    (PCODE)0,                   // 32
    (PCODE)Load_R9              // 33
};

PCODE GPRegsRefRoutines[] =
{
    (PCODE)Load_Ref_RCX,        // 0
    (PCODE)Load_Ref_RDX,        // 1
    (PCODE)Load_Ref_R8,         // 2
    (PCODE)Load_Ref_R9,         // 3
};

PCODE FPRegsRoutines[] =
{
    (PCODE)Load_XMM0,                // 00
    (PCODE)Load_XMM0_XMM1,           // 01
    (PCODE)Load_XMM0_XMM1_XMM2,      // 02
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3, // 03
    (PCODE)0,                        // 10    
    (PCODE)Load_XMM1,                // 11
    (PCODE)Load_XMM1_XMM2,           // 12
    (PCODE)Load_XMM1_XMM2_XMM3,      // 13
    (PCODE)0,                        // 20
    (PCODE)0,                        // 21
    (PCODE)Load_XMM2,                // 22
    (PCODE)Load_XMM2_XMM3,           // 23
    (PCODE)0,                        // 30
    (PCODE)0,                        // 31
    (PCODE)0,                        // 32
    (PCODE)Load_XMM3                 // 33
};

#else // TARGET_WINDOWS

extern "C" void Load_RDI();
extern "C" void Load_RDI_RSI();
extern "C" void Load_RDI_RSI_RDX();
extern "C" void Load_RDI_RSI_RDX_RCX();
extern "C" void Load_RDI_RSI_RDX_RCX_R8();
extern "C" void Load_RDI_RSI_RDX_RCX_R8_R9();
extern "C" void Load_RSI();
extern "C" void Load_RSI_RDX();
extern "C" void Load_RSI_RDX_RCX();
extern "C" void Load_RSI_RDX_RCX_R8();
extern "C" void Load_RSI_RDX_RCX_R8_R9();
extern "C" void Load_RDX();
extern "C" void Load_RDX_RCX();
extern "C" void Load_RDX_RCX_R8();
extern "C" void Load_RDX_RCX_R8_R9();
extern "C" void Load_RCX();
extern "C" void Load_RCX_R8();
extern "C" void Load_RCX_R8_R9();
extern "C" void Load_R8();
extern "C" void Load_R8_R9();
extern "C" void Load_R9();

extern "C" void Load_Ref_RDI();
extern "C" void Load_Ref_RSI();
extern "C" void Load_Ref_RDX();
extern "C" void Load_Ref_RCX();
extern "C" void Load_Ref_R8();
extern "C" void Load_Ref_R9();

PCODE GPRegsRoutines[] =
{
    (PCODE)Load_RDI,                    // 00
    (PCODE)Load_RDI_RSI,                // 01
    (PCODE)Load_RDI_RSI_RDX,            // 02
    (PCODE)Load_RDI_RSI_RDX_RCX,        // 03
    (PCODE)Load_RDI_RSI_RDX_RCX_R8,     // 04
    (PCODE)Load_RDI_RSI_RDX_RCX_R8_R9,  // 05
    (PCODE)0,                           // 10    
    (PCODE)Load_RSI,                    // 11
    (PCODE)Load_RSI_RDX,                // 12
    (PCODE)Load_RSI_RDX_RCX,            // 13
    (PCODE)Load_RSI_RDX_RCX_R8,         // 14
    (PCODE)Load_RSI_RDX_RCX_R8_R9,      // 15
    (PCODE)0,                           // 20
    (PCODE)0,                           // 21
    (PCODE)Load_RDX,                    // 22
    (PCODE)Load_RDX_RCX,                // 23
    (PCODE)Load_RDX_RCX_R8,             // 24
    (PCODE)Load_RDX_RCX_R8_R9,          // 25
    (PCODE)0,                           // 30
    (PCODE)0,                           // 31
    (PCODE)0,                           // 32
    (PCODE)Load_RCX,                    // 33
    (PCODE)Load_RCX_R8,                 // 34
    (PCODE)Load_RCX_R8_R9,              // 35
    (PCODE)0,                           // 40
    (PCODE)0,                           // 41
    (PCODE)0,                           // 42
    (PCODE)0,                           // 43
    (PCODE)Load_R8,                     // 44
    (PCODE)Load_R8_R9,                  // 45
    (PCODE)0,                           // 50
    (PCODE)0,                           // 51
    (PCODE)0,                           // 52
    (PCODE)0,                           // 53
    (PCODE)0,                           // 54
    (PCODE)Load_R9                      // 55
};

PCODE GPRegsRefRoutines[] =
{
    (PCODE)Load_Ref_RDI,        // 0
    (PCODE)Load_Ref_RSI,        // 1
    (PCODE)Load_Ref_RDX,        // 2
    (PCODE)Load_Ref_RCX,        // 3
    (PCODE)Load_Ref_R8,         // 4
    (PCODE)Load_Ref_R9         // 5
};

extern "C" void Load_XMM0();
extern "C" void Load_XMM0_XMM1();
extern "C" void Load_XMM0_XMM1_XMM2();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3_XMM4();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6();
extern "C" void Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7();
extern "C" void Load_XMM1();
extern "C" void Load_XMM1_XMM2();
extern "C" void Load_XMM1_XMM2_XMM3();
extern "C" void Load_XMM1_XMM2_XMM3_XMM4();
extern "C" void Load_XMM1_XMM2_XMM3_XMM4_XMM5();
extern "C" void Load_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6();
extern "C" void Load_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7();
extern "C" void Load_XMM2();
extern "C" void Load_XMM2_XMM3();
extern "C" void Load_XMM2_XMM3_XMM4();
extern "C" void Load_XMM2_XMM3_XMM4_XMM5();
extern "C" void Load_XMM2_XMM3_XMM4_XMM5_XMM6();
extern "C" void Load_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7();
extern "C" void Load_XMM3();
extern "C" void Load_XMM3_XMM4();
extern "C" void Load_XMM3_XMM4_XMM5();
extern "C" void Load_XMM3_XMM4_XMM5_XMM6();
extern "C" void Load_XMM3_XMM4_XMM5_XMM6_XMM7();
extern "C" void Load_XMM4();
extern "C" void Load_XMM4_XMM5();
extern "C" void Load_XMM4_XMM5_XMM6();
extern "C" void Load_XMM4_XMM5_XMM6_XMM7();
extern "C" void Load_XMM5();
extern "C" void Load_XMM5_XMM6();
extern "C" void Load_XMM5_XMM6_XMM7();
extern "C" void Load_XMM6();
extern "C" void Load_XMM6_XMM7();
extern "C" void Load_XMM7();

PCODE FPRegsRoutines[] =
{
    (PCODE)Load_XMM0,                                   // 00
    (PCODE)Load_XMM0_XMM1,                              // 01
    (PCODE)Load_XMM0_XMM1_XMM2,                         // 02
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3,                    // 03
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3_XMM4,               // 04
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5,          // 05
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6,     // 06
    (PCODE)Load_XMM0_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7,// 07
    (PCODE)0,                                           // 10    
    (PCODE)Load_XMM1,                                   // 11
    (PCODE)Load_XMM1_XMM2,                              // 12
    (PCODE)Load_XMM1_XMM2_XMM3,                         // 13
    (PCODE)Load_XMM1_XMM2_XMM3_XMM4,                    // 14
    (PCODE)Load_XMM1_XMM2_XMM3_XMM4_XMM5,               // 15
    (PCODE)Load_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6,          // 16
    (PCODE)Load_XMM1_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7,     // 17
    (PCODE)0,                                           // 20
    (PCODE)0,                                           // 21
    (PCODE)Load_XMM2,                                   // 22
    (PCODE)Load_XMM2_XMM3,                              // 23
    (PCODE)Load_XMM2_XMM3_XMM4,                         // 24
    (PCODE)Load_XMM2_XMM3_XMM4_XMM5,                    // 25
    (PCODE)Load_XMM2_XMM3_XMM4_XMM5_XMM6,               // 26
    (PCODE)Load_XMM2_XMM3_XMM4_XMM5_XMM6_XMM7,          // 27
    (PCODE)0,                                           // 30
    (PCODE)0,                                           // 31
    (PCODE)0,                                           // 32
    (PCODE)Load_XMM3,                                   // 33
    (PCODE)Load_XMM3_XMM4,                              // 34
    (PCODE)Load_XMM3_XMM4_XMM5,                         // 35
    (PCODE)Load_XMM3_XMM4_XMM5_XMM6,                    // 36
    (PCODE)Load_XMM3_XMM4_XMM5_XMM6_XMM7,               // 37
    (PCODE)0,                                           // 40
    (PCODE)0,                                           // 41
    (PCODE)0,                                           // 42
    (PCODE)0,                                           // 43
    (PCODE)Load_XMM4,                                   // 44
    (PCODE)Load_XMM4_XMM5,                              // 45
    (PCODE)Load_XMM4_XMM5_XMM6,                         // 46
    (PCODE)Load_XMM4_XMM5_XMM6_XMM7,                    // 47
    (PCODE)0,                                           // 50
    (PCODE)0,                                           // 51
    (PCODE)0,                                           // 52
    (PCODE)0,                                           // 53
    (PCODE)0,                                           // 54
    (PCODE)Load_XMM5,                                   // 55
    (PCODE)Load_XMM5_XMM6,                              // 56
    (PCODE)Load_XMM5_XMM6_XMM7,                         // 57
    (PCODE)0,                                           // 60
    (PCODE)0,                                           // 61
    (PCODE)0,                                           // 62
    (PCODE)0,                                           // 63
    (PCODE)0,                                           // 64
    (PCODE)0,                                           // 65
    (PCODE)Load_XMM6,                                   // 66
    (PCODE)Load_XMM6_XMM7,                              // 67
    (PCODE)0,                                           // 70
    (PCODE)0,                                           // 71
    (PCODE)0,                                           // 72
    (PCODE)0,                                           // 73
    (PCODE)0,                                           // 74
    (PCODE)0,                                           // 75
    (PCODE)0,                                           // 76
    (PCODE)Load_XMM7                                    // 77
};

#endif // TARGET_WINDOWS

#endif // TARGET_AMD64

#ifdef TARGET_ARM64

extern "C" void Load_X0();
extern "C" void Load_X0_X1();
extern "C" void Load_X0_X1_X2();
extern "C" void Load_X0_X1_X2_X3();
extern "C" void Load_X0_X1_X2_X3_X4();
extern "C" void Load_X0_X1_X2_X3_X4_X5();
extern "C" void Load_X0_X1_X2_X3_X4_X5_X6();
extern "C" void Load_X0_X1_X2_X3_X4_X5_X6_X7();
extern "C" void Load_X1();
extern "C" void Load_X1_X2();
extern "C" void Load_X1_X2_X3();
extern "C" void Load_X1_X2_X3_X4();
extern "C" void Load_X1_X2_X3_X4_X5();
extern "C" void Load_X1_X2_X3_X4_X5_X6();
extern "C" void Load_X1_X2_X3_X4_X5_X6_X7();
extern "C" void Load_X2();
extern "C" void Load_X2_X3();
extern "C" void Load_X2_X3_X4();
extern "C" void Load_X2_X3_X4_X5();
extern "C" void Load_X2_X3_X4_X5_X6();
extern "C" void Load_X2_X3_X4_X5_X6_X7();
extern "C" void Load_X3();
extern "C" void Load_X3_X4();
extern "C" void Load_X3_X4_X5();
extern "C" void Load_X3_X4_X5_X6();
extern "C" void Load_X3_X4_X5_X6_X7();
extern "C" void Load_X4();
extern "C" void Load_X4_X5();
extern "C" void Load_X4_X5_X6();
extern "C" void Load_X4_X5_X6_X7();
extern "C" void Load_X5();
extern "C" void Load_X5_X6();
extern "C" void Load_X5_X6_X7();
extern "C" void Load_X6();
extern "C" void Load_X6_X7();
extern "C" void Load_X7();

PCODE GPRegsRoutines[] =
{
    (PCODE)Load_X0,                         // 00
    (PCODE)Load_X0_X1,                      // 01
    (PCODE)Load_X0_X1_X2,                   // 02
    (PCODE)Load_X0_X1_X2_X3,                // 03
    (PCODE)Load_X0_X1_X2_X3_X4,             // 04
    (PCODE)Load_X0_X1_X2_X3_X4_X5,          // 05
    (PCODE)Load_X0_X1_X2_X3_X4_X5_X6,       // 06
    (PCODE)Load_X0_X1_X2_X3_X4_X5_X6_X7,    // 07
    (PCODE)0,                               // 10    
    (PCODE)Load_X1,                         // 11
    (PCODE)Load_X1_X2,                      // 12
    (PCODE)Load_X1_X2_X3,                   // 13
    (PCODE)Load_X1_X2_X3_X4,                // 14
    (PCODE)Load_X1_X2_X3_X4_X5,             // 15
    (PCODE)Load_X1_X2_X3_X4_X5_X6,          // 16
    (PCODE)Load_X1_X2_X3_X4_X5_X6_X7,       // 17
    (PCODE)0,                               // 20
    (PCODE)0,                               // 21
    (PCODE)Load_X2,                         // 22
    (PCODE)Load_X2_X3,                      // 23
    (PCODE)Load_X2_X3_X4,                   // 24
    (PCODE)Load_X2_X3_X4_X5,                // 25
    (PCODE)Load_X2_X3_X4_X5_X6,             // 26
    (PCODE)Load_X2_X3_X4_X5_X6_X7,          // 27
    (PCODE)0,                               // 30
    (PCODE)0,                               // 31   
    (PCODE)0,                               // 32
    (PCODE)Load_X3,                          // 33
    (PCODE)Load_X3_X4,                      // 34
    (PCODE)Load_X3_X4_X5,                   // 35
    (PCODE)Load_X3_X4_X5_X6,                // 36
    (PCODE)Load_X3_X4_X5_X6_X7,             // 37
    (PCODE)0,                               // 40
    (PCODE)0,                               // 41
    (PCODE)0,                               // 42
    (PCODE)0,                               // 43
    (PCODE)Load_X4,                         // 44
    (PCODE)Load_X4_X5,                      // 45
    (PCODE)Load_X4_X5_X6,                   // 46
    (PCODE)Load_X4_X5_X6_X7,                // 47
    (PCODE)0,                               // 50
    (PCODE)0,                               // 51
    (PCODE)0,                               // 52
    (PCODE)0,                               // 53
    (PCODE)0,                               // 54
    (PCODE)Load_X5,                         // 55
    (PCODE)Load_X5_X6,                      // 56
    (PCODE)Load_X5_X6_X7,                   // 57
    (PCODE)0,                               // 60
    (PCODE)0,                               // 61
    (PCODE)0,                               // 62
    (PCODE)0,                               // 63
    (PCODE)0,                               // 64
    (PCODE)0,                               // 65
    (PCODE)Load_X6,                         // 66
    (PCODE)Load_X6_X7,                      // 67
    (PCODE)0,                               // 70
    (PCODE)0,                               // 71
    (PCODE)0,                               // 72
    (PCODE)0,                               // 73
    (PCODE)0,                               // 74
    (PCODE)0,                               // 75
    (PCODE)0,                               // 76
    (PCODE)Load_X7                          // 77
};

extern "C" void Load_D0();
extern "C" void Load_D0_D1();
extern "C" void Load_D0_D1_D2();
extern "C" void Load_D0_D1_D2_D3();
extern "C" void Load_D0_D1_D2_D3_D4();
extern "C" void Load_D0_D1_D2_D3_D4_D5();
extern "C" void Load_D0_D1_D2_D3_D4_D5_D6();
extern "C" void Load_D0_D1_D2_D3_D4_D5_D6_D7();
extern "C" void Load_D1();
extern "C" void Load_D1_D2();
extern "C" void Load_D1_D2_D3();
extern "C" void Load_D1_D2_D3_D4();
extern "C" void Load_D1_D2_D3_D4_D5();
extern "C" void Load_D1_D2_D3_D4_D5_D6();
extern "C" void Load_D1_D2_D3_D4_D5_D6_D7();
extern "C" void Load_D2();
extern "C" void Load_D2_D3();
extern "C" void Load_D2_D3_D4();
extern "C" void Load_D2_D3_D4_D5();
extern "C" void Load_D2_D3_D4_D5_D6();
extern "C" void Load_D2_D3_D4_D5_D6_D7();
extern "C" void Load_D3();
extern "C" void Load_D3_D4();
extern "C" void Load_D3_D4_D5();
extern "C" void Load_D3_D4_D5_D6();
extern "C" void Load_D3_D4_D5_D6_D7();
extern "C" void Load_D4();
extern "C" void Load_D4_D5();
extern "C" void Load_D4_D5_D6();
extern "C" void Load_D4_D5_D6_D7();
extern "C" void Load_D5();
extern "C" void Load_D5_D6();
extern "C" void Load_D5_D6_D7();
extern "C" void Load_D6();
extern "C" void Load_D6_D7();
extern "C" void Load_D7();

PCODE FPRegsRoutines[] =
{
    (PCODE)Load_D0,                         // 00
    (PCODE)Load_D0_D1,                      // 01
    (PCODE)Load_D0_D1_D2,                   // 02
    (PCODE)Load_D0_D1_D2_D3,                // 03
    (PCODE)Load_D0_D1_D2_D3_D4,             // 04
    (PCODE)Load_D0_D1_D2_D3_D4_D5,          // 05
    (PCODE)Load_D0_D1_D2_D3_D4_D5_D6,       // 06
    (PCODE)Load_D0_D1_D2_D3_D4_D5_D6_D7,    // 07
    (PCODE)0,                               // 10    
    (PCODE)Load_D1,                         // 11
    (PCODE)Load_D1_D2,                      // 12
    (PCODE)Load_D1_D2_D3,                   // 13
    (PCODE)Load_D1_D2_D3_D4,                // 14
    (PCODE)Load_D1_D2_D3_D4_D5,             // 15
    (PCODE)Load_D1_D2_D3_D4_D5_D6,          // 16
    (PCODE)Load_D1_D2_D3_D4_D5_D6_D7,       // 17
    (PCODE)0,                               // 20
    (PCODE)0,                               // 21
    (PCODE)Load_D2,                         // 22
    (PCODE)Load_D2_D3,                      // 23
    (PCODE)Load_D2_D3_D4,                   // 24
    (PCODE)Load_D2_D3_D4_D5,                // 25
    (PCODE)Load_D2_D3_D4_D5_D6,             // 26
    (PCODE)Load_D2_D3_D4_D5_D6_D7,          // 27
    (PCODE)0,                               // 30
    (PCODE)0,                               // 31   
    (PCODE)0,                               // 32
    (PCODE)Load_D3,                         // 33
    (PCODE)Load_D3_D4,                      // 34
    (PCODE)Load_D3_D4_D5,                   // 35
    (PCODE)Load_D3_D4_D5_D6,                // 36
    (PCODE)Load_D3_D4_D5_D6_D7,             // 37
    (PCODE)0,                               // 40
    (PCODE)0,                               // 41
    (PCODE)0,                               // 42
    (PCODE)0,                               // 43
    (PCODE)Load_D4,                         // 44
    (PCODE)Load_D4_D5,                      // 45
    (PCODE)Load_D4_D5_D6,                   // 46
    (PCODE)Load_D4_D5_D6_D7,                // 47
    (PCODE)0,                               // 50
    (PCODE)0,                               // 51
    (PCODE)0,                               // 52
    (PCODE)0,                               // 53
    (PCODE)0,                               // 54
    (PCODE)Load_D5,                         // 55
    (PCODE)Load_D5_D6,                      // 56
    (PCODE)Load_D5_D6_D7,                   // 57
    (PCODE)0,                               // 60
    (PCODE)0,                               // 61
    (PCODE)0,                               // 62
    (PCODE)0,                               // 63
    (PCODE)0,                               // 64
    (PCODE)0,                               // 65
    (PCODE)Load_D6,                         // 66
    (PCODE)Load_D6_D7,                      // 67
    (PCODE)0,                               // 70
    (PCODE)0,                               // 71
    (PCODE)0,                               // 72
    (PCODE)0,                               // 73
    (PCODE)0,                               // 74
    (PCODE)0,                               // 75
    (PCODE)0,                               // 76
    (PCODE)Load_D7                          // 77
};
#endif // TARGET_ARM64

PCODE GetGPRegRangeLoadRoutine(int r1, int r2)
{
    int index = r1 * NUM_ARGUMENT_REGISTERS + r2;
    return GPRegsRoutines[index];
}

PCODE GetGPRegRefLoadRoutine(int r)
{
    return GPRegsRefRoutines[r];
}

PCODE GetFPRegRangeLoadRoutine(int x1, int x2)
{
    int index = x1 * NUM_FLOAT_ARGUMENT_REGISTERS + x2;
    return FPRegsRoutines[index];
}

PCODE GetStackRangeLoadRoutine(int s1, int s2)
{
    // Stack range is not supported yet
    assert(!"Stack range is not supported yet");
    return NULL;
}

extern "C" void CallJittedMethodRetVoid(PCODE *routines, int8_t*pArgs, int totalStackSize);
extern "C" void CallJittedMethodRetDouble(PCODE *routines, int8_t*pArgs, int8_t*pRet, int totalStackSize);
extern "C" void CallJittedMethodRetI8(PCODE *routines, int8_t*pArgs, int8_t*pRet, int totalStackSize);
extern "C" void CallJittedMethodRetBuff(PCODE *routines, int8_t*pArgs, int8_t*pRet, int totalStackSize);

void InvokeCompiledMethod(MethodDesc *pMD, int8_t *pArgs, int8_t *pRet)
{
    printf("InvokeCompiledMethod %s.%s with signature %s\n", pMD->m_pszDebugClassName, pMD->m_pszDebugMethodName, pMD->m_pszDebugMethodSignature);

    PCODE routines[16];

    MetaSig sig(pMD);
    ArgIterator argIt(&sig);
    int ofs = 0;
    DWORD arg = 0;
    const int NO_RANGE = -1;
    int r1 = argIt.HasThis() ? 0 :  NO_RANGE; // The "this" argument register is not enumerated by the arg iterator.
    int r2 = 0;
    int x1 = NO_RANGE; // indicates that there is no active range of FP registers
    int x2 = 0;
    int s1 = NO_RANGE; // indicates that there is no active range of stack arguments
    int s2 = 0;
    int routineIndex = 0;
    int totalStackSize = 0; // TODO: use argIt.SizeOfArgStack
    for (; TransitionBlock::InvalidOffset != (ofs = argIt.GetNextOffset()); arg++)
    {
        ArgLocDesc argLocDesc;
        argIt.GetArgLoc(ofs, &argLocDesc);

        // TODO: handle structs passed in registers, consider {float, int} and {int, float} would use the same arg registers, but load them in a reverse order.
        // This is only possible on unix amd64, Windows pass structs larger than 64 bits by reference
        // Interesting case on Windows x64 - {int, float} is passed in a single general purpose register, does the interpreter store it that way?
        // TODO: handle value types passing by reference on platforms that do that, like Windows
        // TODO: it seems that arm64 doesn't use the Q registers for passing floats / doubles
        // TODO: Apple arm64 specialities
        

        // Check if we have a range of registers or stack arguments that we need to store because the current argument
        // terminates it.
        if ((argLocDesc.m_cGenReg == 0 || argIt.IsArgPassedByRef()) && (r1 != NO_RANGE))
        {
            // No GP register is used to pass the current argument, but we already have a range of GP registers,
            // store the routine for the range
            printf("r%d..%d\n", r1, r2);
            routines[routineIndex++] = GetGPRegRangeLoadRoutine(r1, r2);
            r1 = NO_RANGE;
        }
        else if (((argLocDesc.m_cFloatReg == 0)) && (x1 != NO_RANGE))
        {
            // No floating point register is used to pass the current argument, but we already have a range of FP registers,
            // store the routine for the range
            printf("x%d..%d\n", x1, x2);
            routines[routineIndex++] = GetFPRegRangeLoadRoutine(x1, x2);
            x1 = NO_RANGE;
        }
        else if ((argLocDesc.m_byteStackSize == 0) && (s1 != NO_RANGE))
        {
            // No stack argument is used to pass the current argument, but we already have a range of stack arguments,
            // store the routine for the range
            printf("stack index %d, size %d\n", s1, s2 - s1 + 1);
            totalStackSize += s2 - s1 + 1;
            routines[routineIndex++] = (PCODE)Load_Stack;
            routines[routineIndex++] = ((int64_t)(s2 - s1 + 1) << 32) | s1;
            s1 = NO_RANGE;
        }

        if (argLocDesc.m_cGenReg != 0)
        {                       
            if (argIt.IsArgPassedByRef())
            {
                // Arguments passed by reference are handled separately, because the interpreter stores the value types on its stack by value.
                // So the argument loading routine needs to load the address of the argument. To avoid explosion of number of the routines,
                // we always process single argument passed by reference using single routine.
                printf("r%d value type by reference, size %d\n", argLocDesc.m_idxGenReg, argIt.GetArgSize());
                routines[routineIndex++] = GetGPRegRefLoadRoutine(argLocDesc.m_idxGenReg);
                routines[routineIndex++] = argIt.GetArgSize();
            }
            else
            {
                if (r1 == NO_RANGE) // No active range yet
                {
                    // Start a new range
                    r1 = argLocDesc.m_idxGenReg;
                    r2 = r1 + argLocDesc.m_cGenReg - 1;
                } 
                else if (argLocDesc.m_idxGenReg == r2 + 1)
                {
                    // Extend an existing range
                    r2 += argLocDesc.m_cGenReg;
                }
                else
                {
                    // Discontinuous range - store a routine for the current and start a new one
                    printf("r%d..%d\n", r1, r2);
                    routines[routineIndex++] = GetGPRegRangeLoadRoutine(r1, r2);
                    r1 = argLocDesc.m_idxGenReg;
                    r2 = r1 + argLocDesc.m_cGenReg - 1;
                }
            }
        }

        if (argLocDesc.m_cFloatReg != 0)
        {
            if (x1 == NO_RANGE) // No active range yet
            {
                // Start a new range
                x1 = argLocDesc.m_idxFloatReg;
                x2 = x1 + argLocDesc.m_cFloatReg - 1;
            } 
            else if (argLocDesc.m_idxFloatReg == x2 + 1)
            {
                // Extend an existing range
                x2 += argLocDesc.m_cFloatReg;
            }
            else
            {
                // Discontinuous range - store a routine for the current and start a new one
                printf("x%d..%d\n", x1, x2);
                routines[routineIndex++] = GetFPRegRangeLoadRoutine(x1, x2);
                x1 = argLocDesc.m_idxFloatReg;
                x2 = x1 + argLocDesc.m_cFloatReg - 1;
            }
        }

        if (argLocDesc.m_byteStackSize != 0)
        {
            if (s1 == NO_RANGE) // No active range yet
            {
                // Start a new range
                s1 = argLocDesc.m_byteStackIndex;
                s2 = s1 + argLocDesc.m_byteStackSize - 1;
            } 
            else if (argLocDesc.m_byteStackIndex == s2 + 1)
            {
                // Extend an existing range
                s2 += argLocDesc.m_byteStackSize;
            }
            else
            {
                // Discontinuous range - store a routine for the current and start a new one
                printf("stack index %d, size %d\n", s1, s2 - s1 + 1);
                totalStackSize += s2 - s1 + 1;
                routines[routineIndex++] = (PCODE)Load_Stack;
                routines[routineIndex++] = ((int64_t)(s2 - s1 + 1) << 32) | s1;
                s1 = argLocDesc.m_byteStackIndex;
                s2 = s1 + argLocDesc.m_byteStackSize - 1;
            }
        }
    }

    // All arguments were processed, but there is likely a pending ranges to store.
    if (r1 != NO_RANGE)
    {
        printf("r%d..%d\n", r1, r2);
        routines[routineIndex++] = GetGPRegRangeLoadRoutine(r1, r2);
    }
    else if (x1 != NO_RANGE)
    {
        printf("x%d..%d\n", x1, x2);
        routines[routineIndex++] = GetFPRegRangeLoadRoutine(x1, x2);
    }
    else if (s1 != NO_RANGE)
    {
        printf("stack index %d, size %d\n", s1, s2 - s1 + 1);
        totalStackSize += s2 - s1 + 1;
        routines[routineIndex++] = (PCODE)Load_Stack;
        routines[routineIndex++] = ((int64_t)(s2 - s1 + 1) << 32) | s1;
    }

    routines[routineIndex] = pMD->GetNativeCode(); // The method to call
    //routines[routineIndex] = pMD->GetSingleCallableAddrOfCode();

    totalStackSize = ALIGN_UP(totalStackSize, 16); // Align the stack to 16 bytes

    if (argIt.HasRetBuffArg())
    {
        CallJittedMethodRetBuff(routines, pArgs, pRet, totalStackSize);
    }
    else
    {
        TypeHandle thReturnValueType;
        CorElementType thReturnType = sig.GetReturnTypeNormalized(&thReturnValueType);

        // TODO: consider adding a routine for return value processing and having a single CallJittedMethod. It would be beneficial for caching
        // It may have to be handled in a special way though, since after returning from the target, the current routine address would be gone. Unless we store it in a nonvol register
        switch (thReturnType)
        {
            case ELEMENT_TYPE_BOOLEAN:
            case ELEMENT_TYPE_CHAR:
            case ELEMENT_TYPE_I1:
            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_I2:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_I4:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_I8:
            case ELEMENT_TYPE_U8:
            case ELEMENT_TYPE_I:
            case ELEMENT_TYPE_U:
            case ELEMENT_TYPE_CLASS:
            case ELEMENT_TYPE_OBJECT:
            case ELEMENT_TYPE_STRING:
            case ELEMENT_TYPE_PTR:
            case ELEMENT_TYPE_BYREF:
            case ELEMENT_TYPE_TYPEDBYREF:
            case ELEMENT_TYPE_ARRAY:
            case ELEMENT_TYPE_SZARRAY:
            case ELEMENT_TYPE_FNPTR:
                CallJittedMethodRetI8(routines, pArgs, pRet, totalStackSize);
                break;
            case ELEMENT_TYPE_R4:
            case ELEMENT_TYPE_R8:
                CallJittedMethodRetDouble(routines, pArgs, pRet, totalStackSize);
                break;
            case ELEMENT_TYPE_VOID:
                CallJittedMethodRetVoid(routines, pArgs, totalStackSize);
                break;
            case ELEMENT_TYPE_VALUETYPE:
#ifdef TARGET_AMD64
#ifdef TARGET_WINDOWS
                if (thReturnValueType.AsMethodTable()->IsIntrinsicType())
                {
                    // 128 bit vector types returned in xmm0 too. E.g. Vector2
                    CallJittedMethodRetDouble(routines, pArgs, pRet, totalStackSize);
                }
                else
                {
                    // POD structs smaller than 64 bits are returned in rax
                    CallJittedMethodRetI8(routines, pArgs, pRet, totalStackSize);
                }
#else // TARGET_WINDOWS
                // TODO: struct in registers
                _ASSERTE(!"Struct returns by value in registers are not supported yet");
#endif // TARGET_WINDOWS
#elif TARGET_ARM64
                // HFA, HVA, POD structs smaller than 128 bits
#else
                _ASSERTE(!"Struct returns by value are not supported yet");
#endif
                break;
            default:
                _ASSERTE(!"Unexpected return type");
                break;
        }
    }
}

typedef void* (*HELPER_FTN_PP)(void*);

thread_local InterpThreadContext *t_pThreadContext = NULL;

InterpThreadContext* InterpGetThreadContext()
{
    InterpThreadContext *threadContext = t_pThreadContext;

    if (!threadContext)
    {
        threadContext = new InterpThreadContext;
        // FIXME VirtualAlloc/mmap with INTERP_STACK_ALIGNMENT alignment
        threadContext->pStackStart = threadContext->pStackPointer = (int8_t*)malloc(INTERP_STACK_SIZE);
        threadContext->pStackEnd = threadContext->pStackStart + INTERP_STACK_SIZE;

        t_pThreadContext = threadContext;
        return threadContext;
    }
    else
    {
        return threadContext;
    }
}

#define LOCAL_VAR_ADDR(offset,type) ((type*)(stack + (offset)))
#define LOCAL_VAR(offset,type) (*LOCAL_VAR_ADDR(offset, type))
// TODO once we have basic EH support
#define NULL_CHECK(o)

void InterpExecMethod(InterpreterFrame *pInterpreterFrame, InterpMethodContextFrame *pFrame, InterpThreadContext *pThreadContext)
{
    const int32_t *ip;
    int8_t *stack;

    InterpMethod *pMethod = *(InterpMethod**)pFrame->startIp;
    pThreadContext->pStackPointer = pFrame->pStack + pMethod->allocaSize;
    ip = pFrame->startIp + sizeof(InterpMethod*) / sizeof(int32_t);
    stack = pFrame->pStack;

    int32_t returnOffset, callArgsOffset, methodSlot;

MAIN_LOOP:
    while (true)
    {
        // Interpreter-TODO: This is only needed to enable SOS see the exact location in the interpreted method.
        // Neither the GC nor the managed debugger needs that as they walk the stack when the runtime is suspended
        // and we can save the IP to the frame at the suspension time.
        // It will be useful for testing e.g. the debug info at various locations in the current method, so let's
        // keep it for such purposes until we don't need it anymore.
        pFrame->ip = (int32_t*)ip;

        switch (*ip)
        {
            case INTOP_INITLOCALS:
                memset(stack + ip[1], 0, ip[2]);
                ip += 3;
                break;
            case INTOP_MEMBAR:
                MemoryBarrier();
                ip++;
                break;
            case INTOP_LDC_I4:
                LOCAL_VAR(ip[1], int32_t) = ip[2];
                ip += 3;
                break;
            case INTOP_LDC_I4_0:
                LOCAL_VAR(ip[1], int32_t) = 0;
                ip += 2;
                break;
            case INTOP_LDC_I8_0:
                LOCAL_VAR(ip[1], int64_t) = 0;
                ip += 2;
                break;
            case INTOP_LDC_I8:
                LOCAL_VAR(ip[1], int64_t) = (int64_t)ip[2] + ((int64_t)ip[3] << 32);
                ip += 4;
                break;
            case INTOP_LDC_R4:
                LOCAL_VAR(ip[1], int32_t) = ip[2];
                ip += 3;
                break;
            case INTOP_LDC_R8:
                LOCAL_VAR(ip[1], int64_t) = (int64_t)ip[2] + ((int64_t)ip[3] << 32);
                ip += 4;
                break;
            case INTOP_LDPTR:
                LOCAL_VAR(ip[1], void*) = pMethod->pDataItems[ip[2]];
                ip += 3;
                break;
            case INTOP_RET:
                // Return stack slot sized value
                *(int64_t*)pFrame->pRetVal = LOCAL_VAR(ip[1], int64_t);
                goto EXIT_FRAME;
            case INTOP_RET_VT:
                memmove(pFrame->pRetVal, stack + ip[1], ip[2]);
                goto EXIT_FRAME;
            case INTOP_RET_VOID:
                goto EXIT_FRAME;

            case INTOP_LDLOCA:
                LOCAL_VAR(ip[1], void*) = stack + ip[2];
                ip += 3;
                break;;

#define MOV(argtype1,argtype2) \
    LOCAL_VAR(ip [1], argtype1) = LOCAL_VAR(ip [2], argtype2); \
    ip += 3;
            // When loading from a local, we might need to sign / zero extend to 4 bytes
            // which is our minimum "register" size in interp. They are only needed when
            // the address of the local is taken and we should try to optimize them out
            // because the local can't be propagated.
            case INTOP_MOV_I4_I1: MOV(int32_t, int8_t); break;
            case INTOP_MOV_I4_U1: MOV(int32_t, uint8_t); break;
            case INTOP_MOV_I4_I2: MOV(int32_t, int16_t); break;
            case INTOP_MOV_I4_U2: MOV(int32_t, uint16_t); break;
            // Normal moves between vars
            case INTOP_MOV_4: MOV(int32_t, int32_t); break;
            case INTOP_MOV_8: MOV(int64_t, int64_t); break;

            case INTOP_MOV_VT:
                memmove(stack + ip[1], stack + ip[2], ip[3]);
                ip += 4;
                break;

            case INTOP_CONV_R_UN_I4:
                LOCAL_VAR(ip[1], double) = (double)LOCAL_VAR(ip[2], uint32_t);
                ip += 3;
                break;
            case INTOP_CONV_R_UN_I8:
                LOCAL_VAR(ip[1], double) = (double)LOCAL_VAR(ip[2], uint64_t);
                ip += 3;
                break;
            case INTOP_CONV_I1_I4:
                LOCAL_VAR(ip[1], int32_t) = (int8_t)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_I1_I8:
                LOCAL_VAR(ip[1], int32_t) = (int8_t)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_I1_R4:
                LOCAL_VAR(ip[1], int32_t) = (int8_t)(int32_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_I1_R8:
                LOCAL_VAR(ip[1], int32_t) = (int8_t)(int32_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_U1_I4:
                LOCAL_VAR(ip[1], int32_t) = (uint8_t)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_U1_I8:
                LOCAL_VAR(ip[1], int32_t) = (uint8_t)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_U1_R4:
                LOCAL_VAR(ip[1], int32_t) = (uint8_t)(uint32_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_U1_R8:
                LOCAL_VAR(ip[1], int32_t) = (uint8_t)(uint32_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_I2_I4:
                LOCAL_VAR(ip[1], int32_t) = (int16_t)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_I2_I8:
                LOCAL_VAR(ip[1], int32_t) = (int16_t)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_I2_R4:
                LOCAL_VAR(ip[1], int32_t) = (int16_t)(int32_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_I2_R8:
                LOCAL_VAR(ip[1], int32_t) = (int16_t)(int32_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_U2_I4:
                LOCAL_VAR(ip[1], int32_t) = (uint16_t)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_U2_I8:
                LOCAL_VAR(ip[1], int32_t) = (uint16_t)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_U2_R4:
                LOCAL_VAR(ip[1], int32_t) = (uint16_t)(uint32_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_U2_R8:
                LOCAL_VAR(ip[1], int32_t) = (uint16_t)(uint32_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_I4_R4:
                LOCAL_VAR(ip[1], int32_t) = (int32_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;;
            case INTOP_CONV_I4_R8:
                LOCAL_VAR(ip[1], int32_t) = (int32_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;;

            case INTOP_CONV_U4_R4:
            case INTOP_CONV_U4_R8:
                assert(0);
                break;

            case INTOP_CONV_I8_I4:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_I8_U4:
                LOCAL_VAR(ip[1], int64_t) = (uint32_t)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;;
            case INTOP_CONV_I8_R4:
                LOCAL_VAR(ip[1], int64_t) = (int64_t)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_I8_R8:
                LOCAL_VAR(ip[1], int64_t) = (int64_t)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_R4_I4:
                LOCAL_VAR(ip[1], float) = (float)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;;
            case INTOP_CONV_R4_I8:
                LOCAL_VAR(ip[1], float) = (float)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_R4_R8:
                LOCAL_VAR(ip[1], float) = (float)LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_CONV_R8_I4:
                LOCAL_VAR(ip[1], double) = (double)LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_CONV_R8_I8:
                LOCAL_VAR(ip[1], double) = (double)LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_CONV_R8_R4:
                LOCAL_VAR(ip[1], double) = (double)LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_CONV_U8_R4:
            case INTOP_CONV_U8_R8:
                // TODO
                assert(0);
                break;

            case INTOP_SWITCH:
            {
                uint32_t val = LOCAL_VAR(ip[1], uint32_t);
                uint32_t n = ip[2];
                ip += 3;
                if (val < n)
                {
                    ip += val;
                    ip += *ip;
                }
                else
                {
                    ip += n;
                }
                break;
            }

            case INTOP_BR:
                ip += ip[1];
                break;

#define BR_UNOP(datatype, op)           \
    if (LOCAL_VAR(ip[1], datatype) op)  \
        ip += ip[2];                    \
    else \
        ip += 3;

            case INTOP_BRFALSE_I4:
                BR_UNOP(int32_t, == 0);
                break;
            case INTOP_BRFALSE_I8:
                BR_UNOP(int64_t, == 0);
                break;
            case INTOP_BRTRUE_I4:
                BR_UNOP(int32_t, != 0);
                break;
            case INTOP_BRTRUE_I8:
                BR_UNOP(int64_t, != 0);
                break;

#define BR_BINOP_COND(cond) \
    if (cond)               \
        ip += ip[3];        \
    else                    \
        ip += 4;

#define BR_BINOP(datatype, op) \
    BR_BINOP_COND(LOCAL_VAR(ip[1], datatype) op LOCAL_VAR(ip[2], datatype))

            case INTOP_BEQ_I4:
                BR_BINOP(int32_t, ==);
                break;
            case INTOP_BEQ_I8:
                BR_BINOP(int64_t, ==);
                break;
            case INTOP_BEQ_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(!isunordered(f1, f2) && f1 == f2);
                break;
            }
            case INTOP_BEQ_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(!isunordered(d1, d2) && d1 == d2);
                break;
            }
            case INTOP_BGE_I4:
                BR_BINOP(int32_t, >=);
                break;
            case INTOP_BGE_I8:
                BR_BINOP(int64_t, >=);
                break;
            case INTOP_BGE_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(!isunordered(f1, f2) && f1 >= f2);
                break;
            }
            case INTOP_BGE_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(!isunordered(d1, d2) && d1 >= d2);
                break;
            }
            case INTOP_BGT_I4:
                BR_BINOP(int32_t, >);
                break;
            case INTOP_BGT_I8:
                BR_BINOP(int64_t, >);
                break;
            case INTOP_BGT_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(!isunordered(f1, f2) && f1 > f2);
                break;
            }
            case INTOP_BGT_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(!isunordered(d1, d2) && d1 > d2);
                break;
            }
            case INTOP_BLT_I4:
                BR_BINOP(int32_t, <);
                break;
            case INTOP_BLT_I8:
                BR_BINOP(int64_t, <);
                break;
            case INTOP_BLT_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(!isunordered(f1, f2) && f1 < f2);
                break;
            }
            case INTOP_BLT_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(!isunordered(d1, d2) && d1 < d2);
                break;
            }
            case INTOP_BLE_I4:
                BR_BINOP(int32_t, <=);
                break;
            case INTOP_BLE_I8:
                BR_BINOP(int64_t, <=);
                break;
            case INTOP_BLE_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(!isunordered(f1, f2) && f1 <= f2);
                break;
            }
            case INTOP_BLE_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(!isunordered(d1, d2) && d1 <= d2);
                break;
            }
            case INTOP_BNE_UN_I4:
                BR_BINOP(uint32_t, !=);
                break;
            case INTOP_BNE_UN_I8:
                BR_BINOP(uint64_t, !=);
                break;
            case INTOP_BNE_UN_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(isunordered(f1, f2) || f1 != f2);
                break;
            }
            case INTOP_BNE_UN_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(isunordered(d1, d2) || d1 != d2);
                break;
            }
            case INTOP_BGE_UN_I4:
                BR_BINOP(uint32_t, >=);
                break;
            case INTOP_BGE_UN_I8:
                BR_BINOP(uint64_t, >=);
                break;
            case INTOP_BGE_UN_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(isunordered(f1, f2) || f1 >= f2);
                break;
            }
            case INTOP_BGE_UN_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(isunordered(d1, d2) || d1 >= d2);
                break;
            }
            case INTOP_BGT_UN_I4:
                BR_BINOP(uint32_t, >);
                break;
            case INTOP_BGT_UN_I8:
                BR_BINOP(uint64_t, >);
                break;
            case INTOP_BGT_UN_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(isunordered(f1, f2) || f1 > f2);
                break;
            }
            case INTOP_BGT_UN_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(isunordered(d1, d2) || d1 > d2);
                break;
            }
            case INTOP_BLE_UN_I4:
                BR_BINOP(uint32_t, <=);
                break;
            case INTOP_BLE_UN_I8:
                BR_BINOP(uint64_t, <=);
                break;
            case INTOP_BLE_UN_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(isunordered(f1, f2) || f1 <= f2);
                break;
            }
            case INTOP_BLE_UN_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(isunordered(d1, d2) || d1 <= d2);
                break;
            }
            case INTOP_BLT_UN_I4:
                BR_BINOP(uint32_t, <);
                break;
            case INTOP_BLT_UN_I8:
                BR_BINOP(uint64_t, <);
                break;
            case INTOP_BLT_UN_R4:
            {
                float f1 = LOCAL_VAR(ip[1], float);
                float f2 = LOCAL_VAR(ip[2], float);
                BR_BINOP_COND(isunordered(f1, f2) || f1 < f2);
                break;
            }
            case INTOP_BLT_UN_R8:
            {
                double d1 = LOCAL_VAR(ip[1], double);
                double d2 = LOCAL_VAR(ip[2], double);
                BR_BINOP_COND(isunordered(d1, d2) || d1 < d2);
                break;
            }

            case INTOP_ADD_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) + LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_ADD_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) + LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_ADD_R4:
                LOCAL_VAR(ip[1], float) = LOCAL_VAR(ip[2], float) + LOCAL_VAR(ip[3], float);
                ip += 4;
                break;
            case INTOP_ADD_R8:
                LOCAL_VAR(ip[1], double) = LOCAL_VAR(ip[2], double) + LOCAL_VAR(ip[3], double);
                ip += 4;
                break;
            case INTOP_ADD_I4_IMM:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) + ip[3];
                ip += 4;
                break;
            case INTOP_ADD_I8_IMM:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) + ip[3];
                ip += 4;
                break;
            case INTOP_SUB_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) - LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SUB_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) - LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_SUB_R4:
                LOCAL_VAR(ip[1], float) = LOCAL_VAR(ip[2], float) - LOCAL_VAR(ip[3], float);
                ip += 4;
                break;
            case INTOP_SUB_R8:
                LOCAL_VAR(ip[1], double) = LOCAL_VAR(ip[2], double) - LOCAL_VAR(ip[3], double);
                ip += 4;
                break;

            case INTOP_MUL_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) * LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_MUL_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) * LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_MUL_R4:
                LOCAL_VAR(ip[1], float) = LOCAL_VAR(ip[2], float) * LOCAL_VAR(ip[3], float);
                ip += 4;
                break;
            case INTOP_MUL_R8:
                LOCAL_VAR(ip[1], double) = LOCAL_VAR(ip[2], double) * LOCAL_VAR(ip[3], double);
                ip += 4;
                break;

            case INTOP_DIV_I4:
            {
                int32_t i1 = LOCAL_VAR(ip[2], int32_t);
                int32_t i2 = LOCAL_VAR(ip[3], int32_t);
                if (i2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                if (i2 == -1 && i1 == INT32_MIN)
                    assert(0); // Interpreter-TODO: OverflowException
                LOCAL_VAR(ip[1], int32_t) = i1 / i2;
                ip += 4;
                break;
            }
            case INTOP_DIV_I8:
            {
                int64_t l1 = LOCAL_VAR(ip[2], int64_t);
                int64_t l2 = LOCAL_VAR(ip[3], int64_t);
                if (l2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                if (l2 == -1 && l1 == INT64_MIN)
                    assert(0); // Interpreter-TODO: OverflowException
                LOCAL_VAR(ip[1], int64_t) = l1 / l2;
                ip += 4;
                break;
            }
            case INTOP_DIV_R4:
                LOCAL_VAR(ip[1], float) = LOCAL_VAR(ip[2], float) / LOCAL_VAR(ip[3], float);
                ip += 4;
                break;
            case INTOP_DIV_R8:
                LOCAL_VAR(ip[1], double) = LOCAL_VAR(ip[2], double) / LOCAL_VAR(ip[3], double);
                ip += 4;
                break;
            case INTOP_DIV_UN_I4:
            {
                uint32_t i2 = LOCAL_VAR(ip[3], uint32_t);
                if (i2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                LOCAL_VAR(ip[1], uint32_t) = LOCAL_VAR(ip[2], uint32_t) / i2;
                ip += 4;
                break;
            }
            case INTOP_DIV_UN_I8:
            {
                uint64_t l2 = LOCAL_VAR(ip[3], uint64_t);
                if (l2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                LOCAL_VAR(ip[1], uint64_t) = LOCAL_VAR(ip[2], uint64_t) / l2;
                ip += 4;
                break;
            }

            case INTOP_REM_I4:
            {
                int32_t i1 = LOCAL_VAR(ip[2], int32_t);
                int32_t i2 = LOCAL_VAR(ip[3], int32_t);
                if (i2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                if (i2 == -1 && i1 == INT32_MIN)
                    assert(0); // Interpreter-TODO: OverflowException
                LOCAL_VAR(ip[1], int32_t) = i1 % i2;
                ip += 4;
                break;
            }
            case INTOP_REM_I8:
            {
                int64_t l1 = LOCAL_VAR(ip[2], int64_t);
                int64_t l2 = LOCAL_VAR(ip[3], int64_t);
                if (l2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                if (l2 == -1 && l1 == INT64_MIN)
                    assert(0); // Interpreter-TODO: OverflowException
                LOCAL_VAR(ip[1], int64_t) = l1 % l2;
                ip += 4;
                break;
            }
            case INTOP_REM_R4:
                LOCAL_VAR(ip[1], float) = fmodf(LOCAL_VAR(ip[2], float), LOCAL_VAR(ip[3], float));
                ip += 4;
                break;
            case INTOP_REM_R8:
                LOCAL_VAR(ip[1], double) = fmod(LOCAL_VAR(ip[2], double), LOCAL_VAR(ip[3], double));
                ip += 4;
                break;
            case INTOP_REM_UN_I4:
            {
                uint32_t i2 = LOCAL_VAR(ip[3], uint32_t);
                if (i2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                LOCAL_VAR(ip[1], uint32_t) = LOCAL_VAR(ip[2], uint32_t) % i2;
                ip += 4;
                break;
            }
            case INTOP_REM_UN_I8:
            {
                uint64_t l2 = LOCAL_VAR(ip[3], uint64_t);
                if (l2 == 0)
                    assert(0); // Interpreter-TODO: DivideByZeroException
                LOCAL_VAR(ip[1], uint64_t) = LOCAL_VAR(ip[2], uint64_t) % l2;
                ip += 4;
                break;
            }

            case INTOP_SHL_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) << LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SHL_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) << LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SHR_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) >> LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SHR_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) >> LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SHR_UN_I4:
                LOCAL_VAR(ip[1], uint32_t) = LOCAL_VAR(ip[2], uint32_t) >> LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_SHR_UN_I8:
                LOCAL_VAR(ip[1], uint64_t) = LOCAL_VAR(ip[2], uint64_t) >> LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;

            case INTOP_NEG_I4:
                LOCAL_VAR(ip[1], int32_t) = - LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_NEG_I8:
                LOCAL_VAR(ip[1], int64_t) = - LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;
            case INTOP_NEG_R4:
                LOCAL_VAR(ip[1], float) = - LOCAL_VAR(ip[2], float);
                ip += 3;
                break;
            case INTOP_NEG_R8:
                LOCAL_VAR(ip[1], double) = - LOCAL_VAR(ip[2], double);
                ip += 3;
                break;
            case INTOP_NOT_I4:
                LOCAL_VAR(ip[1], int32_t) = ~ LOCAL_VAR(ip[2], int32_t);
                ip += 3;
                break;
            case INTOP_NOT_I8:
                LOCAL_VAR(ip[1], int64_t) = ~ LOCAL_VAR(ip[2], int64_t);
                ip += 3;
                break;

            case INTOP_AND_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) & LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_AND_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) & LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_OR_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) | LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_OR_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) | LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_XOR_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) ^ LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_XOR_I8:
                LOCAL_VAR(ip[1], int64_t) = LOCAL_VAR(ip[2], int64_t) ^ LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;

#define CMP_BINOP_FP(datatype, op, noOrderVal)      \
    do {                                            \
        datatype f1 = LOCAL_VAR(ip[2], datatype);   \
        datatype f2 = LOCAL_VAR(ip[3], datatype);   \
        if (isunordered(f1, f2))                    \
            LOCAL_VAR(ip[1], int32_t) = noOrderVal; \
        else                                        \
            LOCAL_VAR(ip[1], int32_t) = f1 op f2;   \
        ip += 4;                                    \
    } while (0)

            case INTOP_CEQ_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) == LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_CEQ_I8:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int64_t) == LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_CEQ_R4:
                CMP_BINOP_FP(float, ==, 0);
                break;
            case INTOP_CEQ_R8:
                CMP_BINOP_FP(double, ==, 0);
                break;

            case INTOP_CGT_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) > LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_CGT_I8:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int64_t) > LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_CGT_R4:
                CMP_BINOP_FP(float, >, 0);
                break;
            case INTOP_CGT_R8:
                CMP_BINOP_FP(double, >, 0);
                break;

            case INTOP_CGT_UN_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], uint32_t) > LOCAL_VAR(ip[3], uint32_t);
                ip += 4;
                break;
            case INTOP_CGT_UN_I8:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], uint32_t) > LOCAL_VAR(ip[3], uint32_t);
                ip += 4;
                break;
            case INTOP_CGT_UN_R4:
                CMP_BINOP_FP(float, >, 1);
                break;
            case INTOP_CGT_UN_R8:
                CMP_BINOP_FP(double, >, 1);
                break;

            case INTOP_CLT_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int32_t) < LOCAL_VAR(ip[3], int32_t);
                ip += 4;
                break;
            case INTOP_CLT_I8:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], int64_t) < LOCAL_VAR(ip[3], int64_t);
                ip += 4;
                break;
            case INTOP_CLT_R4:
                CMP_BINOP_FP(float, <, 0);
                break;
            case INTOP_CLT_R8:
                CMP_BINOP_FP(double, <, 0);
                break;

            case INTOP_CLT_UN_I4:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], uint32_t) < LOCAL_VAR(ip[3], uint32_t);
                ip += 4;
                break;
            case INTOP_CLT_UN_I8:
                LOCAL_VAR(ip[1], int32_t) = LOCAL_VAR(ip[2], uint64_t) < LOCAL_VAR(ip[3], uint64_t);
                ip += 4;
                break;
            case INTOP_CLT_UN_R4:
                CMP_BINOP_FP(float, <, 1);
                break;
            case INTOP_CLT_UN_R8:
                CMP_BINOP_FP(double, <, 1);
                break;

#define LDIND(dtype, ftype)                                 \
    do {                                                    \
        char *src = LOCAL_VAR(ip[2], char*);                \
        NULL_CHECK(src);                                    \
        LOCAL_VAR(ip[1], dtype) = *(ftype*)(src + ip[3]);   \
        ip += 4;                                            \
    } while (0)

            case INTOP_LDIND_I1:
                LDIND(int32_t, int8_t);
                break;
            case INTOP_LDIND_U1:
                LDIND(int32_t, uint8_t);
                break;
            case INTOP_LDIND_I2:
                LDIND(int32_t, int16_t);
                break;
            case INTOP_LDIND_U2:
                LDIND(int32_t, uint16_t);
                break;
            case INTOP_LDIND_I4:
                LDIND(int32_t, int32_t);
                break;
            case INTOP_LDIND_I8:
                LDIND(int64_t, int64_t);
                break;
            case INTOP_LDIND_R4:
                LDIND(float, float);
                break;
            case INTOP_LDIND_R8:
                LDIND(double, double);
                break;
            case INTOP_LDIND_VT:
            {
                char *src = LOCAL_VAR(ip[2], char*);
                NULL_CHECK(obj);
                memcpy(stack + ip[1], (char*)src + ip[3], ip[4]);
                ip += 5;
                break;
            }

#define STIND(dtype, ftype)                                         \
    do                                                              \
    {                                                               \
        char *dst = LOCAL_VAR(ip[1], char*);                        \
        NULL_CHECK(dst);                                            \
        *(ftype*)(dst + ip[3]) = (ftype)(LOCAL_VAR(ip[2], dtype));  \
        ip += 4;                                                    \
    } while (0)

            case INTOP_STIND_I1:
                STIND(int32_t, int8_t);
                break;
            case INTOP_STIND_U1:
                STIND(int32_t, uint8_t);
                break;
            case INTOP_STIND_I2:
                STIND(int32_t, int16_t);
                break;
            case INTOP_STIND_U2:
                STIND(int32_t, uint16_t);
                break;
            case INTOP_STIND_I4:
                STIND(int32_t, int32_t);
                break;
            case INTOP_STIND_I8:
                STIND(int64_t, int64_t);
                break;
            case INTOP_STIND_R4:
                STIND(float, float);
                break;
            case INTOP_STIND_R8:
                STIND(double, double);
                break;
            case INTOP_STIND_O:
            {
                char *dst = LOCAL_VAR(ip[1], char*);
                OBJECTREF storeObj = LOCAL_VAR(ip[2], OBJECTREF);
                NULL_CHECK(obj);
                SetObjectReferenceUnchecked((OBJECTREF*)(dst + ip[3]), storeObj);
                ip += 4;
                break;
            }
            case INTOP_STIND_VT_NOREF:
            {
                char *dest = LOCAL_VAR(ip[1], char*);
                NULL_CHECK(dest);
                memcpyNoGCRefs(dest + ip[3], stack + ip[2], ip[4]);
                ip += 5;
                break;
            }
            case INTOP_STIND_VT:
            {
                MethodTable *pMT = (MethodTable*)pMethod->pDataItems[ip[4]];
                char *dest = LOCAL_VAR(ip[1], char*);
                NULL_CHECK(dest);
                CopyValueClassUnchecked(dest + ip[3], stack + ip[2], pMT);
                ip += 5;
                break;
            }
            case INTOP_LDFLDA:
            {
                char *src = LOCAL_VAR(ip[2], char*);
                NULL_CHECK(src);
                LOCAL_VAR(ip[1], char*) = src + ip[3];
                ip += 4;
                break;
            }

            case INTOP_CALL_HELPER_PP:
            {
                HELPER_FTN_PP helperFtn = (HELPER_FTN_PP)pMethod->pDataItems[ip[2]];
                HELPER_FTN_PP* helperFtnSlot = (HELPER_FTN_PP*)pMethod->pDataItems[ip[3]];
                void* helperArg = pMethod->pDataItems[ip[4]];

                if (!helperFtn)
                    helperFtn = *helperFtnSlot;
                // This can call either native or compiled managed code. For an interpreter
                // only configuration, this might be problematic, at least performance wise.
                // FIXME We will need to handle exception throwing here.
                LOCAL_VAR(ip[1], void*) = helperFtn(helperArg);

                ip += 5;
                break;
            }

            case INTOP_CALL:
            {
                returnOffset = ip[1];
                callArgsOffset = ip[2];
                methodSlot = ip[3];

                ip += 4;
CALL_INTERP_SLOT:
                const int32_t *targetIp;
                size_t targetMethod = (size_t)pMethod->pDataItems[methodSlot];
                if (targetMethod & INTERP_METHOD_DESC_TAG)
                {
                    // First execution of this call. Ensure target method is compiled and
                    // patch the data item slot with the actual method code.
                    MethodDesc *pMD = (MethodDesc*)(targetMethod & ~INTERP_METHOD_DESC_TAG);
                    PCODE code = pMD->GetNativeCode();
                    if (!code) {
                        // This is an optimization to ensure that the stack walk will not have to search
                        // for the topmost frame in the current InterpExecMethod. It is not required
                        // for correctness, as the stack walk will find the topmost frame anyway. But it
                        // would need to seek through the frames to find it.
                        // An alternative approach would be to update the topmost frame during stack walk
                        // to make the probability that the next stack walk will need to search only a
                        // small subset of frames high.
                        pInterpreterFrame->SetTopInterpMethodContextFrame(pFrame);

                        // if (!(pMD->IsIL() || pMD->IsNoMetadata()))
                        // {
                        //     InvokeCompiledMethod(pMD, stack + callArgsOffset, stack + returnOffset);
                        //     break;
                        //     //printf("Attempted to execute method without IL %s.%s with signature %s from interpreter.\n", pMD->m_pszDebugClassName, pMD->m_pszDebugMethodName, pMD->m_pszDebugMethodSignature);
                        //     //assert(0);
                        // }
                        // else
                        {
                            GCX_PREEMP();
                            pMD->PrepareInitialCode(CallerGCMode::Coop);
                            code = pMD->GetNativeCode();
                        }
                    }
                    pMethod->pDataItems[methodSlot] = (void*)code;
                    targetIp = (const int32_t*)code;
                }
                else
                {
                    // At this stage in the implementation, we assume this is pointer to
                    // interpreter code. In the future, this should probably be tagged pointer
                    // for interpreter call or normal pointer for JIT/R2R call.
                    targetIp = (const int32_t*)targetMethod;                   
                }

                EECodeInfo codeInfo((PCODE)targetIp);
                if (!codeInfo.IsValid())
                {
                    printf("Attempted to execute native code from interpreter.\n");
                    assert(0);
                }
                else if (codeInfo.GetCodeManager() != ExecutionManager::GetInterpreterCodeManager())
                {
                    MethodDesc *pMD = codeInfo.GetMethodDesc();
                    InvokeCompiledMethod(pMD, stack + callArgsOffset, stack + returnOffset);
                    break;
                    // printf("Attempted to execute JITted / crossgened method %s.%s with signature %s from interpreter.\n", pMD->m_pszDebugClassName, pMD->m_pszDebugMethodName, pMD->m_pszDebugMethodSignature);
                    // assert(0);
                }


                // Save current execution state for when we return from called method
                pFrame->ip = ip;

                // Allocate child frame.
                {
                    InterpMethodContextFrame *pChildFrame = pFrame->pNext;
                    if (!pChildFrame)
                    {
                        pChildFrame = (InterpMethodContextFrame*)alloca(sizeof(InterpMethodContextFrame));
                        pChildFrame->pNext = NULL;
                        pFrame->pNext = pChildFrame;
                    }
                    pChildFrame->ReInit(pFrame, targetIp, stack + returnOffset, stack + callArgsOffset);
                    pFrame = pChildFrame;
                }
                assert (((size_t)pFrame->pStack % INTERP_STACK_ALIGNMENT) == 0);

                // Set execution state for the new frame
                pMethod = *(InterpMethod**)pFrame->startIp;
                stack = pFrame->pStack;
                ip = pFrame->startIp + sizeof(InterpMethod*) / sizeof(int32_t);
                pThreadContext->pStackPointer = stack + pMethod->allocaSize;
                break;
            }
            case INTOP_NEWOBJ:
            {
                returnOffset = ip[1];
                callArgsOffset = ip[2];
                methodSlot = ip[3];

                OBJECTREF objRef = AllocateObject((MethodTable*)pMethod->pDataItems[ip[4]]);

                // This is return value
                LOCAL_VAR(returnOffset, OBJECTREF) = objRef;
                // Set `this` arg for ctor call
                LOCAL_VAR (callArgsOffset, OBJECTREF) = objRef;
                ip += 5;

                goto CALL_INTERP_SLOT;
            }
            case INTOP_NEWOBJ_VT:
            {
                returnOffset = ip[1];
                callArgsOffset = ip[2];
                methodSlot = ip[3];

                int32_t vtSize = ip[4];
                void *vtThis = stack + returnOffset;

                // clear the valuetype
                memset(vtThis, 0, vtSize);
                // pass the address of the valuetype
                LOCAL_VAR(callArgsOffset, void*) = vtThis;

                ip += 5;
                goto CALL_INTERP_SLOT;
            }
            case INTOP_ZEROBLK_IMM:
                memset(LOCAL_VAR(ip[1], void*), 0, ip[2]);
                ip += 3;
                break;
            case INTOP_FAILFAST:
                assert(0);
                break;
            default:
                assert(0);
                break;
        }
    }

EXIT_FRAME:
    if (pFrame->pParent && pFrame->pParent->ip)
    {
        // Return to the main loop after a non-recursive interpreter call
        pFrame->ip = NULL;
        pFrame = pFrame->pParent;
        ip = pFrame->ip;
        stack = pFrame->pStack;
        pMethod = *(InterpMethod**)pFrame->startIp;
        pFrame->ip = NULL;

        pThreadContext->pStackPointer = pFrame->pStack + pMethod->allocaSize;
        goto MAIN_LOOP;
    }

    pThreadContext->pStackPointer = pFrame->pStack;
}

#endif // FEATURE_INTERPRETER
