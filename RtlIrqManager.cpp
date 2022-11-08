//==========================================================================================================
// RtlIrqManager.cpp - Implements an interface to the interrupt-manager RTL module
//==========================================================================================================
#include <string.h>
#include "RtlIrqManager.h"


//------------------------------------------------------------------
// These are the valid registers, as offsets from the base address
//------------------------------------------------------------------
enum
{
    REG_INTR,
    REG_CLEAR
};
//------------------------------------------------------------------



//==========================================================================================================
// getActiveInterrupts() - Returns a bitmap of which interrupts are currently awaiting servicing
//==========================================================================================================
uint32_t RtlIrqManager::getActiveInterrupts()
{
    return baseAddr_[REG_INTR];
}
//==========================================================================================================


//==========================================================================================================
// clearInterrupts() - Call this to indicate that one or more interrupts have been serviced
//    
// Passed: irqBitMap = bitmap of which IRQs are being acknowledged as serviced
//==========================================================================================================
void RtlIrqManager::clearInterrupts(uint32_t irqBitmap)
{
    baseAddr_[REG_CLEAR] = irqBitmap;    
}
//==========================================================================================================