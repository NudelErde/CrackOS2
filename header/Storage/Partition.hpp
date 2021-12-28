#pragma once
#include "BasicOutput/Output.hpp"
#include "Memory/memory.hpp"

class Storage;
class PartitionTable;// provides its partitions (is created with a Storage)
class Partition;     // has read and write and shared pointer to its partition table

class PartitionTable {
public:
    inline PartitionTable(weak_ptr<Storage> storage) : storage(storage){};
    virtual ~PartitionTable(){};

    virtual unique_ptr<Partition> getPartition(uint32_t index) = 0;
    virtual uint32_t getPartitionCount() = 0;

    inline bool valid() {
        return !storage.expired() && !self.expired();
    }

    inline weak_ptr<Storage> getStorage() {
        return storage;
    }

protected:
    weak_ptr<Storage> storage;
    weak_ptr<PartitionTable> self;

    friend class Storage;
    friend class Partition;
};

class Partition {
public:
    virtual uint64_t getSize() = 0;
    virtual int64_t read(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual int64_t write(uint64_t offset, uint64_t size, uint8_t* buffer) = 0;

    virtual ~Partition(){};

    inline bool valid() {
        return partitionTable.exists();
    }

    inline Partition(shared_ptr<PartitionTable> partitionTable) : partitionTable(partitionTable){};

protected:
    shared_ptr<PartitionTable> partitionTable;
};

class OffsetImplementationPartition : public Partition {
public:
    inline uint64_t getSize() override {
        return size;
    }

    int64_t read(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    int64_t write(uint64_t offset, uint64_t size, uint8_t* buffer) override;

    inline OffsetImplementationPartition(shared_ptr<PartitionTable> partitionTable, uint64_t offset, uint64_t size)
        : Partition(partitionTable), offset(offset), size(size) {}

    inline ~OffsetImplementationPartition() {}

private:
    uint64_t size;
    uint64_t offset;
};

class MBRPartition : public OffsetImplementationPartition {
public:
    inline MBRPartition(shared_ptr<PartitionTable> partitionTable, uint64_t offset, uint64_t size, bool bootable,
                        uint8_t type)
        : OffsetImplementationPartition(partitionTable, offset, size), bootable(bootable), type(type) {}
    inline ~MBRPartition() {}

private:
    bool bootable;
    uint8_t type;
};

class MBRPartitionTable : public PartitionTable {
public:
    MBRPartitionTable(weak_ptr<Storage> storage);
    ~MBRPartitionTable();

    unique_ptr<Partition> getPartition(uint32_t index) override;
    uint32_t getPartitionCount() override;

    static bool isUsableTableType(shared_ptr<Storage> storage);

private:
    uint8_t bootSector[512]{};
};