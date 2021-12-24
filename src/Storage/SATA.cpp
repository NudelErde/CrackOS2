#include "Storage/SATA.hpp"
#include "BasicOutput/Output.hpp"
#include "Memory/memory.hpp"

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

SATA::Controller::Controller(PCI& pci) : pci(pci) {
}

void SATA::Controller::init(shared_ptr<SATA::Controller> me) {
    //print BARS
    for (int i = 0; i < 6; i++) {
        PCI::BAR bar = pci.getBar(i);
        if (bar.exists()) {
            if (bar.isMemory()) {
                Output::getDefault()->printf("AHCI: BAR %d: %llx MEM %s\n", i, bar.baseAddress, bar.is64Bit() ? "64" : "32");
            } else {
                Output::getDefault()->printf("AHCI: BAR %d: %llx IO\n", i, bar.baseAddress);
            }
        }
    }
}

SATA::SATA(shared_ptr<Controller> controller) : controller(controller) {
}

uint64_t SATA::getSize() {
}
void SATA::read(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
void SATA::write(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
