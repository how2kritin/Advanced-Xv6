// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// Mini Project - 3 xv6 Spec-2
struct {
    struct spinlock refLock;
    int refCounts[PHYSTOP/4096 + 1];
} refs;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
    initlock(&refs.refLock, "refs");
    acquire(&refs.refLock);
    for(uint64 i = 0; i < PHYSTOP/4096 + 1; i++) refs.refCounts[i] = 0;
    release(&refs.refLock);

  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
      incrRef(p); // Increase reference first before freeing, so that initially it doesn't panic.
      kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// Mini Project - 3 xv6 Spec - 2.
void
kfree(void *pa)
{
    decrRef(pa); // Instead of *actually* free-ing the page, decrease references to the page.
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
      memset((char *) r, 5, PGSIZE); // fill with junk
      incrRef((void*)r); // Add 1 reference to this page, basically, initialise it to 1.
  }
  return (void*)r;
}

// ---------------------

// Mini Project - 3 xv6 Spec-2
// Implementing reference counts for pages.

// Basically, first, I need to store the reference counts somewhere, and it must be unique for each page represented by a pa.
// Idea (from MIT PDOS 2023 COW lab): You could index the array with the page's physical address divided by PGSIZE,
// and give the array a number of elements equal to highest physical address of any page placed on the free list by kinit() in kalloc.c

// Increment reference count:
void incrRef(void* pa) {
    acquire(&refs.refLock);
    refs.refCounts[(uint64)pa/PGSIZE]++;
    release(&refs.refLock);
}

// Decrement reference count:
void decrRef(void* pa) {
    acquire(&refs.refLock);
    if(refs.refCounts[(uint64)pa/PGSIZE] <= 0) panic("decrRef -> attempted to decrease number of references to a non-existent/free'd page!");
    if((--refs.refCounts[(uint64)pa/PGSIZE]) == 0) { // If number of references hit 0, free that page.
        // Old kfree code below:
        struct run *r;

        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
            panic("decrRef -> kfree");

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run*)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    }
    release(&refs.refLock);
}

// ---------------------
