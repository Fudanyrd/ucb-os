/** Test file IO */
#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  /* file name */
  const char *fn = "fio.c";
  int fd = open (fn);

  const char *dst = "empty.txt";
  int dd = open (dst);
  printf ("allocate fd = %d, %d\n", fd, dd);

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
      write (dd, buf, n);
    }

  /** Just close one file to detect not-close-on-exit or
     double-close-on-exit error. */
  close (fd);
  exit (fd < 0);
}
