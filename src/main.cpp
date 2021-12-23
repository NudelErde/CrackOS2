#include "ACPI/multiboot.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "stdint.h"

alignas(4096) char firstKernelStack[4096]{};

extern "C" void main(uint64_t multiboot) {
    Output::init();
    Output::getDefault()->clear();
    Output::getDefault()->setCursor(0, 0);
    Output::getDefault()->print("Output initialized\n");
    Interrupt::setupInterruptVectorTable();
    Output::getDefault()->print("Interrupts initialized\n");
    PageTable::init();
    Output::getDefault()->print("Page table initialized\n");
    readMultiboot(multiboot);
    Output::getDefault()->print("Multiboot read\n");

    stop();
}