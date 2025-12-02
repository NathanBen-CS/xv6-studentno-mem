#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/kalloc.h"   // contains struct memstats and getmemstats()

// Helper function to print allocator stats
void print_stats(const char *msg, struct memstats *s) {
    if (getmemstats(s) < 0) {
        printf("getmemstats failed\n");
        exit(1);
    }
    printf("%s: strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
           msg, s->strategy, s->allocated_blocks, s->total_allocated_bytes, s->free_blocks);
}

int main(int argc, char *argv[])
{
    struct memstats s;

    printf("=== Student Allocator Worst-Fit Strategy Test ===\n");

    // Initial stats
    print_stats("Initial", &s);

    // Step 1: Allocate blocks to create fragmentation
    void *A = student_malloc(300);
    void *B = student_malloc(600);
    void *C = student_malloc(200);

    if (!A || !B || !C) { printf("Initial allocation failed\n"); exit(1); }
    printf("Allocated blocks: A=300, B=600, C=200\n");

    // Step 2: Free A and B to create free blocks of different sizes
    student_free(A);
    student_free(B);
    printf("Freed blocks A(300) and B(600)\n");
    print_stats("After freeing A and B", &s);

    printf("Expectation: Allocator uses WORST-FIT strategy (largest block first)\n");

    // Step 3: Allocate X=250 -> should choose the largest free block (600)
    void *X = student_malloc(250);
    if (!X) { printf("malloc(250) failed\n"); exit(1); }
    printf("Allocated X=250\n");
    print_stats("After allocating X", &s);

    // Step 4: Free X and allocate Y=280 -> should again pick the largest free block
    student_free(X);
    printf("Freed X(250)\n");
    print_stats("After freeing X", &s);

    void *Y = student_malloc(280);
    if (!Y) { printf("malloc(280) failed\n"); exit(1); }
    printf("Allocated Y=280\n");
    print_stats("After allocating Y", &s);

    // Step 5: Cleanup remaining blocks
    student_free(C);
    student_free(Y);
    printf("Freed remaining blocks C and Y\n");
    print_stats("After cleanup", &s);

    printf("=== Worst-Fit Strategy Test Completed ===\n");
    exit(0);
}
