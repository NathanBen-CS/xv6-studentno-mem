#include "types.h"   // <--- defines 'uint'

// ========================
// STUDENT ALLOCATOR FOR 220227286
// ========================

#define STUDENT_NUMBER       220227286U
#define STUDENT_MAGIC        86U
#define DEFAULT_BLOCK_SIZE   768U
#define ALIGNED_BYTES        4U
#define ALLOC_STRATEGY       2U  // worst-fit
#define FREE_LIST_TARGET     19U

struct memstats {
    uint student_number;
    uint strategy;
    uint allocated_blocks;
    uint total_allocated_bytes;
    uint free_blocks;
};

struct block_header {
    uint size;              // payload bytes
    uint magic;
    uint allocated;         // 0 free, 1 allocated
    struct block_header *next;
};