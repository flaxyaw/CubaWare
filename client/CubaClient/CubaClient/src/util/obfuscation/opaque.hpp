//entire thing made by __cpuid, and ai?
//seeding off GetTickCount so IDA can't resolve the branch at analysis time, L hexrays
#pragma once

#include <windows.h>

//per build random constants injected by CMake, every binary is bytecode-unique.
#ifndef POLY_K1
#define POLY_K1 0x1337u
#endif
#ifndef POLY_K2
#define POLY_K2 0xABCDu
#endif
#ifndef POLY_K3
#define POLY_K3 0x5A3Cu
#endif
#ifndef POLY_K4
#define POLY_K4 0xF00Du
#endif

namespace opaque {
    //set to GetTickCount() at startup, nonzero, unknown to IDA
    extern volatile DWORD _seed;
}

//mba: 2*(x & ~x) + (x ^ ~x) = 2*0 + 0xFFFFFFFF = 0xFFFFFFFF != 0, always true
//POLY_K1/K2 differ per build so the generated constant in .text changes every compile
#define OP_TRUE() \
    ((2u * (opaque::_seed & (~opaque::_seed)) + (opaque::_seed ^ (~opaque::_seed)) + POLY_K1) != POLY_K1 - 1u)

//mba: (x & ~x) = 0 always
#define OP_FALSE() \
    (((opaque::_seed & (~opaque::_seed)) ^ POLY_K2) == POLY_K2 + 1u)

//generates register pressure and fake CFG edges IDA cant resolve
#define OPAQUE_JUNK() do { \
    volatile DWORD _j = opaque::_seed ^ POLY_K1; \
    _j = (_j * 0x6Bu ^ (_j >> 17u)) ^ POLY_K2; \
    _j = (_j + POLY_K3) * POLY_K4; \
    (void)_j; \
} while (0)

//heavier variant -> MBA chain back through seed, 6+ instructions in output
#define OPAQUE_JUNK_HEAVY() do { \
    volatile DWORD _a = opaque::_seed ^ POLY_K4; \
    volatile DWORD _b = ~_a; \
    volatile DWORD _c = (_a & _b); \
    volatile DWORD _d = (_a | _b) ^ POLY_K3; \
    _d = (_d * POLY_K1) ^ (_c + POLY_K2); \
    (void)_d; \
} while (0)
