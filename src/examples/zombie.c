/** A program that exits faster than its child.
   Will this break the kernel? */

#include <stdio.h>
#include <syscall.h>

int main (int argc, char **argv) {
  if (argc < 2) {
  	printf ("Usage zombe <exe>\n");
  	exit (0);
  }
  printf ("Start execution %s\n", argv[1]);
  
  exec (argv[1]);
  exit (0);
}

