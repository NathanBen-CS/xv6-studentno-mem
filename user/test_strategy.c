#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/kalloc.h"   // contains struct memstats and getmemstats()

// user/test_strategy.c
// Use student_malloc / student_free to create fragmentation,
// and use getmemstats() to read kernel memstats and confirm worst-fit behavior.

int
main(int argc, char *argv[])
{
  struct memstats s;

  printf("=== Student Allocator Strategy Test (uses student_malloc/free + getmemstats) ===\n");

  // Start by printing initial stats
  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("Initial: strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  // 1) Create fragmentation:
  // allocate three blocks A, B, C with different sizes
  void *A = student_malloc(300);
  void *B = student_malloc(600);
  void *C = student_malloc(200);

  if (!A || !B || !C) { printf("alloc failed\n"); exit(1); }
  printf("Allocated A=300, B=600, C=200\n");

  // 2) Free A and B to create free blocks {300, 600}
  student_free(A);
  student_free(B);
  printf("Freed A(300) and B(600) -> free blocks should include 300 and 600\n");

  // get stats and print them
  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("After freeing A,B: strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  // Expectation: strategy == WORST-FIT (value printed for your allocator). We print it for manual check.
  printf("Expected strategy: worst-fit (strategy value should match your allocator's setting, e.g. 2)\n");

  // 3) Worst-fit test #1:
  // Request size that both free blocks can satisfy, e.g. 250 -> both 300 and 600 are big enough
  void *X = student_malloc(250);
  if (!X) { printf("malloc(250) failed\n"); exit(1); }
  printf("Allocated X = malloc(250)\n");

  // get stats and print them
  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("After allocating X(250): strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  printf("Expectation: worst-fit should have chosen the LARGEST free block (600) for the 250 request.\n");

  // 4) Return X, then test another request that should pick the other block
  student_free(X);
  printf("Freed X(250) -> returning to fragmented free blocks\n");

  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("After freeing X: strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  // Now request 280 (fits in 300, and in 600) — worst-fit should pick 600 again if it truly picks largest,
  // but if allocator behavior splits differently, you can inspect the stats for confirmation.
  void *Y = student_malloc(280);
  if (!Y) { printf("malloc(280) failed\n"); exit(1); }
  printf("Allocated Y = malloc(280)\n");

  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("After allocating Y(280): strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  printf("Expectation: worst-fit should pick the currently largest free block.\n");

  // 5) Clean up: free remaining blocks
  student_free(C);
  student_free(Y);

  if (getmemstats(&s) < 0) { printf("getmemstats failed\n"); exit(1); }
  printf("After cleanup: strategy=%d, allocated_blocks=%d, total_allocated_bytes=%d, free_blocks=%d\n",
         s.strategy, s.allocated_blocks, s.total_allocated_bytes, s.free_blocks);

  printf("Strategy test finished. Inspect the printed values to confirm worst-fit behavior.\n");

  exit(0);
}
