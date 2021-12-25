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
    struct BAR {
        uint64_t baseAddress;
        uint8_t type;

        inline bool isMemory() {
            return !(type & 0b1);
        }
        inline bool isIO() {
            return type & 0b1;
        }
        inline bool is64Bit() {
            return type & 0b100;
        }
        inline bool is32Bit() {
            return !(type & 0b100);
        }
        inline bool exists() {
            return baseAddress != 0;
        }

        template<typename T>
        T* getAs() {
            if (isMemory()) {
                return (T*) baseAddress;
            } else {
                return nullptr;
            }
        }

        struct BarAccess {
            BAR* bar;
            uint64_t offset;

            template<typename T>
            operator T() {
                return bar->read<T>(offset);
            }

            template<typename T>
            BarAccess& operator=(T value) {
                bar->write(offset, value);
                return *this;
            }
        };

        inline BarAccess operator[](uint64_t offset) {
            return BarAccess{this, offset};
        }

        inline BAR operator+(uint64_t offset) {
            return {baseAddress + offset, type};
        }

        inline BAR operator-(uint64_t offset) {
            return {baseAddress - offset, type};
        }

        uint8_t readByte(uint64_t offset);
        uint16_t readWord(uint64_t offset);
        uint32_t readDWord(uint64_t offset);
        uint64_t readQWord(uint64_t offset);
        void writeByte(uint64_t offset, uint8_t value);
        void writeWord(uint64_t offset, uint16_t value);
        void writeDWord(uint64_t offset, uint32_t value);
        void writeQWord(uint64_t offset, uint64_t value);
        template<typename T>
        inline T read(uint64_t offset) {
            if constexpr (sizeof(T) == sizeof(uint8_t)) {
                uint8_t res = readByte(offset);
                return *reinterpret_cast<T*>(&res);
            } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
                uint16_t res = readWord(offset);
                return *reinterpret_cast<T*>(&res);
            } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
                uint32_t res = readDWord(offset);
                return *reinterpret_cast<T*>(&res);
            } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
                uint64_t res = readQWord(offset);
                return *reinterpret_cast<T*>(&res);
            }
        }
        template<typename T>
        inline void write(uint64_t offset, T value) {
            if constexpr (sizeof(T) == sizeof(uint8_t)) {
                writeByte(offset, *reinterpret_cast<uint8_t*>(&value));
            } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
                writeWord(offset, *reinterpret_cast<uint16_t*>(&value));
            } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
                writeDWord(offset, *reinterpret_cast<uint32_t*>(&value));
            } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
                writeQWord(offset, *reinterpret_cast<uint64_t*>(&value));
            }
        }
    };

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
    uint32_t readConfigDWord(uint64_t offset);
    uint64_t readConfigQWord(uint64_t offset);
    void writeConfigByte(uint64_t offset, uint8_t value);
    void writeConfigWord(uint64_t offset, uint16_t value);
    void writeConfigDWord(uint64_t offset, uint32_t value);
    void writeConfigQWord(uint64_t offset, uint64_t value);

    BAR getBar(uint8_t index);

    template<typename T>
    inline T readConfig(uint64_t offset) {
        if constexpr (sizeof(T) == sizeof(uint8_t)) {
            uint8_t res = readConfigByte(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
            uint16_t res = readConfigWord(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
            uint32_t res = readConfigDWord(offset);
            return *reinterpret_cast<T*>(&res);
        } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            uint64_t res = readConfigQWord(offset);
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
            writeConfigDWord(offset, *reinterpret_cast<uint32_t*>(&value));
        } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
            writeConfigQWord(offset, *reinterpret_cast<uint64_t*>(&value));
        }
    }
};
