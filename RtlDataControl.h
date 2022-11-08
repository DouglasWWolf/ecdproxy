//==========================================================================================================
// RtlDataControl.h - Defines an interface to an RTL data control module.
//==========================================================================================================
#pragma once
#include <stdint.h>
#include <string>
using std::string;

class RtlDataControl
{
public:

    // Call this to set the AXI base address of this RTL module
    void setBaseAddress(uint8_t* p) {baseAddr_ = (volatile uint32_t*)p;}

    // Call this to begin a data transfer and pre-load the RTL FIFO
    void start(uint64_t addr0, uint64_t addr1, uint64_t buffSize);

protected:

    volatile uint32_t* baseAddr_;
};

