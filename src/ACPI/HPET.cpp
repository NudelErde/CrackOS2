#include "ACPI/HPET.hpp"
#include "ACPI/APIC.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/tempMapping.hpp"
#include "stdint.h"

struct HPETParser : public ACPI::TableParser {
    bool parse(ACPI::TableHeader* table);
    const char* getSignature();
};

const char* HPETParser::getSignature() {
    return "HPET";
}

struct address_structure {
    uint8_t address_space_id;// 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed));

struct HPETTable {
    ACPI::TableHeader header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count : 5;
    uint8_t counter_size : 1;
    uint8_t reserved : 1;
    uint8_t legacy_replacement : 1;
    uint16_t pci_vendor_id;
    address_structure address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

static HPETTable* hpetTable;
static uint8_t* hpetAddress;
static uint64_t tickTime;
static uint8_t numberOfCounters;

bool HPETParser::parse(ACPI::TableHeader* table) {
    if (memcmp(table->Signature, "HPET", 4) != 0) {
        return false;
    }
    hpetTable = (HPETTable*) table;
    hpetAddress = TempMemory::mapPages(hpetTable->address.address, 1, false);
    if (hpetTable->address.address_space_id != 0) {
        Output::getDefault()->print("HPET: Unsupported address space\n");
        return false;
    }

    uint32_t counterPeriod = (uint32_t) ((*(uint64_t*) (hpetAddress)) >> 32);
    numberOfCounters = (((*(uint32_t*) hpetAddress) >> 8) & 0x1F) + 1;
    tickTime = counterPeriod;
    bool bit64 = (*(uint32_t*) hpetAddress) & (1 << 13);
    bool legacy = (*(uint64_t*) hpetAddress) & (1 << 15);

    Output::getDefault()->printf("HPET: %hhu counters, %uns/tick\n", numberOfCounters, counterPeriod / 1000000);
    Output::getDefault()->printf("HPET: %s-bit mode\n", bit64 ? "64" : "32");
    Output::getDefault()->printf("HPET: supports legacy replacement: %s\n", legacy ? "yes" : "no");
    //disable hpet legacy replacement and enable counter
    *(uint64_t*) (hpetAddress + 0x10) = 0b1;

    for (uint8_t i = 0; i < numberOfCounters; i++) {
        uint64_t config = *(uint64_t*) (hpetAddress + 0x100 + 0x20 * i);
        config &= ~0b11111100001110;
        config |= ((24 - 1 - i) << 9);
        *(uint64_t*) (hpetAddress + 0x100 + 0x20 * i) = config;
    }

    return true;
}

void HPET::setTimer(time::nanosecond nanoseconds, uint8_t timer, uint8_t interrupt) {
    if (timer >= numberOfCounters) {
        return;
    }
    APIC::mapInterrupt((24 - 1 - timer), interrupt, APIC::getCPUID());
    uint64_t config = *(uint64_t*) (hpetAddress + 0x100 + 0x20 * timer);
    uint64_t clockTicks = nanoseconds.value * 1000000 / tickTime;
    uint64_t currentTime = *(uint64_t*) (hpetAddress + 0x0F0);
    *(uint64_t*) (hpetAddress + 0x108 + 0x20 * timer) = clockTicks + currentTime;
    config |= 0b100;// enable interrupts
    *(uint64_t*) (hpetAddress + 0x100 + 0x20 * timer) = config;
}

static uint8_t hpetParserBuffer[sizeof(HPETParser)];

ACPI::TableParser* HPET::getParser() {
    return new (hpetParserBuffer) HPETParser();
}