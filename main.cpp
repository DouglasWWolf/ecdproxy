#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "ecdproxy.h"

// The addresses and size of the ping-pong buffers
const uint64_t PPB0 = 0x100000000;
const uint64_t PPB1 = 0x200000000;
const uint32_t PPB_BLOCKS = 0x100000000LL / 2048;

// Userspace pointer to reserved RAM
uint8_t* physMem;

using namespace std;

// Forward declarations
uint8_t* mapPhysMem(uint64_t physAddr, size_t size);
void     fillBuffer(int which, uint32_t row);
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
//=================================================================================================
void execute()
{
    bool ok;

    // Map the reserved RAM block into userspace
    cout << "Mapping physical RAM\n";
    physMem = mapPhysMem(0x100000000, 0x200000000);
   
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
    cout << "Version is " << version << "\n";
    string date = proxy.getMasterBitstreamDate();
    cout << "Date: " << date << "\n";

    // Fill the ping-pong buffers
    fillBuffer(0, 0);
    fillBuffer(1, 0);

    // Start the data transfer
    proxy.prepareDataTransfer(PPB0, PPB1, PPB_BLOCKS);

    // And sleep forever
    cout << "Waiting for interrupts\n";
    while(1) sleep(999999);

}
//=================================================================================================


//=================================================================================================
// mapPhysMem() - Maps phyiscal memory addresses into user-space
//=================================================================================================
uint8_t* mapPhysMem(uint64_t physAddr, size_t size)
{
    const char* filename = "/dev/mem";

    // These are the memory protection flags we'll use when mapping the device into memory
    const int protection = PROT_READ | PROT_WRITE;

    // Open the /dev/mem device
    int fd = ::open(filename, O_RDWR| O_SYNC);

    // If that open failed, we're done here
    if (fd < 0) throw std::runtime_error("Must be root.  Use sudo.");

    // Map the resources of this PCI device's BAR into our user-space memory map
    void* ptr = ::mmap(0, size, protection, MAP_SHARED, fd, physAddr);

    // If a mapping error occurs, it's fatal
    if (ptr == MAP_FAILED) 
    {
        perror("mmap failed");
        throw std::runtime_error("mmap failed");
    }


   // Hand the user-space pointer to the caller
   return (uint8_t*) ptr;
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
    
    // This is the first row number of the first row in this buffer
    // uint32_t startingRow = (irqCounter * 2 + irq) * PPB_BLOCKS; 
    
    // Fill the buffer with data
    // fillBuffer(irq, startingRow);

    // Notify the ECD-Master that this buffer has been refilled
    notifyBufferFull(irq);
}
//=================================================================================================


//=================================================================================================
// fillBuffer() - This stuffs some data into the buffer for the purposes of our demo
//=================================================================================================
void fillBuffer(int which, uint32_t row)
{
    printf("filling buffer #%i\n", which);

    uint8_t marker = (which == 0 ? 0x00 : 0x80);
    uint64_t memOffset = (which == 0) ? 0 : (PPB1 - PPB0);

    uint32_t* ptr = (uint32_t*) (physMem + memOffset);
    
    memset(ptr, which, PPB_BLOCKS * 2048);
    printf("filling buffer #%i complete\n", which);

    return;


    for (uint32_t block = 0; block < PPB_BLOCKS; ++block)
    {
        uint8_t row_h = (row >> 8) & 0xFF;
        uint8_t row_l = (row     ) & 0xFF;

        for (int cycle = 0; cycle < 64; ++cycle)
        {
            uint32_t value = (cycle << 24) | (row_l << 16) | (row_h << 8) | marker;
            for (int index=0; index <8; ++index) *ptr++ = value;
        }

        ++row;
    }


}
//=================================================================================================