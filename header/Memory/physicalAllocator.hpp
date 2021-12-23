#pragma once
#include "ACPI/ACPI.hpp"
#include "stdint.h"

class PhysicalAllocator {
public:
    static void readMultibootInfos(uint8_t* ptr);

    static uint64_t getTotalMemorySize();
    static uint64_t getMaxAddress();
    static uint64_t getUsedMemorySize();

    /**
     * @brief Allocates physical memory.
     * @param count the number of pages to allocate.
     * @return the physical address of the allocated memory.
     * @note The allocated memory is not initialized.
     * @note The allocated memory is marked as used.
     */
    static uint64_t allocatePhysicalMemory(uint64_t count);

    /**
     * @brief Frees physical memory.
     * @param address the physical address of the memory to free.
     * @param count the number of pages to free.
     * @note The freed memory is marked as unused.
     */
    static void freePhysicalMemory(uint64_t address, uint64_t count);
};