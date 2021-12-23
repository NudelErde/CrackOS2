#include "BasicOutput/VGATextOut.hpp"
#include "CPUControl/cpu.hpp"

VGATextOut::VGATextOut() {
    index = 0;
    textBuffer = (volatile char*) 0xB8000;
}

void VGATextOut::print(char c) {
    if (c == '\n') {
        setY(getY() + 1);
        setX(0);
    } else {
        textBuffer[index * 2] = c;
        textBuffer[index * 2 + 1] = 0x07;
        index++;
    }
}

void VGATextOut::clear() {
    for (uint64_t i = 0; i < 80 * 25; i++) {
        textBuffer[i * 2] = ' ';
        textBuffer[i * 2 + 1] = 0x07;
    }
}

uint64_t VGATextOut::getWidth() {
    return 80;
}

uint64_t VGATextOut::getHeight() {
    return 25;
}

uint64_t VGATextOut::getX() {
    return index % 80;
}

uint64_t VGATextOut::getY() {
    return index / 80;
}

void VGATextOut::setX(uint64_t x) {
    index = x + getY() * 80;
}

void VGATextOut::setY(uint64_t y) {
    index = y * 80 + getX();
}