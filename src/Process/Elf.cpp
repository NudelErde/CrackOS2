#include "Process/Elf.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/physicalAllocator.hpp"
#include "Memory/tempMapping.hpp"
#include "Storage/Filesystem.hpp"

void ElfFile::printHeader() {
    FileHeader& header = data->header;
    Output::getDefault()->printf("ELF Header:\n");
    Output::getDefault()->printf("  Magic:   %hhx %hhx %hhx %hhx\n", header.magic[0], header.magic[1], header.magic[2], header.magic[3]);
    Output::getDefault()->printf("  Class:                             %hhx\n", header.class_);
    Output::getDefault()->printf("  Data:                              %hhx\n", header.data);
    Output::getDefault()->printf("  Version:                           %hhx\n", header.version_);
    Output::getDefault()->printf("  Type:                              %hx\n", header.type);
    Output::getDefault()->printf("  Machine:                           %hx\n", header.machine);
    Output::getDefault()->printf("  Version:                           0x%x\n", header.version);
    Output::getDefault()->printf("  Entry:                             0x%lx\n", header.entry);
    Output::getDefault()->printf("  Start of program header:           %lu (bytes into file)\n", header.phoff);
    Output::getDefault()->printf("  Start of section header:           %lu (bytes into file)\n", header.shoff);
    Output::getDefault()->printf("  Flags:                             0x%x\n", header.flags);
    Output::getDefault()->printf("  Size of this header:               %hu (bytes)\n", header.ehsize);
    Output::getDefault()->printf("  Size of program headers:           %hu (bytes)\n", header.phentsize);
    Output::getDefault()->printf("  Number of program headers:         %hu\n", header.phnum);
    Output::getDefault()->printf("  Size of section headers:           %hu (bytes)\n", header.shentsize);
    Output::getDefault()->printf("  Number of section headers:         %hu\n", header.shnum);
    Output::getDefault()->printf("  Section header string table index: %hu\n", header.shstrndx);
}

uint64_t ElfFile::getEntryPoint() const {
    return data->header.entry;
}

uint64_t ElfFile::getSectionCount() const {
    return data->header.shnum;
}

uint64_t ElfFile::getProgramSegmentCount() const {
    return data->header.phnum;
}

const char* ElfFile::Data::getSectionName(uint64_t index) const {
    if (index >= header.shnum) {
        return nullptr;
    }
    return (const char*) sectionHeadersStringTable.get() + sectionHeaders[index].name;
}

ElfFile::SectionHeader ElfFile::getSection(uint64_t section) const {
    return SectionHeader(data, section);
}
ElfFile::ProgramHeader ElfFile::getProgramSegment(uint64_t segment) const {
    return ProgramHeader(data, segment);
}

ElfFile::ElfFile(const char* name) {
    data = make_shared<Data>();
    data->filename = name;
    data->valid = false;
    Filesystem::read(name, 0, sizeof(data->header), (uint8_t*) &data->header);
    if (data->header.magic[0] != 0x7F || data->header.magic[1] != 'E' || data->header.magic[2] != 'L' || data->header.magic[3] != 'F') {
        Output::getDefault()->printf("Elf: Invalid magic in ELF file: %s\n", name);
        return;
    }

    //64 bit, x86, little endian, version 1, executable, non relocatable, non shared object, non core
    if (data->header.class_ != 2 || data->header.data != 1 || data->header.version != 1 || data->header.version_ != 1 || data->header.type != 2 || data->header.machine != 0x3E /*AMD-x86_64*/) {
        Output::getDefault()->printf("Elf: Invalid format in ELF file: %s\n", name);
        Output::getDefault()->printf("Elf: Format: Class[%hhu], Data[%hhu], Version[%hhu], Type[%hu], Machine[%hx]\n", data->header.class_, data->header.data, data->header.version, data->header.type, data->header.machine);
        return;
    }

    //load program headers
    uint32_t phoff = data->header.phoff;
    uint32_t phnum = data->header.phnum;
    uint32_t phentsize = data->header.phentsize;
    //  check if size of program header is correct
    if (phentsize != sizeof(ProgramHeader::ElfProgHeader)) {
        Output::getDefault()->printf("Elf: Invalid program header size in ELF file: %s\n", name);
        Output::getDefault()->printf("Elf: Expected %lu, got %u\n", sizeof(ProgramHeader::ElfProgHeader), phentsize);
        return;
    }
    //  allocate memory for program headers
    data->programHeaders = unique_ptr(new ProgramHeader::ElfProgHeader[phnum]);
    //  read program headers
    int64_t programHeaderReadResult = Filesystem::read(name, phoff, phnum * phentsize, (uint8_t*) data->programHeaders.get());
    if (programHeaderReadResult != phnum * phentsize) {
        Output::getDefault()->printf("Elf: Failed to read program headers in ELF file: %s\n", name);
        return;
    }

    //load section headers
    uint32_t shoff = data->header.shoff;
    uint32_t shnum = data->header.shnum;
    uint32_t shentsize = data->header.shentsize;
    //  check if size of section header is correct
    if (shentsize != sizeof(SectionHeader::ElfSecHeader)) {
        Output::getDefault()->printf("Elf: Invalid section header size in ELF file: %s\n", name);
        Output::getDefault()->printf("Elf: Expected %lu, got %u\n", sizeof(SectionHeader::ElfSecHeader), shentsize);
        return;
    }
    //  allocate memory for section headers
    data->sectionHeaders = unique_ptr(new SectionHeader::ElfSecHeader[shnum]);
    //  read section headers
    int64_t sectionHeaderReadResult = Filesystem::read(name, shoff, shnum * shentsize, (uint8_t*) data->sectionHeaders.get());
    if (sectionHeaderReadResult != shnum * shentsize) {
        Output::getDefault()->printf("Elf: Failed to read section headers in ELF file: %s\n", name);
        return;
    }

    //load section header string table
    uint32_t stringSectionIndex = data->header.shstrndx;
    uint32_t stringSectionOffset = data->sectionHeaders[stringSectionIndex].fileOffset;
    uint32_t stringSectionSize = data->sectionHeaders[stringSectionIndex].size;
    //  allocate memory for section header string table
    data->sectionHeadersStringTable = unique_ptr(new uint8_t[stringSectionSize]);
    //  read section header string table
    if (Filesystem::read(name, stringSectionOffset, stringSectionSize, data->sectionHeadersStringTable.get()) < stringSectionSize) {
        Output::getDefault()->printf("Elf: Failed to read section header string table in ELF file: %s\n", name);
        return;
    }

    data->valid = true;
}

unique_ptr<Process> ElfFile::spawnProcess() {
    unique_ptr<Process> process = make_unique<Process>();
    for (uint64_t i = 0; i < getProgramSegmentCount(); ++i) {
        auto segment = getProgramSegment(i);
        if (segment.getType() == ProgramHeader::Type::Load) {
            Mapping::Flags flag;
            flag.writable = segment.header().flags & 1;
            flag.executable = segment.header().flags & 2;
            uint64_t pageCount = (segment.getSize() + pageSize - 1) / pageSize;
            uint64_t start = segment.getVirtAddr();
            for (uint64_t index = 0; index < pageCount; ++index) {
                uint64_t page = start + index * pageSize;
                uint64_t physical = PhysicalAllocator::allocatePhysicalMemory(1);
                process->getProcessMemory().map(page, physical, pageSize, flag);
            }
        }
    }
    process->getProcessMemory().compact();
    return process;
}

ElfFile::SectionHeader::SectionHeader(shared_ptr<Data> data, uint64_t index) : data(data), index(index) {
}

ElfFile::SectionHeader::ElfSecHeader& ElfFile::SectionHeader::header() {
    return data->sectionHeaders[index];
}

ElfFile::SectionHeader::Type ElfFile::SectionHeader::getType() {
    return (Type) header().type;
}

const char* ElfFile::SectionHeader::getTypeName() {
    switch (header().type) {
        case 0:
            return "NULL";
        case 1:
            return "PROGBITS";
        case 2:
            return "SYMTAB";
        case 3:
            return "STRTAB";
        case 4:
            return "RELA";
        case 5:
            return "HASH";
        case 6:
            return "DYNAMIC";
        case 7:
            return "NOTE";
        case 8:
            return "NOBITS";
        case 9:
            return "REL";
        case 10:
            return "SHLIB";
        case 11:
            return "DYNSYM";
        case 14:
            return "INIT_ARRAY";
        case 15:
            return "FINI_ARRAY";
        case 16:
            return "PREINIT_ARRAY";
        case 17:
            return "GROUP";
        case 18:
            return "SYMTAB_SHNDX";
        case 19:
            return "NUM";
        default:
            return "Unknown";
    }
}

const char* ElfFile::SectionHeader::getName() {
    return data->getSectionName(index);
}

uint64_t ElfFile::SectionHeader::getSize() {
    return header().size;
}

ElfFile::ProgramHeader::ProgramHeader(shared_ptr<Data> data, uint64_t index) : data(data), index(index) {
}

ElfFile::ProgramHeader::ElfProgHeader& ElfFile::ProgramHeader::header() {
    return data->programHeaders[index];
}

ElfFile::ProgramHeader::Type ElfFile::ProgramHeader::getType() {
    return (Type) header().type;
}

const char* ElfFile::ProgramHeader::getTypeName() {
    switch (header().type) {
        case 0:
            return "NULL";
        case 1:
            return "LOAD";
        case 2:
            return "DYNAMIC";
        case 3:
            return "INTERP";
        case 4:
            return "NOTE";
        case 5:
            return "SHLIB";
        case 6:
            return "PHDR";
        case 7:
            return "TLS";
        default:
            return "Unknown";
    };
}

uint64_t ElfFile::ProgramHeader::getSize() {
    return header().memSize;
}

uint64_t ElfFile::ProgramHeader::getFileOffset() {
    return header().fileOffset;
}

uint64_t ElfFile::ProgramHeader::getVirtAddr() {
    return header().virtAddr;
}

uint64_t ElfFile::ProgramHeader::getFileSize() {
    return header().fileSize;
}
