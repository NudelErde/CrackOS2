#pragma once

#include <LanguageFeatures/SmartPointer.hpp>
#include <Process/Process.hpp>
#include <stdint.h>

class ElfFile {
private:
    struct Data;

public:
    class SectionHeader {
    private:
        shared_ptr<Data> data;
        uint64_t index;
        friend class ElfFile;

    public:
        struct ElfSecHeader {
            uint32_t name;
            uint32_t type;
            uint64_t flags;
            uint64_t virtAddr;
            uint64_t fileOffset;
            uint64_t size;
            uint32_t link;
            uint32_t info;
            uint64_t align;
            uint64_t entsize;
        } __attribute__((packed));

        enum class Type {
            Null = 0,
            ProgBits = 1,
            SymTab = 2,
            StrTab = 3,
            Rela = 4,
            Hash = 5,
            Dynamic = 6,
            Note = 7,
            NoBits = 8,
            Rel = 9,
            ShLib = 10,
            DynSym = 11,
            InitArray = 14,
            FiniArray = 15,
            PreInitArray = 16,
            Group = 17,
            SymTabShndx = 18,
            Num = 19
        };

    private:
        ElfSecHeader& header();

    public:
        SectionHeader(shared_ptr<Data> data, uint64_t index);
        Type getType();
        const char* getTypeName();
        const char* getName();
        uint64_t getSize();
    };

    class ProgramHeader {
    private:
        shared_ptr<Data> data;
        uint64_t index;
        friend class ElfFile;

    public:
        struct ElfProgHeader {
            uint32_t type;
            uint32_t flags;
            uint64_t fileOffset;
            uint64_t virtAddr;
            uint64_t physAddr;
            uint64_t fileSize;
            uint64_t memSize;
            uint64_t align;
        } __attribute__((packed));

        enum class Type {
            Null = 0,
            Load = 1,
            Dynamic = 2,
            Interp = 3,
            Note = 4,
            ShLib = 5,
            Phdr = 6,
            Tls = 7,
            Num = 8
        };

    private:
        ElfProgHeader& header();

    public:
        ProgramHeader(shared_ptr<Data> data, uint64_t index);
        Type getType();
        const char* getTypeName();
        uint64_t getSize();
        uint64_t getFileOffset();
        uint64_t getVirtAddr();
        uint64_t getFileSize();
    };

private:
    struct FileHeader {
        uint8_t magic[4];  // 0x7F 'E' 'L' 'F'
        uint8_t class_;    // 1 = 32-bit, 2 = 64-bit
        uint8_t data;      // 1 = little-endian, 2 = big-endian
        uint8_t version_;  // 1 = current
        uint8_t pad[9];    // padding
        uint16_t type;     // 1 = relocatable, 2 = executable, 3 = shared object, 4 = core
        uint16_t machine;  // 3 = x86
        uint32_t version;  // 1 = current
        uint64_t entry;    // entry point
        uint64_t phoff;    // program header offset
        uint64_t shoff;    // section header offset
        uint32_t flags;    // 1 = little-endian, 2 = big-endian
        uint16_t ehsize;   // size of ELF header
        uint16_t phentsize;// size of program header entry
        uint16_t phnum;    // number of program header entries
        uint16_t shentsize;// size of section header entry
        uint16_t shnum;    // number of section header entries
        uint16_t shstrndx; // section header string table index
    } __attribute__((packed));
    static_assert(sizeof(FileHeader) == 64, "FileHeader is not 64 bytes");

    struct Data {
        const char* filename;
        bool valid;
        unique_ptr<SectionHeader::ElfSecHeader> sectionHeaders;
        unique_ptr<ProgramHeader::ElfProgHeader> programHeaders;
        unique_ptr<uint8_t> sectionHeadersStringTable;
        FileHeader header;

        const char* getSectionName(uint64_t index) const;
    };

    shared_ptr<Data> data;

public:
    ElfFile(const char* fileName);

    uint64_t getSectionCount() const;
    uint64_t getProgramSegmentCount() const;
    uint64_t getEntryPoint() const;


    inline bool isValid() const {
        return data->valid;
    }

    inline const FileHeader& getHeader() const {
        return data->header;
    }

    SectionHeader getSection(uint64_t section) const;
    ProgramHeader getProgramSegment(uint64_t segment) const;

    void printHeader();

    unique_ptr<Process> spawnProcess();
};
