
//==========================================================================================================
// RtlRestartManager.cpp - Implements an interface to the restart-manager RTL module
//==========================================================================================================
#include <unistd.h>
#include <string.h>
#include "RtlRestartManager.h"

//------------------------------------------------------------------
// These are the valid registers, as offsets from the base address
//------------------------------------------------------------------
enum
{
    REG_RESTART
};
//------------------------------------------------------------------




//==========================================================================================================
// restart() - Places the ECD-Master into a known condition, and waits for data to drain out of
//             the system
//==========================================================================================================
void RtlRestartManager::restart()
{
    // Place the ECD-Master RTL design into a known condition
    baseAddr_[REG_RESTART] = 1;
    
    // Wait for data to drain out of the system
    usleep(500000);
}
//==========================================================================================================

