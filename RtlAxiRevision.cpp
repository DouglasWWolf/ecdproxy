//==========================================================================================================
// RtlAxiRevision.cpp - Implements an interface to an axi_revision RTL module
//==========================================================================================================
#include <string.h>
#include "RtlAxiRevision.h"


//------------------------------------------------------------------
// These are the valid registers, as offsets from the base address
//------------------------------------------------------------------
enum
{
    REG_MAJOR = 0,
    REG_MINOR = 1,
    REG_BUILD = 2,
    REG_RCAND = 3,
    REG_DATE  = 4    
};
//------------------------------------------------------------------



//==========================================================================================================
// getVersion() - Returns the version string
//==========================================================================================================
string RtlAxiRevision::getVersion()
{
    char buffer[32];

    // Fetch the components of the version string 
    int major = baseAddr_[REG_MAJOR];
    int minor = baseAddr_[REG_MINOR];
    int build = baseAddr_[REG_BUILD];
    int rcand = baseAddr_[REG_RCAND];

    // Turn the components of the version number into "x.y.z"
    sprintf(buffer, "%i.%i.%i", major, minor, build);

    // If the release-candidate number isn't zero...
    if (rcand)
    {
        // Find the end of the buffer
        char* p = strchr(buffer, 0);

        // And append the release-candidate suffix to the version number
        sprintf(p, "-rc%i", rcand);
    }

    // And hand the resulting version string to the caller
    return buffer;
}
//==========================================================================================================


//==========================================================================================================
// getDate() - Returns the bitstream build date as a string
//==========================================================================================================
string RtlAxiRevision::getDate()
{
    char buffer[32];
    const char* monthName[] =
    {
        "",
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };


    // Fetch the date, packed into a 32-bit word
    int packedDate = baseAddr_[REG_DATE];

    // The top 8 bits are the month
    int month = (packedDate >> 24) & 0xFF;

    // The next 8 bits are the day
    int day   = (packedDate >> 16) & 0xFF;
    
    // The bottom 16 bits are the year
    int year  = packedDate & 0xFFFF;

    // If the month is out of range, this can't possibly be a date
    if (month < 1 || month > 12) return "unknown";

    // Format the date like this: 28-Sep-2022
    sprintf(buffer, "%i-%s-%i", day, monthName[month], year);

    // And hand the resulting string to the caller
    return buffer;
}
//==========================================================================================================

