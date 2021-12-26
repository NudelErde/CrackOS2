#include "CPUControl/interrupts.hpp"
#include "ACPI/APIC.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"
#include "Debug/exceptionHandler.hpp"
#include "LanguageFeatures/memory.hpp"
#include "Memory/memory.hpp"

struct InterruptGate {
    uint16_t lowAddress;
    SegmentSelector segmentSelector;
    uint8_t interruptStack : 3;
    uint8_t zero0 : 5;
    uint8_t type : 4;
    uint8_t zero1 : 1;
    uint8_t descriptorPrivilegeLevel : 2;
    bool present : 1;
    uint16_t middleAddress;
    uint32_t highAddress;
    uint32_t reserved;
};
static_assert(sizeof(InterruptGate) == sizeof(uint8_t) * 16, "InterruptGate is not 16 bytes");

alignas(pageSize) InterruptGate interruptVectorTable[256] = {};
static_assert(sizeof(interruptVectorTable) == pageSize, "Interrupt vector table size is not page size");
static_assert(alignof(interruptVectorTable) == pageSize, "Interrupt vector table alignment is not page size");

constexpr uint32_t PIC1 = 0x20;
constexpr uint32_t PIC2 = 0xA0;
constexpr uint32_t PIC1_COMMAND = PIC1;
constexpr uint32_t PIC1_DATA = (PIC1 + 1);
constexpr uint32_t PIC2_COMMAND = PIC2;
constexpr uint32_t PIC2_DATA = (PIC2 + 1);

constexpr uint32_t ICW1_ICW4 = 0x01;
constexpr uint32_t ICW1_SINGLE = 0x02;
constexpr uint32_t ICW1_INTERVAL4 = 0x04;
constexpr uint32_t ICW1_LEVEL = 0x08;
constexpr uint32_t ICW1_INIT = 0x10;
constexpr uint32_t ICW4_8086 = 0x01;
constexpr uint32_t ICW4_AUTO = 0x02;
constexpr uint32_t ICW4_BUF_SLAV = 0x0;
constexpr uint32_t ICW4_BUF_MASTE = 0x0;
constexpr uint32_t ICW4_SFNM = 0x10;

static struct {
    uint16_t hardwareInterruptMask;
    uint8_t offset;
    bool isActive;
} PICData;
static bool APICEnabled;

static void remapPICInterrupts(uint8_t offset) {
    PICData.hardwareInterruptMask = 0xFF;
    PICData.isActive = true;

    // mask all interrupts
    out8(PIC1_DATA, 0xFF);
    out8(PIC2_DATA, 0xFF);

    // remap
    out8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);// starts the initialization sequence (in cascade mode)
    io_wait();
    out8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out8(PIC1_DATA, offset);// ICW2: Master PIC vector offset
    io_wait();
    out8(PIC2_DATA, offset + 8);// ICW2: Slave PIC vector offset
    io_wait();
    out8(PIC1_DATA, 4);// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    out8(PIC2_DATA, 2);// ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out8(PIC1_DATA, ICW4_8086);
    io_wait();
    out8(PIC2_DATA, ICW4_8086);
    io_wait();
}

void Interrupt::enableInterrupts() {
    asm volatile("sti");
}
void Interrupt::disableInterrupts() {
    asm volatile("cli");
}

bool Interrupt::isInterruptEnabled() {
    uint64_t flags;
    asm volatile("pushf; pop %0"
                 : "=r"(flags));
    return flags & (1 << 9);
}

static void fillInterruptVectorTable();

void systemError(Interrupt& i) {
    Output::getDefault()->print("System error\n");
    Output::getDefault()->printf("Interrupt: %hhu\n", i.getInterruptNumber());
    stop();
}

Interrupt::InterruptHandler callbacks[256]{};

void Interrupt::setupInterruptVectorTable() {
    APICEnabled = false;
    memset(interruptVectorTable, 0, sizeof(interruptVectorTable));
    memset(callbacks, 0, sizeof(callbacks));
    fillInterruptVectorTable();

    bool isEnabled = isInterruptEnabled();
    disableInterrupts();

    PICData.offset = 256 - 16;
    remapPICInterrupts(PICData.offset);

    uint8_t lidtData[10];
    //store size of interrupt vector table - 1
    lidtData[0] = 0xFF;
    lidtData[1] = 0x0F;
    //store address of interrupt vector table in lidt + 2

    lidtData[2] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 0);
    lidtData[3] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 8);
    lidtData[4] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 16);
    lidtData[5] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 24);
    lidtData[6] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 32);
    lidtData[7] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 40);
    lidtData[8] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 48);
    lidtData[9] = (uint8_t) (reinterpret_cast<uint64_t>(interruptVectorTable) >> 56);

    //load idt
    asm volatile("lidt %0" ::"m"(lidtData)
                 : "memory");
    if (isEnabled) {
        enableInterrupts();
    }

    //set interrupt handlers
    for (uint16_t i = 0; i < 256; i++) {
        setupInterruptHandler((uint8_t) i, systemError, {true, 2});
    }
    setupInterruptHandler(14, onPageFault, {true, 1});
}

void Interrupt::setupInterruptHandler(uint8_t interruptNumber, InterruptHandler handlerAddress, InterruptOptions options) {
    bool isEnabled = isInterruptEnabled();
    disableInterrupts();

    // set handler address
    callbacks[interruptNumber] = handlerAddress;

    if (handlerAddress == 0) {
        interruptVectorTable[interruptNumber].present = false;
        return;
    }

    // set constant values
    interruptVectorTable[interruptNumber].zero0 = 0;
    interruptVectorTable[interruptNumber].zero1 = 0;
    interruptVectorTable[interruptNumber].reserved = 0;
    interruptVectorTable[interruptNumber].present = true;
    interruptVectorTable[interruptNumber].type = 0xE;//Interrupt gate

    // set privilege level
    interruptVectorTable[interruptNumber].segmentSelector = toSegmentSelector(Segment::KERNEL_CODE);
    if (options.callableFromUserMode) {
        interruptVectorTable[interruptNumber].descriptorPrivilegeLevel = 3;
    } else {
        interruptVectorTable[interruptNumber].descriptorPrivilegeLevel = 0;
    }

    // set interrupt stack
    interruptVectorTable[interruptNumber].interruptStack = options.interruptStack;

    if (isEnabled) {
        enableInterrupts();
    }
}

uint8_t Interrupt::hardwareInterruptToVector(HardwareInterruptNumberSource source, uint8_t interruptNumber) {
    if (source == HardwareInterruptNumberSource::PIC) {
        if (APICEnabled) {
            uint32_t globalInterrupt = APIC::getSourceOverride(interruptNumber);
            if (globalInterrupt == (uint32_t) ~0ull) {
                // guess that the interruptNumber is the global interrupt
                globalInterrupt = interruptNumber;
            }
            return APIC::getInterrupt(globalInterrupt);
        } else if (PICData.isActive) {
            return interruptNumber + PICData.offset;
        }
    } else if (source == HardwareInterruptNumberSource::APIC) {
        if (APICEnabled) {
            return APIC::getInterrupt(interruptNumber);
        }
    }
    return 0;
}

bool Interrupt::isHardwareInterruptEnabled(HardwareInterruptNumberSource source, uint8_t interruptNumber) {
    switch (source) {
        case HardwareInterruptNumberSource::PIC:
            if (APICEnabled) {
                uint32_t globalInterrupt = APIC::getSourceOverride(interruptNumber);
                if (globalInterrupt == (uint32_t) ~0ull) {
                    // guess that the interruptNumber is the global interrupt
                    globalInterrupt = interruptNumber;
                }
                return APIC::isInterruptEnabled(globalInterrupt);
            }
            if (PICData.isActive) {
                return (PICData.hardwareInterruptMask & (1 << interruptNumber)) == 0;
            }
            return false;
        case HardwareInterruptNumberSource::APIC:
            if (APICEnabled) {
                return APIC::isInterruptEnabled(interruptNumber);
            }
            return false;
    }
    return false;
}
void Interrupt::setHardwareInterruptEnabled(HardwareInterruptNumberSource source, uint8_t interruptNumber, bool enabled) {
    bool intEnabled = isInterruptEnabled();
    disableInterrupts();
    switch (source) {
        case HardwareInterruptNumberSource::PIC:
            if (APICEnabled) {
                uint32_t globalInterrupt = APIC::getSourceOverride(interruptNumber);
                if (globalInterrupt == (uint32_t) ~0ull) {
                    // guess that the interruptNumber is the global interrupt
                    globalInterrupt = interruptNumber;
                }
                APIC::setInterruptEnabled(globalInterrupt, enabled);
                break;
            }

            if (PICData.isActive) {
                if (enabled) {
                    PICData.hardwareInterruptMask &= ~(1 << interruptNumber);
                } else {
                    PICData.hardwareInterruptMask |= (1 << interruptNumber);
                }

                out8(PIC1_DATA, (uint8_t) (PICData.hardwareInterruptMask & 0xFF));
                out8(PIC2_DATA, (uint8_t) (PICData.hardwareInterruptMask >> 8));
                break;
            }
            break;
        case HardwareInterruptNumberSource::APIC:
            if (APICEnabled) {
                APIC::setInterruptEnabled(interruptNumber, enabled);
                break;
            }
            break;
    }
    if (intEnabled) {
        enableInterrupts();
    }
}

void Interrupt::sendEOI(uint8_t interruptNumber) {
    if (PICData.isActive) {
        if (interruptNumber > PICData.offset) {
            if (interruptNumber >= PICData.offset + 8) {
                out8(PIC2_COMMAND, 0x20);
            }
            out8(PIC1_COMMAND, 0x20);
        }
    } else if (APICEnabled) {
        APIC::sendEOI();
    }
}

void Interrupt::switchToAPICMode() {
    APICEnabled = true;
    PICData.isActive = false;
    //disable PIC
    out8(PIC1_DATA, 0xFF);
    out8(PIC2_DATA, 0xFF);

    //set spurious interrupt vector
    APIC::setSpuriousInterruptVector(0xFF);
}

Interrupt::Interrupt(uint8_t interruptNumber, uint64_t errorCode, uint64_t stackFrame, bool hasError)
    : interruptNumber(interruptNumber), errorCode(errorCode), stackFrame(stackFrame), isError(hasError) {
}

//**********************************************************************************************************************
//------------------------------------------[ Interrupt Handler ]-------------------------------------------------------
//**********************************************************************************************************************

void generic_interrupt_handler(uint8_t id, void* stackFrame, uint64_t error = 0, bool hasError = false) {
    Interrupt interrupt(id, error, (uint64_t) stackFrame, hasError);

    if (callbacks[id] != 0) {
        callbacks[id](interrupt);
    } else if (id < 32) {
        systemError(interrupt);
    }

    Interrupt::sendEOI(id);
}

struct interrupt_frame;

__attribute__((interrupt)) void ___interrupt0x00(interrupt_frame* frame) { generic_interrupt_handler(0x00, frame); }
__attribute__((interrupt)) void ___interrupt0x01(interrupt_frame* frame) { generic_interrupt_handler(0x01, frame); }
__attribute__((interrupt)) void ___interrupt0x02(interrupt_frame* frame) { generic_interrupt_handler(0x02, frame); }
__attribute__((interrupt)) void ___interrupt0x03(interrupt_frame* frame) { generic_interrupt_handler(0x03, frame); }
__attribute__((interrupt)) void ___interrupt0x04(interrupt_frame* frame) { generic_interrupt_handler(0x04, frame); }
__attribute__((interrupt)) void ___interrupt0x05(interrupt_frame* frame) { generic_interrupt_handler(0x05, frame); }
__attribute__((interrupt)) void ___interrupt0x06(interrupt_frame* frame) { generic_interrupt_handler(0x06, frame); }
__attribute__((interrupt)) void ___interrupt0x07(interrupt_frame* frame) { generic_interrupt_handler(0x07, frame); }
__attribute__((interrupt)) void ___interrupt0x08(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x08, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x09(interrupt_frame* frame) { generic_interrupt_handler(0x09, frame); }
__attribute__((interrupt)) void ___interrupt0x0A(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x0A, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x0B(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x0B, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x0C(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x0C, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x0D(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x0D, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x0E(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x0E, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x0F(interrupt_frame* frame) { generic_interrupt_handler(0x0F, frame); }
__attribute__((interrupt)) void ___interrupt0x10(interrupt_frame* frame) { generic_interrupt_handler(0x10, frame); }
__attribute__((interrupt)) void ___interrupt0x11(interrupt_frame* frame, uint64_t error) { generic_interrupt_handler(0x11, frame, error, true); }
__attribute__((interrupt)) void ___interrupt0x12(interrupt_frame* frame) { generic_interrupt_handler(0x12, frame); }
__attribute__((interrupt)) void ___interrupt0x13(interrupt_frame* frame) { generic_interrupt_handler(0x13, frame); }
__attribute__((interrupt)) void ___interrupt0x14(interrupt_frame* frame) { generic_interrupt_handler(0x14, frame); }
__attribute__((interrupt)) void ___interrupt0x15(interrupt_frame* frame) { generic_interrupt_handler(0x15, frame); }
__attribute__((interrupt)) void ___interrupt0x16(interrupt_frame* frame) { generic_interrupt_handler(0x16, frame); }
__attribute__((interrupt)) void ___interrupt0x17(interrupt_frame* frame) { generic_interrupt_handler(0x17, frame); }
__attribute__((interrupt)) void ___interrupt0x18(interrupt_frame* frame) { generic_interrupt_handler(0x18, frame); }
__attribute__((interrupt)) void ___interrupt0x19(interrupt_frame* frame) { generic_interrupt_handler(0x19, frame); }
__attribute__((interrupt)) void ___interrupt0x1A(interrupt_frame* frame) { generic_interrupt_handler(0x1A, frame); }
__attribute__((interrupt)) void ___interrupt0x1B(interrupt_frame* frame) { generic_interrupt_handler(0x1B, frame); }
__attribute__((interrupt)) void ___interrupt0x1C(interrupt_frame* frame) { generic_interrupt_handler(0x1C, frame); }
__attribute__((interrupt)) void ___interrupt0x1D(interrupt_frame* frame) { generic_interrupt_handler(0x1D, frame); }
__attribute__((interrupt)) void ___interrupt0x1E(interrupt_frame* frame) { generic_interrupt_handler(0x1E, frame); }
__attribute__((interrupt)) void ___interrupt0x1F(interrupt_frame* frame) { generic_interrupt_handler(0x1F, frame); }
__attribute__((interrupt)) void ___interrupt0x20(interrupt_frame* frame) { generic_interrupt_handler(0x20, frame); }
__attribute__((interrupt)) void ___interrupt0x21(interrupt_frame* frame) { generic_interrupt_handler(0x21, frame); }
__attribute__((interrupt)) void ___interrupt0x22(interrupt_frame* frame) { generic_interrupt_handler(0x22, frame); }
__attribute__((interrupt)) void ___interrupt0x23(interrupt_frame* frame) { generic_interrupt_handler(0x23, frame); }
__attribute__((interrupt)) void ___interrupt0x24(interrupt_frame* frame) { generic_interrupt_handler(0x24, frame); }
__attribute__((interrupt)) void ___interrupt0x25(interrupt_frame* frame) { generic_interrupt_handler(0x25, frame); }
__attribute__((interrupt)) void ___interrupt0x26(interrupt_frame* frame) { generic_interrupt_handler(0x26, frame); }
__attribute__((interrupt)) void ___interrupt0x27(interrupt_frame* frame) { generic_interrupt_handler(0x27, frame); }
__attribute__((interrupt)) void ___interrupt0x28(interrupt_frame* frame) { generic_interrupt_handler(0x28, frame); }
__attribute__((interrupt)) void ___interrupt0x29(interrupt_frame* frame) { generic_interrupt_handler(0x29, frame); }
__attribute__((interrupt)) void ___interrupt0x2A(interrupt_frame* frame) { generic_interrupt_handler(0x2A, frame); }
__attribute__((interrupt)) void ___interrupt0x2B(interrupt_frame* frame) { generic_interrupt_handler(0x2B, frame); }
__attribute__((interrupt)) void ___interrupt0x2C(interrupt_frame* frame) { generic_interrupt_handler(0x2C, frame); }
__attribute__((interrupt)) void ___interrupt0x2D(interrupt_frame* frame) { generic_interrupt_handler(0x2D, frame); }
__attribute__((interrupt)) void ___interrupt0x2E(interrupt_frame* frame) { generic_interrupt_handler(0x2E, frame); }
__attribute__((interrupt)) void ___interrupt0x2F(interrupt_frame* frame) { generic_interrupt_handler(0x2F, frame); }
__attribute__((interrupt)) void ___interrupt0x30(interrupt_frame* frame) { generic_interrupt_handler(0x30, frame); }
__attribute__((interrupt)) void ___interrupt0x31(interrupt_frame* frame) { generic_interrupt_handler(0x31, frame); }
__attribute__((interrupt)) void ___interrupt0x32(interrupt_frame* frame) { generic_interrupt_handler(0x32, frame); }
__attribute__((interrupt)) void ___interrupt0x33(interrupt_frame* frame) { generic_interrupt_handler(0x33, frame); }
__attribute__((interrupt)) void ___interrupt0x34(interrupt_frame* frame) { generic_interrupt_handler(0x34, frame); }
__attribute__((interrupt)) void ___interrupt0x35(interrupt_frame* frame) { generic_interrupt_handler(0x35, frame); }
__attribute__((interrupt)) void ___interrupt0x36(interrupt_frame* frame) { generic_interrupt_handler(0x36, frame); }
__attribute__((interrupt)) void ___interrupt0x37(interrupt_frame* frame) { generic_interrupt_handler(0x37, frame); }
__attribute__((interrupt)) void ___interrupt0x38(interrupt_frame* frame) { generic_interrupt_handler(0x38, frame); }
__attribute__((interrupt)) void ___interrupt0x39(interrupt_frame* frame) { generic_interrupt_handler(0x39, frame); }
__attribute__((interrupt)) void ___interrupt0x3A(interrupt_frame* frame) { generic_interrupt_handler(0x3A, frame); }
__attribute__((interrupt)) void ___interrupt0x3B(interrupt_frame* frame) { generic_interrupt_handler(0x3B, frame); }
__attribute__((interrupt)) void ___interrupt0x3C(interrupt_frame* frame) { generic_interrupt_handler(0x3C, frame); }
__attribute__((interrupt)) void ___interrupt0x3D(interrupt_frame* frame) { generic_interrupt_handler(0x3D, frame); }
__attribute__((interrupt)) void ___interrupt0x3E(interrupt_frame* frame) { generic_interrupt_handler(0x3E, frame); }
__attribute__((interrupt)) void ___interrupt0x3F(interrupt_frame* frame) { generic_interrupt_handler(0x3F, frame); }
__attribute__((interrupt)) void ___interrupt0x40(interrupt_frame* frame) { generic_interrupt_handler(0x40, frame); }
__attribute__((interrupt)) void ___interrupt0x41(interrupt_frame* frame) { generic_interrupt_handler(0x41, frame); }
__attribute__((interrupt)) void ___interrupt0x42(interrupt_frame* frame) { generic_interrupt_handler(0x42, frame); }
__attribute__((interrupt)) void ___interrupt0x43(interrupt_frame* frame) { generic_interrupt_handler(0x43, frame); }
__attribute__((interrupt)) void ___interrupt0x44(interrupt_frame* frame) { generic_interrupt_handler(0x44, frame); }
__attribute__((interrupt)) void ___interrupt0x45(interrupt_frame* frame) { generic_interrupt_handler(0x45, frame); }
__attribute__((interrupt)) void ___interrupt0x46(interrupt_frame* frame) { generic_interrupt_handler(0x46, frame); }
__attribute__((interrupt)) void ___interrupt0x47(interrupt_frame* frame) { generic_interrupt_handler(0x47, frame); }
__attribute__((interrupt)) void ___interrupt0x48(interrupt_frame* frame) { generic_interrupt_handler(0x48, frame); }
__attribute__((interrupt)) void ___interrupt0x49(interrupt_frame* frame) { generic_interrupt_handler(0x49, frame); }
__attribute__((interrupt)) void ___interrupt0x4A(interrupt_frame* frame) { generic_interrupt_handler(0x4A, frame); }
__attribute__((interrupt)) void ___interrupt0x4B(interrupt_frame* frame) { generic_interrupt_handler(0x4B, frame); }
__attribute__((interrupt)) void ___interrupt0x4C(interrupt_frame* frame) { generic_interrupt_handler(0x4C, frame); }
__attribute__((interrupt)) void ___interrupt0x4D(interrupt_frame* frame) { generic_interrupt_handler(0x4D, frame); }
__attribute__((interrupt)) void ___interrupt0x4E(interrupt_frame* frame) { generic_interrupt_handler(0x4E, frame); }
__attribute__((interrupt)) void ___interrupt0x4F(interrupt_frame* frame) { generic_interrupt_handler(0x4F, frame); }
__attribute__((interrupt)) void ___interrupt0x50(interrupt_frame* frame) { generic_interrupt_handler(0x50, frame); }
__attribute__((interrupt)) void ___interrupt0x51(interrupt_frame* frame) { generic_interrupt_handler(0x51, frame); }
__attribute__((interrupt)) void ___interrupt0x52(interrupt_frame* frame) { generic_interrupt_handler(0x52, frame); }
__attribute__((interrupt)) void ___interrupt0x53(interrupt_frame* frame) { generic_interrupt_handler(0x53, frame); }
__attribute__((interrupt)) void ___interrupt0x54(interrupt_frame* frame) { generic_interrupt_handler(0x54, frame); }
__attribute__((interrupt)) void ___interrupt0x55(interrupt_frame* frame) { generic_interrupt_handler(0x55, frame); }
__attribute__((interrupt)) void ___interrupt0x56(interrupt_frame* frame) { generic_interrupt_handler(0x56, frame); }
__attribute__((interrupt)) void ___interrupt0x57(interrupt_frame* frame) { generic_interrupt_handler(0x57, frame); }
__attribute__((interrupt)) void ___interrupt0x58(interrupt_frame* frame) { generic_interrupt_handler(0x58, frame); }
__attribute__((interrupt)) void ___interrupt0x59(interrupt_frame* frame) { generic_interrupt_handler(0x59, frame); }
__attribute__((interrupt)) void ___interrupt0x5A(interrupt_frame* frame) { generic_interrupt_handler(0x5A, frame); }
__attribute__((interrupt)) void ___interrupt0x5B(interrupt_frame* frame) { generic_interrupt_handler(0x5B, frame); }
__attribute__((interrupt)) void ___interrupt0x5C(interrupt_frame* frame) { generic_interrupt_handler(0x5C, frame); }
__attribute__((interrupt)) void ___interrupt0x5D(interrupt_frame* frame) { generic_interrupt_handler(0x5D, frame); }
__attribute__((interrupt)) void ___interrupt0x5E(interrupt_frame* frame) { generic_interrupt_handler(0x5E, frame); }
__attribute__((interrupt)) void ___interrupt0x5F(interrupt_frame* frame) { generic_interrupt_handler(0x5F, frame); }
__attribute__((interrupt)) void ___interrupt0x60(interrupt_frame* frame) { generic_interrupt_handler(0x60, frame); }
__attribute__((interrupt)) void ___interrupt0x61(interrupt_frame* frame) { generic_interrupt_handler(0x61, frame); }
__attribute__((interrupt)) void ___interrupt0x62(interrupt_frame* frame) { generic_interrupt_handler(0x62, frame); }
__attribute__((interrupt)) void ___interrupt0x63(interrupt_frame* frame) { generic_interrupt_handler(0x63, frame); }
__attribute__((interrupt)) void ___interrupt0x64(interrupt_frame* frame) { generic_interrupt_handler(0x64, frame); }
__attribute__((interrupt)) void ___interrupt0x65(interrupt_frame* frame) { generic_interrupt_handler(0x65, frame); }
__attribute__((interrupt)) void ___interrupt0x66(interrupt_frame* frame) { generic_interrupt_handler(0x66, frame); }
__attribute__((interrupt)) void ___interrupt0x67(interrupt_frame* frame) { generic_interrupt_handler(0x67, frame); }
__attribute__((interrupt)) void ___interrupt0x68(interrupt_frame* frame) { generic_interrupt_handler(0x68, frame); }
__attribute__((interrupt)) void ___interrupt0x69(interrupt_frame* frame) { generic_interrupt_handler(0x69, frame); }
__attribute__((interrupt)) void ___interrupt0x6A(interrupt_frame* frame) { generic_interrupt_handler(0x6A, frame); }
__attribute__((interrupt)) void ___interrupt0x6B(interrupt_frame* frame) { generic_interrupt_handler(0x6B, frame); }
__attribute__((interrupt)) void ___interrupt0x6C(interrupt_frame* frame) { generic_interrupt_handler(0x6C, frame); }
__attribute__((interrupt)) void ___interrupt0x6D(interrupt_frame* frame) { generic_interrupt_handler(0x6D, frame); }
__attribute__((interrupt)) void ___interrupt0x6E(interrupt_frame* frame) { generic_interrupt_handler(0x6E, frame); }
__attribute__((interrupt)) void ___interrupt0x6F(interrupt_frame* frame) { generic_interrupt_handler(0x6F, frame); }
__attribute__((interrupt)) void ___interrupt0x70(interrupt_frame* frame) { generic_interrupt_handler(0x70, frame); }
__attribute__((interrupt)) void ___interrupt0x71(interrupt_frame* frame) { generic_interrupt_handler(0x71, frame); }
__attribute__((interrupt)) void ___interrupt0x72(interrupt_frame* frame) { generic_interrupt_handler(0x72, frame); }
__attribute__((interrupt)) void ___interrupt0x73(interrupt_frame* frame) { generic_interrupt_handler(0x73, frame); }
__attribute__((interrupt)) void ___interrupt0x74(interrupt_frame* frame) { generic_interrupt_handler(0x74, frame); }
__attribute__((interrupt)) void ___interrupt0x75(interrupt_frame* frame) { generic_interrupt_handler(0x75, frame); }
__attribute__((interrupt)) void ___interrupt0x76(interrupt_frame* frame) { generic_interrupt_handler(0x76, frame); }
__attribute__((interrupt)) void ___interrupt0x77(interrupt_frame* frame) { generic_interrupt_handler(0x77, frame); }
__attribute__((interrupt)) void ___interrupt0x78(interrupt_frame* frame) { generic_interrupt_handler(0x78, frame); }
__attribute__((interrupt)) void ___interrupt0x79(interrupt_frame* frame) { generic_interrupt_handler(0x79, frame); }
__attribute__((interrupt)) void ___interrupt0x7A(interrupt_frame* frame) { generic_interrupt_handler(0x7A, frame); }
__attribute__((interrupt)) void ___interrupt0x7B(interrupt_frame* frame) { generic_interrupt_handler(0x7B, frame); }
__attribute__((interrupt)) void ___interrupt0x7C(interrupt_frame* frame) { generic_interrupt_handler(0x7C, frame); }
__attribute__((interrupt)) void ___interrupt0x7D(interrupt_frame* frame) { generic_interrupt_handler(0x7D, frame); }
__attribute__((interrupt)) void ___interrupt0x7E(interrupt_frame* frame) { generic_interrupt_handler(0x7E, frame); }
__attribute__((interrupt)) void ___interrupt0x7F(interrupt_frame* frame) { generic_interrupt_handler(0x7F, frame); }
__attribute__((interrupt)) void ___interrupt0x80(interrupt_frame* frame) { generic_interrupt_handler(0x80, frame); }
__attribute__((interrupt)) void ___interrupt0x81(interrupt_frame* frame) { generic_interrupt_handler(0x81, frame); }
__attribute__((interrupt)) void ___interrupt0x82(interrupt_frame* frame) { generic_interrupt_handler(0x82, frame); }
__attribute__((interrupt)) void ___interrupt0x83(interrupt_frame* frame) { generic_interrupt_handler(0x83, frame); }
__attribute__((interrupt)) void ___interrupt0x84(interrupt_frame* frame) { generic_interrupt_handler(0x84, frame); }
__attribute__((interrupt)) void ___interrupt0x85(interrupt_frame* frame) { generic_interrupt_handler(0x85, frame); }
__attribute__((interrupt)) void ___interrupt0x86(interrupt_frame* frame) { generic_interrupt_handler(0x86, frame); }
__attribute__((interrupt)) void ___interrupt0x87(interrupt_frame* frame) { generic_interrupt_handler(0x87, frame); }
__attribute__((interrupt)) void ___interrupt0x88(interrupt_frame* frame) { generic_interrupt_handler(0x88, frame); }
__attribute__((interrupt)) void ___interrupt0x89(interrupt_frame* frame) { generic_interrupt_handler(0x89, frame); }
__attribute__((interrupt)) void ___interrupt0x8A(interrupt_frame* frame) { generic_interrupt_handler(0x8A, frame); }
__attribute__((interrupt)) void ___interrupt0x8B(interrupt_frame* frame) { generic_interrupt_handler(0x8B, frame); }
__attribute__((interrupt)) void ___interrupt0x8C(interrupt_frame* frame) { generic_interrupt_handler(0x8C, frame); }
__attribute__((interrupt)) void ___interrupt0x8D(interrupt_frame* frame) { generic_interrupt_handler(0x8D, frame); }
__attribute__((interrupt)) void ___interrupt0x8E(interrupt_frame* frame) { generic_interrupt_handler(0x8E, frame); }
__attribute__((interrupt)) void ___interrupt0x8F(interrupt_frame* frame) { generic_interrupt_handler(0x8F, frame); }
__attribute__((interrupt)) void ___interrupt0x90(interrupt_frame* frame) { generic_interrupt_handler(0x90, frame); }
__attribute__((interrupt)) void ___interrupt0x91(interrupt_frame* frame) { generic_interrupt_handler(0x91, frame); }
__attribute__((interrupt)) void ___interrupt0x92(interrupt_frame* frame) { generic_interrupt_handler(0x92, frame); }
__attribute__((interrupt)) void ___interrupt0x93(interrupt_frame* frame) { generic_interrupt_handler(0x93, frame); }
__attribute__((interrupt)) void ___interrupt0x94(interrupt_frame* frame) { generic_interrupt_handler(0x94, frame); }
__attribute__((interrupt)) void ___interrupt0x95(interrupt_frame* frame) { generic_interrupt_handler(0x95, frame); }
__attribute__((interrupt)) void ___interrupt0x96(interrupt_frame* frame) { generic_interrupt_handler(0x96, frame); }
__attribute__((interrupt)) void ___interrupt0x97(interrupt_frame* frame) { generic_interrupt_handler(0x97, frame); }
__attribute__((interrupt)) void ___interrupt0x98(interrupt_frame* frame) { generic_interrupt_handler(0x98, frame); }
__attribute__((interrupt)) void ___interrupt0x99(interrupt_frame* frame) { generic_interrupt_handler(0x99, frame); }
__attribute__((interrupt)) void ___interrupt0x9A(interrupt_frame* frame) { generic_interrupt_handler(0x9A, frame); }
__attribute__((interrupt)) void ___interrupt0x9B(interrupt_frame* frame) { generic_interrupt_handler(0x9B, frame); }
__attribute__((interrupt)) void ___interrupt0x9C(interrupt_frame* frame) { generic_interrupt_handler(0x9C, frame); }
__attribute__((interrupt)) void ___interrupt0x9D(interrupt_frame* frame) { generic_interrupt_handler(0x9D, frame); }
__attribute__((interrupt)) void ___interrupt0x9E(interrupt_frame* frame) { generic_interrupt_handler(0x9E, frame); }
__attribute__((interrupt)) void ___interrupt0x9F(interrupt_frame* frame) { generic_interrupt_handler(0x9F, frame); }
__attribute__((interrupt)) void ___interrupt0xA0(interrupt_frame* frame) { generic_interrupt_handler(0xA0, frame); }
__attribute__((interrupt)) void ___interrupt0xA1(interrupt_frame* frame) { generic_interrupt_handler(0xA1, frame); }
__attribute__((interrupt)) void ___interrupt0xA2(interrupt_frame* frame) { generic_interrupt_handler(0xA2, frame); }
__attribute__((interrupt)) void ___interrupt0xA3(interrupt_frame* frame) { generic_interrupt_handler(0xA3, frame); }
__attribute__((interrupt)) void ___interrupt0xA4(interrupt_frame* frame) { generic_interrupt_handler(0xA4, frame); }
__attribute__((interrupt)) void ___interrupt0xA5(interrupt_frame* frame) { generic_interrupt_handler(0xA5, frame); }
__attribute__((interrupt)) void ___interrupt0xA6(interrupt_frame* frame) { generic_interrupt_handler(0xA6, frame); }
__attribute__((interrupt)) void ___interrupt0xA7(interrupt_frame* frame) { generic_interrupt_handler(0xA7, frame); }
__attribute__((interrupt)) void ___interrupt0xA8(interrupt_frame* frame) { generic_interrupt_handler(0xA8, frame); }
__attribute__((interrupt)) void ___interrupt0xA9(interrupt_frame* frame) { generic_interrupt_handler(0xA9, frame); }
__attribute__((interrupt)) void ___interrupt0xAA(interrupt_frame* frame) { generic_interrupt_handler(0xAA, frame); }
__attribute__((interrupt)) void ___interrupt0xAB(interrupt_frame* frame) { generic_interrupt_handler(0xAB, frame); }
__attribute__((interrupt)) void ___interrupt0xAC(interrupt_frame* frame) { generic_interrupt_handler(0xAC, frame); }
__attribute__((interrupt)) void ___interrupt0xAD(interrupt_frame* frame) { generic_interrupt_handler(0xAD, frame); }
__attribute__((interrupt)) void ___interrupt0xAE(interrupt_frame* frame) { generic_interrupt_handler(0xAE, frame); }
__attribute__((interrupt)) void ___interrupt0xAF(interrupt_frame* frame) { generic_interrupt_handler(0xAF, frame); }
__attribute__((interrupt)) void ___interrupt0xB0(interrupt_frame* frame) { generic_interrupt_handler(0xB0, frame); }
__attribute__((interrupt)) void ___interrupt0xB1(interrupt_frame* frame) { generic_interrupt_handler(0xB1, frame); }
__attribute__((interrupt)) void ___interrupt0xB2(interrupt_frame* frame) { generic_interrupt_handler(0xB2, frame); }
__attribute__((interrupt)) void ___interrupt0xB3(interrupt_frame* frame) { generic_interrupt_handler(0xB3, frame); }
__attribute__((interrupt)) void ___interrupt0xB4(interrupt_frame* frame) { generic_interrupt_handler(0xB4, frame); }
__attribute__((interrupt)) void ___interrupt0xB5(interrupt_frame* frame) { generic_interrupt_handler(0xB5, frame); }
__attribute__((interrupt)) void ___interrupt0xB6(interrupt_frame* frame) { generic_interrupt_handler(0xB6, frame); }
__attribute__((interrupt)) void ___interrupt0xB7(interrupt_frame* frame) { generic_interrupt_handler(0xB7, frame); }
__attribute__((interrupt)) void ___interrupt0xB8(interrupt_frame* frame) { generic_interrupt_handler(0xB8, frame); }
__attribute__((interrupt)) void ___interrupt0xB9(interrupt_frame* frame) { generic_interrupt_handler(0xB9, frame); }
__attribute__((interrupt)) void ___interrupt0xBA(interrupt_frame* frame) { generic_interrupt_handler(0xBA, frame); }
__attribute__((interrupt)) void ___interrupt0xBB(interrupt_frame* frame) { generic_interrupt_handler(0xBB, frame); }
__attribute__((interrupt)) void ___interrupt0xBC(interrupt_frame* frame) { generic_interrupt_handler(0xBC, frame); }
__attribute__((interrupt)) void ___interrupt0xBD(interrupt_frame* frame) { generic_interrupt_handler(0xBD, frame); }
__attribute__((interrupt)) void ___interrupt0xBE(interrupt_frame* frame) { generic_interrupt_handler(0xBE, frame); }
__attribute__((interrupt)) void ___interrupt0xBF(interrupt_frame* frame) { generic_interrupt_handler(0xBF, frame); }
__attribute__((interrupt)) void ___interrupt0xC0(interrupt_frame* frame) { generic_interrupt_handler(0xC0, frame); }
__attribute__((interrupt)) void ___interrupt0xC1(interrupt_frame* frame) { generic_interrupt_handler(0xC1, frame); }
__attribute__((interrupt)) void ___interrupt0xC2(interrupt_frame* frame) { generic_interrupt_handler(0xC2, frame); }
__attribute__((interrupt)) void ___interrupt0xC3(interrupt_frame* frame) { generic_interrupt_handler(0xC3, frame); }
__attribute__((interrupt)) void ___interrupt0xC4(interrupt_frame* frame) { generic_interrupt_handler(0xC4, frame); }
__attribute__((interrupt)) void ___interrupt0xC5(interrupt_frame* frame) { generic_interrupt_handler(0xC5, frame); }
__attribute__((interrupt)) void ___interrupt0xC6(interrupt_frame* frame) { generic_interrupt_handler(0xC6, frame); }
__attribute__((interrupt)) void ___interrupt0xC7(interrupt_frame* frame) { generic_interrupt_handler(0xC7, frame); }
__attribute__((interrupt)) void ___interrupt0xC8(interrupt_frame* frame) { generic_interrupt_handler(0xC8, frame); }
__attribute__((interrupt)) void ___interrupt0xC9(interrupt_frame* frame) { generic_interrupt_handler(0xC9, frame); }
__attribute__((interrupt)) void ___interrupt0xCA(interrupt_frame* frame) { generic_interrupt_handler(0xCA, frame); }
__attribute__((interrupt)) void ___interrupt0xCB(interrupt_frame* frame) { generic_interrupt_handler(0xCB, frame); }
__attribute__((interrupt)) void ___interrupt0xCC(interrupt_frame* frame) { generic_interrupt_handler(0xCC, frame); }
__attribute__((interrupt)) void ___interrupt0xCD(interrupt_frame* frame) { generic_interrupt_handler(0xCD, frame); }
__attribute__((interrupt)) void ___interrupt0xCE(interrupt_frame* frame) { generic_interrupt_handler(0xCE, frame); }
__attribute__((interrupt)) void ___interrupt0xCF(interrupt_frame* frame) { generic_interrupt_handler(0xCF, frame); }
__attribute__((interrupt)) void ___interrupt0xD0(interrupt_frame* frame) { generic_interrupt_handler(0xD0, frame); }
__attribute__((interrupt)) void ___interrupt0xD1(interrupt_frame* frame) { generic_interrupt_handler(0xD1, frame); }
__attribute__((interrupt)) void ___interrupt0xD2(interrupt_frame* frame) { generic_interrupt_handler(0xD2, frame); }
__attribute__((interrupt)) void ___interrupt0xD3(interrupt_frame* frame) { generic_interrupt_handler(0xD3, frame); }
__attribute__((interrupt)) void ___interrupt0xD4(interrupt_frame* frame) { generic_interrupt_handler(0xD4, frame); }
__attribute__((interrupt)) void ___interrupt0xD5(interrupt_frame* frame) { generic_interrupt_handler(0xD5, frame); }
__attribute__((interrupt)) void ___interrupt0xD6(interrupt_frame* frame) { generic_interrupt_handler(0xD6, frame); }
__attribute__((interrupt)) void ___interrupt0xD7(interrupt_frame* frame) { generic_interrupt_handler(0xD7, frame); }
__attribute__((interrupt)) void ___interrupt0xD8(interrupt_frame* frame) { generic_interrupt_handler(0xD8, frame); }
__attribute__((interrupt)) void ___interrupt0xD9(interrupt_frame* frame) { generic_interrupt_handler(0xD9, frame); }
__attribute__((interrupt)) void ___interrupt0xDA(interrupt_frame* frame) { generic_interrupt_handler(0xDA, frame); }
__attribute__((interrupt)) void ___interrupt0xDB(interrupt_frame* frame) { generic_interrupt_handler(0xDB, frame); }
__attribute__((interrupt)) void ___interrupt0xDC(interrupt_frame* frame) { generic_interrupt_handler(0xDC, frame); }
__attribute__((interrupt)) void ___interrupt0xDD(interrupt_frame* frame) { generic_interrupt_handler(0xDD, frame); }
__attribute__((interrupt)) void ___interrupt0xDE(interrupt_frame* frame) { generic_interrupt_handler(0xDE, frame); }
__attribute__((interrupt)) void ___interrupt0xDF(interrupt_frame* frame) { generic_interrupt_handler(0xDF, frame); }
__attribute__((interrupt)) void ___interrupt0xE0(interrupt_frame* frame) { generic_interrupt_handler(0xE0, frame); }
__attribute__((interrupt)) void ___interrupt0xE1(interrupt_frame* frame) { generic_interrupt_handler(0xE1, frame); }
__attribute__((interrupt)) void ___interrupt0xE2(interrupt_frame* frame) { generic_interrupt_handler(0xE2, frame); }
__attribute__((interrupt)) void ___interrupt0xE3(interrupt_frame* frame) { generic_interrupt_handler(0xE3, frame); }
__attribute__((interrupt)) void ___interrupt0xE4(interrupt_frame* frame) { generic_interrupt_handler(0xE4, frame); }
__attribute__((interrupt)) void ___interrupt0xE5(interrupt_frame* frame) { generic_interrupt_handler(0xE5, frame); }
__attribute__((interrupt)) void ___interrupt0xE6(interrupt_frame* frame) { generic_interrupt_handler(0xE6, frame); }
__attribute__((interrupt)) void ___interrupt0xE7(interrupt_frame* frame) { generic_interrupt_handler(0xE7, frame); }
__attribute__((interrupt)) void ___interrupt0xE8(interrupt_frame* frame) { generic_interrupt_handler(0xE8, frame); }
__attribute__((interrupt)) void ___interrupt0xE9(interrupt_frame* frame) { generic_interrupt_handler(0xE9, frame); }
__attribute__((interrupt)) void ___interrupt0xEA(interrupt_frame* frame) { generic_interrupt_handler(0xEA, frame); }
__attribute__((interrupt)) void ___interrupt0xEB(interrupt_frame* frame) { generic_interrupt_handler(0xEB, frame); }
__attribute__((interrupt)) void ___interrupt0xEC(interrupt_frame* frame) { generic_interrupt_handler(0xEC, frame); }
__attribute__((interrupt)) void ___interrupt0xED(interrupt_frame* frame) { generic_interrupt_handler(0xED, frame); }
__attribute__((interrupt)) void ___interrupt0xEE(interrupt_frame* frame) { generic_interrupt_handler(0xEE, frame); }
__attribute__((interrupt)) void ___interrupt0xEF(interrupt_frame* frame) { generic_interrupt_handler(0xEF, frame); }
__attribute__((interrupt)) void ___interrupt0xF0(interrupt_frame* frame) { generic_interrupt_handler(0xF0, frame); }
__attribute__((interrupt)) void ___interrupt0xF1(interrupt_frame* frame) { generic_interrupt_handler(0xF1, frame); }
__attribute__((interrupt)) void ___interrupt0xF2(interrupt_frame* frame) { generic_interrupt_handler(0xF2, frame); }
__attribute__((interrupt)) void ___interrupt0xF3(interrupt_frame* frame) { generic_interrupt_handler(0xF3, frame); }
__attribute__((interrupt)) void ___interrupt0xF4(interrupt_frame* frame) { generic_interrupt_handler(0xF4, frame); }
__attribute__((interrupt)) void ___interrupt0xF5(interrupt_frame* frame) { generic_interrupt_handler(0xF5, frame); }
__attribute__((interrupt)) void ___interrupt0xF6(interrupt_frame* frame) { generic_interrupt_handler(0xF6, frame); }
__attribute__((interrupt)) void ___interrupt0xF7(interrupt_frame* frame) { generic_interrupt_handler(0xF7, frame); }
__attribute__((interrupt)) void ___interrupt0xF8(interrupt_frame* frame) { generic_interrupt_handler(0xF8, frame); }
__attribute__((interrupt)) void ___interrupt0xF9(interrupt_frame* frame) { generic_interrupt_handler(0xF9, frame); }
__attribute__((interrupt)) void ___interrupt0xFA(interrupt_frame* frame) { generic_interrupt_handler(0xFA, frame); }
__attribute__((interrupt)) void ___interrupt0xFB(interrupt_frame* frame) { generic_interrupt_handler(0xFB, frame); }
__attribute__((interrupt)) void ___interrupt0xFC(interrupt_frame* frame) { generic_interrupt_handler(0xFC, frame); }
__attribute__((interrupt)) void ___interrupt0xFD(interrupt_frame* frame) { generic_interrupt_handler(0xFD, frame); }
__attribute__((interrupt)) void ___interrupt0xFE(interrupt_frame* frame) { generic_interrupt_handler(0xFE, frame); }
__attribute__((interrupt)) void ___interrupt0xFF(interrupt_frame* frame) { generic_interrupt_handler(0xFF, frame); }

static void set(InterruptGate& gate, uint64_t funct) {
    gate.lowAddress = (uint16_t) (((uint64_t) funct) & 0xFFFF);
    gate.middleAddress = (uint16_t) ((((uint64_t) funct) >> 16) & 0xFFFF);
    gate.highAddress = (uint32_t) ((((uint64_t) funct) >> 32) & 0xFFFFFFFF);
}

void fillInterruptVectorTable() {
    set(interruptVectorTable[0x00], (uint64_t) (&___interrupt0x00));
    set(interruptVectorTable[0x01], (uint64_t) (&___interrupt0x01));
    set(interruptVectorTable[0x02], (uint64_t) (&___interrupt0x02));
    set(interruptVectorTable[0x03], (uint64_t) (&___interrupt0x03));
    set(interruptVectorTable[0x04], (uint64_t) (&___interrupt0x04));
    set(interruptVectorTable[0x05], (uint64_t) (&___interrupt0x05));
    set(interruptVectorTable[0x06], (uint64_t) (&___interrupt0x06));
    set(interruptVectorTable[0x07], (uint64_t) (&___interrupt0x07));
    set(interruptVectorTable[0x08], (uint64_t) (&___interrupt0x08));
    set(interruptVectorTable[0x09], (uint64_t) (&___interrupt0x09));
    set(interruptVectorTable[0x0A], (uint64_t) (&___interrupt0x0A));
    set(interruptVectorTable[0x0B], (uint64_t) (&___interrupt0x0B));
    set(interruptVectorTable[0x0C], (uint64_t) (&___interrupt0x0C));
    set(interruptVectorTable[0x0D], (uint64_t) (&___interrupt0x0D));
    set(interruptVectorTable[0x0E], (uint64_t) (&___interrupt0x0E));
    set(interruptVectorTable[0x0F], (uint64_t) (&___interrupt0x0F));
    set(interruptVectorTable[0x10], (uint64_t) (&___interrupt0x10));
    set(interruptVectorTable[0x11], (uint64_t) (&___interrupt0x11));
    set(interruptVectorTable[0x12], (uint64_t) (&___interrupt0x12));
    set(interruptVectorTable[0x13], (uint64_t) (&___interrupt0x13));
    set(interruptVectorTable[0x14], (uint64_t) (&___interrupt0x14));
    set(interruptVectorTable[0x15], (uint64_t) (&___interrupt0x15));
    set(interruptVectorTable[0x16], (uint64_t) (&___interrupt0x16));
    set(interruptVectorTable[0x17], (uint64_t) (&___interrupt0x17));
    set(interruptVectorTable[0x18], (uint64_t) (&___interrupt0x18));
    set(interruptVectorTable[0x19], (uint64_t) (&___interrupt0x19));
    set(interruptVectorTable[0x1A], (uint64_t) (&___interrupt0x1A));
    set(interruptVectorTable[0x1B], (uint64_t) (&___interrupt0x1B));
    set(interruptVectorTable[0x1C], (uint64_t) (&___interrupt0x1C));
    set(interruptVectorTable[0x1D], (uint64_t) (&___interrupt0x1D));
    set(interruptVectorTable[0x1E], (uint64_t) (&___interrupt0x1E));
    set(interruptVectorTable[0x1F], (uint64_t) (&___interrupt0x1F));
    set(interruptVectorTable[0x20], (uint64_t) (&___interrupt0x20));
    set(interruptVectorTable[0x21], (uint64_t) (&___interrupt0x21));
    set(interruptVectorTable[0x22], (uint64_t) (&___interrupt0x22));
    set(interruptVectorTable[0x23], (uint64_t) (&___interrupt0x23));
    set(interruptVectorTable[0x24], (uint64_t) (&___interrupt0x24));
    set(interruptVectorTable[0x25], (uint64_t) (&___interrupt0x25));
    set(interruptVectorTable[0x26], (uint64_t) (&___interrupt0x26));
    set(interruptVectorTable[0x27], (uint64_t) (&___interrupt0x27));
    set(interruptVectorTable[0x28], (uint64_t) (&___interrupt0x28));
    set(interruptVectorTable[0x29], (uint64_t) (&___interrupt0x29));
    set(interruptVectorTable[0x2A], (uint64_t) (&___interrupt0x2A));
    set(interruptVectorTable[0x2B], (uint64_t) (&___interrupt0x2B));
    set(interruptVectorTable[0x2C], (uint64_t) (&___interrupt0x2C));
    set(interruptVectorTable[0x2D], (uint64_t) (&___interrupt0x2D));
    set(interruptVectorTable[0x2E], (uint64_t) (&___interrupt0x2E));
    set(interruptVectorTable[0x2F], (uint64_t) (&___interrupt0x2F));
    set(interruptVectorTable[0x30], (uint64_t) (&___interrupt0x30));
    set(interruptVectorTable[0x31], (uint64_t) (&___interrupt0x31));
    set(interruptVectorTable[0x32], (uint64_t) (&___interrupt0x32));
    set(interruptVectorTable[0x33], (uint64_t) (&___interrupt0x33));
    set(interruptVectorTable[0x34], (uint64_t) (&___interrupt0x34));
    set(interruptVectorTable[0x35], (uint64_t) (&___interrupt0x35));
    set(interruptVectorTable[0x36], (uint64_t) (&___interrupt0x36));
    set(interruptVectorTable[0x37], (uint64_t) (&___interrupt0x37));
    set(interruptVectorTable[0x38], (uint64_t) (&___interrupt0x38));
    set(interruptVectorTable[0x39], (uint64_t) (&___interrupt0x39));
    set(interruptVectorTable[0x3A], (uint64_t) (&___interrupt0x3A));
    set(interruptVectorTable[0x3B], (uint64_t) (&___interrupt0x3B));
    set(interruptVectorTable[0x3C], (uint64_t) (&___interrupt0x3C));
    set(interruptVectorTable[0x3D], (uint64_t) (&___interrupt0x3D));
    set(interruptVectorTable[0x3E], (uint64_t) (&___interrupt0x3E));
    set(interruptVectorTable[0x3F], (uint64_t) (&___interrupt0x3F));
    set(interruptVectorTable[0x40], (uint64_t) (&___interrupt0x40));
    set(interruptVectorTable[0x41], (uint64_t) (&___interrupt0x41));
    set(interruptVectorTable[0x42], (uint64_t) (&___interrupt0x42));
    set(interruptVectorTable[0x43], (uint64_t) (&___interrupt0x43));
    set(interruptVectorTable[0x44], (uint64_t) (&___interrupt0x44));
    set(interruptVectorTable[0x45], (uint64_t) (&___interrupt0x45));
    set(interruptVectorTable[0x46], (uint64_t) (&___interrupt0x46));
    set(interruptVectorTable[0x47], (uint64_t) (&___interrupt0x47));
    set(interruptVectorTable[0x48], (uint64_t) (&___interrupt0x48));
    set(interruptVectorTable[0x49], (uint64_t) (&___interrupt0x49));
    set(interruptVectorTable[0x4A], (uint64_t) (&___interrupt0x4A));
    set(interruptVectorTable[0x4B], (uint64_t) (&___interrupt0x4B));
    set(interruptVectorTable[0x4C], (uint64_t) (&___interrupt0x4C));
    set(interruptVectorTable[0x4D], (uint64_t) (&___interrupt0x4D));
    set(interruptVectorTable[0x4E], (uint64_t) (&___interrupt0x4E));
    set(interruptVectorTable[0x4F], (uint64_t) (&___interrupt0x4F));
    set(interruptVectorTable[0x50], (uint64_t) (&___interrupt0x50));
    set(interruptVectorTable[0x51], (uint64_t) (&___interrupt0x51));
    set(interruptVectorTable[0x52], (uint64_t) (&___interrupt0x52));
    set(interruptVectorTable[0x53], (uint64_t) (&___interrupt0x53));
    set(interruptVectorTable[0x54], (uint64_t) (&___interrupt0x54));
    set(interruptVectorTable[0x55], (uint64_t) (&___interrupt0x55));
    set(interruptVectorTable[0x56], (uint64_t) (&___interrupt0x56));
    set(interruptVectorTable[0x57], (uint64_t) (&___interrupt0x57));
    set(interruptVectorTable[0x58], (uint64_t) (&___interrupt0x58));
    set(interruptVectorTable[0x59], (uint64_t) (&___interrupt0x59));
    set(interruptVectorTable[0x5A], (uint64_t) (&___interrupt0x5A));
    set(interruptVectorTable[0x5B], (uint64_t) (&___interrupt0x5B));
    set(interruptVectorTable[0x5C], (uint64_t) (&___interrupt0x5C));
    set(interruptVectorTable[0x5D], (uint64_t) (&___interrupt0x5D));
    set(interruptVectorTable[0x5E], (uint64_t) (&___interrupt0x5E));
    set(interruptVectorTable[0x5F], (uint64_t) (&___interrupt0x5F));
    set(interruptVectorTable[0x60], (uint64_t) (&___interrupt0x60));
    set(interruptVectorTable[0x61], (uint64_t) (&___interrupt0x61));
    set(interruptVectorTable[0x62], (uint64_t) (&___interrupt0x62));
    set(interruptVectorTable[0x63], (uint64_t) (&___interrupt0x63));
    set(interruptVectorTable[0x64], (uint64_t) (&___interrupt0x64));
    set(interruptVectorTable[0x65], (uint64_t) (&___interrupt0x65));
    set(interruptVectorTable[0x66], (uint64_t) (&___interrupt0x66));
    set(interruptVectorTable[0x67], (uint64_t) (&___interrupt0x67));
    set(interruptVectorTable[0x68], (uint64_t) (&___interrupt0x68));
    set(interruptVectorTable[0x69], (uint64_t) (&___interrupt0x69));
    set(interruptVectorTable[0x6A], (uint64_t) (&___interrupt0x6A));
    set(interruptVectorTable[0x6B], (uint64_t) (&___interrupt0x6B));
    set(interruptVectorTable[0x6C], (uint64_t) (&___interrupt0x6C));
    set(interruptVectorTable[0x6D], (uint64_t) (&___interrupt0x6D));
    set(interruptVectorTable[0x6E], (uint64_t) (&___interrupt0x6E));
    set(interruptVectorTable[0x6F], (uint64_t) (&___interrupt0x6F));
    set(interruptVectorTable[0x70], (uint64_t) (&___interrupt0x70));
    set(interruptVectorTable[0x71], (uint64_t) (&___interrupt0x71));
    set(interruptVectorTable[0x72], (uint64_t) (&___interrupt0x72));
    set(interruptVectorTable[0x73], (uint64_t) (&___interrupt0x73));
    set(interruptVectorTable[0x74], (uint64_t) (&___interrupt0x74));
    set(interruptVectorTable[0x75], (uint64_t) (&___interrupt0x75));
    set(interruptVectorTable[0x76], (uint64_t) (&___interrupt0x76));
    set(interruptVectorTable[0x77], (uint64_t) (&___interrupt0x77));
    set(interruptVectorTable[0x78], (uint64_t) (&___interrupt0x78));
    set(interruptVectorTable[0x79], (uint64_t) (&___interrupt0x79));
    set(interruptVectorTable[0x7A], (uint64_t) (&___interrupt0x7A));
    set(interruptVectorTable[0x7B], (uint64_t) (&___interrupt0x7B));
    set(interruptVectorTable[0x7C], (uint64_t) (&___interrupt0x7C));
    set(interruptVectorTable[0x7D], (uint64_t) (&___interrupt0x7D));
    set(interruptVectorTable[0x7E], (uint64_t) (&___interrupt0x7E));
    set(interruptVectorTable[0x7F], (uint64_t) (&___interrupt0x7F));
    set(interruptVectorTable[0x80], (uint64_t) (&___interrupt0x80));
    set(interruptVectorTable[0x81], (uint64_t) (&___interrupt0x81));
    set(interruptVectorTable[0x82], (uint64_t) (&___interrupt0x82));
    set(interruptVectorTable[0x83], (uint64_t) (&___interrupt0x83));
    set(interruptVectorTable[0x84], (uint64_t) (&___interrupt0x84));
    set(interruptVectorTable[0x85], (uint64_t) (&___interrupt0x85));
    set(interruptVectorTable[0x86], (uint64_t) (&___interrupt0x86));
    set(interruptVectorTable[0x87], (uint64_t) (&___interrupt0x87));
    set(interruptVectorTable[0x88], (uint64_t) (&___interrupt0x88));
    set(interruptVectorTable[0x89], (uint64_t) (&___interrupt0x89));
    set(interruptVectorTable[0x8A], (uint64_t) (&___interrupt0x8A));
    set(interruptVectorTable[0x8B], (uint64_t) (&___interrupt0x8B));
    set(interruptVectorTable[0x8C], (uint64_t) (&___interrupt0x8C));
    set(interruptVectorTable[0x8D], (uint64_t) (&___interrupt0x8D));
    set(interruptVectorTable[0x8E], (uint64_t) (&___interrupt0x8E));
    set(interruptVectorTable[0x8F], (uint64_t) (&___interrupt0x8F));
    set(interruptVectorTable[0x90], (uint64_t) (&___interrupt0x90));
    set(interruptVectorTable[0x91], (uint64_t) (&___interrupt0x91));
    set(interruptVectorTable[0x92], (uint64_t) (&___interrupt0x92));
    set(interruptVectorTable[0x93], (uint64_t) (&___interrupt0x93));
    set(interruptVectorTable[0x94], (uint64_t) (&___interrupt0x94));
    set(interruptVectorTable[0x95], (uint64_t) (&___interrupt0x95));
    set(interruptVectorTable[0x96], (uint64_t) (&___interrupt0x96));
    set(interruptVectorTable[0x97], (uint64_t) (&___interrupt0x97));
    set(interruptVectorTable[0x98], (uint64_t) (&___interrupt0x98));
    set(interruptVectorTable[0x99], (uint64_t) (&___interrupt0x99));
    set(interruptVectorTable[0x9A], (uint64_t) (&___interrupt0x9A));
    set(interruptVectorTable[0x9B], (uint64_t) (&___interrupt0x9B));
    set(interruptVectorTable[0x9C], (uint64_t) (&___interrupt0x9C));
    set(interruptVectorTable[0x9D], (uint64_t) (&___interrupt0x9D));
    set(interruptVectorTable[0x9E], (uint64_t) (&___interrupt0x9E));
    set(interruptVectorTable[0x9F], (uint64_t) (&___interrupt0x9F));
    set(interruptVectorTable[0xA0], (uint64_t) (&___interrupt0xA0));
    set(interruptVectorTable[0xA1], (uint64_t) (&___interrupt0xA1));
    set(interruptVectorTable[0xA2], (uint64_t) (&___interrupt0xA2));
    set(interruptVectorTable[0xA3], (uint64_t) (&___interrupt0xA3));
    set(interruptVectorTable[0xA4], (uint64_t) (&___interrupt0xA4));
    set(interruptVectorTable[0xA5], (uint64_t) (&___interrupt0xA5));
    set(interruptVectorTable[0xA6], (uint64_t) (&___interrupt0xA6));
    set(interruptVectorTable[0xA7], (uint64_t) (&___interrupt0xA7));
    set(interruptVectorTable[0xA8], (uint64_t) (&___interrupt0xA8));
    set(interruptVectorTable[0xA9], (uint64_t) (&___interrupt0xA9));
    set(interruptVectorTable[0xAA], (uint64_t) (&___interrupt0xAA));
    set(interruptVectorTable[0xAB], (uint64_t) (&___interrupt0xAB));
    set(interruptVectorTable[0xAC], (uint64_t) (&___interrupt0xAC));
    set(interruptVectorTable[0xAD], (uint64_t) (&___interrupt0xAD));
    set(interruptVectorTable[0xAE], (uint64_t) (&___interrupt0xAE));
    set(interruptVectorTable[0xAF], (uint64_t) (&___interrupt0xAF));
    set(interruptVectorTable[0xB0], (uint64_t) (&___interrupt0xB0));
    set(interruptVectorTable[0xB1], (uint64_t) (&___interrupt0xB1));
    set(interruptVectorTable[0xB2], (uint64_t) (&___interrupt0xB2));
    set(interruptVectorTable[0xB3], (uint64_t) (&___interrupt0xB3));
    set(interruptVectorTable[0xB4], (uint64_t) (&___interrupt0xB4));
    set(interruptVectorTable[0xB5], (uint64_t) (&___interrupt0xB5));
    set(interruptVectorTable[0xB6], (uint64_t) (&___interrupt0xB6));
    set(interruptVectorTable[0xB7], (uint64_t) (&___interrupt0xB7));
    set(interruptVectorTable[0xB8], (uint64_t) (&___interrupt0xB8));
    set(interruptVectorTable[0xB9], (uint64_t) (&___interrupt0xB9));
    set(interruptVectorTable[0xBA], (uint64_t) (&___interrupt0xBA));
    set(interruptVectorTable[0xBB], (uint64_t) (&___interrupt0xBB));
    set(interruptVectorTable[0xBC], (uint64_t) (&___interrupt0xBC));
    set(interruptVectorTable[0xBD], (uint64_t) (&___interrupt0xBD));
    set(interruptVectorTable[0xBE], (uint64_t) (&___interrupt0xBE));
    set(interruptVectorTable[0xBF], (uint64_t) (&___interrupt0xBF));
    set(interruptVectorTable[0xC0], (uint64_t) (&___interrupt0xC0));
    set(interruptVectorTable[0xC1], (uint64_t) (&___interrupt0xC1));
    set(interruptVectorTable[0xC2], (uint64_t) (&___interrupt0xC2));
    set(interruptVectorTable[0xC3], (uint64_t) (&___interrupt0xC3));
    set(interruptVectorTable[0xC4], (uint64_t) (&___interrupt0xC4));
    set(interruptVectorTable[0xC5], (uint64_t) (&___interrupt0xC5));
    set(interruptVectorTable[0xC6], (uint64_t) (&___interrupt0xC6));
    set(interruptVectorTable[0xC7], (uint64_t) (&___interrupt0xC7));
    set(interruptVectorTable[0xC8], (uint64_t) (&___interrupt0xC8));
    set(interruptVectorTable[0xC9], (uint64_t) (&___interrupt0xC9));
    set(interruptVectorTable[0xCA], (uint64_t) (&___interrupt0xCA));
    set(interruptVectorTable[0xCB], (uint64_t) (&___interrupt0xCB));
    set(interruptVectorTable[0xCC], (uint64_t) (&___interrupt0xCC));
    set(interruptVectorTable[0xCD], (uint64_t) (&___interrupt0xCD));
    set(interruptVectorTable[0xCE], (uint64_t) (&___interrupt0xCE));
    set(interruptVectorTable[0xCF], (uint64_t) (&___interrupt0xCF));
    set(interruptVectorTable[0xD0], (uint64_t) (&___interrupt0xD0));
    set(interruptVectorTable[0xD1], (uint64_t) (&___interrupt0xD1));
    set(interruptVectorTable[0xD2], (uint64_t) (&___interrupt0xD2));
    set(interruptVectorTable[0xD3], (uint64_t) (&___interrupt0xD3));
    set(interruptVectorTable[0xD4], (uint64_t) (&___interrupt0xD4));
    set(interruptVectorTable[0xD5], (uint64_t) (&___interrupt0xD5));
    set(interruptVectorTable[0xD6], (uint64_t) (&___interrupt0xD6));
    set(interruptVectorTable[0xD7], (uint64_t) (&___interrupt0xD7));
    set(interruptVectorTable[0xD8], (uint64_t) (&___interrupt0xD8));
    set(interruptVectorTable[0xD9], (uint64_t) (&___interrupt0xD9));
    set(interruptVectorTable[0xDA], (uint64_t) (&___interrupt0xDA));
    set(interruptVectorTable[0xDB], (uint64_t) (&___interrupt0xDB));
    set(interruptVectorTable[0xDC], (uint64_t) (&___interrupt0xDC));
    set(interruptVectorTable[0xDD], (uint64_t) (&___interrupt0xDD));
    set(interruptVectorTable[0xDE], (uint64_t) (&___interrupt0xDE));
    set(interruptVectorTable[0xDF], (uint64_t) (&___interrupt0xDF));
    set(interruptVectorTable[0xE0], (uint64_t) (&___interrupt0xE0));
    set(interruptVectorTable[0xE1], (uint64_t) (&___interrupt0xE1));
    set(interruptVectorTable[0xE2], (uint64_t) (&___interrupt0xE2));
    set(interruptVectorTable[0xE3], (uint64_t) (&___interrupt0xE3));
    set(interruptVectorTable[0xE4], (uint64_t) (&___interrupt0xE4));
    set(interruptVectorTable[0xE5], (uint64_t) (&___interrupt0xE5));
    set(interruptVectorTable[0xE6], (uint64_t) (&___interrupt0xE6));
    set(interruptVectorTable[0xE7], (uint64_t) (&___interrupt0xE7));
    set(interruptVectorTable[0xE8], (uint64_t) (&___interrupt0xE8));
    set(interruptVectorTable[0xE9], (uint64_t) (&___interrupt0xE9));
    set(interruptVectorTable[0xEA], (uint64_t) (&___interrupt0xEA));
    set(interruptVectorTable[0xEB], (uint64_t) (&___interrupt0xEB));
    set(interruptVectorTable[0xEC], (uint64_t) (&___interrupt0xEC));
    set(interruptVectorTable[0xED], (uint64_t) (&___interrupt0xED));
    set(interruptVectorTable[0xEE], (uint64_t) (&___interrupt0xEE));
    set(interruptVectorTable[0xEF], (uint64_t) (&___interrupt0xEF));
    set(interruptVectorTable[0xF0], (uint64_t) (&___interrupt0xF0));
    set(interruptVectorTable[0xF1], (uint64_t) (&___interrupt0xF1));
    set(interruptVectorTable[0xF2], (uint64_t) (&___interrupt0xF2));
    set(interruptVectorTable[0xF3], (uint64_t) (&___interrupt0xF3));
    set(interruptVectorTable[0xF4], (uint64_t) (&___interrupt0xF4));
    set(interruptVectorTable[0xF5], (uint64_t) (&___interrupt0xF5));
    set(interruptVectorTable[0xF6], (uint64_t) (&___interrupt0xF6));
    set(interruptVectorTable[0xF7], (uint64_t) (&___interrupt0xF7));
    set(interruptVectorTable[0xF8], (uint64_t) (&___interrupt0xF8));
    set(interruptVectorTable[0xF9], (uint64_t) (&___interrupt0xF9));
    set(interruptVectorTable[0xFA], (uint64_t) (&___interrupt0xFA));
    set(interruptVectorTable[0xFB], (uint64_t) (&___interrupt0xFB));
    set(interruptVectorTable[0xFC], (uint64_t) (&___interrupt0xFC));
    set(interruptVectorTable[0xFD], (uint64_t) (&___interrupt0xFD));
    set(interruptVectorTable[0xFE], (uint64_t) (&___interrupt0xFE));
    set(interruptVectorTable[0xFF], (uint64_t) (&___interrupt0xFF));
}