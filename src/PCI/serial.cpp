#include "PCI/serial.hpp"
#include "BasicOutput/Output.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

class SerialPCIHandler : public PCI::Handler {
public:
    void onDeviceFound(PCI& device) override;
};

static uint8_t buffer[4096];

void SerialPCIHandler::onDeviceFound(PCI& device) {
    if (device.classCode == 0x07 && device.subclassCode == 0x00) {
        Serial* serial = new Serial(device);
        if (serial->isValid()) {
            uint64_t size = Output::readBuffer(buffer, 4096);
            for (uint64_t i = 0; i < size; i++) {
                serial->printImpl((char) buffer[i]);
            }
            Output::setDefault(new CombinedOutput(Output::getDefault(), serial));
        }
    }
}

static uint8_t serialBuffer[sizeof(Serial)];
PCI::Handler* Serial::getPCIHandler() {
    return new ((void*) serialBuffer) SerialPCIHandler();
}

Serial::Serial(PCI& pci) : pci(pci) {
    valid = false;
    bar = pci.getBar(0);
    bar[1] = (uint8_t) 0x00;// disable all interrupts
    bar[3] = (uint8_t) 0x80;// enable DLAB
    bar[0] = (uint8_t) 0x0C;// set baud rate divisor to 12 (low) (115200/9600 = 12)
    bar[1] = (uint8_t) 0x00;//                            (high)
    bar[3] = (uint8_t) 0x03;// 8 bits, no parity, one stop bit
    bar[2] = (uint8_t) 0x00;// disable fifo
    bar[4] = (uint8_t) 0x13;// data terminal ready, request to send, loopback mode

    bar[0] = (uint8_t) 0xAE;       // send AE to loopback
    if ((uint8_t) bar[0] == 0xAE) {// check loopback
        valid = true;
        bar[4] = (uint8_t) 0x03;// data terminal ready, request to send
    }
    Output::getDefault()->printf("Serial (%x %s): %s\n", bar.baseAddress, bar.isMemory() ? "MEM" : "IO", valid ? "found" : "not found");
}
void Serial::printImpl(char c) {
    if (!valid) {
        return;
    }
    while (!(((uint8_t) bar[5]) & 0x20)) {}// wait for transmit buffer empty
    bar[0] = (uint8_t) c;
}