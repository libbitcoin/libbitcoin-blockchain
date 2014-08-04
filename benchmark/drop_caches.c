#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{        
    // Run this on binary after:
    // sudo chown root drop_caches
    // sudo chmod 4755 drop_caches
    setuid(0);
    clearenv();
    system("sync");
    system("echo 1 > /proc/sys/vm/drop_caches");
}

