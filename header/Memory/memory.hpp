#pragma once

#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "LanguageFeatures/SmartPointer.hpp"
#include "LanguageFeatures/Types.hpp"
#include <stdint.h>

constexpr uint64_t pageSize = 0x1000;

struct SegmentSelector {
    uint8_t requestedPrivilegeLevel : 2;
    bool inLocalTable : 1;
    uint16_t index : 13;
};
static_assert(sizeof(SegmentSelector) == sizeof(uint16_t));

enum class Segment {
    KERNEL_CODE = 1,
    KERNEL_DATA = 2,
    USER_CODE = 3,
    USER_DATA = 4,
};

inline SegmentSelector toSegmentSelector(Segment segment) {
    SegmentSelector selector;
    if (segment < Segment::USER_CODE) {
        selector.requestedPrivilegeLevel = 0;
    } else {
        selector.requestedPrivilegeLevel = 3;
    }
    selector.inLocalTable = false;
    selector.index = (uint16_t) segment;
    return selector;
}

inline void out8(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint8_t in8(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

inline void out16(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint16_t in16(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

inline void out32(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint32_t in32(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80"
                 :
                 : "a"(0));
}

[[nodiscard]] inline void* operator new(uint64_t size, void* ptr) noexcept {
    return ptr;
}
