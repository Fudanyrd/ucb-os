/** Here I use exit to check argument can be passed into kernel. */
#include <syscall.h>

int main(int argc, char **argv) {
  const int magic = 0xa1b23c4f;
  /* List of magic you can try out:
   0x12345678;
   0x87654321; 
   0xabcdef90;
   etc.
  */

  for (int i = 0; i < 10; ++i)
    {
      exit (magic);
    }
}
