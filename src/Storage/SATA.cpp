#include "Storage/SATA.hpp"
#include "BasicOutput/Output.hpp"
#include "Memory/memory.hpp"

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
            SATA* sata = new SATA(me);
            if (!sata->tryToInit(i)) {
                delete sata;
            } else {
                Storage::addStorage(sata);
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

SATA::SATA(shared_ptr<Controller> controller) : controller(controller) {
}

bool SATA::tryToInit(uint8_t port) {
    this->port = port;
    // never trust your code -> check if this port exists
    if (!(controller->portsAvailable & (1 << port))) {
        return false;
    }
    // Output::getDefault()->printf("SATA: Trying to init port %hhu\n", port);
    dbar = controller->getABAR() + 0x100 + (port * 0x80);

    //TODO: init port


    //init sectorCount


    return true;
}

PCI::BAR& SATA::getDBAR() {
    return dbar;
}

uint64_t SATA::getSize() {
    return sectorCount * sectorSize;
}
void SATA::read(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
void SATA::write(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
