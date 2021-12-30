#include "Storage/Partition.hpp"
#include "BasicOutput/Output.hpp"
#include "Storage/Ext4.hpp"
#include "Storage/Filesystem.hpp"
#include "Storage/Storage.hpp"

int64_t OffsetImplementationPartition::write(uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (offset + size > this->size) {
        return -1;
    }
    if (auto ptr = partitionTable->getStorage().lock(); ptr) {
        return ptr->write(offset + offset, size, buffer);
    }
    return -1;
}

int64_t OffsetImplementationPartition::read(uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (offset + size > this->size) {
        return -1;
    }
    if (auto ptr = partitionTable->getStorage().lock(); ptr) {
        return ptr->read(offset + offset, size, buffer);
    }
    return -1;
}

MBRPartitionTable::MBRPartitionTable(weak_ptr<Storage> storage) : PartitionTable(storage) {
    if (auto ptr = storage.lock(); ptr) {
        ptr->read(0, 512, bootSector);
    }
}
MBRPartitionTable::~MBRPartitionTable() {}

shared_ptr<Filesystem> MBRPartition::createFilesystem(shared_ptr<Partition> self) {
    if (type == 0x83) {
        shared_ptr<Ext4> ext4 = make_shared<Ext4>(self);
        return static_pointer_cast<Filesystem>(ext4);
    }
    return shared_ptr<Filesystem>();
}

unique_ptr<Partition> MBRPartitionTable::getPartition(uint32_t index) {
    if (index >= 4) {
        return nullptr;
    }
    uint8_t* data = &bootSector[446 + (index * 16)];
    bool bootable = (data[0] == 0x80);
    uint8_t type = data[4];
    uint64_t offset = *(uint32_t*) (data + 8);
    uint64_t size = *(uint32_t*) (data + 12);
    offset *= 512;
    size *= 512;
    if (type == 0 || size == 0) {
        return nullptr;
    }
    return unique_ptr<Partition>(new MBRPartition(self.lock(), offset, size, bootable, type));
}
uint32_t MBRPartitionTable::getPartitionCount() {
    uint64_t count = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (auto ptr = storage.lock(); ptr) {
            auto part = getPartition(i);
            if (part && part->getSize() > 0) {
                count++;
            }
        }
    }
    return count;
}

bool MBRPartitionTable::isUsableTableType(shared_ptr<Storage> storage) {
    uint8_t bootSector[512]{};
    if (storage->read(0, 512, bootSector) != 512) {
        return false;
    }
    if (bootSector[510] != 0x55 || bootSector[511] != 0xAA) {
        return false;
    }

    //check if any partition has type 0xEE -> GPT partition table
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t* data = &bootSector[446 + (i * 16)];
        if (data[4] == 0xEE) {
            return false;
        }
    }

    return true;
}