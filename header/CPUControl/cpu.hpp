#pragma once

#include <stdint.h>

[[noreturn]] void stop();
void halt();

extern "C" uint64_t getRSP();
extern "C" uint64_t getRBP();
extern "C" uint64_t getRIP();

extern "C" uint64_t getCR0();
extern "C" uint64_t getCR2();
extern "C" uint64_t getCR3();
extern "C" uint64_t getCR4();
extern "C" uint64_t getCR8();

extern "C" uint64_t getDR0();
extern "C" uint64_t getDR1();
extern "C" uint64_t getDR2();
extern "C" uint64_t getDR3();
extern "C" uint64_t getDR6();
extern "C" uint64_t getDR7();

extern "C" uint64_t getRAX();
extern "C" uint64_t getRBX();
extern "C" uint64_t getRCX();
extern "C" uint64_t getRDX();
extern "C" uint64_t getRSI();
extern "C" uint64_t getRDI();
extern "C" uint64_t getR8();
extern "C" uint64_t getR9();
extern "C" uint64_t getR10();
extern "C" uint64_t getR11();
extern "C" uint64_t getR12();
extern "C" uint64_t getR13();
extern "C" uint64_t getR14();
extern "C" uint64_t getR15();

extern "C" uint64_t getFlags();
extern "C" uint64_t getPriviledgeLevel();

extern "C" void invalidatePage(uint64_t address);

static inline void cpuid(uint64_t code, uint64_t* a, uint64_t* b, uint64_t* c, uint64_t* d) {
    asm volatile("cpuid"
                 : "=a"(*a), "=b"(*b),
                   "=c"(*c), "=d"(*d)
                 : "a"(code));
}