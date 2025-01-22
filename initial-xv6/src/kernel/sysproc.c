#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_waitx(void)
{
  uint64 addr, addr1, addr2;
  uint wtime, rtime;
  argaddr(0, &addr);
  argaddr(1, &addr1); // user virtual memory
  argaddr(2, &addr2);
  int ret = waitx(addr, &wtime, &rtime);
  struct proc *p = myproc();
  if (copyout(p->pagetable, addr1, (char *)&wtime, sizeof(int)) < 0)
    return -1;
  if (copyout(p->pagetable, addr2, (char *)&rtime, sizeof(int)) < 0)
    return -1;
  return ret;
}

uint64 // Syscall-2 of Spec-1
sys_sigalarm(void)
{
    // These variables store the values passed to the function. Variables defined as such in proc.h.
    int maxTicks;
    uint64 fn;

    argint(0, &maxTicks); // Retrieve the 0th function argument from the kernel trap. As mentioned in section 4.4 of the xv6-riscv manual.
    argaddr(1, &fn); // Retrieve the 1st function argument from the kernel trap. As mentioned in section 4.4 of the xv6-riscv manual.

    // Store these values in process struct.
    myproc()->maxTicks = maxTicks;
    myproc()->handlerfn = fn;
    myproc()->turnAlarmOn = 1;

    return 0;
}

uint64 // Syscall-2 of Spec-1
sys_sigreturn(void)
{
    memmove(myproc()->trapframe, myproc()->alarmCache, PGSIZE);
    kfree(myproc()->alarmCache);
    myproc()->alarmCache = 0; // The equivalent of setting it back to NULL lol.
    // Basically, restore values from process struct, reset the calledOnce (to let it be called again, but only AFTER sigreturn has been processed. NOT BEFORE!).
    myproc()->currTicks = 0;   // Reset the tick count, so that the previous tick count won't persist, causing the alarm to be called immediately in succession after a sigreturn.
    // NOTE: Don't set turnAlarmOn to 0 here! If you do, then the handler will be called EXACTLY once. That is NOT what sigalarm is supposed to do. Just don't set it to 0 at all!
    return myproc()->trapframe->a0; // Had to do this to pass test3. a0 is the register that stores the return values. (4.3 of xv6-riscv book).
}

uint64 // Modified PBS of Mini Project - 3 of Spec-1
sys_set_priority(void)
{
    // Get the values of the arguments passed to the function.
    int pid, new_priority;
    argint(0, &pid);
    argint(1, &new_priority);

    return modifyPriority(pid, new_priority); // This function can be found in proc.c, and it's declaration in defs.h.
}