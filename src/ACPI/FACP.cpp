#include "ACPI/FACP.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

struct FACPTableParser : public ACPI::TableParser {
    bool parse(ACPI::TableHeader* table) override;

    const char* getSignature() override { return "FACP"; }
};

struct GenericAddressStructure {
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
} __attribute__((packed));
static_assert(sizeof(GenericAddressStructure) == 12, "GenericAddressStructure size is wrong");

struct FACPTable {
    ACPI::TableHeader header;
    uint32_t firmwareCtrl;
    uint32_t dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t reserved;

    uint8_t preferredPowerManagementProfile;
    uint16_t sCI_Interrupt;
    uint32_t sMI_CommandPort;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4BIOS_REQ;
    uint8_t pSTATE_Control;
    uint32_t pM1aEventBlock;
    uint32_t pM1bEventBlock;
    uint32_t pM1aControlBlock;
    uint32_t pM1bControlBlock;
    uint32_t pM2ControlBlock;
    uint32_t pMTimerBlock;
    uint32_t gPE0Block;
    uint32_t gPE1Block;
    uint8_t pM1EventLength;
    uint8_t pM1ControlLength;
    uint8_t pM2ControlLength;
    uint8_t pMTimerLength;
    uint8_t gPE0Length;
    uint8_t gPE1Length;
    uint8_t gPE1Base;
    uint8_t cStateControl;
    uint16_t worstC2Latency;
    uint16_t worstC3Latency;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlarm;
    uint8_t monthAlarm;
    uint8_t century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t bootArchitectureFlags;

    uint8_t reserved2;
    uint32_t flags;

    // 12 byte structure; see below for details
    GenericAddressStructure resetReg;// total size = 128

    uint8_t resetValue;
    uint8_t reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t x_FirmwareControl;
    uint64_t x_Dsdt;

    GenericAddressStructure x_PM1aEventBlock;
    GenericAddressStructure x_PM1bEventBlock;
    GenericAddressStructure x_PM1aControlBlock;
    GenericAddressStructure x_PM1bControlBlock;
    GenericAddressStructure x_PM2ControlBlock;
    GenericAddressStructure x_PMTimerBlock;
    GenericAddressStructure x_GPE0Block;
    GenericAddressStructure x_GPE1Block;
} __attribute__((packed));
static_assert(sizeof(FACPTable) == 244, "FACP table size is wrong");

static uint8_t facpBufferParser[sizeof(FACPTableParser)];

static FACPTable* facpTable;

ACPI::TableParser* FACP::getParser() {
    return new (facpBufferParser) FACPTableParser();
}

bool FACPTableParser::parse(ACPI::TableHeader* table) {
    if (memcmp(table->Signature, "FACP", 4) == 0) {
        FACPTable* facp = (FACPTable*) table;
        const char* powerNames[]{"Unspcified", "Desktop", "Mobile", "Workstation", "Enterprise Server", "SOHO Server", "APM Server", "PtP Server", "Wake-on-LAN", "Sleep-on-LAN", "Virtual Desktop", "Mixed Mode", "Max"};
        uint8_t index = facp->preferredPowerManagementProfile;
        if (index > sizeof(powerNames) / sizeof(char*)) {
            index = 0;
        }
        if (index != 0) {
            Output::getDefault()->printf("FACP: Preferred Power Management Profile: %s (%hhd)\n", powerNames[index], facp->preferredPowerManagementProfile);
        }
        facpTable = facp;
        return true;
    }
    return false;
}
