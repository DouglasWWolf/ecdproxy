//==========================================================================================================
// ecdproxy.cpp - Implements a class that manages the ECD hardware
//==========================================================================================================
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "ecdproxy.h"
#include "config_file.h"
using namespace std;


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
// shell() - Executes a shell command and returns the output as a vector of string
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
//=================================================================================================






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
// init() - Reads in configuration setting from the config file
//==========================================================================================================
void CECDProxy::init(string filename)
{
    CConfigFile cf;
    CConfigScript cs;

    // Read the configuration file and complain if we can't.
    if (!cf.read(filename, false)) throw runtime_error("Cant read file "+filename);

    // Fetch the name of the temporary directory
    cf.get("tmp_dir", &config_.tmpDir);

    // Fetch the name of the Vivado executable
    cf.get("vivado", &config_.vivado);

    // Fetch the TCL script that we will use to program the master bitstream
    cf.get_script_vector("master_programming_script", &config_.masterProgrammingScript);
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

    // Write the Vivado output to a file for later inspection
    writeStrVecToFile(result, resultFilename);

    // Loop through each line of the Vivado output
    for (auto& s : result)
    {
        // Extract the first word from the line
        std::string firstWord = s.substr(0, s.find(" ")) ;     

        // If the first word is "ERROR:", save this line as an error message
        if (firstWord == "ERROR:" && loadError_.empty()) loadError_ = s;

    }

    // Tell the caller whether or not an error occured
    return (loadError_.empty());
}
//==========================================================================================================
