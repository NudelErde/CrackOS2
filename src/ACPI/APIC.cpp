#include "ACPI/APIC.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

static ACPI::TableHeader* acpiTable;

struct APICTableParser : public ACPI::TableParser {
    bool parse(ACPI::TableHeader* table) override;
    const char* getSignature() override;
};

bool APICTableParser::parse(ACPI::TableHeader* table) {
    if (memcmp(table->Signature, "APIC", 4) == 0) {
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