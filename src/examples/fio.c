/** Test file IO */
#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  /* file name */
  const char *fn = "fio.c";
  int fd = open (fn);
  printf ("allocate fd = %d\n", fd);

  if (fd < 0) {
    exit (1);
  }

  char buf[16];
  while (1) 
    {
      int n = read (fd, buf, sizeof (buf));
      if (n <= 0) {
        break;
      }
      write (1, buf, n);
    }

  exit (fd < 0);
}
