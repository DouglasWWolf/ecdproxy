//==========================================================================================================
// ecdproxy.cpp - Implements a class that manages the ECD hardware
//==========================================================================================================
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <thread>
#include "ecdproxy.h"
#include "config_file.h"
#include "PciDevice.h"
#include "UioInterface.h"

// Header files for the various RTL modules
#include "RtlAxiRevision.h"
#include "RtlIrqManager.h"

// Haul in the entire std:: library
using namespace std;

// We need one interface to the PCI bus per executable
static PciDevice PCI;

// Interface to the Linux Userspace-I/O subsystem
static UioInterface UIO;

// We may eventually need a way to associate these with a particular CECDProxy object
static RtlAxiRevision AxiRevision;
static RtlIrqManager  AxiIrqManager;

//==========================================================================================================
// c() - Shorthand way of converting a std::string to a const char*
//==========================================================================================================
const char* c(string& s) {return s.c_str();}
//==========================================================================================================


//==========================================================================================================
// chomp() - Removes any carriage-return or linefeed from the end of a buffer
//==========================================================================================================
static void chomp(char* buffer)
{
    char* p;
    p = strchr(buffer, 10);
    if (p) *p = 0;
    p = strchr(buffer, 13);
    if (p) *p = 0;
}
//==========================================================================================================


//==========================================================================================================
// shell() - Executes a shell command and returns the output as a vector of strings
//==========================================================================================================
static vector<string> shell(const char* fmt, ...)
{
    vector<string> result;
    va_list        ap;
    char           command[1024];
    char           buffer[1024];

    // Format the command
    va_start(ap, fmt);
    vsprintf(command, fmt, ap);
    va_end(ap);

    // Run the command
    FILE* fp = popen(command, "r");

    // If we couldn't do that, give up
    if (fp == nullptr) return result;

    // Fetch every line of the output and push it to our vector of strings
    while (fgets(buffer, sizeof buffer, fp))
    {
        chomp(buffer);
        result.push_back(buffer);
    }

    // When the program finishes, close the FILE*
    fclose(fp);

    // And hand the output of the program (1 string per line) to the caller
    return result;
}
//==========================================================================================================






//==========================================================================================================
// writeStrVecToFile() - Helper function that writes a vector of strings to a file, with a linefeed 
//                       appended to the end of each line.
//==========================================================================================================
static bool writeStrVecToFile(vector<string>& v, string filename)
{
    // Create the output file
    FILE* ofile = fopen(c(filename), "w");
    
    // If we can't create the output file, whine to the caller
    if (ofile == nullptr) return false;    

    // Write each line in the vector to the output file
    for (string& s : v) fprintf(ofile, "%s\n", c(s));

    // We're done with the output file
    fclose(ofile);

    // And tell the caller that all is well
    return true;
}
//==========================================================================================================




//==========================================================================================================
// throwRuntime() - Throws a runtime exception
//==========================================================================================================
static void throwRuntime(const char* fmt, ...)
{
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    throw runtime_error(buffer);
}
//=========================================================================================================



//==========================================================================================================
// CECDProxy() - Default constructor
//==========================================================================================================
CECDProxy::CECDProxy()
{
    // We don't yet know the base addresses of any AXI slave modules
    memset(axiMap_, 0xFF, sizeof axiMap_);    

    // We don't yet know anything about the number of IRQs we monitor
    irqCount_ = 0;
}
//==========================================================================================================


//==========================================================================================================
// ~CECDProxy() - Destructor - deletes the FIFOs that we use to receive interrupts in userspace
//==========================================================================================================
CECDProxy::~CECDProxy() {}
//==========================================================================================================


//==========================================================================================================
// init() - Reads in configuration setting from the config file
//
// Exceptions: This can throw std::runtime_error
//==========================================================================================================
void CECDProxy::init(string filename)
{
    CConfigFile cf;
    CConfigScript cs;

    // If we're not running with root priveleges, give up
    if (geteuid() != 0) throw runtime_error("Must be root to run.  Use sudo.");

    // Read the configuration file and complain if we can't.
    if (!cf.read(filename, false)) throw runtime_error("Cant read file "+filename);

    // Fetch the name of the temporary directory
    cf.get("tmp_dir", &config_.tmpDir);

    // Fetch the name of the Vivado executable
    cf.get("vivado", &config_.vivado);

    // Fetch the PCI vendorID:deviceID of the ecd-master
    cf.get("pci_device", &config_.pciDevice);

    // Fetch the TCL script that we will use to program the master bitstream
    cf.get_script_vector("master_programming_script", &config_.masterProgrammingScript);

    // Fetch the map that gives the base address of various AXI slave modules
    cf.get("axi_map", &cs);

    // Loop through each entry in the AXI map
    while (cs.get_next_line())
    {
        // Fetch the name and base address from the line
        string name = cs.get_next_token();
        uint32_t address = cs.get_next_int();

        // If we're filling in the AXI address of the RTL revision...
        if (name == "master_revision")
        {
            axiMap_[AM_MASTER_REVISION] = address;
            continue;
        }

        // If we're filling in the AXI address of the interrupt manager...
        if (name == "irq_manager")
        {
            axiMap_[AM_IRQ_MANAGER] = address;
            continue;
        }

        // If we get here, we have an unknown device name
        throwRuntime("unknown AXI device '%s'", c(name));
    }

    // Make sure that every axiMap_ entry was defined
    for (int i=0; i<AM_MAX; ++i)
    {
        if (axiMap_[i] == 0xFFFFFFFF) throwRuntime("Missing axi_map definition for index %i", i);
    }
}
//==========================================================================================================



//==========================================================================================================
// loadMasterBitstream() - Uses a JTAG programmer to load a bistream into the master FPGA
//
// Returns: 'true' if the bitstream loaded succesfully, otherwise returns 'false'
//
// On Exit: The TCL script will be in "/tmp/load_ master_bistream.tcl"
//          The Vivado output will be in "/tmp/load_master_bitstream.result"
//          loadError_ will contain the text of any error during the load process
//==========================================================================================================
bool CECDProxy::loadMasterBitstream()
{
    // Assume for a moment that there won't be any errors
    loadError_ = "";

    // This is the filename of the TCL script we're going to generate
    string tclFilename = config_.tmpDir + "/load_master_bitstream.tcl";

    // This is the name of the file that will contain Vivado output from the load process
    string resultFilename = config_.tmpDir + "/load_master_bitstream.result";

    // Write the master-bitstream TCL script to disk
    if (!writeStrVecToFile(config_.masterProgrammingScript, tclFilename)) 
    {
        loadError_ = "Can't write "+tclFilename;
        return false;
    }

    // Use Vivado to load the bitstream into the FPGA via JTAG
    vector<string> result = shell("%s 2>&1 -nojournal -nolog -mode batch -source %s", c(config_.vivado), c(tclFilename));

    // If there was no output from that, it means we couldn't find Vivado
    if (result.empty())
    {
        loadError_ = "Can't run " + config_.vivado;
        return false;        
    }

    // Write the Vivado output to a file for later inspection
    writeStrVecToFile(result, resultFilename);

    // Loop through each line of the Vivado output
    for (auto& s : result)
    {
        // Extract the first word from the line
        std::string firstWord = s.substr(0, s.find(" "));     

        // If the first word is "ERROR:", save this line as an error message
        if (firstWord == "ERROR:" && loadError_.empty()) loadError_ = s;

    }

    // Tell the caller whether or not an error occured
    return (loadError_.empty());
}
//==========================================================================================================


//==========================================================================================================
// startPCI() - (1) Performs a PCI "hot_reset"
//              (2) Maps the memory-mapped PCI resource regions into user-space
//
// Exceptions: This can throw std::runtime_error
//==========================================================================================================
void CECDProxy::startPCI()
{
    // Perform a "PCIe Hot Reset" to get our resource regions mapped into the physical address space
    PCI.hotReset(config_.pciDevice);

    // Initialize the Linux Userspace-I/O subsystem
    int uioIndex = UIO.initialize(config_.pciDevice);

    // Map the memory-mapped resource regions into user-space
    PCI.open(config_.pciDevice);

    // Fetch the list of memory mapped resource regions
    auto& resource = PCI.resourceList();

    // Tell each of the RTL interfaces what their base address is
    AxiRevision  .setBaseAddress(resource[0].baseAddr + axiMap_[AM_MASTER_REVISION]);
    AxiIrqManager.setBaseAddress(resource[0].baseAddr + axiMap_[AM_IRQ_MANAGER    ]);

    // Spawn the thread that sits in a loop and waits for PCI interrupt notifications
    spawnTopLevelInterruptHandler(uioIndex);
}
//==========================================================================================================


//==========================================================================================================
// getMasterBitstreamVersion() - Returns the version string of the RTL design loaded into the ECD-master
//                               FPGA
//==========================================================================================================
string CECDProxy::getMasterBitstreamVersion()
{
    return AxiRevision.getVersion();
}
//==========================================================================================================


//==========================================================================================================
// getMasterBitstreamDate() - Returns the date string of the RTL design loaded into the ECD-master FPGA
//==========================================================================================================
string CECDProxy::getMasterBitstreamDate()
{
    return AxiRevision.getDate();
}
//==========================================================================================================



//==========================================================================================================
// spawnTopLevelInterruptHandler() - Launches "monitorInterrupts" in its own thread in order to wait for
//                                   incoming interrupts and distribute them to their handlers
//==========================================================================================================
void CECDProxy::spawnTopLevelInterruptHandler(int uioDevice)
{
    // If we haven't been initialized, don't do anything
    if (irqCount_ == 0) return;

    // Spawn "monitorInterrupts()" in its own thread
    thread thread(&CECDProxy::monitorInterrupts, this, uioDevice);

    // Let it keep running, even when "thread" goes out of scope
    thread.detach();
}
//==========================================================================================================




//=================================================================================================
// monitorInterrupts() - Sits in a loop reading interrupt notifications and distributing 
//                       notifications to the FIFOs that track each interrupt source
//=================================================================================================
void CECDProxy::monitorInterrupts(int uioDevice)
{
    int      uiofd;
    int      configfd;
    int      err;
    uint32_t interruptCount;
    uint8_t  commandHigh;
    char     filename[64];

    // Generate the filename of the psudeo-file that notifies us of interrupts
    sprintf(filename, "/dev/uio%d", uioDevice);

    // Open the psuedo-file that notifies us of interrupts
    uiofd = open(filename, O_RDONLY);
    if (uiofd < 0)
    {
        perror("uio open:");
        exit(1);
    }

    // Generate the filename of the PCI config-space psuedo-file
    sprintf(filename, "/sys/class/uio/uio%d/device/config", uioDevice);

    // Open the file that gives us access to the PCI device's confiuration space
    configfd = open(filename, O_RDWR);
    if (configfd < 0)
    {
        perror("config open:");
        exit(1);
    }

    // Fetch the upper byte of the PCI configuration space command word
    err = pread(configfd, &commandHigh, 1, 5);
    if (err != 1)
    {
        perror("command config read:");
        exit(1);
    }
    
    // Turn off the "Disable interrupts" flag
    commandHigh &= ~0x4;

    // Loop forever, monitoring incoming interrupt notifications
    while (true)
    {
        // Enable (or re-enable) interrupts
        err = pwrite(configfd, &commandHigh, 1, 5);
        if (err != 1)
        {
            perror("config write:");
            exit(1);
        }

        // Wait for notification that an interrupt has occured
        err = read(uiofd, &interruptCount, 4);
        if (err != 4)
        {
            perror("uio read:");
            exit(1);
        }

        // Fetch the bitmap of active interrupt sources
        uint32_t irqSources = AxiIrqManager.getActiveInterrupts();

        // If there are no interrupt sources, ignore this interrupt
        if (irqSources == 0) continue;

        // If we're in verbose mode, tell the world that an interrupt occured
        printf("Interrupt from sources 0x%08x\n", irqSources);

        // Clear the interrupts from those sources
        AxiIrqManager.clearInterrupts(irqSources);

        // Call the interrupt handler for each pending interrupt 
        for (int i=0; i<irqCount_; ++i)
        {
            if (irqSources & (1<<i)) onInterrupt(i);
        }
    }
}
//=================================================================================================


