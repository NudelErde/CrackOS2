#include "Storage/SATA.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/time.hpp"
#include "Common/Math.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"
#include "Memory/tempMapping.hpp"

//-----------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------[PCI Handler]-----------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------

struct MyHandler : public PCI::Handler {
    void onDeviceFound(PCI& pci) override;
};

static uint8_t buffer[sizeof(MyHandler)];

PCI::Handler* SATA::getPCIHandler() {
    return new (buffer) MyHandler();
}


void MyHandler::onDeviceFound(PCI& pci) {
    if (pci.classCode == 0x01 && pci.subclassCode == 0x06 && pci.progIF == 0x01) {
        shared_ptr<SATA::Controller> controller = make_shared<SATA::Controller>(pci);
        controller->init(controller);
    }
}

//-----------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------[SATA Controller]---------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------

SATA::Controller::Controller(PCI& pci) : pci(pci) {
    abar = pci.getBar(5);
}

void SATA::Controller::init(shared_ptr<SATA::Controller> me) {
    //reset ahci
    getABAR()[4] = (0b1 << 31) | 0b1;
    while ((uint32_t) (getABAR()[4]) & 0b1) {
        //wait for reset to finish
    }
    getABAR()[4] = (0b1 << 31);

    //read static information
    portsAvailable = getABAR()[0xC];
    capabilities = getABAR()[0x0];
    capabilities2 = getABAR()[0x24];
    version = getABAR()[0x10];
    // Output::getDefault()->print("SATA: Ports available: ");
    // Output::getDefault()->print(BitList(portsAvailable));
    // Output::getDefault()->print("\n");

    uint32_t bohc = getABAR()[0x28];

    if ((capabilities2 & 0b1) && (bohc & 0b1)) {
        Output::getDefault()->print("SATA: BIOS handoff\n");
        bohc |= 0b10;
        getABAR()[0x28] = bohc;
        while (true) {
            if ((((uint32_t) getABAR()[0x28]) & 0b11) == 0b10)
                break;
        }
    }

    // try to init each port
    for (uint8_t i = 0; i < 32; ++i) {
        if (portsAvailable & (1 << i)) {
            shared_ptr<SATA> sata = make_shared<SATA>(me);
            if (sata->tryToInit(i)) {
                shared_ptr<Storage> storagePtr = static_pointer_cast<Storage>(sata);
                Storage::addStorage(storagePtr);
                if (MBRPartitionTable::isUsableTableType(storagePtr)) {
                    storagePtr->setPartition(static_pointer_cast<PartitionTable>(make_shared<MBRPartitionTable>(storagePtr)));
                }
                Output::getDefault()->printf("SATA: Port %hhu initialized! Type: %s Size: %d GB\n", i, sata->getTypeName(), sata->getSize() / 1Gi);
            }
        }
    }
}

PCI::BAR& SATA::Controller::getABAR() {
    return abar;
}

//-----------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------[SATA Device]-----------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------

struct FisDmaSetup {
    uint8_t fisType;// 0x41
    uint8_t portMultiplier : 4;
    uint8_t rsv0 : 1;
    bool dataDirection : 1;
    bool interrupt : 1;
    bool autoActivate : 1;
    uint8_t rsved[2];
    uint64_t DMAbufferID;
    uint32_t rsvd;
    uint32_t DMAbufOffset;
    uint32_t TransferCount;
    uint32_t resvd;
} __attribute__((packed));

struct FisH2D {
    uint8_t fisType;// 0x27
    uint8_t portMultiplier : 4;
    uint8_t rsv0 : 3;
    bool isCommand : 1;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t rsv1[4];
};

struct FisD2H {
    uint8_t fisType;// 0x34

    uint8_t portMultiply : 4;
    uint8_t rsv0 : 2;
    bool interrupt : 1;
    uint8_t rsv1 : 1;

    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv2;

    uint8_t countLow;
    uint8_t countHigh;
    uint8_t rsv3[2];
    uint8_t rsv4[4];
} __attribute__((packed));

struct FisPIO {
    uint8_t fisType;// 0x5F
    uint8_t portMultiplier : 4;
    uint8_t rsv0 : 1;
    bool dataDirection : 1;
    bool interrupt : 1;
    uint8_t rsv1 : 1;
    uint8_t status;
    uint8_t error;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv2;
    uint8_t countLow;
    uint8_t countHigh;
    uint8_t rsv3;
    uint8_t e_status;
    uint16_t transferCount;
    uint8_t rsv4[2];
} __attribute__((packed));

struct ReceivedFIS {
    FisDmaSetup dmaSetup;
    uint8_t pad0[4];

    FisPIO pioSetup;
    uint8_t pad1[12];

    FisD2H deviceToHost;
    uint8_t pad2[4];

    // 0x58
    uint8_t sdbfis[2];// Set Device Bit FIS

    // 0x60
    uint8_t ufis[64];

    // 0xA0
    uint8_t rsv[0x100 - 0xA0];
} __attribute__((packed));

struct Port {
    uint32_t commandListAddressLow;
    uint32_t commandListAddressHigh;
    uint32_t fisAddressLow;
    uint32_t fisAddressHigh;
    uint32_t interruptStatus;
    uint32_t interruptEnable;
    uint32_t commandAndStatus;
    uint32_t rsv0;
    uint32_t taskFileData;
    uint32_t signature;
    uint32_t sataStatus;
    uint32_t sataControl;
    uint32_t sataError;
    uint32_t sataActive;
    uint32_t commandIssue;
    uint32_t sataNotification;
    uint32_t fisBasedSwitch;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed));

struct PrdtEntry {
    uint32_t dataBaseAddressLow;
    uint32_t dataBaseAddressHigh;
    uint32_t rsv0;
    uint32_t dataByteCount : 22;
    uint32_t rsv1 : 9;
    bool interruptEnable : 1;
} __attribute__((packed));

static constexpr uint64_t prdtEntryCount = ((pageSize / 0x10) - 64 - 16 - 48) / sizeof(PrdtEntry);

struct CommandTable {
    uint8_t commandFIS[64];

    uint8_t atapiCommand[16];
    uint8_t rsv[48];// Reserved

    PrdtEntry prdtEntries[prdtEntryCount];// Physical region descriptor table entries, 0 ~ 65535

    uint8_t createPrdt(void* address, uint64_t& size);// return prdt count
} __attribute__((packed));

uint8_t CommandTable::createPrdt(void* ptr, uint64_t& size) {
    uint8_t* address = (uint8_t*) ptr;
    uint8_t currentEntryIndex = 0;
    memset(prdtEntries, 0, sizeof(prdtEntries));

    uint64_t lastPhysicalAddress = 0;
    while (size > 0) {
        PrdtEntry* entry = &prdtEntries[currentEntryIndex];
        uint64_t physicalAddress = PageTable::getPhysicalAddress((uint64_t) address);
        uint64_t entrySize = min(size, pageSize);
        uint64_t offsetInPage = ((uint64_t) address) & (pageSize - 1);
        entrySize = min(entrySize, pageSize - offsetInPage);
        if (entry->dataByteCount != 0) {
            //check if the entry is full
            if (entry->dataByteCount + entrySize >= (0b1 << 22) - 1) {
                currentEntryIndex++;
                if (currentEntryIndex >= prdtEntryCount) {
                    return currentEntryIndex;
                }
                entry = &prdtEntries[currentEntryIndex];
            } else if ((lastPhysicalAddress & ~(pageSize - 1)) + pageSize == physicalAddress) {//check if physical address is adjacent to the current entry
                entry->dataByteCount += entrySize;
            }
        }
        lastPhysicalAddress = physicalAddress;

        if (entry->dataByteCount == 0) {
            entry->dataBaseAddressLow = (uint32_t) physicalAddress;
            entry->dataBaseAddressHigh = (uint32_t) (physicalAddress >> 32);
            entry->dataByteCount = (uint32_t) entrySize - 1;// '0' based count => 0 = 1 byte, 1 = 2 bytes, ...
            entry->interruptEnable = false;
        }
        address += entrySize;
        size -= entrySize;
    }
    return currentEntryIndex + 1;
}

static_assert(sizeof(CommandTable) == (pageSize / 0x10));

struct CommandSlot {
    uint8_t commandFISLength : 5;
    bool isATAPI : 1;
    bool write : 1;
    bool prefetchable : 1;

    bool reset : 1;
    bool bist : 1;
    bool clearBusyOnOK : 1;
    uint8_t rsv0 : 1;
    uint8_t portMultiplier : 4;

    uint16_t prdTableLength;

    uint32_t prdByteCount;

    uint32_t commandTableBaseLow;
    uint32_t commandTableBaseHigh;

    uint32_t rsv1[4];

    inline CommandTable* getCommandTable() {
        return (CommandTable*) TempMemory::mapPages((commandTableBaseLow | (((uint64_t) commandTableBaseHigh) << 32)), 1, false);
    }
} __attribute__((packed));

SATA::SATA(shared_ptr<Controller> controller) : controller(controller) {
}

uint64_t SATA::findSlot() {
    Port* port = dbar.getAs<Port>();
    uint8_t maxSlot = ((controller->capabilities >> 8) & 0b11111) + 1;
    uint32_t slots = (port->sataActive | port->commandIssue);
    for (uint64_t i = 0; i < maxSlot; i++) {
        if ((slots & 1) == 0) {
            return i;
        }
        slots >>= 1;
    }
    return ~0;
}

bool SATA::waitForTask(uint64_t slot) {
    Port* port = dbar.getAs<Port>();
    while (true) {
        if (!(port->commandIssue & (0b1 << slot)))
            return true;
        if (port->sataError) {
            return false;
        }
        if (port->taskFileData & 0b1)
            return false;// error
    }
}

bool SATA::waitForFinish() {
    Port* port = dbar.getAs<Port>();
    while (true) {
        if (!(port->taskFileData & (0b10001) << 3)) return true;
        if (port->taskFileData & 0b1) return false;// error
    }
}
bool SATA::tryToInit(uint8_t portIndex) {
    using namespace time;
    this->portIndex = portIndex;
    // never trust your code -> check if this port exists
    if (!(controller->portsAvailable & (1 << portIndex))) {
        return false;
    }
    // Output::getDefault()->printf("SATA: Trying to init port %hhu\n", port);
    dbar = controller->getABAR() + 0x100 + (portIndex * 0x80);

    if (!dbar.isMemory()) {
        return false;//dbar must be in memory
    }


    Port* port = dbar.getAs<Port>();
    port->sataControl = 0b1;
    sleep(1ms);
    port->sataControl = 0b0;

    for (uint32_t i = 0; i < 0xFFFF; ++i) {
        if (port->sataStatus & 0b1111 == 0b11) {// wait for device present and communication ready
            break;
        }
    }

    if ((port->sataStatus & 0b1111) != 0b11) {
        return false;// device not ready
    }
    if ((port->sataStatus & (0b1111 << 8)) != (0b1 << 8)) {
        return false;// device not powered
    }

    port->sataError = ~0;// clear error register by writing 1s (because write = clear because lol funny hihi)

    //stop device
    port->commandAndStatus &= ~0b10001;             // stop device
    while (port->commandAndStatus & (0b11 << 14)) {}// wait for device to stop

    //allocate memory structure for this port
    uint8_t commandSlots = ((controller->capabilities >> 8) & 0b11111) + 1;// 1 to 32 command slots
    uint64_t neededMemory = sizeof(ReceivedFIS) + sizeof(CommandTable) * commandSlots;
    uint64_t neededPageCount = (neededMemory + pageSize - 1) / pageSize;
    uint8_t* ptr = TempMemory::mapPages(PhysicalAllocator::allocatePhysicalMemory(neededPageCount), neededPageCount, false);
    receivedFISPtr = ptr;
    commandSlotPtr = ptr + sizeof(ReceivedFIS);
    memset(ptr, 0, neededMemory);
    uint64_t commandTablePageCount = (commandSlots >= 16) ? 2 : 1;
    commandTablePtr = TempMemory::mapPages(PhysicalAllocator::allocatePhysicalMemory(commandTablePageCount), commandTablePageCount, false);
    memset(commandTablePtr, 0, commandTablePageCount * pageSize);
    for (uint64_t i = 0; i < commandSlots; ++i) {
        CommandTable* table = (CommandTable*) (commandTablePtr + (i * sizeof(CommandTable)));
        uint64_t physical = PageTable::getPhysicalAddress((uint64_t) table);
        CommandSlot* slot = (CommandSlot*) (commandSlotPtr + (i * sizeof(CommandSlot)));
        slot->prdTableLength = prdtEntryCount;
        slot->commandTableBaseLow = (uint32_t) physical;
        slot->commandTableBaseHigh = (uint32_t) (physical >> 32);
    }

    //tell port the command slot array
    uint64_t physicalCommandSlot = PageTable::getPhysicalAddress((uint64_t) commandSlotPtr);
    port->commandListAddressLow = (uint32_t) physicalCommandSlot;
    port->commandListAddressHigh = (uint32_t) (physicalCommandSlot >> 32);

    //tell port the received FIS
    uint64_t physicalReceivedFIS = PageTable::getPhysicalAddress((uint64_t) receivedFISPtr);
    port->fisAddressLow = (uint32_t) physicalReceivedFIS;
    port->fisAddressHigh = (uint32_t) (physicalReceivedFIS >> 32);

    //start device
    while (port->commandAndStatus & (0b1 << 15)) {}// wait for clean state
    port->commandAndStatus |= 0b10001;             // start device

    //send identify command
    uint64_t slotID = findSlot();
    if (slotID >= 32) {
        return false;
    }
    CommandSlot* commandSlot = (CommandSlot*) (commandSlotPtr + (slotID * sizeof(CommandSlot)));
    CommandTable* commandTable = commandSlot->getCommandTable();
    FisH2D* fis = (FisH2D*) &(commandTable->commandFIS);
    memset(fis, 0, sizeof(FisH2D));
    fis->fisType = 0x27;
    fis->command = 0xEC;
    fis->isCommand = true;
    commandSlot->commandFISLength = sizeof(FisH2D) / sizeof(uint32_t);

    uint8_t buffer[512]{};
    commandTable->prdtEntries[0].dataByteCount = sizeof(buffer);
    commandTable->prdtEntries[0].dataBaseAddressLow = (uint32_t) PageTable::getPhysicalAddress((uint64_t) buffer);
    commandTable->prdtEntries[0].dataBaseAddressHigh = (uint32_t) (PageTable::getPhysicalAddress((uint64_t) buffer) >> 32);
    commandTable->prdtEntries[0].interruptEnable = false;
    commandSlot->prdByteCount = sizeof(buffer);
    if (!waitForFinish()) {
        return false;
    }
    port->commandIssue |= 0b1 << slotID;// execute command
    if (!waitForTask(slotID)) {
        return false;
    }

    uint64_t LBA48SectorCount = *(uint64_t*) (buffer + 200);
    uint64_t LBA28SectorCount = *(uint32_t*) (buffer + 120);

    //init sectorCount
    if (LBA48SectorCount) {
        sectorCount = LBA48SectorCount;
        hasLBA48 = true;
    } else if (LBA28SectorCount) {
        sectorCount = LBA28SectorCount;
        hasLBA48 = false;
    }

    switch (port->signature) {
        case 0xEB140101:
            type = Type::SATAPI;
            break;
        case 0xC33C0101:
            type = Type::SEMB;
            break;
        case 0x96690101:
            type = Type::PM;
            break;
        case 0x00000101:
            type = Type::SATA;
            break;
        case 0xFFFFFFFF:
        default:
            type = Type::UNKNOWN;
            break;
    }

    return true;
}

uint8_t SATA::getType() {
    return (uint8_t) type;
}
const char* SATA::getTypeName() {
    const char* names[]{
            "SATA",
            "SATAPI",
            "SEMB",
            "PM",
            "UNKNOWN"};
    return names[getType()];
}

PCI::BAR& SATA::getDBAR() {
    return dbar;
}

void SATA::writeH2DCommand(uint32_t slotID, uint8_t command, uint64_t sectorIndex, uint64_t sectorCount) {
    CommandSlot* slot = (CommandSlot*) (commandSlotPtr + slotID);
    CommandTable* table = slot->getCommandTable();
    FisH2D* fis = (FisH2D*) &(table->commandFIS);
    slot->commandFISLength = sizeof(FisH2D) / sizeof(uint32_t);
    fis->fisType = 0x27;
    fis->device = 1 << 6;// LBA mode
    fis->command = command;
    fis->isCommand = true;
    fis->lba0 = (uint8_t) sectorIndex;
    fis->lba1 = (uint8_t) (sectorIndex >> 8);
    fis->lba2 = (uint8_t) (sectorIndex >> 16);
    fis->lba3 = (uint8_t) (sectorIndex >> 24);
    fis->lba4 = (uint8_t) (sectorIndex >> 32);
    fis->lba5 = (uint8_t) (sectorIndex >> 40);
    fis->countl = (uint8_t) sectorCount;
    fis->counth = (uint8_t) (sectorCount >> 8);
}

uint64_t SATA::getSize() {
    return sectorCount * sectorSize;
}
int64_t SATA::read(uint64_t offset, uint64_t size, uint8_t* buffer) {
    //handle offset that starts in a sector
    int64_t res = 0;
    if (offset % 512 != 0) {
        // read first requested sector
        uint8_t tmp[512];
        res = read(offset & ~(512 - 1), 512, tmp);

        // copy requested data to buffer (less than 512 bytes)
        memcpy(buffer, tmp + (offset % 512), min(512 - (offset % 512), size));
        if (offset % 512 > size) {
            return res;
        }

        // update offset and size
        size -= 512 - (offset % 512);
        buffer += 512 - (offset % 512);
        offset += 512 - (offset % 512);
        if (offset % 512 != 0) {
            Output::getDefault()->printf("SATA implementation error %s:%u\n", __FILE__, __LINE__);
            stop();
        }

        // no return because the following code reads the rest of the data
    }
    //handle size that ends in the middle of a sector
    if (size % 512 != 0) {
        int64_t res1 = read(offset, size & ~(512 - 1), buffer);//read everything except the last sector
        //read last sector
        uint8_t tmp[512];
        int64_t res2 = read(offset + size - (size % 512), 512, tmp);
        memcpy(buffer + size - (size % 512), tmp, size % 512);
        // return because everything is read
        if (res1 == -1 || res2 == -1) {
            return -1;
        }
        return res1 + res2;
    }
    //handle misaligned buffer (has to be aligned to 2byte boundary)
    if ((uint64_t) buffer % 2 != 0) {
        uint8_t* tmp = new uint8_t[size];// allocate temporary buffer (this sucks)
        res = read(offset, size, tmp);
        memcpy(buffer, tmp, size);
        delete[] tmp;
        return res;
    }

    //everything is nice and aligned
    uint64_t sectorCount = size / 512;
    uint64_t sectorIndex = offset / 512;

    Port* port = dbar.getAs<Port>();

    uint32_t slotID = findSlot();
    if (slotID > 32) {
        return -1;
    }
    CommandSlot* slot = (CommandSlot*) (commandSlotPtr + slotID * sizeof(CommandSlot));
    CommandTable* table = slot->getCommandTable();

    uint64_t startSize = size;
    uint8_t prdtCount = table->createPrdt(buffer, size);
    slot->prdTableLength = prdtCount;
    uint64_t usedSectors = (startSize - size) / 512;
    writeH2DCommand(slotID, 0x20, sectorIndex, usedSectors);
    if (!waitForFinish()) {
        return -1;
    }
    port->commandIssue |= 0b1 << slotID;// execute command
    if (!waitForTask(slotID)) {
        return -1;
    }
    return startSize - size + res;
}
int64_t SATA::write(uint64_t offset, uint64_t size, uint8_t* buffer) {
    //handle offset that starts in a sector
    int64_t res = 0;
    if (offset % 512 != 0) {
        // write first requested sector
        uint8_t tmp[512];
        memcpy(tmp + (offset % 512), buffer, min(512 - (offset % 512), size));
        res = write(offset & ~(512 - 1), 512, tmp);

        // copy requested data to buffer (less than 512 bytes)
        if (offset % 512 > size) {
            return res;
        }

        // update offset and size
        size -= 512 - (offset % 512);
        buffer += 512 - (offset % 512);
        offset += 512 - (offset % 512);
        if (offset % 512 != 0) {
            Output::getDefault()->printf("SATA implementation error %s:%u\n", __FILE__, __LINE__);
            stop();
        }

        // no return because the following code writes the rest of the data
    }
    //handle size that ends in the middle of a sector
    if (size % 512 != 0) {
        int64_t res1 = write(offset, size & ~(512 - 1), buffer);//write everything except the last sector
        //read last sector
        uint8_t tmp[512];
        memcpy(tmp, buffer + size - (size % 512), size % 512);
        int64_t res2 = write(offset + size - (size % 512), 512, tmp);
        // return because everything is written
        if (res1 == -1 || res2 == -1) {
            return -1;
        }
        return res1 + res2;
    }
    //handle misaligned buffer (has to be aligned to 2byte boundary)
    if ((uint64_t) buffer % 2 != 0) {
        uint8_t* tmp = new uint8_t[size];// allocate temporary buffer (this sucks)
        memcpy(tmp, buffer, size);
        res = write(offset, size, tmp);
        delete[] tmp;
        return res;
    }

    //everything is nice and aligned
    uint64_t sectorCount = size / 512;
    uint64_t sectorIndex = offset / 512;

    Port* port = dbar.getAs<Port>();

    uint32_t slotID = findSlot();
    if (slotID > 32) {
        return -1;
    }
    CommandSlot* slot = (CommandSlot*) (commandSlotPtr + slotID * sizeof(CommandSlot));
    CommandTable* table = slot->getCommandTable();

    uint64_t startSize = size;
    uint8_t prdtCount = table->createPrdt(buffer, size);
    slot->prdTableLength = prdtCount;
    uint64_t usedSectors = (startSize - size) / 512;
    writeH2DCommand(slotID, 0x35, sectorIndex, usedSectors);
    if (!waitForFinish()) {
        return -1;
    }
    port->commandIssue |= 0b1 << slotID;// execute command
    if (!waitForTask(slotID)) {
        return -1;
    }
    return startSize - size + res;
}
