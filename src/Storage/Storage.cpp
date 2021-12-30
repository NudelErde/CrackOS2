#include "Storage/Storage.hpp"
#include "Common/Units.hpp"

static uint8_t firstStorageSharedPointerBuffer[sizeof(shared_ptr<Storage>)]{};
static shared_ptr<Storage>* firstStorage;
static uint64_t storageCount = 0;

void Storage::addStorage(const shared_ptr<Storage>& obj) {
    if (firstStorage == nullptr) {
        firstStorage = new (firstStorageSharedPointerBuffer) shared_ptr<Storage>();
        storageCount = 0;
    }
    obj->next = *firstStorage;
    *firstStorage = obj;


    uint64_t partitionCount = 0;
    if (obj->getSize() > 0) {
        if (MBRPartitionTable::isUsableTableType(obj)) {
            obj->setPartition(static_pointer_cast<PartitionTable>(make_shared<MBRPartitionTable>(obj)));
            partitionCount = obj->getPartition()->getPartitionCount();
        }
    }

    const char* unit = "B";
    uint64_t divider = 1;
    if (obj->getSize() >= 1Ti) {
        unit = "TiB";
        divider = 1Ti;
    } else if (obj->getSize() >= 1Gi) {
        unit = "GiB";
        divider = 1Gi;
    } else if (obj->getSize() >= 1Mi) {
        unit = "MiB";
        divider = 1Mi;
    } else if (obj->getSize() >= 1Ki) {
        unit = "KiB";
        divider = 1Ki;
    }
    Output::getDefault()->printf("Storage: Device %hhu initialized! Type: %s Size: %u %s", storageCount, obj->getTypeName(), obj->getSize() / divider, unit);
    if (partitionCount > 0) {
        Output::getDefault()->printf(" with %u partition%s\n", partitionCount, partitionCount == 1 ? "" : "s");
    } else {
        Output::getDefault()->print('\n');
    }
    storageCount++;
}
const shared_ptr<Storage>& Storage::getFirst() {
    return *firstStorage;
}