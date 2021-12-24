#include "Memory/heap.hpp"
#include "BasicOutput/Output.hpp"
#include "Common/Units.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/physicalAllocator.hpp"

constexpr uint64_t kernelHeapStart = 80Ti;

struct PointerData {
    uint64_t size;       // stores the real size (requested + sizeof(PointerData))
    uint64_t nextVirtual;// linked list forwared pointer
    uint64_t prevVirtual;// linked list backward pointer
};

PointerData dummy;

void initHeap() {
    dummy.size = sizeof(PointerData);
    dummy.nextVirtual = 0;
    dummy.prevVirtual = 0;
}

void* kmalloc(uint64_t requestedSize) {
    uint64_t realSize = requestedSize + sizeof(PointerData);
    uint64_t pageCount = (realSize + pageSize - 1) / pageSize;// round up
    PointerData* data = &dummy;
    //try to fit the requested size in an allocated page
    //need to iterate through the linked list of PointerData
    while (data->nextVirtual != 0) {
        data = (PointerData*) data->nextVirtual;
        if (realSize > pageSize / 2)
            continue;// only insert in allocated block if realSize is smaller than half of the page

        uint64_t startOfThisBlock = ((uint64_t) data);
        uint64_t endOfThisBlock = startOfThisBlock + data->size;
        uint64_t endOfThisBlockPage = endOfThisBlock & ~(pageSize - 1);// the last page that is part of this block
        // if next entry is in same page as the end of this block continue
        if (data->nextVirtual > endOfThisBlockPage && data->nextVirtual < endOfThisBlockPage + pageSize)
            continue;

        uint64_t remainingSize = endOfThisBlockPage + pageSize - endOfThisBlock;
        if (remainingSize > realSize) {
            PointerData* newData = (PointerData*) endOfThisBlock;
            newData->size = realSize;
            newData->nextVirtual = data->nextVirtual;
            newData->prevVirtual = (uint64_t) data;
            if (data->nextVirtual != 0) {
                PointerData* nextData = (PointerData*) data->nextVirtual;
                nextData->prevVirtual = (uint64_t) newData;
            }
            data->nextVirtual = (uint64_t) newData;
            return (void*) (endOfThisBlock + sizeof(PointerData));
        }
    }
    //only reached if we are at the end of the linked list
    //allocate a new page and find the needed pages of virtual memory
    uint64_t physicalMemory = PhysicalAllocator::allocatePhysicalMemory(pageCount);
    if (physicalMemory == ~0ull)
        return nullptr;
    data = &dummy;
    while (data->nextVirtual != 0) {
        data = (PointerData*) data->nextVirtual;
        if (data->nextVirtual == 0) {
            break;
        }
        uint64_t startOfThisBlock = ((uint64_t) data);
        uint64_t endOfThisBlock = startOfThisBlock + data->size;
        uint64_t lastUsedPageIndex = (endOfThisBlock + pageSize - 1) / pageSize;// for example 45
        uint64_t startOfNextBlockPageIndex = data->nextVirtual / pageSize;      // for example 60
        uint64_t unusedPages = startOfNextBlockPageIndex - lastUsedPageIndex;   // should be 15
        if (unusedPages >= pageCount) {
            // we can fit the requested size in the current block

            // map the needed pages of virtual memory
            uint64_t virtualBase = lastUsedPageIndex * pageSize;
            for (uint64_t i = 0; i < pageCount; i++) {
                uint64_t virtualPage = virtualBase + i * pageSize;
                uint64_t physicalPage = physicalMemory + i * pageSize;
                PageTable::map(physicalPage, (void*) virtualPage, {
                                                                          .writeEnable = true,
                                                                          .userAvailable = false,
                                                                          .writeThrough = false,
                                                                          .cacheDisable = false,
                                                                          .executeDisable = false,
                                                                  });
            }
            PointerData* newData = (PointerData*) virtualBase;
            newData->size = realSize;
            newData->nextVirtual = data->nextVirtual;
            newData->prevVirtual = (uint64_t) data;
            if (data->nextVirtual != 0) {
                PointerData* nextData = (PointerData*) data->nextVirtual;
                nextData->prevVirtual = (uint64_t) newData;
            }
            data->nextVirtual = (uint64_t) newData;
            return (void*) (virtualBase + sizeof(PointerData));
        }
    }
    //there is not enough virtual memory between the allready allocated pointers
    //we need to allocate virtual memory behind the last pointer
    uint64_t virtualBase = ((uint64_t) data + data->size + pageSize - 1) & ~(pageSize - 1);
    if (data == &dummy) {
        virtualBase = kernelHeapStart;
    }
    for (uint64_t i = 0; i < pageCount; i++) {
        uint64_t virtualPage = virtualBase + i * pageSize;
        uint64_t physicalPage = physicalMemory + i * pageSize;
        PageTable::map(physicalPage, (void*) virtualPage, {
                                                                  .writeEnable = true,
                                                                  .userAvailable = false,
                                                                  .writeThrough = false,
                                                                  .cacheDisable = false,
                                                                  .executeDisable = false,
                                                          });
    }
    PointerData* newData = (PointerData*) virtualBase;
    newData->size = realSize;
    newData->nextVirtual = 0;
    newData->prevVirtual = (uint64_t) data;
    data->nextVirtual = (uint64_t) newData;
    return (void*) (virtualBase + sizeof(PointerData));
}
void kfree(void* ptr) {
    PointerData* data = (PointerData*) ((uint64_t) ptr - sizeof(PointerData));
    if (data->nextVirtual == 0 && data->prevVirtual == 0) {
        return;
    }
    uint64_t startOfThisBlock = ((uint64_t) data);
    uint64_t endOfThisBlock = startOfThisBlock + data->size;
    uint64_t extendedEndOfThisBlock = (endOfThisBlock | (pageSize - 1)) + 1;        // example: (0x1555 | 0xfff) + 1 = 0x1fff + 1 = 0x2000
    uint64_t pageStartOfThisBlock = startOfThisBlock & ~(pageSize - 1);             // example: 0x1000
    uint64_t pageCount = (extendedEndOfThisBlock - pageStartOfThisBlock) / pageSize;// example: 0x2000 - 0x1000 = 0x1000 / 0x1000 = 0x1
    bool endPageUsed = false;
    bool beginPageUsed = false;
    if (data->nextVirtual != 0) {
        if (data->nextVirtual < extendedEndOfThisBlock && data->nextVirtual >= extendedEndOfThisBlock - pageSize) {
            endPageUsed = true;
        }
    }
    if (data->prevVirtual < startOfThisBlock + pageSize && data->prevVirtual >= startOfThisBlock) {
        beginPageUsed = true;
    }

    //remove data from linked list
    if (data->nextVirtual) {
        PointerData* nextData = (PointerData*) data->nextVirtual;
        nextData->prevVirtual = data->prevVirtual;
    }
    if (data->prevVirtual) {
        PointerData* prevData = (PointerData*) data->prevVirtual;
        prevData->nextVirtual = data->nextVirtual;
    }
    data->nextVirtual = 0;
    data->prevVirtual = 0;

    if (pageCount == 1) {
        if (!endPageUsed && !beginPageUsed) {
            PhysicalAllocator::freePhysicalMemory(pageStartOfThisBlock, 1);
        }
    } else {
        uint64_t start = pageStartOfThisBlock + pageSize;
        uint64_t count = pageCount - 2;
        if (!beginPageUsed) {
            start -= pageSize;
            count += 1;
        }
        if (!endPageUsed) {
            count += 1;
        }
        PhysicalAllocator::freePhysicalMemory(start, count);
    }
}