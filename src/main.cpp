#include "ACPI/multiboot.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "PCI/pci.hpp"
#include "stdint.h"

alignas(4096) char firstKernelStack[4096]{};

extern "C" void main(uint64_t multiboot) {
    Output::init();
    Output::getDefault()->clear();
    Output::getDefault()->setCursor(0, 0);
    Interrupt::setupInterruptVectorTable();
    PageTable::init();
    readMultiboot(multiboot);
    ACPI::init();
    PCI::iterateDevices([](PCI& device, void*) {
        Output::getDefault()->printf("Class: %hhx:%hhx:%hhx  ", device.classCode, device.subclassCode, device.progIF);
        Output::getDefault()->printf("Vendor: %hx:%hx  ", device.vendorID, device.deviceID);
        Output::getDefault()->printf("Location: %hhx:%hhx.%hhx\n", device.bus, device.device, device.function);
    },
                        nullptr);

    Output::getDefault()->print("Done!\n");

    stop();
}