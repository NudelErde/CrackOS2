#pragma once

#include <stdint.h>

class PageTable {
public:
    struct Option {
        bool writeEnable;
        bool userAvailable;
        bool writeThrough;
        bool cacheDisable;
        bool executeDisable;
    };

    /**
     * @brief Maps a physical page to a given virtual address.
     * @param physicalAddress the physical address of the page to map.
     * @param virtualAddress the virtual address to map the page to.
     * @param options the options to apply to the mapping.
     * @note The mapped page is not initialized.
     * @note The mapped physical memory region is not marked as used.
     */
    static void map(uint64_t physicalAddress, void* virtualAddress, Option option = {});

    /**
     * @brief Returns the physical address that the given virtual address maps to.
     * @param virtualAddress the virtual address to get the physical address of.
     * @return the physical address that the given virtual address maps to or 0 if the virtual address is not mapped.
     */
    static uint64_t getPhysicalAddress(uint64_t virtualAddress);

    /**
     * @brief unmaps the given virtual address.
     * @param virtualAddress the virtual address to unmap.
     * @note The unmapped physical memory region is not marked as unused.
     */
    static void unmap(uint64_t virtualAddress);
    static void init();
};