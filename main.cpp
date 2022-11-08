#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "ecdproxy.h"

using namespace std;

CECDProxy proxy;

void execute()
{

    proxy.init("ecd_proxy.conf");


    cout << "Loading bitstream \n";
    
    bool ok = proxy.loadMasterBitstream();
    if (!ok) printf("%s\n", proxy.getLoadError().c_str());

    proxy.startPCI();

    string version = proxy.getMasterBitstreamVersion();
    cout << "Version is " << version << "\n";

    string date = proxy.getMasterBitstreamDate();
    cout << "Date: " << date << "\n";

    cout << "Waiting for interrupts\n";
    while(1) sleep(999999);

}

int main()
{
    printf("Hello Proxy!\n");

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
