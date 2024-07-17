/** Check that if open one file twice, the kernel can record offset 
   for each. */

#include <syscall.h>

int main (int argc, char **argv) {
  const char *fn = "word.txt";

  /* create 2 file descriptor */
  int fd1 = open (fn);
  int fd2 = open (fn);
  if (fd1 < 0 || fd2 < 0) {
    exit (1);
  }

  /* output should be "word\nword\n". */
  char buf[8];
  read (fd1, buf, 4);
  write (1, buf, 4);
  write (1, "\n", 1);
  read (fd2, buf, 4);
  write (1, buf, 4);
  write (1, "\n", 1);

  close (fd1);
  close (fd2);
  exit (0);
}
