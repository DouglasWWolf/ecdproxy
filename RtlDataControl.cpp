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



//==========================================================================================================
// start() - Pre-loads the RTL FIFO with data from our ping-pong buffers, and prepares the RTL design
//           to start sending data over QSFP as requested by the ECD
//
// Passed: addr0    = Physical address of the 1st part of the ping-pong buffer
//         addr1    = Physical address of the 2nd part of the ping-pong buffer
//         buffSize = The size of each buffer in units of 2048 byte blocks
//==========================================================================================================
void RtlDataControl::start(uint64_t addr0, uint64_t addr1, uint32_t buffSize)
{
    // Give our AXI slave the physical addresses of our ping-pong buffers
    baseAddr_[REG_PPB0H] = (addr0 >> 32) & 0xFFFFFFFF;
    baseAddr_[REG_PPB0L] = (addr0      ) & 0xFFFFFFFF;    
    baseAddr_[REG_PPB1H] = (addr1 >> 32) & 0xFFFFFFFF;
    baseAddr_[REG_PPB1L] = (addr1      ) & 0xFFFFFFFF;    

    // Give our AXI slave the size of those buffers, in units of 2048 byte blocks
    baseAddr_[REG_PPB_SIZE] = buffSize;

    // Start the data transfer
    baseAddr_[REG_START] = 1;
}
//==========================================================================================================



//==========================================================================================================
// notifyBufferFull() - Notifies the data control module that the specified ping-pong buffer has
//                      been replenished with data.
//==========================================================================================================
void RtlDataControl::notifyBufferFull(int which)
{
    // Ensure that a legitimate buffer number is passed in 
    if (which < 0 || which > 1) return;

    // Write a '1' to the appropriate bit in the PPB_RDY register
    baseAddr_[REG_PPB_RDY]  = (1 << which);
}
//==========================================================================================================
