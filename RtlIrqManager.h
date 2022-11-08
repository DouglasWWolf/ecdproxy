//==========================================================================================================
// RtlIrqManager.h - Defines an interface to an RTL interrupt manager module.
//==========================================================================================================
#pragma once
#include <stdint.h>
#include <string>
using std::string;

class RtlIrqManager
{
public:

    void setBaseAddress(uint8_t* p) {baseAddr_ = (uint32_t*)p;}

    uint32_t getActiveInterrupts();

    void     clearInterrupts(uint32_t irqBitmap);

protected:

    volatile uint32_t* baseAddr_;
};

