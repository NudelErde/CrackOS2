#include "Storage/Storage.hpp"

static uint8_t firstStorageSharedPointerBuffer[sizeof(shared_ptr<Storage>)]{};
static shared_ptr<Storage>* firstStorage;

void Storage::addStorage(const shared_ptr<Storage>& obj) {
    if (firstStorage == nullptr) {
        firstStorage = new (firstStorageSharedPointerBuffer) shared_ptr<Storage>();
    }
    obj->next = *firstStorage;
    *firstStorage = obj;
}
const shared_ptr<Storage>& Storage::getFirst() {
    return *firstStorage;
}