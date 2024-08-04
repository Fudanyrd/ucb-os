/** Allocate a very large buffer, and sequentially set the value of 
 * the buffer, then validate previous write. */

#include <syscall.h>
#include <stdio.h>
#include <stdint.h>

/* This buffer equals 512 memory pages, which the os must allow. */
static char buf[2 * 1024 * 1024];

#define MAGIC 0x3f6598a1L

void 
validate_buf (void)
{
  char *b = buf;
  int64_t *pt = b;
  int64_t *end = (int64_t *)(b + sizeof (buf));

  while (pt != end)
    {
      if (*pt != MAGIC) /* Fail */
        exit (2);
      pt++;
    }
}

int
main (void)
{
  /* walk down the buffer, set bit to 0. */
  char *b = buf;
  printf ("buffer addr: %p\n", b);
  int64_t *pt = buf;
  int64_t *end = (int64_t *)(b + sizeof (buf));

  /* Fill the entire buffer with a magic number. */
  while (pt != end)
    {
      *pt = MAGIC;
      ++pt;
    }
  
  /* Check that we have correctly set the number. */
  validate_buf (); 

  /* OK, congrats! */
  exit (0);
}
