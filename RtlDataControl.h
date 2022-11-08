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

    void setBaseAddress(uint8_t* p) {baseAddr_ = (volatile uint32_t*)p;}

protected:

    volatile uint32_t* baseAddr_;
};

