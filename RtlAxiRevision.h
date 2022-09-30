//==========================================================================================================
// RtlAxiRevision.h - Defines an interface to an RTL axi_revision module.
//==========================================================================================================
#pragma once
#include <stdint.h>
#include <string>
using std::string;

class RtlAxiRevision
{
public:

    void setBaseAddress(uint8_t* p) {baseAddr_ = (uint32_t*)p;}
    
    string getVersion();
    
    string getDate();


protected:

    uint32_t*   baseAddr_;
};

