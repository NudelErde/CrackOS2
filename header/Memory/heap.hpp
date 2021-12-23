#pragma once
#include "stdint.h"

void initHeap();
void* kmalloc(uint64_t size);
void kfree(void* ptr);
