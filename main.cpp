#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "ecdproxy.h"


using namespace std;

// This is the geometry of a single block.  Right now, one block = 1 row of data
const uint32_t BYTES_PER_CYCLE = 32;
const uint32_t CYCLES_PER_BLOCK = 64;

// At 32 bytes-per-cycle and 64 cycles-per-block, this works out to be exactly 2K (2048 bytes)
const uint32_t BYTES_PER_BLOCK = BYTES_PER_CYCLE * CYCLES_PER_BLOCK;

// The addresses and size of the ping-pong buffers
const uint64_t CONTIG_BUFFER = 0x100000000;   // Address 4G
const uint32_t CONTIG_BLOCKS = 0x100000000LL / BYTES_PER_BLOCK;  // However many rows will fit into 4GB

// Forward declarations
void     execute();
void     parseCommandLine(const char** argv);

// Interfaces to the ECD_Master/ECD RTL designs
class ECD : public CECDProxy
{
    void onInterrupt(int irq, uint64_t irqCounter);
} proxy;


//=================================================================================================
// These flags are set by command-line switches
//=================================================================================================
bool loadEcdFPGA    = false;
bool loadMasterFPGA = false;
//=================================================================================================


//=================================================================================================
// main() - Execution starts here
//=================================================================================================
int main(int argc, const char** argv)
{
    parseCommandLine(argv);

    printf("Proxy Test!\n");

    try
    {
        execute();
    }
    catch(const std::exception& e)
    {
        printf("%s\n", e.what());
        exit(1);
    }
}
//=================================================================================================


//=================================================================================================
// parseCommandLine() - Parses the command line looking for switches
//
// On Exit: if "-ecd"  switch was used, "loadEcdFPGA" is 'true'
//          If "-ecdm" switch was used, "loadMasterFPGA" is 'true'
//=================================================================================================
void parseCommandLine(const char** argv)
{
    while (*++argv)
    {
        const char* arg = *argv;

        if (strcmp(arg, "-ecd") == 0)
        {
            loadEcdFPGA = true;
            continue;
        }        

        if (strcmp(arg, "-ecdm") == 0)
        {
            loadMasterFPGA = true;
            continue;
        }        

        cerr << "Unknown command line switch " << arg << "\n";
        exit(1);
    }    
}
//=================================================================================================


//=================================================================================================
// execute() - Does everything neccessary to begin a data transfer
//
// This routine assumes the run data has already been loaded into the contiguous RAM buffer
//=================================================================================================
void execute()
{
    bool ok;
   
    // Initialize ecdproxy interface
    cout << "Initializing ECDProxy\n";
    proxy.init("ecd_proxy.conf");

    // If the user wants to load the ECD bitstream into the FPGA...
    if (loadEcdFPGA)
    {
        cout << "Loading ECD bitstream \n";    
        ok = proxy.loadEcdBitstream();
        if (!ok)
        {
            printf("%s\n", proxy.getLoadError().c_str());
            exit(1);
        }
    }

    // If the user wants to load the master bitstream into the FPGA...
    if (loadMasterFPGA)
    {
        cout << "Loading Master bitstream \n";    
        ok = proxy.loadMasterBitstream();
        if (!ok)
        {
            printf("%s\n", proxy.getLoadError().c_str());
            exit(1);
        }
    }

    // Perform hot-reset, map PCI device resources, init UIO subsystem, etc.
    proxy.startPCI();

    // Query the RTL design for revision information and display it
    string version = proxy.getMasterBitstreamVersion();
    cout << "RTL version is " << version << "\n";
    string date = proxy.getMasterBitstreamDate();
    cout << "RTL date: " << date << "\n";

    // Check to make sure that both QSFP channels are up
    proxy.checkQsfpStatus(0, true);
    cout << "QSFP Channel 0 is up\n";
    proxy.checkQsfpStatus(1, true);
    cout << "QSFP Channel 1 is up\n";

    // Start the data transfer
    proxy.prepareDataTransfer(CONTIG_BUFFER, CONTIG_BLOCKS);

    // And sleep forever
    cout << "Waiting for interrupts\n";
    while(1) sleep(999999);

}
//=================================================================================================


//=================================================================================================
// onInterrupt() - This gets called whenever a PCI interrupt occurs
//
// Passed: irq        = 0 or 1 (i.e., buffer0 is empty, or buffer1 is empty)
//         irqCounter = The number of times an interrupt has occured for this irq
//=================================================================================================
void ECD::onInterrupt(int irq, uint64_t irqCounter)
{
    // printf for demonstration purposes.  This is impractical in a real application
    printf("Servicing IRQ %i, #%lu\n", irq, irqCounter);
    
}
//=================================================================================================

