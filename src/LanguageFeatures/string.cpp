#include "LanguageFeatures/string.hpp"

void intToString(int64_t value, char* buffer, int base) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    int64_t sign = 1;
    if (value < 0) {
        sign = -1;
        value = -value;
    }
    int64_t i = 0;
    while (value > 0) {
        int64_t digit = value % base;
        if (digit < 10) {
            buffer[i] = '0' + digit;
        } else {
            buffer[i] = 'a' + digit - 10;
        }
        value /= base;
        i++;
    }
    if (sign < 0) {
        buffer[i] = '-';
        i++;
    }
    buffer[i] = '\0';
    reverse(buffer, buffer + i);
}

void reverse(char* buffer, char* end) {
    char* start = buffer;
    char* end2 = end - 1;
    while (start < end2) {
        char temp = *start;
        *start = *end2;
        *end2 = temp;
        start++;
        end2--;
    }
}