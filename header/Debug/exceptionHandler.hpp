#pragma once

#include "CPUControl/interrupts.hpp"

void onPageFault(Interrupt&);
void onInvalidOpcode(Interrupt&);