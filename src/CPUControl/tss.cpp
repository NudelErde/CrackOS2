#include "CPUControl/tss.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

struct TSS {
    uint32_t unused0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint16_t unused3;
    uint16_t iomapbase;
} __attribute__((packed));
static_assert(sizeof(TSS) == 104);

constexpr uint64_t stackPageCount = 1;

alignas(4096) TSS tss;
alignas(4096) uint8_t interruptStack[stackPageCount * pageSize];///< Stack for interrupts should read interrupt id and change to the correct stack (cpu and function specific)
alignas(4096) uint8_t panicStack[stackPageCount * pageSize];    ///< Stack for panics. There should be no way to recover from a panic (except rebooting)

void setupTss(uint64_t index) {
    struct GdtEntry {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_middle;
        uint8_t access;
        uint8_t flags;// flags contains 4 bits of limit
        uint8_t base_high;
        uint32_t base_higher;
        uint32_t reserved;
    } __attribute__((packed));
    static_assert(sizeof(GdtEntry) == 16);

    uint8_t gdtDescriptor[10];
    asm volatile(
            "sgdt %0"
            : "=m"(gdtDescriptor));
    uint64_t gdtAddress = *(uint64_t*) &gdtDescriptor[2];
    GdtEntry* tssEntry = (GdtEntry*) (gdtAddress + index * 8);

    uint64_t base = (uint64_t) &tss;
    uint64_t limit = sizeof(TSS);
    tssEntry->base_low = base & 0xFFFF;
    tssEntry->base_middle = (base >> 16) & 0xFF;
    tssEntry->base_high = (base >> 24) & 0xFF;
    tssEntry->base_higher = (base >> 32) & 0xFFFFFFFF;

    tssEntry->limit_low = (limit & 0xFFFF);
    tssEntry->flags = ((limit >> 16) & 0x0F);

    tssEntry->access = 0b10001001;
    tssEntry->flags |= 0b00000000;

    memset(&tss, 0, sizeof(TSS));

    tss.ist1 = ((uint64_t) interruptStack) + sizeof(interruptStack) - 16;
    tss.ist2 = ((uint64_t) panicStack) + sizeof(panicStack) - 16;

    uint16_t selector = (index << 3) | 0b000;// load tss
    asm(R"(
            mov %0, %%ax
            ltr %%ax
        )" ::"m"(selector)
        : "rax");
}