#include "PCI/pci.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

class PCI_ACPI_TableParser : public ACPI::TableParser {
public:
    void parse(uint8_t* table) override;
    const char* getSignature() override;
};

void PCI_ACPI_TableParser::parse(uint8_t* table) {
    //sanity check if table is valid
    if (memcmp(table, "MCFG", 4) != 0) {
        return;
    }
}

const char* PCI_ACPI_TableParser::getSignature() {
    return "MCFG";
}

static uint8_t defaultBuffer[sizeof(PCI_ACPI_TableParser)];

ACPI::TableParser* PCI::getACPITableParser() {
    return new ((void*) defaultBuffer) PCI_ACPI_TableParser();
}