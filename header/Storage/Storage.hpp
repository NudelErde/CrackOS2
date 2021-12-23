#pragma once

#include "stdint.h"

class Storage {
public:
    virtual uint64_t getSize() = 0;
    virtual void read(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual void write(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
};