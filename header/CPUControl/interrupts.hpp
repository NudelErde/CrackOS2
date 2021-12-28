#pragma once

#include <stdint.h>

class Interrupt {
public:
    using InterruptHandler = void (*)(Interrupt&);

    struct InterruptOptions {
        bool callableFromUserMode = true;
        uint8_t interruptStack = 0;
    };

    enum class HardwareInterruptNumberSource {
        PIC, // Programmable Interrupt Controller
        APIC,// Advanced Programmable Interrupt Controller
    };

    static void enableInterrupts();
    static void disableInterrupts();
    static bool isInterruptEnabled();
    static void setupInterruptVectorTable();
    static void setupInterruptHandler(uint8_t interruptNumber, InterruptHandler handlerAddress, InterruptOptions options = {true, 0});
    template<uint8_t interruptNumber>
    static inline void softwareInterrupt() {
        asm volatile("int %0"
                     :
                     : "i"(interruptNumber));
    }

    static uint8_t hardwareInterruptToVector(HardwareInterruptNumberSource source, uint8_t interruptNumber);

    static bool isHardwareInterruptEnabled(HardwareInterruptNumberSource source, uint8_t interruptNumber);
    static void setHardwareInterruptEnabled(HardwareInterruptNumberSource source, uint8_t interruptNumber, bool enabled);

    static void sendEOI(uint8_t interruptNumber);

    static void switchToAPICMode();

public:
    Interrupt() = default;
    Interrupt(uint8_t interruptNumber, uint64_t errorCode, uint64_t stackFrame, bool hasError);

    [[nodiscard]] inline uint8_t getInterruptNumber() const {
        return interruptNumber;
    }

    [[nodiscard]] inline uint64_t getErrorCode() const {
        return errorCode;
    }

    [[nodiscard]] inline uint64_t getStackFrame() const {
        return stackFrame;
    }

    [[nodiscard]] inline bool hasError() const {
        return isError;
    }

    [[nodiscard]] inline uint64_t getInstruction() const {
        return ((uint64_t*) stackFrame)[0];
    }

    [[nodiscard]] inline uint64_t getStack() const {
        return ((uint64_t*) stackFrame)[3];
    }

private:
    uint8_t interruptNumber;
    uint64_t errorCode;
    uint64_t stackFrame;
    bool isError;
};
