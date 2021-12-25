#include "ACPI/APIC.hpp"
#include "ACPI/multiboot.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"
#include "CPUControl/time.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/heap.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "PCI/pci.hpp"
#include "stdint.h"

alignas(4096) char firstKernelStack[4096]{};

extern "C" void main(uint64_t multiboot) {
    using namespace time;
    Output::init();
    Output::getDefault()->clear();
    Output::getDefault()->setCursor(0, 0);
    Interrupt::setupInterruptVectorTable();
    PageTable::init();
    readMultiboot(multiboot);
    ACPI::init();
    PCI::init();
    Output::getDefault()->print("Start 5000ms sleep\n");
    sleep(5000ms);
    Output::getDefault()->print("Slept 5000ms\n");

    Output::getDefault()->print("Done!\n");

    stop();
}