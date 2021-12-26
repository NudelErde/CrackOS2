#include "ACPI/APIC.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"
#include "CPUControl/time.hpp"
#include "Common/Symbols.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/tempMapping.hpp"

union RedirectionEntry {
    struct {
        uint64_t vector : 8;
        uint64_t delvMode : 3;
        uint64_t destMode : 1;
        uint64_t delvStatus : 1;
        uint64_t pinPolarity : 1;
        uint64_t remoteIRR : 1;
        uint64_t triggerMode : 1;
        uint64_t mask : 1;
        uint64_t reserved : 39;
        uint64_t destination : 8;
    };
    struct {
        uint32_t lowerDword;
        uint32_t upperDword;
    };
} __attribute__((packed));

static_assert(sizeof(RedirectionEntry) == sizeof(uint8_t) * 8, "RedirectionEntry is not 8 bytes");


static ACPI::TableHeader* acpiTable;
static uint64_t localAPICAddress;
static uint8_t processorCount;

struct APICTableParser : public ACPI::TableParser {
    bool parse(ACPI::TableHeader* table) override;
    const char* getSignature() override;
};

struct MADT {
    ACPI::TableHeader header;
    uint32_t localAPICAddress;
    uint32_t flags;
} __attribute__((packed));
static_assert(sizeof(MADT) == 44, "MADT is not 44 bytes");

struct EntryBase {
    uint8_t type;
    uint8_t size;
} __attribute__((packed));

static uint32_t cpuBitmap;

bool APICTableParser::parse(ACPI::TableHeader* table) {
    cpuBitmap = 0;
    if (memcmp(table->Signature, "APIC", 4) == 0) {
        processorCount = 0;
        MADT* madt = (MADT*) table;
        localAPICAddress = (uint64_t) TempMemory::mapPages(madt->localAPICAddress, 1, false);
        EntryBase* entry = (EntryBase*) (madt + 1);
        uint64_t remainingSize = madt->header.Length - sizeof(MADT);
        while (remainingSize > 0) {
            if (entry->type == 0) {
                struct ProcessorLocalAPIC {
                    EntryBase base;
                    uint8_t processorID;
                    uint8_t localAPICID;
                    uint32_t flags;
                } __attribute__((packed));
                static_assert(sizeof(ProcessorLocalAPIC) == 8, "ProcessorLocalAPIC is not 8 bytes");
                ProcessorLocalAPIC* processor = (ProcessorLocalAPIC*) entry;
                Output::getDefault()->printf("APIC: Processor %hhu, local APIC %hhu\n", processor->processorID, processor->localAPICID);
                processorCount++;
                cpuBitmap |= 1ull << processor->localAPICID;
            }
            remainingSize -= entry->size;
            entry = (EntryBase*) ((uint64_t) entry + entry->size);
        }
        Output::getDefault()->printf("APIC: Found %hhu processor(s)\n", processorCount);
        acpiTable = table;
        Interrupt::switchToAPICMode();
        return true;
    }
    return false;
}
const char* APICTableParser::getSignature() {
    return "APIC";
}

static uint8_t apicTableParserBuffer[sizeof(APICTableParser)];

ACPI::TableParser* APIC::getParser() {
    return new (apicTableParserBuffer) APICTableParser();
}

void APIC::set(uint64_t offset, uint32_t data) {
    uint32_t volatile* reg = (uint32_t volatile*) (localAPICAddress + offset);
    *reg = data;
}
uint32_t APIC::get(uint64_t offset) {
    uint32_t volatile* reg = (uint32_t volatile*) (localAPICAddress + offset);
    return *reg;
}

static uint32_t readIOApic(uint8_t* address, uint8_t offset) {
    uint32_t volatile* ioapic = (uint32_t volatile*) address;
    ioapic[0] = offset;
    return ioapic[4];
}

static void writeIOApic(uint8_t* address, uint8_t offset, uint32_t data) {
    uint32_t volatile* ioapic = (uint32_t volatile*) address;
    ioapic[0] = offset;
    ioapic[4] = data;
}

static uint32_t getBaseInterrupt(uint32_t interruptNumber) {
    EntryBase* entry = (EntryBase*) ((uint8_t*) acpiTable + sizeof(MADT));
    uint64_t remainingSize = acpiTable->Length - sizeof(MADT);
    while (remainingSize) {
        remainingSize -= entry->size;
        if (entry->type == 1) {
            struct IOAPIC {
                EntryBase header;
                uint8_t id;
                uint8_t reserved;
                uint32_t address;
                uint32_t globalSystemInterruptBase;
            } __attribute__((packed));
            IOAPIC* ioapic = (IOAPIC*) entry;
            uint32_t globalSystemInterruptBase = ioapic->globalSystemInterruptBase;
            if (globalSystemInterruptBase <= interruptNumber && globalSystemInterruptBase + 24 > interruptNumber) {
                uint8_t* address = TempMemory::mapPages(ioapic->address, 1, false);
                uint8_t redirectionCount = (readIOApic(address, 0x01) >> 16) & 0xFF;
                if (globalSystemInterruptBase + redirectionCount > interruptNumber) {
                    return globalSystemInterruptBase;
                }
            }
        }
        entry = (EntryBase*) ((uint64_t) entry + entry->size);
    }
    return 0;
}

static uint8_t* getIOAPICAddress(uint32_t interruptNumber) {
    EntryBase* entry = (EntryBase*) ((uint8_t*) acpiTable + sizeof(MADT));
    uint64_t remainingSize = acpiTable->Length - sizeof(MADT);
    while (remainingSize) {
        remainingSize -= entry->size;
        if (entry->type == 1) {
            struct IOAPIC {
                EntryBase header;
                uint8_t id;
                uint8_t reserved;
                uint32_t address;
                uint32_t globalSystemInterruptBase;
            } __attribute__((packed));
            IOAPIC* ioapic = (IOAPIC*) entry;
            uint32_t globalSystemInterruptBase = ioapic->globalSystemInterruptBase;
            if (globalSystemInterruptBase <= interruptNumber && globalSystemInterruptBase + 24 > interruptNumber) {
                uint8_t* address = TempMemory::mapPages(ioapic->address, 1, false);
                uint8_t redirectionCount = (readIOApic(address, 0x01) >> 16) & 0xFF;
                if (globalSystemInterruptBase + redirectionCount > interruptNumber) {
                    return address;
                }
            }
        }
        entry = (EntryBase*) ((uint64_t) entry + entry->size);
    }
    return nullptr;
}

void APIC::setSpuriousInterruptVector(uint8_t vector) {
    uint32_t current = get(0x0F0);
    set(0x0F0, ((uint32_t) vector) | 0x100 | current);
    //disable all ioapic interrupts
    EntryBase* entry = (EntryBase*) ((uint8_t*) acpiTable + sizeof(MADT));
    uint64_t remainingSize = acpiTable->Length - sizeof(MADT);
    while (remainingSize) {
        remainingSize -= entry->size;
        if (entry->type == 1) {
            struct IOAPIC {
                EntryBase header;
                uint8_t id;
                uint8_t reserved;
                uint32_t address;
                uint32_t globalSystemInterruptBase;
            } __attribute__((packed));
            IOAPIC* ioapic = (IOAPIC*) entry;
            uint32_t globalSystemInterruptBase = ioapic->globalSystemInterruptBase;
            uint8_t* address = TempMemory::mapPages(ioapic->address, 1, false);
            uint8_t redirectionCount = (readIOApic(address, 0x01) >> 16) & 0xFF;
            for (uint8_t i = 0; i < redirectionCount; i++) {
                RedirectionEntry entry;
                entry.lowerDword = readIOApic(address, 0x10 + i * 2);
                entry.upperDword = readIOApic(address, 0x10 + i * 2 + 1);
                entry.mask = 1;
                writeIOApic(address, 0x10 + i * 2, entry.lowerDword);
                writeIOApic(address, 0x10 + i * 2 + 1, entry.upperDword);
            }
        }
        entry = (EntryBase*) ((uint64_t) entry + entry->size);
    }
}

bool APIC::isInterruptEnabled(uint32_t interruptNumber) {
    uint8_t* address = getIOAPICAddress(interruptNumber);
    if (address == nullptr) {
        return false;
    }
    interruptNumber -= getBaseInterrupt(interruptNumber);
    RedirectionEntry entry;
    entry.lowerDword = readIOApic(address, 0x10 + interruptNumber * 2);
    entry.upperDword = readIOApic(address, 0x10 + interruptNumber * 2 + 1);
    return entry.mask == 0;
}
void APIC::setInterruptEnabled(uint32_t interruptNumber, bool enabled) {
    uint8_t* address = getIOAPICAddress(interruptNumber);
    if (address == nullptr) {
        return;
    }
    interruptNumber -= getBaseInterrupt(interruptNumber);
    RedirectionEntry entry;
    entry.lowerDword = readIOApic(address, 0x10 + interruptNumber * 2);
    entry.upperDword = readIOApic(address, 0x10 + interruptNumber * 2 + 1);

    entry.mask = enabled ? 0 : 1;

    writeIOApic(address, 0x10 + interruptNumber * 2, entry.lowerDword);
    writeIOApic(address, 0x10 + interruptNumber * 2 + 1, entry.upperDword);
}
void APIC::mapInterrupt(uint8_t interruptNumber, uint8_t resultVector, uint8_t cpuId) {
    uint8_t* address = getIOAPICAddress(interruptNumber);
    if (address == nullptr) {
        return;
    }
    interruptNumber -= getBaseInterrupt(interruptNumber);
    RedirectionEntry entry;
    entry.lowerDword = readIOApic(address, 0x10 + interruptNumber * 2);
    entry.upperDword = readIOApic(address, 0x10 + interruptNumber * 2 + 1);

    entry.vector = resultVector;
    entry.delvMode = 0;
    entry.destMode = 0;
    entry.delvStatus = 0;
    entry.pinPolarity = 0;
    entry.triggerMode = 0;
    entry.mask = 0;
    entry.destination = cpuId;

    writeIOApic(address, 0x10 + interruptNumber * 2, entry.lowerDword);
    writeIOApic(address, 0x10 + interruptNumber * 2 + 1, entry.upperDword);
}

uint32_t APIC::getSourceOverride(uint8_t oldSource) {
    EntryBase* entry = (EntryBase*) ((uint8_t*) acpiTable + sizeof(MADT));
    uint64_t remainingSize = acpiTable->Length - sizeof(MADT);
    while (remainingSize) {
        remainingSize -= entry->size;
        if (entry->type == 2) {
            struct SourceOverride {
                EntryBase header;
                uint8_t bus;
                uint8_t source;
                uint32_t globalSystemInterruptBase;
            } __attribute__((packed));
            SourceOverride* sourceOverride = (SourceOverride*) entry;
            if (sourceOverride->source == oldSource) {
                return (sourceOverride->globalSystemInterruptBase);
            }
        }
        entry = (EntryBase*) ((uint64_t) entry + entry->size);
    }
    return (uint32_t) ~0ull;
}

uint8_t APIC::getInterrupt(uint32_t hardwareInterrupt) {
    uint8_t* address = getIOAPICAddress(hardwareInterrupt);
    if (address == nullptr) {
        return 0xFF;
    }
    hardwareInterrupt -= getBaseInterrupt(hardwareInterrupt);
    RedirectionEntry entry;
    entry.lowerDword = readIOApic(address, 0x10 + hardwareInterrupt * 2);
    entry.upperDword = readIOApic(address, 0x10 + hardwareInterrupt * 2 + 1);

    return entry.vector;
}

uint8_t APIC::getCPUID() {
    return (uint8_t) (get(0x020) >> 24);
}

void APIC::sendEOI() {
    set(0xB0, 0);
}

void APIC::sendInterrupt(uint8_t vector, uint8_t destinationMode, uint8_t targetCpu, uint8_t targetSelector, bool deassert) {
    uint32_t target = get(0x310);
    target &= (0b1111ull << 24);
    target |= (targetCpu << 24);
    set(0x310, target);
    uint32_t command = 0;
    command |= (destinationMode << 8);
    command |= vector;
    command |= (targetSelector << 18);
    if (!deassert) {
        command |= (0b1 << 14);
    } else {
        command |= (0b1 << 15);
    }
    set(0x300, command);
    while (get(0x300) & (1 << 12)) {}
}

static bool secondaryCpuInInit;// this should be a mutex or stuff
static uint8_t secondaryCpuId;

static void initCPU(uint8_t cpuid, uint64_t entry) {
    using namespace time;
    while (secondaryCpuInInit) {}

    secondaryCpuInInit = true;
    APIC::sendInterrupt(entry / pageSize, 5, cpuid, 0, false);
    sleep(1ms);
    APIC::sendInterrupt(entry / pageSize, 5, cpuid, 0, true);
    sleep(1ms);
    Output::getDefault()->printf("APIC: Secondary CPU %hhu starting: ", cpuid);
    secondaryCpuId = cpuid;
    APIC::sendInterrupt(entry / pageSize, 6, cpuid, 0, false);
    sleep(100ms);
}

extern "C" void secondaryCpuMain() {
    //TODO allocate stack
    //TODO change stack
    //TODO add stuff that returns cpu specific memory
    uint8_t cpuid = secondaryCpuId;
    Output::getDefault()->printf("CPU %hhu started\n", cpuid);
    secondaryCpuInInit = false;
    stop();
}

void APIC::initAllCPUs() {
    uint64_t entry;
    saveReadSymbol("trampolineStart", entry);
    if (entry % pageSize != 0) {
        Output::getDefault()->print("Invalid trampoline start address!\n");
    } else {
        secondaryCpuInInit = false;
        for (uint64_t i = 0; i < 64; ++i) {
            if (cpuBitmap & (1ull << i)) {
                if (i == APIC::getCPUID()) {
                    continue;// do not init self
                }
                initCPU(i, entry);
            }
        }
    }
}