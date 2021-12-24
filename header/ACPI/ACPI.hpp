#pragma once
#include <stdint.h>

class ACPI {
public:
    struct TableHeader {
        char Signature[4];
        uint32_t Length;
        uint8_t Revision;
        uint8_t Checksum;
        char OEMID[6];
        char OEMTableID[8];
        uint32_t OEMRevision;
        uint32_t CreatorID;
        uint32_t CreatorRevision;
    };
    static_assert(sizeof(TableHeader) == 36, "TableHeader size is wrong");

    class TableParser {
    public:
        virtual ~TableParser() = default;

        virtual bool parse(ACPI::TableHeader* table) = 0;
        virtual const char* getSignature() = 0;
    };

    inline ACPI(TableParser** tables, uint64_t size) : tables(tables), tablesSize(size), index(0) {}

    inline void addTableParser(TableParser* parser) { tables[index++] = parser; }
    void parse(uint64_t table);

    static void init();

private:
    TableParser** tables;
    uint64_t tablesSize;
    uint64_t index;
};