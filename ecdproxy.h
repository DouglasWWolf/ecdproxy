//==========================================================================================================
// ecdproxy.h - Defines a class that manages the ECD hardware
//==========================================================================================================
#pragma once
#include <string>
#include <vector>

class CECDProxy
{
public:

    // Call this once to read in the configuration file.  Can throw std::runtime_error
    void init(std::string filename);

    // Loads the bitstream into the master FPGA
    bool loadMasterBitstream();

    // If loading a bitstream fails, this will return an error message
    std::string getLoadError() {return loadError_;}

protected:
    
    // If loading a bitstream fails, the error will be stored here
    std::string loadError_;

    // These values are read in from the config file durint init()
    struct
    {
        std::string tmpDir;
        std::string vivado;
        std::vector<std::string> masterProgrammingScript;
        std::vector<std::string> slaveProgrammingScript;
    } config_;
    
};

