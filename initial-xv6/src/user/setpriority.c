#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
#ifndef PBS
    printf("\e[0;31m" "Invalid operation for this scheduler type!\n"
                      "This operation is available only for PBS.\n" "\e[0m");
#else
    if(argc != 3) {
        printf("\e[0;31m" "Invalid arguments.\n" "\e[0;36m" "Usage: setpriority pid priority\n" "\e[0m");
        exit(0);
    }
    int pid = atoi(argv[1]), priority = atoi(argv[2]);
    if(priority < 0 || priority > 100) {
        printf("\e[0;31m" "Invalid range for static priority. Please enter a value between 0 and 100.\n" "\e[0m");
        exit(0);
    }
    int retVal = set_priority(pid, priority);
    if(retVal == -1) printf("\e[0;31m" "A process with the pid %d doesn't exist!\n" "\e[0m", pid);
    else printf("\e[0;32m" "Static priority of process with pid %d successfully changed.\n"
                      "Old static priority of this process was %d.\n" "\e[0m", pid, retVal);
#endif
    exit(0);
}