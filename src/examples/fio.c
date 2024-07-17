/** Test file IO */
#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  /* file name */
  const char *fn = "fio";
  int fd = open (fn);
  printf ("allocate fd = %d\n", fd);

  exit (fd < 0);
}
