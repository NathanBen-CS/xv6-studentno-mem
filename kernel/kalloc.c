// kernel/kalloc.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h"

extern char end[];

struct run { struct run *next; };

struct { struct spinlock lock; struct run *freelist; } kmem;

// ========================
// XV6 PAGE ALLOCATOR
// ========================

void freerange(void *pa_start, void *pa_end); // <-- add this

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

// ========================
// STUDENT ALLOCATOR FOR 220227286
// ========================

#define STUDENT_NUMBER       220227286U
#define STUDENT_MAGIC        86U
#define DEFAULT_BLOCK_SIZE   768U
#define ALIGNED_BYTES        4U
#define ALLOC_STRATEGY       2U  // worst-fit
#define FREE_LIST_TARGET     19U

struct block_header {
    uint size;             // payload bytes
    uint magic;
    uint allocated;        // 0 free, 1 allocated
    struct block_header *next;
};

static struct block_header *free_list_head = 0;
struct spinlock student_lock;
static uint stat_allocated_blocks = 0;
static uint stat_total_allocated_bytes = 0;
static uint stat_free_blocks = 0;

static inline uint align_up(uint n, uint align) {
    return (n + (align-1)) & ~(align-1);
}

static inline void* header_to_payload(struct block_header *h) {
    return (void*)((char*)h + sizeof(struct block_header));
}

static inline struct block_header* payload_to_header(void *p) {
    return (struct block_header*)((char*)p - sizeof(struct block_header));
}

// add new page as one free block
static void student_add_page(void *pg) {
    struct block_header *h = (struct block_header*)pg;
    h->size = PGSIZE - sizeof(struct block_header);
    h->magic = STUDENT_MAGIC;
    h->allocated = 0;
    acquire(&student_lock);
    h->next = free_list_head;
    free_list_head = h;
    stat_free_blocks++;
    release(&student_lock);
}

// find worst-fit block (largest that fits)
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

// split block if large enough
static void split_block(struct block_header *b, uint size) {
    uint min_remain = sizeof(struct block_header) + ALIGNED_BYTES;
    if (b->size >= size + min_remain) {
        struct block_header *rem = 
            (struct block_header*)((char*)b + sizeof(struct block_header) + size);
        rem->size = b->size - size - sizeof(struct block_header);
        rem->magic = STUDENT_MAGIC;
        rem->allocated = 0;
        rem->next = b->next;
        b->size = size;
        b->next = rem;
        stat_free_blocks++;
    }
}

void student_init() {
    initlock(&student_lock, "student_alloc");
    void *pg = kalloc();
    if (!pg) { free_list_head=0; return; }
    student_add_page(pg);
}

void* student_malloc(uint size) {
    if (size == 0) return 0;
    size = align_up(size, ALIGNED_BYTES);

    acquire(&student_lock);
    struct block_header **pp = find_worst_fit_ptr(size);
    if (!pp) {
        release(&student_lock);
        void *pg = kalloc();
        if (!pg) return 0;
        student_add_page(pg);
        return student_malloc(size);
    }

    struct block_header *b = *pp;
    split_block(b, size);
    // remove from free list if fully allocated
    if (b->allocated == 0) {
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

void student_free(void *ptr) {
    if (!ptr) return;
    struct block_header *b = payload_to_header(ptr);
    if (b->magic != STUDENT_MAGIC) panic("student_free: invalid block");

    acquire(&student_lock);
    if (!b->allocated) panic("student_free: double free");

    b->allocated = 0;
    b->next = free_list_head;
    free_list_head = b;
    stat_allocated_blocks--;
    stat_total_allocated_bytes -= b->size;
    stat_free_blocks++;
    release(&student_lock);
}

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
