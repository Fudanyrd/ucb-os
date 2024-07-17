#include <syscall.h>
#include <stdio.h>

int main (int argc, char **argv) {
  char buf[6];

  int fd = open ("word.txt");
  if (fd < 0) {
    exit (1);
  }

  for (int i = 0; i < 4; ++i) {
    seek (fd, i * 5);
    int bytes = read (fd, buf, 4);
    if (bytes < 4) {
      exit (1);
    }
    write (1, buf, 4);
    printf (", now at %d\n", tell (fd));
  }

  close (fd);
  exit (0);
}
