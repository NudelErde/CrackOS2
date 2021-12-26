#pragma once

#include "stdint.h"

class Storage {
public:
    virtual uint64_t getSize() = 0;
    virtual int64_t read(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual int64_t write(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;

    virtual ~Storage() = default;

    inline Storage* getNext() {
        return next;
    }
    static void addStorage(Storage*);
    static Storage* getFirst();

private:
    Storage* next;
};