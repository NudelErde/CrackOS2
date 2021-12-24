#include "Storage/Storage.hpp"

static Storage* firstStorage = nullptr;

void Storage::addStorage(Storage* obj) {
    obj->next = firstStorage;
    firstStorage = obj;
}
Storage* Storage::getFirst() {
    return firstStorage;
}