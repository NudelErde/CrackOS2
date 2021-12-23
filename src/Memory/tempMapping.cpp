#include "Memory/tempMapping.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "Common/Units.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"

uint8_t* TempMemory::mapPages(uint64_t physicalAddress, uint64_t count, bool user) {
    if (!user) {
        return (uint8_t*) (physicalAddress + 32Ti + 64Ti);
    }
    uint64_t baseAddress = 32Ti;
    uint64_t virtualAddress = baseAddress + physicalAddress;
    for (uint64_t i = 0; i < count; i++) {
        if (PageTable::getPhysicalAddress(virtualAddress) != physicalAddress || physicalAddress == 0) {
            PageTable::map(physicalAddress + i * pageSize, virtualAddress + i * pageSize,
                           {
                                   .writeEnable = true,  //
                                   .userAvailable = true,//
                                   .writeThrough = true, //
                                   .cacheDisable = true, //
                                   .executeDisable = true//
                           });
        }
    }
    return (uint8_t*) (virtualAddress | (physicalAddress & 0xFFF));
}