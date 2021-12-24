#include "Memory/pageTable.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "Common/Symbols.hpp"
#include "Common/Units.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"
#include "Memory/physicalAllocator.hpp"
#include "Memory/tempMapping.hpp"

struct PageEntry {
    union {
        uint64_t raw;
        struct {
            uint64_t present : 1;
            uint64_t writeEnable : 1;
            uint64_t userModeEnable : 1;
            uint64_t writeThrough : 1;
            uint64_t cacheDisable : 1;
            uint64_t accessed : 1;
            uint64_t dirty : 1;
            uint64_t bigPage : 1;
            uint64_t global : 1;
            uint64_t available1 : 3;
            uint64_t encAddress : 36;
            uint64_t zero : 4;
            uint64_t available2 : 7;
            uint64_t protectionKey : 4;
            uint64_t executeDisable : 1;
        };
    };
} __attribute__((__packed__));

static_assert(sizeof(PageEntry) == sizeof(void*));

struct PagingPage {
    PageEntry entries[4096 / sizeof(PageEntry)];
};

struct PagingAddress {
    union {
        struct {
            uint16_t inPage : 12;
            uint64_t level1 : 9;
            uint64_t level2 : 9;
            uint64_t level3 : 9;
            uint64_t level4 : 9;
            uint64_t bitExtension : 16;
        };
        void* pointer;
    };
} __attribute__((__packed__));

static_assert(sizeof(PagingPage) == pageSize, "PagingPage size is not pageSize");

static uint64_t virtualLevel4Address;
static bool inInit;// use physical address in init

alignas(4096) static char pagingInitPageBuffer[4096 * 6]{};
static uint8_t pagingInitPageIndex;

static PagingPage* allocatePage() {
    PagingPage* page;
    if (inInit) {
        if (pagingInitPageIndex >= sizeof(pagingInitPageBuffer) / sizeof(pagingInitPageBuffer[0])) {
            Output::getDefault()->print("Paging: Out of memory in init\n");
            stop();
        }
        page = (PagingPage*) (((uint64_t) pagingInitPageBuffer) + (pagingInitPageIndex++) * pageSize);
    } else {
        page = (PagingPage*) TempMemory::mapPages(PhysicalAllocator::allocatePhysicalMemory(1), 1, false);
    }
    memset(page, 0, pageSize);
    return page;
}

static PagingPage* getLevel4Page() {
    if (inInit) {
        uint64_t address = getCR3();
        return (PagingPage*) address;
    }
    return (PagingPage*) virtualLevel4Address;
}

static PagingPage* getLinkedPage(PageEntry& entry, bool allocating) {
    if (entry.present) {
        if (inInit) {
            return (PagingPage*) (entry.encAddress << 12);
        } else {
            PagingPage* page = (PagingPage*) TempMemory::mapPages(entry.encAddress << 12, 1, false);
            return page;
        }
    } else {
        if (allocating) {
            PagingPage* page = allocatePage();
            *((uint64_t*) &page) &= ~(pageSize - 1);// shouldn't be necessary
            uint64_t physical = PageTable::getPhysicalAddress((uint64_t) page);
            memset(page, 0, sizeof(PagingPage));
            entry.raw = (uint64_t) physical;
            entry.present = true;
            entry.executeDisable = false;
            entry.writeEnable = true;
            entry.userModeEnable = true;
            entry.writeThrough = false;
            entry.cacheDisable = false;
            entry.dirty = false;
            entry.accessed = false;
            entry.bigPage = false;
            entry.zero = 0;
            entry.global = false;
            entry.available1 = 0;
            entry.available2 = 0;
            entry.protectionKey = 0;
            return page;
        } else {
            return nullptr;
        }
    }
}

void PageTable::init() {
    uint64_t level4Address = getCR3();
    //map level 4 page to virtual
    inInit = true;
    virtualLevel4Address = level4Address + 96Ti;
    memset(pagingInitPageBuffer, 0, sizeof(pagingInitPageBuffer));
    // map 0 - 512Gi to 96Ti - 96.5Ti
    PagingAddress baseAddress;
    baseAddress.pointer = (void*) 96Ti;
    PagingPage* level4Page = (PagingPage*) level4Address;
    PagingPage* level3Page = getLinkedPage(level4Page->entries[baseAddress.level4], true);
    for (uint64_t i = 0; i < 512; ++i) {
        level3Page->entries[i].raw = 1Gi * i;
        level3Page->entries[i].bigPage = true;// 1Gi page
        level3Page->entries[i].present = true;
        level3Page->entries[i].writeEnable = true;
        level3Page->entries[i].userModeEnable = false;
        level3Page->entries[i].writeThrough = true;
        level3Page->entries[i].cacheDisable = true;
        level3Page->entries[i].dirty = false;
        level3Page->entries[i].accessed = false;
        level3Page->entries[i].global = false;
        level3Page->entries[i].available1 = 0;
        level3Page->entries[i].available2 = 0;
        level3Page->entries[i].protectionKey = 0;
        level3Page->entries[i].executeDisable = false;
        invalidatePage(96Ti + (1Gi * i));
    }
    inInit = false;
}

void PageTable::unmap(uint64_t virtualAddress) {
    PagingAddress address;
    address.pointer = (void*) virtualAddress;
    PagingPage* level4Page = getLevel4Page();
    PagingPage* level3Page = getLinkedPage(level4Page->entries[address.level4], false);
    if (!level3Page) { return; }
    if (level3Page->entries[address.level3].bigPage) {
        level3Page->entries[address.level3].present = false;
    }
    PagingPage* level2Page = getLinkedPage(level3Page->entries[address.level3], false);
    if (!level2Page) { return; }
    if (level2Page->entries[address.level2].bigPage) {
        level2Page->entries[address.level2].present = false;
    }
    PagingPage* level1Page = getLinkedPage(level2Page->entries[address.level2], false);
    if (!level1Page) { return; }
    level1Page->entries[address.level1].present = false;
}


void PageTable::map(uint64_t physicalAddress, void* virtualAddress, Option option) {
    PagingAddress address;
    address.pointer = virtualAddress;
    PagingPage* level4Page = getLevel4Page();
    PagingPage* level3Page = getLinkedPage(level4Page->entries[address.level4], true);
    PagingPage* level2Page = getLinkedPage(level3Page->entries[address.level3], true);
    PagingPage* level1Page = getLinkedPage(level2Page->entries[address.level2], true);

    option.executeDisable = false;//TODO check if execute disable is supported and enable it else disable executeDisable

    level1Page->entries[address.level1].raw = physicalAddress & ~(pageSize - 1);
    level1Page->entries[address.level1].writeEnable = option.writeEnable;
    level1Page->entries[address.level1].userModeEnable = option.userAvailable;
    level1Page->entries[address.level1].writeThrough = option.writeThrough;
    level1Page->entries[address.level1].cacheDisable = option.cacheDisable;
    level1Page->entries[address.level1].executeDisable = option.executeDisable;
    level1Page->entries[address.level1].present = true;
    level1Page->entries[address.level1].dirty = false;
    level1Page->entries[address.level1].accessed = false;
    level1Page->entries[address.level1].bigPage = false;
    level1Page->entries[address.level1].global = false;
    level1Page->entries[address.level1].zero = 0;
    level1Page->entries[address.level1].available1 = 0;
    level1Page->entries[address.level1].protectionKey = 0;
    level1Page->entries[address.level1].available2 = 0;
    invalidatePage((uint64_t) virtualAddress);
}

uint64_t PageTable::getPhysicalAddress(uint64_t virtualAddress) {
    PagingAddress address;
    address.pointer = (void*) virtualAddress;
    PagingPage* level4Page = getLevel4Page();
    PagingPage* level3Page = getLinkedPage(level4Page->entries[address.level4], false);
    if (!level3Page) { return 0; }
    if (level3Page->entries[address.level3].bigPage) {
        return (level3Page->entries[address.level3].encAddress << 12) | address.inPage | (address.level2 << 21) | (address.level1 << 12);
    }
    PagingPage* level2Page = getLinkedPage(level3Page->entries[address.level3], false);
    if (!level2Page) { return 0; }
    if (level2Page->entries[address.level2].bigPage) {
        return (level2Page->entries[address.level2].encAddress << 12) | address.inPage | (address.level1 << 12);
    }
    PagingPage* level1Page = getLinkedPage(level2Page->entries[address.level2], false);
    if (!level1Page) { return 0; }
    if (level1Page->entries[address.level1].present) {
        return (level1Page->entries[address.level1].encAddress << 12) | address.inPage;
    }
    return 0;
}