#pragma once

#include "stdint.h"

struct BitList {
    template<typename T>
    BitList(T t) {
        value = *reinterpret_cast<uint64_t*>(&t);
        value &= (1ull << (sizeof(T) * 8ull)) - 1ull;
    }

    uint64_t value;
};

class Output {
public:
    virtual ~Output() = default;

    virtual void print(char c) = 0;
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
    static void init();
};