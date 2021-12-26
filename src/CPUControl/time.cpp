#include "CPUControl/time.hpp"
#include "ACPI/HPET.hpp"
#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "CPUControl/interrupts.hpp"

static volatile bool sleepDone = false;
static bool hasInit = false;

void handleSleep(Interrupt& inter) {
    sleepDone = true;
}

void sleep(time::nanosecond d) {
    if (!hasInit) {
        Interrupt::setupInterruptHandler(32, handleSleep);
        hasInit = true;
        sleepDone = true;
    }
    while (!sleepDone) {}// active wait until i can sleep <- this is very bad but yeah

    HPET::setTimer(d.value, 2, 32);
    sleepDone = false;
    bool interruptEnabled = Interrupt::isInterruptEnabled();
    Interrupt::enableInterrupts();
    while (!sleepDone) {
        Interrupt::enableInterrupts();
        halt();// should not block interrupts
    }
    if (!interruptEnabled) {
        Interrupt::disableInterrupts();
    }
}
void sleep(time::microsecond d) {
    sleep(time::nanosecond(d));
}
void sleep(time::millisecond d) {
    sleep(time::nanosecond(d));
}
void sleep(time::second d) {
    uint64_t count = d.value;
    while (count > 0) {
        sleep(time::millisecond(1000));
        count--;
    }
}
void sleep(time::minute d) {
    sleep(time::second(d));
}
void sleep(time::hour d) {
    sleep(time::second(d));
}
void sleep(time::day d) {
    sleep(time::second(d));
}