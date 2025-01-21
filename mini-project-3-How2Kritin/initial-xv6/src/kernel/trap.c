#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8)
  {
    // system call

    if (killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  }
  else if ((which_dev = devintr()) != 0)
  {
      // ok
  }
  // when r_scause() is 15, it implies that we ran into a page fault. Found this in the riscv error codes at https://pdos.csail.mit.edu/6.828/2019/lec/l-usingvm.pdf slide 6.
  else if (r_scause() == 15)
  {
      uint64 va;
      pte_t *pte;
      va = PGROUNDDOWN(r_stval());
      if (va >= MAXVA) {
          p->killed = 1;
          goto skipRest;
      }
      if((pte = walk(p->pagetable, va, 0)) == 0) { // To get the pte address. Figured out from old uvmcopy, and bottom of riscv.h for PGROUNDOWN.
          p->killed = 1;
          goto skipRest;
      }
      if((*pte & PTE_V) == 0) {
          p->killed = 1;
          goto skipRest;
      }
      if(*pte & PTE_COW) { // Do the following, ONLY if it is a copy-on-write page.
          uint64 pa;
          uint flags;
          char *mem;

          pa = PTE2PA(*pte);
          if(pa == 0){
              p->killed = 1;
              goto skipRest;
          }
          flags = PTE_FLAGS(*pte);

          // Change the flags of only the final page, not that of parent!
          flags |= PTE_W;
          flags &= ~PTE_COW;

//          printf("unset cow pa: %p; flags: %d %d %d %d\n", PTE2PA(*pte), !!(flags & PTE_COW), !!(flags & PTE_W), !!(flags & PTE_R), !!(flags & PTE_X));
          mem = kalloc();
          if(mem == 0){
              p->killed = 1;
              goto skipRest;
          }
          memmove(mem, (void *) pa, PGSIZE);
          *pte = PA2PTE(mem) | flags;
          kfree((void *) pa);
          sfence_vma(); // Flush the TLB after changing *anything* in the page table. Found at page 36 of xv6 book.
      }
      else p->killed = 1; // Else, a STORE pagefault should've never occurred in the first place. Kill this process.
      skipRest:
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

    // Syscall-2 of Spec-1
    if(which_dev == 2 && p->turnAlarmOn){ // For a timer interrupt, turn alarm on.
        p->currTicks++;
        // "==" will prevent re-entrant alarm calls, as this process will be called exactly once; when the required tick count is equal.
        if(p->currTicks == p->maxTicks){ // Call the handler and cache most recent state, ONLY WHEN YOU HIT THE REQUIRED COUNT! Don't just waste memory and time willy-nilly by caching it EVERY. SINGLE. TIME.
            p->alarmCache = kalloc(); // alloc only when required.
            memmove(p->alarmCache, p->trapframe, PGSIZE);
            p->trapframe->epc = p->handlerfn; // Set the program counter to the handler function's address.
        }
    }

    // Modified PBS of Mini Project - 3
#ifdef PBS
    if(which_dev == 2){
        // Every tick, iterate through the process list and increment running time, waiting time and sleeping time.
        for (p = proc; p < &proc[NPROC]; p++)
        {
          acquire(&p->lock);
          if(p->state == RUNNING) p->rTime++;
          else if(p->state == SLEEPING) p->sTime++;
          else if(p->state == RUNNABLE) p->wTime++;
          release(&p->lock);
        }
    }
#endif

  if (killed(p))
    exit(-1);

  // Spec-2 of MP2 and Mini Project - 3 Modified PBS
#if defined(DEFAULT) || defined(MLFQ) || defined(PBS) // Basically, only for MLFQ, DEFAULT (RR) and PBS, we must yield when clock timer hits. Else, don't bother!
  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();
#endif

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

    // Spec-2 of MP2 and Mini Project - 3 Modified PBS
#if defined(DEFAULT) || defined(MLFQ) || defined(PBS) // Basically, only for MLFQ, DEFAULT (RR) and PBS, we must yield when clock timer hits. Else, don't bother!
  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();
#endif
    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
}

void clockintr()
{
  acquire(&tickslock);
  ticks++;
  update_time();
  // for (struct proc *p = proc; p < &proc[NPROC]; p++)
  // {
  //   acquire(&p->lock);
  //   if (p->state == RUNNING)
  //   {
  //     printf("here");
  //     p->rtime++;
  //   }
  //   // if (p->state == SLEEPING)
  //   // {
  //   //   p->wtime++;
  //   // }
  //   release(&p->lock);
  // }
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  }
  else
  {
    return 0;
  }
}
