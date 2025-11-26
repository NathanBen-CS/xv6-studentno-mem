#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/kalloc.h"   // contains struct memstats

int main(int argc, char *argv[]) {

    printf("=== Student Allocator Basic Test ===\n");

    // Allocate several blocks of different sizes
    void *a = student_malloc(100);
    void *b = student_malloc(200);
    void *c = student_malloc(50);
    void *d = student_malloc(400);

    printf("Allocated blocks: 100, 200, 50, 400 bytes\n");

    // Free a couple of them
    student_free(b);
    student_free(c);
    printf("Freed blocks of 200 and 50 bytes\n");

    // Retrieve memory stats
    struct memstats stats;
    if (getmemstats(&stats) < 0) {
        printf("getmemstats failed\n");
        exit(1);
    }

    // Print the stats returned by your allocator
    printf("Student number: %d\n", stats.student_number);
    printf("Allocation strategy: %d\n", stats.strategy);
    printf("Allocated blocks: %d\n", stats.allocated_blocks);
    printf("Total allocated bytes: %d\n", stats.total_allocated_bytes);
    printf("Free blocks: %d\n", stats.free_blocks);

    // Free remaining blocks
    student_free(a);
    student_free(d);
    printf("Freed remaining blocks\n");

    exit(0);
}
