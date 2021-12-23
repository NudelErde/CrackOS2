#include "LanguageFeatures/memory.hpp"
#include "BasicOutput/Output.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include <stdint.h>

extern "C" void* memset(void* ptr, int v, uint64_t num) {
    uint8_t value = (uint8_t) v;
    uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
    uint64_t val = value | (value << 8) | (value << 16) | (value << 24);
    val |= (val << 32);
    uint64_t i = 0;
    for (; i < num && (i & 7); ++i) {// align to 8 bytes
        p[i] = value;
    }
    for (; i + 7 < num; i += 8) {// fill 8 bytes at a time
        *reinterpret_cast<uint64_t*>(p + i) = val;
    }
    for (; i < num; ++i) {// fill the rest
        p[i] = value;
    }
    return ptr;
}

extern "C" void* memcpy(void* destination, const void* source, uint64_t num) {
    uint8_t* d = (uint8_t*) destination;
    const uint8_t* s = (const uint8_t*) source;
    for (uint64_t i = 0; i < num; ++i) {
        d[i] = s[i];
    }
    return destination;
}

extern "C" int memcmp(const void* ptr1, const void* ptr2, uint64_t num) {
    const uint8_t* p1 = (const uint8_t*) ptr1;
    const uint8_t* p2 = (const uint8_t*) ptr2;
    for (uint64_t i = 0; i < num; ++i) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

extern "C" char* strncpy(char* destination, const char* source, uint64_t num) {
    char* d = destination;
    const char* s = source;
    uint64_t i;
    for (i = 0; i < num; ++i) {
        d[i] = s[i];
        if (s[i] == '\0') {
            break;
        }
    }
    for (; i < num; ++i) {
        d[i] = '\0';
    }
    return destination;
}

extern "C" int strcmp(const char* str1, const char* str2) {
    for (uint64_t i = 0;; ++i) {
        if (str1[i] != str2[i]) {
            return str1[i] - str2[i];
        }
        if (str1[i] == '\0') {// str1[i] == str2[i] == '\0' because the previous if statement would have returned if they were not equal
            return 0;
        }
    }
}

extern "C" uint64_t strlen(const char* str) {
    for (uint64_t i = 0;; ++i) {
        if (str[i] == '\0') {
            return i;
        }
    }
}


extern "C" void __cxa_pure_virtual() {
    //todo error handling
    VGATextOut::getDefault()->print("pure virtual function called\n");
    stop();
}

void operator delete(void* p) {
    //check if p is null
    if (p == nullptr) {
        return;
    }
    //check if kernel memory is loaded
    //TODO
}

void operator delete(void* p, uint64_t size) {
    //check if p is null
    if (p == nullptr) {
        return;
    }
    //check if kernel memory is loaded
    //TODO
}

void* operator new(uint64_t size) {
    //check if kernel memory is loaded
    //TODO
    return nullptr;
}