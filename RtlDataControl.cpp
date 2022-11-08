//==========================================================================================================
// RtlDataControl.cpp - Implements an interface to the data-control RTL module
//==========================================================================================================
#include <string.h>
#include "RtlDataControl.h"


//------------------------------------------------------------------
// These are the valid registers, as offsets from the base address
//------------------------------------------------------------------
enum
{

    REG_PPB0H    = 0,   // Ping Pong Buffer #0, hi 32-bits
    REG_PPB0L    = 1,   // Ping Pong Buffer #0, lo 32-bits
    REG_PPB1H    = 2,   // Ping Pong Buffer #1, hi 32-bits
    REG_PPB1L    = 3,   // Ping Pong Buffer #1, lo 32-bits
    REG_PPB_SIZE = 4,   // Ping Pong buffer size in 2048-byte blocks
    REG_START    = 10,  // A write to this register starts data transfer
    REG_PPB_RDY  = 11   // Used to signal that a PPB has been loaded with data
};
//------------------------------------------------------------------


void RtlDataControl::start(uint64_t addr0, uint64_t addr1, uint64_t buffSize)
{

    
}