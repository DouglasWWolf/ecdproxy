#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "ecdproxy.h"

CECDProxy proxy;
int main()
{
    printf("Hello Proxy!\n");

    proxy.init("ecd_proxy.conf");

    bool ok = proxy.loadMasterBitstream();

    if (!ok) printf("%s\n", proxy.getLoadError().c_str());

}
