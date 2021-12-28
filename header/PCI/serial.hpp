#pragma once
#include "BasicOutput/Output.hpp"
#include "PCI/pci.hpp"

class Serial : public Output {
public:
    static PCI::Handler* getPCIHandler();

    Serial(PCI& pci);
    void printImpl(char c) override;

    inline void clear() {}
    inline uint64_t getWidth() { return 0; }
    inline uint64_t getHeight() { return 0; }
    inline uint64_t getX() { return 0; }
    inline uint64_t getY() { return 0; }
    inline void setX(uint64_t x) {}
    inline void setY(uint64_t y) {}

    inline bool isValid() { return valid; }

private:
    PCI pci;
    PCI::BAR bar;
    bool valid;
};