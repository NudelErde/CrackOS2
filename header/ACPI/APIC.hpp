#pragma once

#include "ACPI/ACPI.hpp"
#include <stdint.h>

class APIC {
public:
    static ACPI::TableParser* getParser();
    static void setSpuriousInterruptVector(uint8_t vector);
    static void set(uint64_t offset, uint32_t data);
    static uint32_t get(uint64_t offset);
    static uint32_t getSourceOverride(uint8_t oldSource);
    static uint8_t getInterrupt(uint32_t hardwareInterrupt);
    static bool isInterruptEnabled(uint32_t interruptNumber);
    static void setInterruptEnabled(uint32_t interruptNumber, bool enabled);
    static void mapInterrupt(uint8_t interruptNum, uint8_t resultVector, uint8_t cpuId);
    static uint8_t getCPUID();
    static void sendEOI();
};