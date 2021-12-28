#include "Memory/physicalAllocator.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "Common/Symbols.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/pageTable.hpp"
#include "Memory/tempMapping.hpp"

static uint64_t maxAddress;
static uint64_t totalMemorySize;
static uint64_t usedMemorySize;
static uint64_t unusableRegionCount;
static uint64_t usableRegionsCount;

struct MemoryTag {
    uint32_t type;
    uint32_t size;
    uint32_t entrySize;
    uint32_t entryVersion;
} __attribute__((__packed__));
struct MemoryData {
    uint64_t baseAddress;
    uint64_t length;
    uint32_t type;
    uint32_t rsv;
} __attribute__((__packed__));

static_assert(sizeof(MemoryTag) == 16, "MemoryTag is not 16 bytes");
static_assert(sizeof(MemoryData) == 24, "MemoryData is not 24 bytes");

using Callback = void (*)(void* context, uint64_t baseAddress, uint64_t length, uint32_t type);

static void readMemoryInfos(uint8_t* ptr, void* context, Callback callback) {
    MemoryTag* tag = (MemoryTag*) ptr;
    if (tag->type != 6) {
        Output::getDefault()->printf("readMemoryInfos: called with wrong tag type %d\n", tag->type);
        stop();
    }
    uint32_t size = tag->size - sizeof(MemoryTag);
    uint32_t entryCount = size / tag->entrySize;
    MemoryData* data = (MemoryData*) (ptr + sizeof(MemoryTag));
    for (uint32_t i = 0; i < entryCount; i++) {
        callback(context, data->baseAddress, data->length, data->type);
        data++;
    }
}

/**
 * @brief Collects information about the memory (total size).
 * @param memoryMap the pointer to the table from multiboot.
 */
static void getStaticData(uint8_t* ptr) {
    totalMemorySize = 0;
    usedMemorySize = 0;
    maxAddress = 0;
    unusableRegionCount = 0;
    usableRegionsCount = 0;
    readMemoryInfos(ptr, nullptr, [](void* context, uint64_t baseAddress, uint64_t length, uint32_t type) {
        if (type == 1) {
            totalMemorySize += length;
            if (maxAddress < baseAddress + length) {
                maxAddress = baseAddress + length;
            }
            usableRegionsCount++;
        } else {
            unusableRegionCount++;
        }
    });
}

uint64_t PhysicalAllocator::getTotalMemorySize() {
    return totalMemorySize;
}
uint64_t PhysicalAllocator::getMaxAddress() {
    return maxAddress;
}
uint64_t PhysicalAllocator::getUsedMemorySize() {
    return usedMemorySize;
}

struct MemoryRegionDescriptor {
    enum class Type {
        Unclaimed,     // Free to use
        UnusableMemory,// Not usable because the bios or uefi is using it or the memory is defective
        AcpiTable,     // Used by the acpi tables (e.g. the memory map) can be read and written but not allocated
        Claimed,       // Used but was allocated using allocatePhysicalMemory
        Ignore         // Ignore this entry
    };
    uint64_t startPageIndex;
    uint64_t pageCount;
    Type type;
    uint64_t nextPhysicalAddress;
};

uint64_t PhysicalAllocator::allocatePhysicalMemory(uint64_t count) {
    //iterate over free memory regions
    MemoryRegionDescriptor* region = (MemoryRegionDescriptor*) TempMemory::mapPages(0, 1, false);
    MemoryRegionDescriptor* last = region;
    region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    while (region->nextPhysicalAddress) {// can only find usable memory regions
        if (region->pageCount >= count) {
            //found a region
            region->pageCount -= count;// remove memory from the back
            uint64_t physicalAddress = region->startPageIndex + region->pageCount;
            usedMemorySize += count;
            if (region->pageCount == 0) {
                //remove the region
                last->nextPhysicalAddress = region->nextPhysicalAddress;
                region->nextPhysicalAddress = 0;
            }
            return physicalAddress * pageSize;
        }
        last = region;
        region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    }
    Output::getDefault()->printf("Unable to allocate %lu pages (out of physical memory)\n", count);
    stop();
    return ~0;
}
void PhysicalAllocator::freePhysicalMemory(uint64_t address, uint64_t count) {
    //check if given memory region is in unusable memory
    MemoryRegionDescriptor* region = (MemoryRegionDescriptor*) TempMemory::mapPages(0, 1, false);
    MemoryRegionDescriptor* start = region;
    uint64_t pageIndex = address / pageSize;
    if (pageIndex == 0) {
        //the first page is unusable
        return;
    }
    for (uint64_t i = 1; i < unusableRegionCount + 1; ++i) {// index 0 is our special region
        if (pageIndex <= region[i].startPageIndex + region[i].pageCount && region[i].startPageIndex <= pageIndex + count) {
            return;
        }
    }

    MemoryRegionDescriptor* last = nullptr;
    region = start;

    while (region->nextPhysicalAddress) {
        //[a1,a2) and [b1,b2)
        //a1 <= b2 && b1 <= a2
        if (pageIndex <= region->startPageIndex + region->pageCount && region->startPageIndex <= pageIndex + count) {
            // the region we are trying to free can not be freed because it already is
            return;
        }

        if (last != nullptr) {
            //check if the region start is after the last regions end and the region end is before the start of the region we are looking at
            if (last->startPageIndex + last->pageCount < pageIndex && pageIndex + count < region->startPageIndex + region->pageCount) {
                MemoryRegionDescriptor* newRegion = (MemoryRegionDescriptor*) TempMemory::mapPages(pageIndex * pageSize, 1, false);
                newRegion->startPageIndex = pageIndex;
                newRegion->pageCount = count;
                newRegion->type = MemoryRegionDescriptor::Type::Unclaimed;
                newRegion->nextPhysicalAddress = PageTable::getPhysicalAddress((uint64_t) region);
                last->nextPhysicalAddress = pageIndex * pageSize;
                break;
            }
        }
        last = region;
        region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    }

    // connect adjacent regions
    last = nullptr;
    region = start;
    while (region->nextPhysicalAddress) {
        if (last != nullptr) {
            if (last->startPageIndex + last->pageCount + 1 == region->startPageIndex) {
                last->pageCount += region->pageCount;
                last->nextPhysicalAddress = region->nextPhysicalAddress;
                region->nextPhysicalAddress = 0;
                //only load next region as there could be 3 or more regions adjacent to each other
                region = (MemoryRegionDescriptor*) TempMemory::mapPages(last->nextPhysicalAddress, 1, false);
                continue;
            }
        }
        last = region;
        region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    }
}

struct Context1 {
    MemoryRegionDescriptor* regions;
};
struct Context2 {
    MemoryRegionDescriptor* last;
};

static void initMemoryInfos(uint8_t* ptr) {
    if (sizeof(MemoryRegionDescriptor) * (unusableRegionCount + 1) > pageSize) {
        Output::getDefault()->print("Too many unusable memory regions\n");
        stop();
    }
    uint8_t* staticInfoPage = TempMemory::mapPages(0, 1, false);

    // leave space for pointer to first descriptor of usable memory
    MemoryRegionDescriptor* regions = (MemoryRegionDescriptor*) (staticInfoPage);
    regions[0].startPageIndex = 0;
    regions[0].pageCount = 0;
    regions[0].type = MemoryRegionDescriptor::Type::Ignore;
    regions[0].nextPhysicalAddress = 0;
    regions++;
    Context1 context1;
    context1.regions = regions;
    readMemoryInfos(ptr, &context1, [](void* context, uint64_t baseAddress, uint64_t length, uint32_t type) {
        Context1* ctx = (Context1*) context;
        if (type == 1) {
            return;
        }
        if (type == 3) {
            ctx->regions->type = MemoryRegionDescriptor::Type::AcpiTable;
        } else {
            ctx->regions->type = MemoryRegionDescriptor::Type::UnusableMemory;
        }
        ctx->regions->startPageIndex = baseAddress / pageSize;
        ctx->regions->pageCount = length / pageSize;
        ctx->regions++;
    });

    // iterate over usable memory regions and create a MemoryRegionDescriptor in the first page and link them like a unidirectional linked list
    // special case is the memory region that starts with the null page, because that first page is used for the static info
    // the pointer to the linked list is at the beginning of the static info page

    Context2 context2;
    regions = (MemoryRegionDescriptor*) (staticInfoPage);
    context2.last = regions;
    readMemoryInfos(ptr, &context2, [](void* context, uint64_t baseAddress, uint64_t length, uint32_t type) {
        Context2* ctx = (Context2*) context;
        if (type != 1) {
            return;
        }
        if (baseAddress == 0) {
            baseAddress += pageSize;
            length -= pageSize;
        }
        MemoryRegionDescriptor* descriptor = (MemoryRegionDescriptor*) TempMemory::mapPages(baseAddress, 1, false);
        descriptor->startPageIndex = baseAddress / pageSize;
        descriptor->pageCount = length / pageSize;
        descriptor->type = MemoryRegionDescriptor::Type::Unclaimed;
        descriptor->nextPhysicalAddress = 0;
        ctx->last->nextPhysicalAddress = baseAddress;
        ctx->last = descriptor;
        Output::getDefault()->printf("Found usable memory region at 0x%llx with size 0x%llx\n", baseAddress, length);
    });
}

static void setUsed(uint64_t start, uint64_t length) {
    MemoryRegionDescriptor* region = (MemoryRegionDescriptor*) TempMemory::mapPages(0, 1, false);
    MemoryRegionDescriptor* last = region;
    region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    while ((uint64_t) region != 96Ti) {// can only find usable memory regions
        // Output::getDefault()->printf("Memory: Looking at region at 0x%llx with size 0x%llx\n", region->startPageIndex * pageSize, region->pageCount * pageSize);
        // [a1,a2] and [b1,b2] // start and end inclusive
        // a1 = start / pageSize, a2 = (start + length + pageSize - 1) / pageSize
        // b1 = region->startPageIndex, b2 = region->startPageIndex + region->pageCount
        // case 1a: a1----a2----b1----b2 -> do nothing
        // case 1b: b1----b2----a1----a2 -> do nothing
        // case 2:  a1----b1----b2----a2 -> delete region b1----b2
        // case 3:  a1----b1----a2----b2 -> change start of region to a2
        // case 4:  b1----a1----b2----a2 -> change end of region to a1
        // case 5:  b1----a1----a2----b2 -> change end of region to a1 and create region a2----b2
        uint64_t a1 = start / pageSize;
        uint64_t a2 = (start + length + pageSize - 1) / pageSize;
        a2 += 1;// add one page to be save
        uint64_t b1 = region->startPageIndex;
        uint64_t b2 = region->startPageIndex + region->pageCount;
        if (a2 <= b1 || b2 <= a1) {
            // case 1
            // do nothing
        } else if (a1 <= b1 && b2 <= a2) {
            // case 2
            // delete region b1----b2
            // Output::getDefault()->printf("Memory: Case2: Deleting region at 0x%llx with size 0x%llx\n", b1 * pageSize, b2 * pageSize);
            last->nextPhysicalAddress = region->nextPhysicalAddress;
            region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
            // Output::getDefault()->printf("Memory: Deleted region\n");
            continue;
        } else if (a1 <= b1) {
            // case 3
            // change start of region to a2
            // Output::getDefault()->printf("Memory: Case3: Changing start of region at 0x%llx with size 0x%llx to 0x%llx\n", b1 * pageSize, b2 * pageSize, a2 * pageSize);
            uint64_t newStart = a2 * pageSize;
            MemoryRegionDescriptor* newRegion = (MemoryRegionDescriptor*) TempMemory::mapPages(newStart, 1, false);
            newRegion->startPageIndex = a2;
            newRegion->pageCount = b2 - a2;// end - start
            newRegion->type = region->type;
            newRegion->nextPhysicalAddress = region->nextPhysicalAddress;
            last->nextPhysicalAddress = newStart;
            region = newRegion;
            // Output::getDefault()->printf("Memory: Region is now at 0x%llx with size 0x%llx\n", region->startPageIndex * pageSize, region->pageCount * pageSize);
        } else if (b2 <= a2) {
            // case 4
            // change end of region to a1
            // Output::getDefault()->printf("Memory: Case4: Changing end of region at 0x%llx with size 0x%llx to 0x%llx\n", b1 * pageSize, b2 * pageSize, a1 * pageSize);
            region->pageCount = a1 - b1;
            // Output::getDefault()->printf("Memory: Region is now at 0x%llx with size 0x%llx\n", region->startPageIndex * pageSize, region->pageCount * pageSize);
        } else {
            // case 5
            // change end of region to a1 and create region a2----b2
            // Output::getDefault()->printf("Memory: Case5: Changing end of region at 0x%llx with size 0x%llx to 0x%llx and creating region at 0x%llx with size 0x%llx\n", b1 * pageSize, b2 * pageSize, a1 * pageSize, a2 * pageSize, b2 * pageSize);

            // change end of region to a1
            region->pageCount = a1 - b1;
            // Output::getDefault()->printf("Memory: Region is now at 0x%llx with size 0x%llx\n", region->startPageIndex * pageSize, region->pageCount * pageSize);

            // create region a2----b2
            uint64_t newStart = a2 * pageSize;
            MemoryRegionDescriptor* newRegion = (MemoryRegionDescriptor*) TempMemory::mapPages(newStart, 1, false);
            newRegion->startPageIndex = a2;
            newRegion->pageCount = b2 - a2;
            newRegion->type = region->type;
            newRegion->nextPhysicalAddress = region->nextPhysicalAddress;
            region->nextPhysicalAddress = newStart;
            // Output::getDefault()->printf("Memory: Region is now at 0x%llx with size 0x%llx\n", region->startPageIndex * pageSize, region->pageCount * pageSize);
        }

        last = region;
        region = (MemoryRegionDescriptor*) TempMemory::mapPages(region->nextPhysicalAddress, 1, false);
    }
}

void PhysicalAllocator::readMultibootInfos(uint8_t* ptr) {
    getStaticData(ptr);
    initMemoryInfos(ptr);
    //set kernel used
    uint64_t trampolineStart;
    saveReadSymbol("trampolineStart", trampolineStart);
    uint64_t trampolineEnd;
    saveReadSymbol("trampolineEnd", trampolineEnd);
    uint64_t kernelStart;
    saveReadSymbol("physical_start", kernelStart);
    uint64_t kernelEnd;
    saveReadSymbol("physical_end", kernelEnd);

    setUsed(kernelStart, kernelEnd - kernelStart);
    Output::getDefault()->printf("Memory: Kernel used: 0x%llx - 0x%llx\n", kernelStart, kernelEnd);
    setUsed(trampolineStart, trampolineEnd - trampolineStart);
    Output::getDefault()->printf("Memory: Trampoline used: 0x%llx - 0x%llx\n", trampolineStart, trampolineEnd);
}