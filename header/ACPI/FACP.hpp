#pragma once

#include "ACPI/ACPI.hpp"
#include "stdint.h"

class FACP {
public:
    static ACPI::TableParser* getParser();
};