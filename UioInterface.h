
//=================================================================================================
// UioInterface.h - Defines an interface to the Linux Userspace-I/O subsystem
//=================================================================================================
#pragma once
#include <string>

class UioInterface
{
public:

    // Initializes the Linux Userspace-I/O subsystem
    static int initialize(std::string device);
};
