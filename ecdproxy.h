//==========================================================================================================
// ecdproxy.h - Defines a class that manages the ECD hardware
//
// General flow of operations:
//
//   [1] init()                - To read the configuration file
//   [2] loadMasterBitstream() - To load the bitstream into the FPGA
//   [3] startPCI()            - Initialize the PCI subsystem
//==========================================================================================================
#pragma once
#include <string>
#include <vector>
#include <map>

class CECDProxy
{
public:

    // Default constructor
    CECDProxy();

    // Call this once to read in the configuration file.  Can throw std::runtime_error
    void init(std::string filename);

    // Loads the bitstream into the master FPGA
    bool loadMasterBitstream();

    // If loading a bitstream fails, this will return an error message
    std::string getLoadError() {return loadError_;}

    // Perform a PCI hot-reset and map PCI resource regions into userspace.
    // Can throw std:runtime_error
    void startPCI();

    // Call these to fetch the version number or date of the master bitstream
    // after it has been loaded
    std::string getMasterBitstreamVersion();
    std::string getMasterBitstreamDate();

protected:

    // Maximum number of interrupt request sources we can support
    enum {MAX_IRQS = 32};

    // Creates the FIFOs that we use to receive PCI interrupts
    void createIntrFIFOs(std::string dir, int irqCount);

    // Cleans up (i.e., deletes) FIFOs created by "createIntrFIFOs"
    void cleanupIntrFIFOs();

    // Constains the path to the FIFOs we use to receive interrupt notifications
    std::string intrFifoPath_;

    // This is the number of distinct interrupt-request sources
    int irqCount_;

    // The numerically highest file descriptor for our interrupt FIFOs
    int highestIntrFD_;

    // One file descriptor per interrupt request source
    int intrFD_[MAX_IRQS];

    // If loading a bitstream fails, the error will be stored here
    std::string loadError_;

    enum
    {   
        AM_MASTER_REVISION,
        AM_INT_MANAGER,
        AM_MAX
    };

    // One address per AM_xxxx constant
    uint32_t axiMap_[AM_MAX];

    // These values are read in from the config file durint init()
    struct
    {
        std::string tmpDir;
        std::string vivado;
        std::string pciDevice;
        std::vector<std::string> masterProgrammingScript;
        std::vector<std::string> slaveProgrammingScript;

    } config_;
    
};

