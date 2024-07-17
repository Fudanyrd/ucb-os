/** Test create/remove files */

#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  const char *fn = "nums.txt";
  const char *content = "1234 5678 9547\n";

  int fd;  
  fd = open (fn);
  if (fd >= 0) {
    /* file exists. */
    exit (1);
  }

  /* try creating file */
  if (create (fn, 128) < 0) {
    exit (2);
  }

  /* try opening file */
  fd = open (fn);
  if (fd < 0) {
    exit (3);
  }

  /* try writing file */
  const int len = 16;
  if (write (fd, content, len) != len) {
    exit (4);
  }
  close (fd);

  /* try opening and reaing file */
  char buf[32];
  fd = open (fn);
  if (fd < 0) {
    exit (5);
  }
  if (read (fd, buf, len) != len) {
    exit (6);
  }
  /* check the content read */
  write (1, buf, len);
  printf ("\n");
  
  /* OK */
  close (fd);
  exit (0);
}
