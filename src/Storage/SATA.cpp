#include "Storage/SATA.hpp"
#include "BasicOutput/Output.hpp"
#include "Memory/memory.hpp"

struct MyHandler : public PCI::Handler {
    void onDeviceFound(PCI& pci) override;
};

struct uint8_t buffer[sizeof(MyHandler)];

PCI::Handler* SATA::getPCIHandler() {
    return new (buffer) MyHandler();
}

void MyHandler::onDeviceFound(PCI& pci) {
    if (pci.classCode == 0x01 && pci.subclassCode == 0x06 && pci.progIF == 0x01) {
        Output::getDefault()->print("Found SATA device!\n");
    }
}