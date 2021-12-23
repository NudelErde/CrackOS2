#pragma once
#include <stdint.h>


class ACPI {
public:
    class TableParser {
    public:
        virtual ~TableParser() = default;

        virtual void parse(uint8_t* table) = 0;
        virtual const char* getSignature() = 0;
    };

    inline ACPI(TableParser** tables, uint64_t size) : tables(tables), tablesSize(size), index(0) {}

    inline void addTableParser(TableParser* parser) { tables[index++] = parser; }
    void parse(uint8_t* table);

private:
    TableParser** tables;
    uint64_t tablesSize;
    uint64_t index;
};