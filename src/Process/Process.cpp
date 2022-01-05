#include "Process/Process.hpp"
#include "LanguageFeatures/Types.hpp"
#include "Memory/pageTable.hpp"

Mapping::Mapping() {
    head = nullptr;
}

void Mapping::map(uint64_t virtAddr, uint64_t physAddr, uint64_t size, Flags flags) {
    auto entry = make_unique<Entry>();
    entry->virtAddr = virtAddr;
    entry->physAddr = physAddr;
    entry->size = size;
    entry->flags = flags;
    entry->next = std::move(head);
    head = std::move(entry);
}

void Mapping::unmap(uint64_t virtAddr, uint64_t size) {
    //look for entry that contains virtAddr
    for (auto entry = head.get(); entry != nullptr; entry = entry->next.get();) {
        if (entry->virtAddr <= virtAddr && entry->virtAddr + entry->size >= virtAddr + size) {
            //found entry
            if (entry->virtAddr == virtAddr && entry->size == size) {
                //remove entry
                head = std::move(entry->next);
            } else {
                //split entry
                auto newEntry = make_unique<Entry>();
                newEntry->virtAddr = entry->virtAddr + size;
                newEntry->physAddr = entry->physAddr + size;
                newEntry->size = entry->size - size;
                newEntry->flags = entry->flags;
                newEntry->next = std::move(entry->next);
                entry->size = virtAddr - entry->virtAddr;
                entry->next = std::move(newEntry);
            }
            return;
        }
    }
}

void Mapping::compact() {
    bool compacted;
    do {
        compacted = false;
        for (auto entry = head.get(); entry != nullptr; entry = entry->next.get()) {
            if (entry->next != nullptr && entry->virtAddr + entry->size == entry->next->virtAddr && entry->physAddr + entry->size == entry->next->physAddr) {
                //merge entries
                entry->size += entry->next->size;
                entry->next = std::move(entry->next->next);
                compacted = true;
            }
        }
    } while (compacted);
}

void Mapping::load() {
    for (auto entry = head.get(); entry != nullptr; entry = entry->next.get()) {
        auto virtAddr = entry->virtAddr;
        auto physAddr = entry->physAddr;
        auto size = entry->size;
        auto flags = entry->flags;
        for (uint64_t i = 0; i < size; i += pageSize) {
            auto page = physAddr + i;
            auto virtPage = virtAddr + i;
            PageTable::map(page, (uint8_t*) virtPage, {
                                                              /*writeEnable:*/ flags.writable,
                                                              /*userAvailable:*/ true,
                                                              /*writeThrough:*/ false,
                                                              /*cacheDisable:*/ false,
                                                              /*executeDisable:*/ false,
                                                      });
        }
    }
}

void Mapping::unload() {
    for (auto entry = head.get(); entry != nullptr; entry = entry->next.get()) {
        auto virtAddr = entry->virtAddr;
        auto size = entry->size;
        for (uint64_t i = 0; i < size; i += pageSize) {
            auto virtPage = virtAddr + i;
            PageTable::unmap(virtPage);
        }
    }
}