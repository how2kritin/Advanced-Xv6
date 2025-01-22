#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

// Tester program to check within xv6 shell.
// Syscall-1 of Spec-1
// Reference: https://cs631.cs.usfca.edu/guides/adding-a-syscall-to-xv6

int
main(int argc, char *argv[])
{
    printf("Current read count is %d\n", getreadcount());
#ifdef FCFS
    printf("You chose FCFS!\n");
#elif defined(MLFQ)
    printf("You chose MLFQ!\n");
#elif defined(PBS)
    printf("You chose MLFQ!\n");
#else
    printf("You chose Default!\n");
#endif
    exit(0);
}