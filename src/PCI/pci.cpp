#include "PCI/pci.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/tempMapping.hpp"
#include "Storage/SATA.hpp"

//------------------------------------------------------------------------------
//-----------------------[  Get PCI configuartion space ]-----------------------
//------------------------------------------------------------------------------

struct ConfigurationSpaceBaseAddress {
    uint64_t baseAddress;
    uint16_t segmentGroupNumber;
    uint8_t startBus;
    uint8_t endBus;
    uint8_t reserved[4];
} __attribute__((packed));
static_assert(sizeof(ConfigurationSpaceBaseAddress) == 16, "ConfigurationSpaceBaseAddress is not 16 bytes");

static ConfigurationSpaceBaseAddress* baseAddresses = 0;
static uint16_t baseAddressesCount = 0;

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
    // has extra 8 bytes as padding
    baseAddresses = (ConfigurationSpaceBaseAddress*) (((uint8_t*) table) + sizeof(ACPI::TableHeader) + 8);
    baseAddressesCount = (table->Length - sizeof(ACPI::TableHeader) - 8) / 16;
    return true;
}

const char* PCI_ACPI_TableParser::getSignature() {
    return "MCFG";
}

static uint8_t defaultBuffer[sizeof(PCI_ACPI_TableParser)];

ACPI::TableParser* PCI::getACPITableParser() {
    return new ((void*) defaultBuffer) PCI_ACPI_TableParser();
}

//------------------------------------------------------------------------------
//------------------------[ Init and discover devices  ]------------------------
//------------------------------------------------------------------------------

static uint64_t getConfigurationSpace(uint8_t bus, uint8_t device, uint8_t function) {
    for (uint16_t i = 0; i < baseAddressesCount; i++) {
        // bus < baseAddresses[i].endBus could also be bus <= baseAddresses[i].endBus
        // i don't know and i didn't find any documentation about this so i just guessed
        if (baseAddresses[i].startBus <= bus && bus <= baseAddresses[i].endBus) {
            uint64_t base = baseAddresses[i].baseAddress + ((bus - baseAddresses[i].startBus) << 20) + (device << 15) + (function << 12);
            return base;
        }
    }
    return 0;
}

//reads all devices on a bus and calls it self recursively for all sub-buses
static void iterateDevicesHelperBus(uint8_t bus, PCI::Callback callback, void* context);

//reads all the functions of the device and calls the callback for each existing function
static void iterateDevicesHelperDevice(uint8_t bus, uint8_t device, PCI::Callback callback, void* context);

//reads the header and calls the callback if the function is present
static void iterateDevicesHelperFunction(uint8_t bus, uint8_t device, uint8_t function, PCI::Callback callback, void* context);

static void iterateDevicesHelperFunction(uint8_t bus, uint8_t device, uint8_t function, PCI::Callback callback, void* context) {
    PCI pciDevice;
    pciDevice.bus = bus;
    pciDevice.device = device;
    pciDevice.function = function;
    pciDevice.physicalAddress = getConfigurationSpace(bus, device, function);
    uint8_t* ptr = TempMemory::mapPages(pciDevice.physicalAddress, 1, false);// configuration space is exactly one page (4KiB)
    uint16_t vendor = pciDevice.readConfigWord(0);
    if (vendor == 0xFFFF) {
        return;// device not present
    }

    pciDevice.vendorID = vendor;
    pciDevice.deviceID = pciDevice.readConfigWord(2);

    pciDevice.revisionID = pciDevice.readConfigByte(8);
    pciDevice.progIF = pciDevice.readConfigByte(9);
    pciDevice.subclassCode = pciDevice.readConfigByte(10);
    pciDevice.classCode = pciDevice.readConfigByte(11);
    pciDevice.headerType = pciDevice.readConfigByte(14);

    callback(pciDevice, context);
}

struct Info {
    PCI::Callback outerCallback;
    void* outerContext;
};

static void iterateDevicesHelperDevice(uint8_t bus, uint8_t device, PCI::Callback callback, void* context) {
    Info info;
    info.outerCallback = callback;
    info.outerContext = context;
    iterateDevicesHelperFunction(
            bus, device, 0, [](PCI& pciDevice, void* context) {
                Info* info = (Info*) context;
                info->outerCallback(pciDevice, info->outerContext);
                if (pciDevice.headerType & 0x80) {
                    for (uint8_t function = 1; function < 8; function++) {
                        iterateDevicesHelperFunction(pciDevice.bus, pciDevice.device, function, info->outerCallback, info->outerContext);
                    }
                }
            },
            &info);
}

static void iterateDevicesHelperBus(uint8_t bus, PCI::Callback callback, void* context) {
    Info info;
    info.outerCallback = callback;
    info.outerContext = context;
    for (uint8_t device = 0; device < 32; device++) {
        iterateDevicesHelperDevice(
                bus, device, [](PCI& pciDevice, void* context) {
                    Info* info = (Info*) context;
                    //check if the device is a bridge
                    if (pciDevice.classCode == 0x06 && pciDevice.subclassCode == 0x04) {
                        //it is a bridge
                        uint8_t secondaryBus = pciDevice.readConfigByte(0x19);
                        iterateDevicesHelperBus(secondaryBus, info->outerCallback, info->outerContext);
                    } else {
                        info->outerCallback(pciDevice, info->outerContext);
                    }
                },
                &info);
    }
}

void PCI::iterateDevices(Callback callback, void* context) {
    Info info;
    info.outerCallback = callback;
    info.outerContext = context;
    for (uint64_t i = 0; i < baseAddressesCount; ++i) {
        //read start bridge
        iterateDevicesHelperFunction(
                baseAddresses[i].startBus, 0, 0,
                [](PCI& pci, void* context) {
                    Info* myInfo = (Info*) context;
                    uint8_t busCount = 1;
                    //check if device is multi-function
                    if (pci.headerType & 0x80) {
                        busCount = 8;
                    }
                    //read all buses
                    for (uint8_t bus = 0; bus < busCount; ++bus) {
                        iterateDevicesHelperBus(bus, myInfo->outerCallback, myInfo->outerContext);
                    }
                },
                &info);
    }
}

PCI::Handler* first;

void PCI::addHandler(PCI::Handler* handler) {
    handler->next = first;
    first = handler;
}

void PCI::init() {
    first = nullptr;
    PCI::addHandler(SATA::getPCIHandler());
    Output::getDefault()->printf("PCI: Class       Vendor      Location\n");
    PCI::iterateDevices([](PCI& device, void*) {
        Output::getDefault()->printf("     %2hhx:%2hhx:%2hhx    %4hx:%4hx   %2hhx:%2hhx.%1hhx\n",
                                     device.classCode, device.subclassCode, device.progIF,
                                     device.vendorID, device.deviceID,
                                     device.bus, device.device, device.function);
        for (PCI::Handler* handler = first; handler != nullptr; handler = handler->next) {
            handler->onDeviceFound(device);
        }
    },
                        nullptr);
}

//------------------------------------------------------------------------------
//----------------------[ Configuartion space access ]--------------------------
//------------------------------------------------------------------------------

uint8_t PCI::readConfigByte(uint64_t offset) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    return ptr[offset];
}
uint16_t PCI::readConfigWord(uint64_t offset) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    return *(uint16_t*) (ptr + offset);
}
uint32_t PCI::readConfigDWord(uint64_t offset) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    return *(uint32_t*) (ptr + offset);
}
uint64_t PCI::readConfigQWord(uint64_t offset) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    return *(uint64_t*) (ptr + offset);
}
void PCI::writeConfigByte(uint64_t offset, uint8_t value) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    ptr[offset] = value;
}
void PCI::writeConfigWord(uint64_t offset, uint16_t value) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    *(uint16_t*) (ptr + offset) = value;
}
void PCI::writeConfigDWord(uint64_t offset, uint32_t value) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    *(uint32_t*) (ptr + offset) = value;
}
void PCI::writeConfigQWord(uint64_t offset, uint64_t value) {
    uint8_t* ptr = TempMemory::mapPages(physicalAddress, 1, false);
    *(uint64_t*) (ptr + offset) = value;
}

PCI::BAR PCI::getBar(uint8_t index) {
    if (index > 5) {
        return BAR{0, 0};
    }
    uint64_t address = readConfigDWord(0x10 + index * 4);
    if (address == 0) {
        return BAR{0, 0};
    }
    //if memory and 64bit extend address by 4 bytes in next bar
    if ((address & 0b101) == 0b100) {//bit 0 must be off = memory, bit 2 must be on = 64bit
        address |= ((uint64_t) readConfigDWord(0x10 + index * 4 + 4)) << 32;
    }
    if (address & 0b1) {// io
        return BAR{address & ~0b11ull, (uint8_t) (address & 0b11ull)};
    } else {
        uint64_t memory = address & ~0b1111ull;
        return BAR{(uint64_t) TempMemory::mapPages(memory, 1, false), (uint8_t) (address & 0b1111ull)};
    }
}

uint8_t PCI::BAR::readByte(uint64_t offset) {
    if (isIO()) {
        return in8((uint16_t) (offset + baseAddress));
    } else {
        return *(uint8_t*) (baseAddress + offset);
    }
}
uint16_t PCI::BAR::readWord(uint64_t offset) {
    if (isIO()) {
        return in16((uint16_t) (offset + baseAddress));
    } else {
        return *(uint16_t*) (baseAddress + offset);
    }
}
uint32_t PCI::BAR::readDWord(uint64_t offset) {
    if (isIO()) {
        return in32((uint16_t) (offset + baseAddress));
    } else {
        return *(uint32_t*) (baseAddress + offset);
    }
}
uint64_t PCI::BAR::readQWord(uint64_t offset) {
    if (isIO()) {
        return in32((uint16_t) (offset + baseAddress)) || ((uint64_t) in32((uint16_t) (offset + baseAddress + 4)) << 32);
    } else {
        return *(uint64_t*) (baseAddress + offset);
    }
}
void PCI::BAR::writeByte(uint64_t offset, uint8_t value) {
    if (isIO()) {
        out8((uint16_t) (offset + baseAddress), value);
    } else {
        *(uint8_t*) (baseAddress + offset) = value;
    }
}
void PCI::BAR::writeWord(uint64_t offset, uint16_t value) {
    if (isIO()) {
        out16((uint16_t) (offset + baseAddress), value);
    } else {
        *(uint16_t*) (baseAddress + offset) = value;
    }
}
void PCI::BAR::writeDWord(uint64_t offset, uint32_t value) {
    if (isIO()) {
        out32((uint16_t) (offset + baseAddress), value);
    } else {
        *(uint32_t*) (baseAddress + offset) = value;
    }
}
void PCI::BAR::writeQWord(uint64_t offset, uint64_t value) {
    if (isIO()) {
        out32((uint16_t) (offset + baseAddress), value);
        out32((uint16_t) (offset + baseAddress + 4), value >> 32);
    } else {
        *(uint64_t*) (baseAddress + offset) = value;
    }
}
