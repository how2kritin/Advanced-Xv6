# Copy On Write (COW)

## References: 
https://pdos.csail.mit.edu/6.828/2023/labs/cow.html

## Hints:
Here's a reasonable plan of attack.

    
    Modify uvmcopy() to map the parent's physical pages into the child, instead of allocating new pages. Clear PTE_W in the PTEs of both child and parent for pages that have PTE_W set.
    Modify usertrap() to recognize page faults. When a write page-fault occurs on a COW page that was originally writeable, allocate a new page with kalloc(), copy the old page to the new page, and install the new page in the PTE with PTE_W set. Pages that were originally read-only (not mapped PTE_W, like pages in the text segment) should remain read-only and shared between parent and child; a process that tries to write such a page should be killed.
    Ensure that each physical page is freed when the last PTE reference to it goes away -- but not before. A good way to do this is to keep, for each physical page, a "reference count" of the number of user page tables that refer to that page. Set a page's reference count to one when kalloc() allocates it. Increment a page's reference count when fork causes a child to share the page, and decrement a page's count each time any process drops the page from its page table. kfree() should only place a page back on the free list if its reference count is zero. It's OK to to keep these counts in a fixed-size array of integers. You'll have to work out a scheme for how to index the array and how to choose its size. For example, you could index the array with the page's physical address divided by 4096, and give the array a number of elements equal to highest physical address of any page placed on the free list by kinit() in kalloc.c. Feel free to modify kalloc.c (e.g., kalloc() and kfree()) to maintain the reference counts.
    Modify copyout() to use the same scheme as page faults when it encounters a COW page. 

Some hints:

    It may be useful to have a way to record, for each PTE, whether it is a COW mapping. You can use the RSW (reserved for software) bits in the RISC-V PTE for this.
    usertests -q explores scenarios that cowtest does not test, so don't forget to check that all tests pass for both.
    Some helpful macros and definitions for page table flags are at the end of kernel/riscv.h.
    If a COW page fault occurs and there's no free memory, the process should be killed. 

## Notes to self:
1. uvmcopy() is in vm.c.
2. At the bottom of riscv.h, there's PTE flags. I disabled PTE_W for both parent and child in uvmcopy().
3. usertrap() is in trap.c. Found out when a pagefault occurs, by looking at https://pdos.csail.mit.edu/6.828/2019/lec/l-usingvm.pdf slide 6.
4. Implementing reference counts in kalloc.c. making sure to lock this global variable beforehand, so that no concurrency issues arise.
5. According to page 33 of xv6 book, in a PTE, bits 8, 9 and 10 are RSW (reserved for software). I will be using the 8th bit to mark COW flag.
6. when r_scause() is 15, it implies that we ran into a page fault. I noticed this, by observing the usertrap(): unexpected scause error by intentionally causing a page fault.
7. copyout() is also in vm.c.
8. all new functions for reference counts in kalloc.c have been defined in defs.h.
9. Flushing TLB after making any changes to pagetable (in uvmcopy(), copyout() and usertrap()) with sfence_vma(). Found in page 36 of the xv6 book.

---

# PBS

## Notes to self:
1. Check PBS doubts doc Q15, 16 and 17.