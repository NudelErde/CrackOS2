#include "ACPI/APIC.hpp"
#include "ACPI/multiboot.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"
#include "CPUControl/time.hpp"
#include "CPUControl/tss.hpp"
#include "Common/Symbols.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/heap.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "PCI/pci.hpp"
#include "stdint.h"

alignas(4096) char firstKernelStack[4096 * 4]{};

extern "C" void main(uint64_t multiboot) {
    Output::init();
    Output::getDefault()->clear();
    Output::getDefault()->setCursor(0, 0);
    setupTss(5);
    Interrupt::setupInterruptVectorTable();
    PageTable::init();
    readMultiboot(multiboot);
    ACPI::init();
    Interrupt::enableInterrupts();
    APIC::initAllCPUs();
    PCI::init();

    Output::getDefault()->print("Done!\n");

    stop();
}