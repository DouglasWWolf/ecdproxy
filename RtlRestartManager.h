//==========================================================================================================
// RtlRestartManager.h - Defines an interface to an RTL restart manager module.
//==========================================================================================================
#pragma once
#include <stdint.h>
#include <string>
using std::string;

class RtlRestartManager
{
public:

    void setBaseAddress(uint8_t* p) {baseAddr_ = (volatile uint32_t*)p;}

    void restart();

protected:

    volatile uint32_t* baseAddr_;
};

