#pragma once
#include "Memory/memory.hpp"
#include "PCI/pci.hpp"
#include "Storage/Storage.hpp"

class SATA : public Storage {
public:
    class Controller {
    public:
        Controller(PCI& pci);
        void init(shared_ptr<Controller> me);///< Initialize the controller and adds the SATA devices to the storage list.

        PCI::BAR& getABAR();

    private:
        PCI pci;
        uint32_t portsAvailable;
        uint32_t capabilities;
        uint32_t capabilities2;
        uint32_t version;
        PCI::BAR abar;

        friend class SATA;
    };

    SATA(shared_ptr<Controller> controller);

    uint64_t getSize() override;
    void read(uint64_t offset, uint64_t size, uint8_t* buffer) override;
    void write(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    static PCI::Handler* getPCIHandler();

    constexpr static uint64_t sectorSize = 512;

private:
    bool tryToInit(uint8_t port);

    PCI::BAR& getDBAR();

    shared_ptr<Controller> controller;
    uint8_t port;
    PCI::BAR dbar;
    uint64_t sectorCount;
};