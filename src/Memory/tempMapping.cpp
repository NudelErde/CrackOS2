#include "Memory/tempMapping.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "Common/Units.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"

uint8_t* TempMemory::mapPages(uint64_t physicalAddress, uint64_t count, bool user) {
    if (!user) {
        if (physicalAddress < 512Gi) {
            return (uint8_t*) (physicalAddress + 32Ti + 64Ti);
        } else {
            Output::getDefault()->printf("Physical address %llx is too high for TempMemory::mapPages\n", physicalAddress);
            stop();
        }
    }
    uint64_t baseAddress = 32Ti;
    uint64_t virtualAddress = baseAddress + physicalAddress;
    for (uint64_t i = 0; i < count; i++) {
        if (PageTable::getPhysicalAddress(virtualAddress) != physicalAddress || physicalAddress == 0) {
            PageTable::map(physicalAddress + i * pageSize, (void*) (virtualAddress + i * pageSize),
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