#pragma once
#include "Memory/memory.hpp"
#include "PCI/pci.hpp"
#include "Storage/Storage.hpp"

//TODO: write destructor that frees all allocated memory
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
    int64_t read(uint64_t offset, uint64_t size, uint8_t* buffer) override;
    int64_t write(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    uint8_t getType();
    const char* getTypeName();

    static PCI::Handler* getPCIHandler();

    constexpr static uint64_t sectorSize = 512;

private:
    bool tryToInit(uint8_t port);
    bool sendIdentify();

    PCI::BAR& getDBAR();

    shared_ptr<Controller> controller;
    uint8_t portIndex;
    PCI::BAR dbar;
    uint8_t* commandSlotPtr;
    uint8_t* receivedFISPtr;
    uint8_t* commandTablePtr;

    enum class Type : uint8_t {
        SATA,
        SATAPI,
        SEMB,
        PM,
        UNKNOWN
    };

    Type type;

    uint64_t sectorCount;
    bool hasLBA48;

    uint64_t findSlot();
    bool waitForFinish();
    bool waitForTask(uint64_t slotID);
    void writeH2DCommand(uint32_t slotID, uint8_t command, uint64_t sectorIndex, uint64_t sectorCount);
};