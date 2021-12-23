#include "ACPI/multiboot.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/physicalAllocator.hpp"

Multiboot::Multiboot() {
    for (int i = 0; i < 256; i++) {
        callbacks[i] = nullptr;
    }
}

void Multiboot::setTagDecoder(uint8_t tag, Callback callback) {
    callbacks[tag] = callback;
}

static uint32_t realign(uint32_t i) {
    --i;
    return (i & -8) + 8;
}

void Multiboot::decode(uint8_t* table) {
    struct TagStructure {
        uint32_t total_size;
        uint32_t reserved;
    } __attribute__((__packed__));
    struct BasicTag {
        uint32_t type;
        uint32_t size;
    } __attribute__((__packed__));

    TagStructure head = *(TagStructure*) table;
    table += sizeof(head);
    for (uint32_t remainingSize = head.total_size; remainingSize;) {
        BasicTag* tag = reinterpret_cast<BasicTag*>(table);
        table += realign(tag->size);
        remainingSize -= realign(tag->size);
        uint32_t type = tag->type;
        if (callbacks[tag->type]) {
            callbacks[tag->type]((uint8_t*) tag);
        }
        if (tag->type == 0) {
            break;
        }
    }
}

static uint64_t acpiTableAddress;
static uint8_t* elfSymbols;///< Pointer to the ELF symbols. https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#ELF_002dSymbols


uint64_t getRsdtAddress() {
    return acpiTableAddress;
}

void readMultiboot(uint64_t multiboot) {
    struct RSDPDescriptor {
        char Signature[8];
        uint8_t Checksum;
        char OEMID[6];
        uint8_t Revision;
        uint32_t RsdtAddress;

        uint32_t Length;
        uint64_t XsdtAddress;
        uint8_t ExtendedChecksum;
        uint8_t reserved[3];
    } __attribute__((packed));

    Multiboot memoryDataDecoder;
    memoryDataDecoder.setTagDecoder(0x6, [](uint8_t* entry) {
        PhysicalAllocator::readMultibootInfos(entry);
    });
    memoryDataDecoder.decode((uint8_t*) multiboot);

    acpiTableAddress = 0;
    Multiboot decoder;
    decoder.setTagDecoder(0xE, [](uint8_t* entry) {
        entry += sizeof(uint64_t);//skip tag
        if (memcmp(entry, "RSD PTR ", 8) == 0) {
            Output::getDefault()->print("Multiboot: Found RSDT\n");
            auto rsdt = (RSDPDescriptor*) entry;
            if (acpiTableAddress) {
                return;// skip rsdt if xsdt is already found
            }
            acpiTableAddress = rsdt->RsdtAddress;
        }
    });
    decoder.setTagDecoder(0xF, [](uint8_t* entry) {
        entry += sizeof(uint64_t);//skip tag
        if (memcmp(entry, "RSD PTR ", 8) == 0) {
            Output::getDefault()->print("Multiboot: Found XSDT\n");
            auto xsdt = (RSDPDescriptor*) entry;
            acpiTableAddress = xsdt->XsdtAddress;// overwrite rsdt address
        }
    });
    decoder.setTagDecoder(0x9, [](uint8_t* entry) {
        Output::getDefault()->print("Multiboot: Found debug symbols\n");
        elfSymbols = entry + sizeof(uint64_t);//skip tag
    });
    decoder.decode(reinterpret_cast<uint8_t*>(multiboot));

    Output::getDefault()->printf("ACPI table address: %p\n", acpiTableAddress);
}
