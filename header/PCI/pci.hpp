#pragma once
#include "ACPI/ACPI.hpp"
#include <stdint.h>

class PCI {
public:
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint64_t physicalAddress;

    uint16_t vendorID;
    uint16_t deviceID;
    uint8_t classCode;
    uint8_t subclassCode;
    uint8_t progIF;
    uint8_t revisionID;
    uint8_t headerType;

public:
    struct Handler {
        virtual void onDeviceFound(PCI& device) = 0;
        Handler* next;
    };

    static ACPI::TableParser* getACPITableParser();

    using Callback = void (*)(PCI&, void*);

    static void iterateDevices(Callback callback, void* context);
    static void addHandler(Handler* handler);
    static void init();

    uint8_t readConfigByte(uint64_t offset);
    uint16_t readConfigWord(uint64_t offset);
    uint32_t readConfigDword(uint64_t offset);
    uint64_t readConfigQword(uint64_t offset);
    void writeConfigByte(uint64_t offset, uint8_t value);
    void writeConfigWord(uint64_t offset, uint16_t value);
    void writeConfigDword(uint64_t offset, uint32_t value);
    void writeConfigQword(uint64_t offset, uint64_t value);

    template<typename T>
    inline T readConfig(uint64_t offset) {
        if constexpr (sizeof(T) == sizeof(uint8_t)) {
            uint8_t res = readConfigByte(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
            uint16_t res = readConfigWord(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
            uint32_t res = readConfigDword(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            uint64_t res = readConfigQword(offset);
            return *reinterpret_cast<T*>(&res);
        }
    }

    template<typename T>
    inline void writeConfig(uint64_t offset, T value) {
        if constexpr (sizeof(T) == sizeof(uint8_t)) {
            writeConfigByte(offset, *reinterpret_cast<uint8_t*>(&value));
        } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
            writeConfigWord(offset, *reinterpret_cast<uint16_t*>(&value));
        } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
            writeConfigDword(offset, *reinterpret_cast<uint32_t*>(&value));
        } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            writeConfigQword(offset, *reinterpret_cast<uint64_t*>(&value));
        }
    }
};
