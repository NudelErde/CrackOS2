#pragma once

#include "ACPI/ACPI.hpp"
#include <stdint.h>

class APIC {
public:
    static ACPI::TableParser* getParser();
};