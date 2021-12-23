#include "ACPI/ACPI.hpp"
#include "ACPI/multiboot.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/tempMapping.hpp"
#include "PCI/pci.hpp"

//https://uefi.org/sites/default/files/resources/ACPI_6_3_final_Jan30.pdf

void ACPI::init() {
    ACPI::TableParser* buffer[4];
    ACPI acpi(buffer, 4);
    acpi.addTableParser(PCI::getACPITableParser());

    acpi.parse(getRsdtAddress());
}

void ACPI::parse(uint64_t table) {
    //should start with signature
    uint8_t* ptr = TempMemory::mapPages(table, 1, false);
    ACPI::TableHeader* header = (ACPI::TableHeader*) ptr;

    uint8_t pointerSize = 0;
    uint64_t pointerMask = 0;

    if (memcmp(header->Signature, "RSDT", 4) == 0) {
        pointerSize = 4;
        pointerMask = 0xFFFFFFFFull;
    } else if (memcmp(header->Signature, "XSDT", 4) == 0) {
        pointerSize = 8;
        pointerMask = 0xFFFFFFFFFFFFFFFFull;
    } else {
        return;
    }

    uint64_t remaining = header->Length - sizeof(ACPI::TableHeader);
    ptr += sizeof(ACPI::TableHeader);
    for (; remaining > 0; remaining -= pointerSize, ptr += pointerSize) {
        uint64_t pointer = *(uint64_t*) ptr;
        pointer &= pointerMask;
        ACPI::TableHeader* table = (ACPI::TableHeader*) TempMemory::mapPages(pointer, 1, false);
        bool handled = false;
        for (uint64_t i = 0; i < tablesSize; i++) {
            if (tables[i] == 0 || tables[i]->getSignature() == nullptr) {
                continue;
            }
            if (memcmp(table->Signature, tables[i]->getSignature(), 4) == 0) {
                if (tables[i]->parse(table)) {
                    handled = true;
                }
            }
        }
        char buffer[5]{};
        memcpy(buffer, table->Signature, 4);
        buffer[4] = 0;
        Output::getDefault()->printf("Found ACPI table: %s (%s)\n", buffer, handled ? "handled" : "unhandled");
    }
}