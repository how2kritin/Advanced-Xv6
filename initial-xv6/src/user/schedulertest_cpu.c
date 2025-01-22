#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"

#define NFORK 5

int main()
{
    int n, pid;
    int wtime, rtime;
    int twtime = 0, trtime = 0;
    for (n = 0; n < NFORK; n++) {
        pid = fork();
        if (pid < 0)
            break;
        if (pid == 0)
        {
#ifdef PBS
            int procPid = getpid();
          set_priority(procPid, 50 + n/2 + 1);
#endif
        for (volatile int i = 0; i < 1000000000; i++)
        {
        } // CPU bound process
        printf("Process %d finished\n", n);
        exit(0);
        }
    }
    for (; n > 0; n--)
    {
        if (waitx(0, &wtime, &rtime) >= 0)
        {
            trtime += rtime;
            twtime += wtime;
        }
    }
    printf("Average rtime %d,  wtime %d\n", trtime / NFORK, twtime / NFORK);
    exit(0);
}