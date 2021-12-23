#include "BasicOutput/Output.hpp"
#include "BasicOutput/VGATextOut.hpp"
#include "Memory/memory.hpp"

uint8_t defaultBuffer[sizeof(VGATextOut)];
Output* defaultOut;

void Output::init() {
    defaultOut = new ((void*) defaultBuffer) VGATextOut();
}

Output* Output::getDefault() {
    return defaultOut;
}

void Output::print(const char* str) {
    for (; *str; ++str) {
        print(*str);
    }
}

void Output::print(uint64_t hex, uint8_t minSize, uint64_t maxSize) {
    if (maxSize < 16) {
        hex &= (1ull << (maxSize * 4)) - 1;
        if (minSize > maxSize) {
            minSize = maxSize;
        }
    }
    minSize = 16 - minSize;
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t digit = (uint8_t) ((hex >> 60) & 0xF);
        hex <<= 4;
        if (digit != 0) {
            minSize = i;
        }
        if (i >= minSize) {
            if (digit < 10) {
                print((char) (digit + '0'));
            } else {
                print((char) (digit - 10 + 'A'));
            }
        }
    }
}

void Output::printDec(int64_t dec, bool printPlus) {
    if (dec < 0) {
        print('-');
        dec = -dec;
    } else if (printPlus) {
        print('+');
    }
    printDec((uint64_t) dec);
}

void Output::printDec(uint64_t dec) {
    uint64_t div = 1;
    while (dec / div >= 10) {
        div *= 10;
    }
    while (div > 0) {
        print((char) ((dec / div) % 10 + '0'));
        div /= 10;
    }
}

//implement specifiers d,i,u,x,s,c and %
//length identifiers:
//  none = 32bit
//  hh (H)= 8bit
//  h = 16bit
//  l = 64bit
//  flags: +, 0, space
//  width: number

void Output::printValueImpl(uint64_t v, char specifier, char length, char flags, uint8_t width) {
    if (!width) width = 1;
    if (specifier == '%') {
        print('%');
        return;
    }
    if (specifier == 's') {
        print((const char*) v);
        return;
    }
    if (specifier == 'c') {
        print((char) v);
        return;
    }
    if (specifier == 'd' || specifier == 'i') {
        if (length == 'h') {
            int16_t v16 = (int16_t) v;
            printDec(v16, flags == '+');
        } else if (length == 'H') {
            int8_t v8 = (int8_t) v;
            printDec(v8, flags == '+');
        } else if (length == 'l') {
            int64_t v64 = (int64_t) v;
            printDec(v64, flags == '+');
        } else {
            int32_t v32 = (int32_t) v;
            printDec(v32, flags == '+');
        }
        return;
    }
    if (specifier == 'u') {
        if (length == 'h') {
            printDec((uint64_t) (v & 0xFFFFull));
        } else if (length == 'H') {
            printDec((uint64_t) (v & 0xFFull));
        } else if (length == 'l') {
            printDec((uint64_t) (v & 0xFFFFFFFFFFFFFFFFull));
        } else {
            printDec((uint64_t) (v & 0xFFFFFFFFull));
        }
        return;
    }
    if (specifier == 'x') {
        if (length == 'h') {
            print(v & 0xFFFFull, width, 4);
        } else if (length == 'H') {
            print(v & 0xFFull, width, 2);
        } else if (length == 'l') {
            print(v & 0xFFFFFFFFFFFFFFFFull, width, 16);
        } else {
            print(v & 0xFFFFFFFFull, width, 8);
        }
        return;
    }
}

void Output::print(BitList bits) {
    print('[');
    bool first = true;
    for (uint64_t i = 0; i < 64; ++i) {
        if (bits.value & 1 << i) {
            if (!first) {
                print(',');
            }
            first = false;
            printDec(i);
        }
    }
    print(']');
}

const char* Output::printFormat(const char* format, char* specifier, char* length, char* flags, uint8_t* width) {
    while (*format) {
        if (*format == '%') {
            ++format;
            break;
        }
        print(*format);
        ++format;
    }
    *specifier = 0;
    *length = 0;
    *flags = 0;
    *width = 0;

    while (*format) {
        //d,i,u,x,s,c,%
        if (*format == '%' || *format == 'd' || *format == 'i' || *format == 'u' || *format == 'x' || *format == 's' || *format == 'c') {
            *specifier = *format;
            ++format;
            break;
        } else if (*format == 'h') {
            if (*length == 'h') {
                *length = 'H';
            } else {
                *length = 'h';
            }
            ++format;
        } else if (*format == 'p') {
            *length = 'l';
            *specifier = 'x';
            ++format;
            break;
        } else if (*format == 'l') {
            *length = 'l';
            ++format;
        } else if (*format == '+') {
            *flags = '+';
            ++format;
        } else if (*format == '0') {
            *flags = '0';
            ++format;
        } else if (*format == ' ') {
            *flags = ' ';
            ++format;
        } else if (*format >= '0' && *format <= '9') {
            *width = *format - '0';
            ++format;
            while (*format >= '0' && *format <= '9') {
                *width = *width * 10 + *format - '0';
                ++format;
            }
        } else {
            ++format;
        }
    }
    return format;
}
