#include "PCI/pci.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

static uint8_t* configurationSpaceBaseAddressAllocationStructures = 0;
static uint64_t configuartionSapceBaseAddressAloocationStructuresLength = 0;

class PCI_ACPI_TableParser : public ACPI::TableParser {
public:
    bool parse(ACPI::TableHeader* table) override;
    const char* getSignature() override;
};

bool PCI_ACPI_TableParser::parse(ACPI::TableHeader* table) {
    //check if table is valid because never trust your code
    if (memcmp(table->Signature, "MCFG", 4) != 0) {
        return false;
    }

    configurationSpaceBaseAddressAllocationStructures = (uint8_t*) (table + 1);
    configuartionSapceBaseAddressAloocationStructuresLength = (table->Length - sizeof(ACPI::TableHeader)) / 16;
    Output::getDefault()->printf("Found PCI configuration with %u segment(s)\n", configuartionSapceBaseAddressAloocationStructuresLength);
    return true;
}

const char* PCI_ACPI_TableParser::getSignature() {
    return "MCFG";
}

static uint8_t defaultBuffer[sizeof(PCI_ACPI_TableParser)];

ACPI::TableParser* PCI::getACPITableParser() {
    return new ((void*) defaultBuffer) PCI_ACPI_TableParser();
}