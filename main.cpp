#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include "ecdproxy.h"

const uint64_t PPB0 = 0x100000000;
const uint64_t PPB1 = 0x140000000;
const uint32_t PPB_BLOCKS = 16;

uint8_t* physMem;

using namespace std;
uint8_t* mapPhysMem(uint64_t physAddr, size_t size);
void    fillBuffer(int which, uint32_t row);

class ECD : public CECDProxy
{
    void onInterrupt(int irq, uint64_t irqCounter);
} proxy;


void execute()
{
    cout << "Mapping physical RAM\n";
    physMem = mapPhysMem(0x100000000, 0x100000000);
    fillBuffer(0, 0);
    fillBuffer(1, 0);

    cout << "Initializing ECDProxy\n";
    proxy.init("ecd_proxy.conf");

//    cout << "Loading bitstream \n";    
//    bool ok = proxy.loadMasterBitstream();
//    if (!ok) printf("%s\n", proxy.getLoadError().c_str());

    proxy.startPCI();

    string version = proxy.getMasterBitstreamVersion();
    cout << "Version is " << version << "\n";

    string date = proxy.getMasterBitstreamDate();
    cout << "Date: " << date << "\n";

    proxy.prepareDataTransfer(PPB0, PPB1, PPB_BLOCKS);

    cout << "Waiting for interrupts\n";
    while(1) sleep(999999);

}

int main()
{
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
    if (ptr == MAP_FAILED) throw std::runtime_error("mmap failed");

   // Hand the user-space pointer to the caller
   return (uint8_t*) ptr;
}
//=================================================================================================



void fillBuffer(int which, uint32_t row)
{
    uint8_t marker = (which == 0 ? 0x00 : 0x80);
    uint64_t memOffset = (which == 0) ? 0 : (PPB1 - PPB0);

    uint32_t* ptr = (uint32_t*) (physMem + memOffset);

    for (int block = 0; block < PPB_BLOCKS; ++block)
    {
        uint8_t row_h = (row >> 8) & 0xFF;
        uint8_t row_l = (row     ) & 0xFF;

        for (int cycle = 0; cycle < 64; ++cycle)
        {
            //printf("IRQ %i, Block %i, cycle %i\n", which, block, cycle);
            uint32_t value = (cycle << 24) | (row_l << 16) | (row_h << 8) | marker;
            for (int index=0; index <8; ++index) *ptr++ = value;
        }

        ++row;
    }
}

uint32_t ppbRow[2];

void ECD::onInterrupt(int irq, uint64_t irqCounter)
{
    printf("Servicing IRQ %i, #%lu\n", irq, irqCounter);
    ppbRow[irq] += PPB_BLOCKS;
    fillBuffer(irq, ppbRow[irq]);
    notifyBufferFull(irq);
}