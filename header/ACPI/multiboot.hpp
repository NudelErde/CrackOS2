#pragma once
#include <stdint.h>

class Multiboot {
public:
    using Callback = void (*)(uint8_t*);

    Multiboot();

    void setTagDecoder(uint8_t tag, Callback callback);
    void decode(uint8_t* table);

private:
    Callback callbacks[256];
};

uint64_t getRsdtAddress();
void readMultiboot(uint64_t multiboot);