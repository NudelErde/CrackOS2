#pragma once

#include "Memory/memory.hpp"
#include "Storage/Partition.hpp"
#include "stdint.h"

class Storage {
public:
    virtual uint64_t getSize() = 0;
    virtual int64_t read(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual int64_t write(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual const char* getTypeName() = 0;

    virtual ~Storage() = default;

    inline const shared_ptr<Storage>& getNext() {
        return next;
    }

    inline const shared_ptr<PartitionTable>& getPartition() {
        return partition;
    }

    inline void setPartition(const shared_ptr<PartitionTable>& partition) {
        this->partition = partition;
    }

    static void addStorage(const shared_ptr<Storage>&);
    static const shared_ptr<Storage>& getFirst();

private:
    shared_ptr<Storage> next;
    shared_ptr<PartitionTable> partition;
};
