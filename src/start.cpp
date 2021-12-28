#include "common.hpp"
#include "stdint.h"

constexpr uint64_t pageSize = 0x1000;

static void readBasicMemoryConstrains(uint64_t);

static void setPageTableForHighKernel(uint64_t);

static void mapFirstFewPages();

static void basicPrint(const char* str);
static void basicPrint(uint64_t hex, uint8_t minSize = 1);

static inline void callMain(uint64_t multibootInfo) {
    uint64_t kernelStack;
    saveReadSymbol("firstKernelStack", kernelStack);
    kernelStack += 0x4000 - 0x10;
    uint64_t mainAddress;
    saveReadSymbol("main", mainAddress);
    //call main using kernel stack with multiboot info as first argument
    basicPrint("         Call main");
    basicPrint(mainAddress);
    asm volatile("movq %0, %%rsp\n\t"
                 "movq %1, %%rax\n\t"
                 "movq %2, %%rdi\n\t"
                 "callq *%%rax\n\t"
                 :
                 : "r"(kernelStack), "r"(mainAddress), "r"(multibootInfo)
                 : "rax", "rdi");
}

extern "C" void __kernel_main(uint32_t multibootInfo) {
    basicPrint("Lower Kernel start");
    setPageTableForHighKernel((uint64_t) multibootInfo);
    basicPrint("Page table set    ");
    callMain((uint64_t) multibootInfo);
    basicPrint("Kernel end        ");
    asm volatile("cli");
    asm volatile("hlt");
}

static void basicPrint(const char* text) {
    volatile uint8_t* textBuffer = (volatile uint8_t*) 0xB8000;
    uint64_t index = 0;
    for (const char* str = text; *str; ++str) {
        textBuffer[index] = *str;
        textBuffer[index + 1] = 0x07;
        index += 2;
    }
}

static void basicPrint(uint64_t hex, uint8_t minSize) {
    minSize = 16 - minSize;
    volatile uint8_t* textBuffer = (volatile uint8_t*) 0xB8000;
    const char* hexEncoder = "0123456789ABCDEF";
    uint64_t index = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t digit = (uint8_t) ((hex >> 60) & 0xF);
        hex <<= 4;
        if (digit != 0) {
            minSize = i;
        }
        if (i >= minSize) {
            textBuffer[index++] = hexEncoder[digit];
            textBuffer[index++] = 0x07;
        }
    }
}

inline static uint32_t realign(uint32_t i) {
    --i;
    return (i & -8) + 8;
}

struct TagStructure {
    uint32_t total_size;
    uint32_t res;
} __attribute__((__packed__));
struct BasicTag {
    uint32_t type;
    uint32_t size;
} __attribute__((__packed__));

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
static_assert(sizeof(PagingAddress) == sizeof(void*));

struct PagingEntry {
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
            uint64_t pageAttributeTable : 1;
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

static_assert(sizeof(PagingEntry) == sizeof(void*));

struct PagingPage {
    PagingEntry entries[1 << 9];
};

static_assert(sizeof(PagingPage) == pageSize);

static void setAddress(PagingEntry& entry, uint64_t address) {
    entry.encAddress = 0;      // remove address data
    uint64_t flags = entry.raw;// flags contains only flag data
    entry.raw = address;       // set only address
    entry.raw |= flags;        // add flags data
    entry.present = 1;         // page is now present
}

static PagingPage* ensurePage(PagingEntry& entry, uint64_t pageBufferStart, uint64_t& index) {
    if (!entry.present) {
        // allocate page
        uint64_t pageAddress = pageBufferStart + index * pageSize;
        index++;
        uint8_t* ptr = (uint8_t*) pageAddress;
        for (uint64_t i = 0; i < pageSize; ++i) {
            ptr[i] = 0;
        }
        setAddress(entry, pageAddress);
        asm volatile("invlpg (%0)" ::"r"(pageAddress)
                     : "memory");
    }
    return (PagingPage*) (entry.encAddress << 12);
}

static void map(uint64_t phyAddress, uint64_t virAddress, uint64_t level4Table, uint64_t pageBufferStart, uint64_t& index) {
    PagingAddress vAddr;
    vAddr.pointer = (void*) virAddress;
    PagingPage* level4Page = (PagingPage*) level4Table;
    PagingPage* level3Page = ensurePage(level4Page->entries[vAddr.level4], pageBufferStart, index);
    PagingPage* level2Page = ensurePage(level3Page->entries[vAddr.level3], pageBufferStart, index);
    PagingPage* level1Page = ensurePage(level2Page->entries[vAddr.level2], pageBufferStart, index);
    setAddress(level1Page->entries[vAddr.level1], phyAddress);
    asm volatile("invlpg (%0)" ::"r"(virAddress)
                 : "memory");
}

alignas(4096) static uint8_t pageBuffer[4096 * 32];

static void setPageTableForHighKernel(uint64_t multibootInfo) {
    uint64_t maxKernelPageCount = ((sizeof(pageBuffer) / pageSize) - 3) * 512;
    uint64_t pageIndex = 0;

    uint64_t phyStart = 0;
    saveReadSymbol("physical_kernel_start", phyStart);
    uint64_t virStart = 0;
    saveReadSymbol("kernel_start", virStart);

    uint64_t phyEnd = 0;
    saveReadSymbol("physical_end", phyEnd);
    phyEnd = (phyEnd + pageSize - 1) & ~(pageSize - 1);
    uint64_t virEnd = 0;
    saveReadSymbol("kernel_end", virEnd);

    uint64_t targetPageCount = (phyEnd - phyStart) / pageSize;
    if (targetPageCount != (virEnd - virStart) / pageSize) {
        basicPrint("    Kernel virtual and physical size mismatch (error while linking?)");
        basicPrint((virEnd - virStart) / pageSize);
        asm("hlt");
        return;
    }

    if (targetPageCount > maxKernelPageCount) {
        basicPrint("    Kernel too big for page table (error while linking?)");
        basicPrint(targetPageCount);
        asm("hlt");
        return;
    }

    uint64_t level4Table = 0;
    asm("mov %%cr3, %0"
        : "=a"(level4Table)
        :);
    for (uint64_t i = 0; i < targetPageCount; ++i) {
        map(phyStart + pageSize * i, virStart + pageSize * i, level4Table, (uint64_t) pageBuffer, pageIndex);
    }
}