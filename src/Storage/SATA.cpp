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
        Storage::addStorage(new SATA(pci));
    }
}

SATA::SATA(PCI& pci) : pci(pci) {
}

uint64_t SATA::getSize() {
}
void SATA::read(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
void SATA::write(uint64_t offset, uint64_t size, uint8_t* buffer) {
}
