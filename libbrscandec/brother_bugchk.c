#include "brother_bugchk.h"

#include <stdio.h>
#include <stdlib.h>

static int nMallocCnt = 0;
static int nFreeCnt = 0;

void* bugchk_malloc(size_t size, const char* filename, int line)
{
  nMallocCnt++;
  void *p = malloc(size);
  if (p) return p;
  fprintf(stderr, "bugchk_malloc(size=%zu), can't allocate@%s(%d)\n", size, filename, line);
  abort();
}

void bugchk_free(void* ptr, const char* filename, int line)
{
  nFreeCnt++;
  free(ptr);  /* free(NULL) is valid C */
}
