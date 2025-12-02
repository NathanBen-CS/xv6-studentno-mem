// kernel/kalloc.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h"     // contains STUDENT_*, DEFAULT_BLOCK_SIZE, ALIGNED_BYTES, FREE_LIST_TARGET, struct block_header, struct memstats

extern char end[];

struct run { struct run *next; };
struct { struct spinlock lock; struct run *freelist; } kmem;

// ---------- xv6 page allocator (unchanged) ----------
void freerange(void *pa_start, void *pa_end);

void kinit(void) {
    initlock(&kmem.lock, "kmem");
    freerange(end, (void*)PHYSTOP);
    student_init();
}

void freerange(void *pa_start, void *pa_end) {
    char *p = (char*)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
        kfree(p);
}

void kfree(void *pa) {
    struct run *r;
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

void *kalloc(void) {
    struct run *r;
    acquire(&kmem.lock);
    r = kmem.freelist;
    if(r) kmem.freelist = r->next;
    release(&kmem.lock);
    if(r) memset((char*)r, 5, PGSIZE);
    return (void*)r;
}

// ---------- student allocator globals ----------
static struct block_header *free_list_head = 0; // head of free blocks (may be unsorted)
struct spinlock student_lock;
static uint stat_allocated_blocks = 0;
static uint stat_total_allocated_bytes = 0;
static uint stat_free_blocks = 0;

// ---------- small helpers ----------
static inline uint align_up(uint n, uint align) {
    return (n + (align-1)) & ~(align-1);
}

static inline void *header_to_payload(struct block_header *h) {
    return (void *)((char*)h + sizeof(struct block_header));
}

static inline struct block_header *payload_to_header(void *p) {
    return (struct block_header *)((char*)p - sizeof(struct block_header));
}

// ---------- split a free block b into [b:size][rem:rest] if possible ----------
// b must be a free block currently in the free list
static void split_block(struct block_header *b, uint size) {
    uint min_remain = sizeof(struct block_header) + ALIGNED_BYTES;
    if (b->size < size + min_remain) return; // too small to split

    // candidate remainder pointer (right after the payload of new b)
    char *rem_ptr = (char*)b + sizeof(struct block_header) + size;

    // ensure remainder stays within b's original page (prevent crossing page boundary)
    uint64 b_addr = (uint64)b;
    uint64 page_start = b_addr & ~(PGSIZE - 1);
    uint64 page_end   = page_start + PGSIZE;
    if ((uint64)rem_ptr + sizeof(struct block_header) > page_end) {
        // remainder would cross page boundary — don't split
        return;
    }

    struct block_header *rem = (struct block_header*)rem_ptr;
    rem->size = b->size - size - sizeof(struct block_header);
    rem->magic = STUDENT_MAGIC;
    rem->allocated = 0;

    // insert rem into b's linked position
    rem->next = b->next;
    b->size = size;
    b->next = rem;

    stat_free_blocks++;
}

// ---------- insert block b into free list in address order and coalesce with neighbors ----------
static void insert_and_coalesce(struct block_header *b) {
    struct block_header *curr = free_list_head;
    struct block_header *prev = 0;

    // find insertion point (sorted by address)
    while (curr && (uint64)curr < (uint64)b) {
        prev = curr;
        curr = curr->next;
    }

    // insert b
    b->next = curr;
    if (prev) prev->next = b;
    else free_list_head = b;

    stat_free_blocks++;

    // try merge with next
    if (b->next) {
        uint64 b_end = (uint64)header_to_payload(b) + b->size;
        if (b_end == (uint64)b->next) {
            struct block_header *n = b->next;
            b->size += sizeof(struct block_header) + n->size;
            b->next = n->next;
            stat_free_blocks--;
        }
    }

    // try merge with previous
    if (prev) {
        uint64 prev_end = (uint64)header_to_payload(prev) + prev->size;
        if (prev_end == (uint64)b) {
            prev->size += sizeof(struct block_header) + b->size;
            prev->next = b->next;
            stat_free_blocks--;
        }
    }
}

// ---------- full_coalesce: sort free list by address then merge all adjacent blocks ----------
static void full_coalesce(void) {
    if (!free_list_head) return;

    // sort (insertion sort) into 'sorted'
    struct block_header *sorted = 0;
    struct block_header *curr = free_list_head;
    while (curr) {
        struct block_header *next = curr->next;
        struct block_header **pp = &sorted;
        while (*pp && (uint64)(*pp) < (uint64)curr)
            pp = &(*pp)->next;
        curr->next = *pp;
        *pp = curr;
        curr = next;
    }
    free_list_head = sorted;

    // merge pass
    curr = free_list_head;
    while (curr && curr->next) {
        uint64 curr_end = (uint64)header_to_payload(curr) + curr->size;
        struct block_header *n = curr->next;
        if (curr_end == (uint64)n) {
            curr->size += sizeof(struct block_header) + n->size;
            curr->next = n->next;
            stat_free_blocks--;
        } else {
            curr = curr->next;
        }
    }
}

// ---------- find worst-fit block pointer-to-pointer (returns &pointer) ----------
static struct block_header **find_worst_fit_ptr(uint need) {
    struct block_header **pp = &free_list_head;
    struct block_header **best = 0;
    uint best_size = 0;
    while (*pp) {
        if (!(*pp)->allocated && (*pp)->size >= need) {
            if ((*pp)->size > best_size) {
                best_size = (*pp)->size;
                best = pp;
            }
        }
        pp = &(*pp)->next;
    }
    return best;
}

// ---------- partition a new physical page into DEFAULT_BLOCK_SIZE blocks and insert safely ----------
static void student_add_page(void *pg) {
    uint64 p = (uint64)pg;
    uint total = PGSIZE;
    uint block_total = sizeof(struct block_header) + DEFAULT_BLOCK_SIZE;
    int blocks_per_page = total / block_total;

    for (int i = 0; i < blocks_per_page; i++) {
        struct block_header *b = (struct block_header*)p;
        b->size = DEFAULT_BLOCK_SIZE;
        b->magic = STUDENT_MAGIC;
        b->allocated = 0;
        b->next = 0;

        // insert sorted + coalesce so adjacent blocks in this page merge if possible
        insert_and_coalesce(b);

        p += block_total;
    }
}

// ---------- initialization ----------
void student_init() {
    initlock(&student_lock, "student_alloc");

    // seed allocator with one page
    void *pg = kalloc();
    if (!pg) { free_list_head = 0; return; }
    student_add_page(pg);
}

// ---------- allocate ----------
void* student_malloc(uint size) {
    if (size == 0) return 0;
    size = align_up(size, ALIGNED_BYTES);

    acquire(&student_lock);

    // try worst-fit
    struct block_header **pp = find_worst_fit_ptr(size);

    // try heavy defrag before giving up
    if (!pp) {
        full_coalesce();
        pp = find_worst_fit_ptr(size);
    }

    // still none: try adding a small number of pages (bounded) to satisfy request
    if (!pp) {
        int pages_tried = 0;
        while (!pp && pages_tried < 4) {
            void *pg = kalloc();
            if (!pg) break;
            student_add_page(pg);
            pages_tried++;
            pp = find_worst_fit_ptr(size);
        }
    }

    // allocation failure
    if (!pp) {
        release(&student_lock);
        return 0;
    }

    struct block_header *b = *pp;

    // attempt to split the block (safe: won't cross page boundary)
    split_block(b, size);

    // remove block (or its head) from free list if it's the one to allocate
    if (!b->allocated) {
        *pp = b->next;
        stat_free_blocks--;
    }

    b->allocated = 1;
    b->next = 0;
    stat_allocated_blocks++;
    stat_total_allocated_bytes += b->size;

    release(&student_lock);
    return header_to_payload(b);
}

// ---------- free ----------
void student_free(void *ptr) {
    if (!ptr) return;

    struct block_header *b = payload_to_header(ptr);
    if (b->magic != STUDENT_MAGIC) panic("student_free: invalid block");

    acquire(&student_lock);

    if (!b->allocated) {
        release(&student_lock);
        panic("student_free: double free");
    }

    b->allocated = 0;
    stat_allocated_blocks--;
    if (stat_total_allocated_bytes >= b->size)
        stat_total_allocated_bytes -= b->size;
    else
        stat_total_allocated_bytes = 0;

    // insert sorted and attempt local coalescing
    insert_and_coalesce(b);

    // if fragmentation high, do a full defrag
    if (stat_free_blocks > FREE_LIST_TARGET - 4) {
        full_coalesce();
    }

    release(&student_lock);
}

// ---------- stats ----------
int student_stats(struct memstats *out) {
    if (!out) return -1;
    acquire(&student_lock);
    out->student_number = STUDENT_NUMBER;
    out->strategy = ALLOC_STRATEGY;
    out->allocated_blocks = stat_allocated_blocks;
    out->total_allocated_bytes = stat_total_allocated_bytes;
    out->free_blocks = stat_free_blocks;
    release(&student_lock);
    return 0;
}
