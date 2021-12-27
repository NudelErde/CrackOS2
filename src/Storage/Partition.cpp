#include "Storage/Partition.hpp"

MBRPartitionTable::MBRPartitionTable(weak_ptr<Storage> storage) : PartitionTable(storage) {}
MBRPartitionTable::~MBRPartitionTable() {}

unique_ptr<Partition> MBRPartitionTable::getPartition(uint32_t index) {
}
uint32_t MBRPartitionTable::getPartitionCount() {
}

bool MBRPartitionTable::isUsableTableType(shared_ptr<Storage> storage) {
    return false;
}