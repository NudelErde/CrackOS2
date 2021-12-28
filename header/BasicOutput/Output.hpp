#pragma once

#include "stdint.h"

struct BitList {
    template<typename T>
    BitList(T t) {
        if constexpr (sizeof(T) <= sizeof(uint8_t)) {
            value = *reinterpret_cast<uint8_t*>(&t);
        } else if constexpr (sizeof(T) <= sizeof(uint16_t)) {
            value = *reinterpret_cast<uint16_t*>(&t);
        } else if constexpr (sizeof(T) <= sizeof(uint32_t)) {
            value = *reinterpret_cast<uint32_t*>(&t);
        } else if constexpr (sizeof(T) <= sizeof(uint64_t)) {
            value = *reinterpret_cast<uint64_t*>(&t);
        }
        bitCount = sizeof(T) * 8;
    }

    uint64_t value;
    uint8_t bitCount;
};

class Output {
public:
    virtual ~Output() = default;

    virtual void printImpl(char c) = 0;

    void print(char c);
    void print(const char* str);
    void print(uint64_t hex, uint8_t minSize = 1, uint64_t maxSize = sizeof(uint64_t) * 2);
    void printDec(int64_t dec, bool printPlus = false);
    void printDec(uint64_t dec);
    void print(BitList bits);
    virtual void clear() = 0;
    virtual uint64_t getWidth() = 0;
    virtual uint64_t getHeight() = 0;
    virtual uint64_t getX() = 0;
    virtual uint64_t getY() = 0;
    virtual void setX(uint64_t x) = 0;
    virtual void setY(uint64_t y) = 0;

    inline void setCursor(uint64_t x, uint64_t y) {
        setX(x);
        setY(y);
    }

    template<typename T>
    void printHex(T&& hex, uint8_t minSize = 1, uint64_t maxSize = sizeof(T) * 2) {
        print(*reinterpret_cast<uint64_t*>(&hex), minSize, maxSize);
    }

    inline void printHexDump(uint8_t* data, uint64_t size) {
        for (uint64_t i = 0; i < size; i++) {
            printHex(data[i], 2);
            print(' ');
            if (i % 16 == 15) {
                print('\n');
            }
        }
    }

    template<typename V, typename... T>
    void printf(const char* format, V v, T... args) {
        char specifier;
        char length;
        char flags;
        uint8_t width;
        const char* fmt = printFormat(format, &specifier, &length, &flags, &width);
        if (specifier != 0)
            printValue(v, specifier, length, flags, width);
        if (*fmt == 0) {
            return;
        }
        printf(fmt, args...);
    }

    template<typename V>
    void printValue(V v, char specifier, char length, char flags, uint8_t width) {
        printValueImpl(*reinterpret_cast<uint64_t*>(&v), specifier, length, flags, width);
    }

    void printValueImpl(uint64_t v, char specifier, char length, char flags, uint8_t width);
    const char* printFormat(const char* format, char* specifier, char* length, char* flags, uint8_t* width);

    inline void printf(const char* format) {
        print(format);
    }

    static Output* getDefault();
    static void setDefault(Output* defaultOutput);
    static uint64_t readBuffer(uint8_t* buffer, uint64_t bufferSize);
    static void init();
};

class CombinedOutput : public Output {
private:
    Output* primary;
    Output* secondary;

public:
    inline CombinedOutput(Output* primary, Output* secondary) : primary(primary), secondary(secondary) {}
    inline void printImpl(char c) override {
        primary->printImpl(c);
        secondary->printImpl(c);
    }
    inline void clear() {
        primary->clear();
        secondary->clear();
    }
    inline uint64_t getWidth() {
        return primary->getWidth();
    }
    inline uint64_t getHeight() {
        return primary->getHeight();
    }
    inline uint64_t getX() {
        return primary->getX();
    }
    inline uint64_t getY() {
        return primary->getY();
    }
    inline void setX(uint64_t x) {
        primary->setX(x);
        secondary->setX(x);
    }
    inline void setY(uint64_t y) {
        primary->setY(y);
        secondary->setY(y);
    }
};