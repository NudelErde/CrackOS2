#pragma once

#include <stdint.h>

class TempMemory {
public:
    /**
     * @brief Maps a physical page to a unique virtual address.
     * @param physicalAddress the physical address of the page to map.
     * @param count the number of pages to map.
     * @param user whether the page is user accessible.
     * @return the virtual address of the mapped page.
     * @note The mapped page is not initialized.
     * @note The mapped physical memory region is not marked as used.
     */
    static uint8_t* mapPages(uint64_t physicalAddress, uint64_t count, bool user);
};