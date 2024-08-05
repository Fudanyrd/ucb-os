#include <stdio.h>
#include <syscall.h>

int
main (void)
{
  char buf[65536];
  const char magic = 0x5b;

  /* Set the value of buffer. */
  for (unsigned i = 0; i < sizeof buf; ++i)
    {
      buf[i] = magic;
    }

  /* Validate stack. */ 
  for (unsigned i = 0; i < sizeof buf; ++i)
    {
      if (buf[i] != magic)
        {
          printf ("At %u: expected 0x5b, got %d.\n", i, (int)buf[i]);
          exit (2);
        }
    }

  exit (0);
}
