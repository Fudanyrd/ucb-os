#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;

  /** These '=' easily distinguish user outputs from kernel 
     outputs */
  printf ("=============================\n");
  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");
  printf ("=============================\n");

  return EXIT_SUCCESS;
}
