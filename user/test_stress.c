#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/kalloc.h"

#define NUM_ITER 100
#define MAX_BLOCK_SIZE 768

int main(int argc, char *argv[]) {
    printf("=== Student Allocator Stress Test ===\n");

    struct memstats stats;
    void *blocks[NUM_ITER];

    // Initialize all pointers to NULL
    for (int i = 0; i < NUM_ITER; i++) {
        blocks[i] = 0;
    }

    // Perform allocations of varying sizes
    for (int i = 0; i < NUM_ITER; i++) {
        int size = MAX_BLOCK_SIZE - ((i * 100) % MAX_BLOCK_SIZE) + 1; // Sizes from 1 to MAX_BLOCK_SIZE
        if (i % 50 == 0) size = 0;           // Edge case: zero size
        blocks[i] = student_malloc(size);

        if (!blocks[i] && size != 0) {
            printf("Allocation failed at iteration %d, size %d\n", i, size);
        }
        else{
            printf("Allocated block %d: requested size %d, got %p\n", i, size, blocks[i]);
        }

        // Occasionally free some blocks to fragment memory
        if (i % 10 == 0 && i > 0) {
            student_free(blocks[i/2]);
            printf("freed block %d: requested size %d, got %p\n", i/2, size, blocks[i/2]);
            blocks[i/2] = 0;

            getmemstats(&stats);

            // Retrieve intermediate memory statistics
            if (getmemstats(&stats) < 0) {
                printf("getmemstats failed\n");
                exit(1);
            }
            printf("=== Intermediate Memory Statistics ===\n");
            printf("Student number: %d\n", stats.student_number);
            printf("Allocation strategy: %d\n", stats.strategy);
            printf("Allocated blocks: %d\n", stats.allocated_blocks);
            printf("Total allocated bytes: %d\n", stats.total_allocated_bytes);
            printf("Free blocks: %d\n", stats.free_blocks); 
        }
    }

    // Free all remaining blocks
    for (int i = 0; i < NUM_ITER; i++) {
        if (blocks[i]) {
            student_free(blocks[i]);
            blocks[i] = 0;
        }
    }

    // Freeing NULL pointer to test edge case
    student_free(0);

    // Retrieve final memory statistics
    if (getmemstats(&stats) < 0) {
        printf("getmemstats failed\n");
        exit(1);
    }

    printf("=== Stress Test Summary ===\n");
    printf("Student number: %d\n", stats.student_number);
    printf("Allocation strategy: %d\n", stats.strategy);
    printf("Allocated blocks: %d\n", stats.allocated_blocks);
    printf("Total allocated bytes: %d\n", stats.total_allocated_bytes);
    printf("Free blocks: %d\n", stats.free_blocks);

    printf("Stress test completed.\n");

    exit(0);
}
