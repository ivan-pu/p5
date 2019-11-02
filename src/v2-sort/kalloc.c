// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
void initialize(char *v);

extern char end[]; // first address after kernel loaded from ELF file
// defined by the kernel linker script in kernel.ld

struct run {
    struct run *next;
};

//struct that keep track of frame number usage
struct frameInfo {
    int pid;
    struct run *frameNum;
};

struct {
    struct spinlock lock;
    int use_lock;
    struct run *freelist;
} kmem;

struct frameInfo framelist[16384];

static void
set(struct run *framenum, int pid) {
    for (int i = 0; i < 16384; i++) {
        if (framelist[i].frameNum == framenum) {
            framelist[i].pid = pid;
            break;
        }
    }
}

static struct run *
nextfree(int pid) {
    if (framelist[0].pid == 0 && (framelist[1].pid == 0 || framelist[1].pid == pid)) {
        framelist[0].pid = pid;
        return framelist[0].frameNum;
    }
    for (int i = 1; i < 16384; i++) {
        if (pid == -2 && !framelist[i].pid) {
            framelist[i].pid = pid;
            return framelist[i].frameNum;
        }
        else if ((framelist[i - 1].pid == pid || framelist[i - 1].pid == 0 || framelist[i - 1].pid == -2)
            && (framelist[i + 1].pid == pid || framelist[i + 1].pid == 0 || framelist[i + 1].pid == -2)
            && !framelist[i].pid) {
            framelist[i].pid = pid;
            return framelist[i].frameNum;
        }
    }
    return 0;
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void kinit1(void *vstart, void *vend) {
    initlock(&kmem.lock, "kmem");
    kmem.use_lock = 0;
    freerange(vstart, vend);
}

void kinit2(void *vstart, void *vend) {
    freerange(vstart, vend);
    struct run *temp = kmem.freelist;
    for (int i = 0; i < 16384; i++) {
        framelist[i].frameNum = temp;
        framelist[i].pid = 0;
        temp = temp->next;
    }
    kmem.use_lock = 1;
}

void freerange(void *vstart, void *vend) {
    char *p;
    p = (char *) PGROUNDUP((uint) vstart);
    for (; p + PGSIZE <= (char *) vend; p += PGSIZE) {
        initialize(p);
    }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(char *v) {

    if ((uint) v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);

    if (kmem.use_lock)
        acquire(&kmem.lock);

    set((struct run *) v, 0);
    if (kmem.use_lock)
        release(&kmem.lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void initialize(char *v) {
    struct run *r;

    if ((uint) v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);

    if (kmem.use_lock)
        acquire(&kmem.lock);
    r = (struct run *) v;
    r->next = kmem.freelist;
    kmem.freelist = r;
    if (kmem.use_lock)
        release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char *
kalloc(void) {
    struct run *r;

    if (kmem.use_lock)
        acquire(&kmem.lock);
    if (!kmem.use_lock){
        r = kmem.freelist;
        if (r) {
            kmem.freelist = r->next;
        }
    }
    else r = nextfree(-2);
    if (kmem.use_lock)
        release(&kmem.lock);
    return (char *) r;
}

char *
kalloc2(int pid) {
    struct run *r;

    if (kmem.use_lock)
        acquire(&kmem.lock);
    if (!kmem.use_lock){
        r = kmem.freelist;
        if (r) {
            kmem.freelist = r->next;
        }
    }
    else r = nextfree(pid);
    if (kmem.use_lock)
        release(&kmem.lock);
    return (char *) r;
}

int dumpMem(int *frames, int *pids, int numframes) {
    if (!frames || !pids || !numframes)
        return -1;
    if (kmem.use_lock)
        acquire(&kmem.lock);
    int count = 0;
    for (int i = 0; i < 16384; i++) {
        if (framelist[i].pid != 0) {
            frames[count] = (V2P(framelist[i].frameNum) >> 12) & 0xffff;
            pids[count++] = framelist[i].pid;
        }
        if (count == numframes) break;
    }
    if (kmem.use_lock)
        release(&kmem.lock);

    return 0;
}

//static void
//remove(int framenum) {
//    for (int i = 0; i < 16384; i++) {
//        if (framenum == framelist[i].frameNum) {
//            for (int j = i; framelist[j].frameNum != 0; j++) {
//                framelist[j] = framelist[j + 1];
//            }
//            endindex--;
//            break;
//        }
//    }
//}
//
//static void
//set(int framenum, int pid) {
//    if (endindex >= 16384) return;
//    for (int i = 0; i < 16384; i++) {
//        if (framelist[i].frameNum == 0) {
//            framelist[i].frameNum = framenum;
//            framelist[i].pid = pid;
//            endindex++;
//            break;
//        }
//        if (framenum > framelist[i].frameNum) {
//            for (int j = endindex; j > i; j--) {
//                framelist[j] = framelist[j - 1];
//            }
//            framelist[i].frameNum = framenum;
//            framelist[i].pid = pid;
//            endindex++;
//            break;
//        }
//    }
//}