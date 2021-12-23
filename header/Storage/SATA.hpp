#pragma once
#include "PCI/pci.hpp"
#include "Storage/Storage.hpp"

class SATA : public Storage {
public:
    uint64_t getSize() override;
    void read(uint64_t offset, uint64_t size, uint8_t* buffer) override;
    void write(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    static PCI::Handler* getPCIHandler();
};