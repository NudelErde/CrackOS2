#pragma once

#define saveReadSymbol(name, output) asm volatile("movabs $" name ", %0" \
                                                  : "=a"(output)::)
