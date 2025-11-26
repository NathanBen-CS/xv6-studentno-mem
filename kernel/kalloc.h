#include "types.h"   // <--- defines 'uint'


struct memstats {
    uint student_number;
    uint strategy;
    uint allocated_blocks;
    uint total_allocated_bytes;
    uint free_blocks;
};