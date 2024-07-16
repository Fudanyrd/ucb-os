#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  char buf[256];

  while (1) {
    printf (">>> ");
    unsigned n = read (0, buf, sizeof (buf));
    if (n <= 1U) {
      break;
    }
    write (1, buf, n);
  }
  printf ("well, bye\n");

  return 0;
}
