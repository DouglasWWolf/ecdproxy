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
const uint64_t PPB0 = 0x100000000;   // Address 4G
const uint64_t PPB1 = 0x200000000;   // Address 8G
const uint32_t PPB_BLOCKS = 0x20000000 / BYTES_PER_BLOCK;  // However many rows will fit into 512MB

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
    cout << "RTL version is " << version << "\n";
    string date = proxy.getMasterBitstreamDate();
    cout << "RTL date: " << date << "\n";

    // Check to make sure that both QSFP channels are up
    proxy.checkQsfpStatus(0, true);
    cout << "QSFP Channel 0 is up\n";
    proxy.checkQsfpStatus(1, true);
    cout << "QSFP Channel 1 is up\n";
    
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
    
    /*
     *
     *     In real life, we would refill the ping-pong buffer here
     * 
     */

    // Notify the ECD-Master that this buffer has been refilled
    notifyBufferFull(irq);
}
//=================================================================================================


//=================================================================================================
// fillBuffer() - This stuffs some data into the DMA buffer for the purposes of our demo
//
//                          <<< THIS ROUTINE IS A KLUDGE >>>
//
// Because of yet unresolved issues with very slow-writes to the DMA buffer, we are reading the
// file into a local user-space buffer then copying it into the DMA buffer.    For reasons we don't
// yet understand, the MMU allows us to copy a user-space buffer into the DMA space buffer faster
// than it allows us to write to it directly.
//
//                               <<< THIS IS A HACK >>>
//
// The hack will be fixed when we figure out how to write a device driver that can allocate
// very large contiguouis blocks.
//   
//=================================================================================================
void fillBuffer(int which, uint32_t row)
{
    // One gigabyte
    const uint32_t ONE_GB = 0x40000000;

    // Tell the user what's taking so long...
    printf("Loading ping-pong buffer #%i\n", which);

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

    // Allocate a 1GB RAM buffer in userspace
    uint8_t* local_buffer = new uint8_t[ONE_GB];

    // Compute how many bytes of data to load...
    uint64_t bytes_remaining = (uint64_t)PPB_BLOCKS * (uint64_t)BYTES_PER_BLOCK;

    // While there is still data to load from the file...
    while (bytes_remaining)
    {
        // We'd like to load the entire remainder of the file
        size_t block_size = bytes_remaining;

        // We're going to load this file in chunks of no more than 1 GB
        if (block_size > ONE_GB) block_size = ONE_GB;

        // Load this chunk of the file into our local user-space buffer
        size_t rc = read(fd, local_buffer, block_size);
        if (rc != block_size)
        {
            perror("read");
            exit(1);
        }

        // Copy the userspace buffer into the contiguous block of physical RAM
        memcpy(ptr, local_buffer, block_size);

        // Bump the pointer to where the next chunk will be stored
        ptr += block_size;

        // And keep track of how many bytes are left to load
        bytes_remaining -= block_size;
    }

    // Free up the local_buffer so we don't leak memory
    delete[] local_buffer;

    // We're done with the input file
    close(fd);
}
//=================================================================================================
