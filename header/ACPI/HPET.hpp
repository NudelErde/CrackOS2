#pragma once

#include "ACPI/ACPI.hpp"
#include "Common/Units.hpp"

class HPET {
public:
    static ACPI::TableParser* getParser();
    static void setTimer(time::nanosecond nanoseconds, uint8_t timer, uint8_t interrupt);
};