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

// The addresses and size of the ping-pong buffers
const uint64_t PPB0 = 0x100000000;   // Address 4G
const uint64_t PPB1 = 0x200000000;   // Address 8G
const uint32_t PPB_BLOCKS = 0x100000000LL / 2048;  // However many rows will fit into 4G

// Userspace pointer to reserved RAM
uint8_t* physMem;

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
    
    // Notify the ECD-Master that this buffer has been refilled
    notifyBufferFull(irq);
}
//=================================================================================================


//=================================================================================================
// fillBuffer() - This stuffs some data into the buffer for the purposes of our demo
//=================================================================================================
void fillBuffer(int which, uint32_t row)
{
    printf("Filling buffer #%i\n", which);

    // The offset into our contiguous buffer depends on which ping-pong buffer we're filling
    uint64_t memOffset = (which == 0) ? 0 : (PPB1 - PPB0);

    // Get a pointer to the start of the appropriate ping-pong buffer
    uint8_t* ptr = (uint8_t*) (physMem + memOffset);

    // Open the data file
    int fd = open("bigdata.dat", O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    // A row if data is 2048 bytes.  Compute how many bytes of data to load...
    uint64_t bytes_remaining = PPB_BLOCKS * 2048;

    // While there is still data to load from the file...
    while (bytes_remaining)
    {
        // We'd like to load the entire remainder of the file
        size_t block_size = bytes_remaining;

        // We're going to load this file in chunks of no more than 1 GB
        if (block_size > 0x40000000) block_size = 0x40000000;

        // Read in this chunk of the file
        printf("Loading %lu\n", block_size);
        size_t rc = read(fd, ptr, block_size);
        if (rc != block_size)
        {
            perror("read");
            exit(1);
        }

        // Bump the pointer to where the next chunk will be stored
        ptr += block_size;

        // And keep track of how many bytes are left to load
        bytes_remaining -= block_size;
    }

    // We're done with the input file
    close(fd);

    // We display a "done" message so we can physically see how long
    // it takes to fill the buffer with data
    printf("filling buffer #%i complete\n", which);

}
//=================================================================================================

