#pragma once
#include "Memory/memory.hpp"
#include "PCI/pci.hpp"
#include "Storage/Storage.hpp"

class SATA : public Storage {
public:
    struct Controller {
        PCI pci;
        Controller(PCI& pci);
        void init(shared_ptr<Controller> me);///< Initialize the controller and adds the SATA devices to the storage list.
    };

    SATA(shared_ptr<Controller> controller);

    uint64_t getSize() override;
    void read(uint64_t offset, uint64_t size, uint8_t* buffer) override;
    void write(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    static PCI::Handler* getPCIHandler();

private:
    shared_ptr<Controller> controller;
};