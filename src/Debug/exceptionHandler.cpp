#include "Debug/exceptionHandler.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "Memory/tempMapping.hpp"

void onPageFault(Interrupt& inter) {
    auto out = Output::getDefault();
    //out->clear();
    //out->setCursor(0, 0);
    uint64_t cr2 = getCR2();
    out->printf("Page fault at %llx\n", cr2);
    out->printf("Error code: %llx\n", inter.getErrorCode());
    //decode error code
    uint64_t errorCode = inter.getErrorCode();
    bool present = errorCode & (1 << 0);
    bool write = errorCode & (1 << 1);
    bool user = errorCode & (1 << 2);
    bool reserved = errorCode & (1 << 3);
    bool id = errorCode & (1 << 4);
    if (!present) {
        out->print("Error caused by a page not present\n");
    }
    if (write) {
        out->print("Instruction was a write\n");
    } else {
        out->print("Instruction was a read\n");
    }
    if (user) {
        out->print("Instruction was in user mode\n");
    }
    if (reserved) {
        out->print("Error caused by a reserved bit\n");
    }
    if (id) {
        out->print("Page fault was during a instruction fetch\n");
    }
    stop();
}

void onInvalidOpcode(Interrupt& inter) {
    uint64_t instructionAddress = inter.getInstruction();
    auto out = Output::getDefault();
    out->printf("Invalid opcode at %llx\n", instructionAddress);
    uint64_t phyAddress = PageTable::getPhysicalAddress(instructionAddress);
    out->printf("Physical address: %llx\n", phyAddress);
    if ((phyAddress & ~(pageSize - 1ull)) != 0) {
        uint8_t* address = TempMemory::mapPages(phyAddress, 1, false);
        out->printf("Memory from: %p to %p\n", (void*) (address - pageSize / 2), (void*) (address + pageSize / 2));
        out->printHexDump(address - pageSize / 2, pageSize);
    }
    stop();
}