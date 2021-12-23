#pragma once

#include <stdint.h>

extern "C" void* memset(void* ptr, int value, uint64_t num);
extern "C" void* memcpy(void* destination, const void* source, uint64_t num);
extern "C" int memcmp(const void* ptr1, const void* ptr2, uint64_t num);

extern "C" char* strncpy(char* destination, const char* source, uint64_t num);
extern "C" int strcmp(const char* str1, const char* str2);
extern "C" uint64_t strlen(const char* str);