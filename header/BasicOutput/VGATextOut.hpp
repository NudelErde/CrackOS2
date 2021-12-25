#pragma once

#include "Output.hpp"

class VGATextOut : public Output {
public:
    VGATextOut();
    ~VGATextOut() override = default;

    void print(char c) override;
    void clear() override;
    uint64_t getWidth() override;
    uint64_t getHeight() override;
    uint64_t getX() override;
    uint64_t getY() override;
    void setX(uint64_t x) override;
    void setY(uint64_t y) override;
    void scroll();

private:
    uint64_t index = 0;
    volatile char* textBuffer;
};