#include "ACPI/APIC.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

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

bool APICTableParser::parse(ACPI::TableHeader* table) {
    if (memcmp(table->Signature, "APIC", 4) == 0) {
        processorCount = 0;
        MADT* madt = (MADT*) table;
        localAPICAddress = madt->localAPICAddress;
        EntryBase* entry = (EntryBase*) (madt + 1);
        uint64_t remainingSize = madt->header.Length - sizeof(MADT);
        while (remainingSize > 0) {
            if (entry->type == 0) {
                processorCount++;
            }
            remainingSize -= entry->size;
            entry = (EntryBase*) ((uint64_t) entry + entry->size);
        }
        Output::getDefault()->printf("Found %hhu processors\n", processorCount);
        acpiTable = table;
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