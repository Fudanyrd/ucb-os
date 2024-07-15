#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/** From interrupt frame, get the system call id. */ 
static int
syscall_id (struct intr_frame *f)
{
  /** How? lib/user/syscall.c gave me the answer:
     Upon a syscall, the macro syscall0, syscall1, syscall2
     handles system call id and arguments in a consistent way:

     +------------+
     |    args    |
     +------------+
     | syscall-id |
     +------------+ <- esp
   */

  /** At first glance, the return value should be *(int *)(f->esp),
     but don't we need address translation here? why? 
     > Q1: what is the operating page table at this point? 
     > Q2: if the kernel page table is operating, how do we 
     read some bytes from user space?
   */
  return *(int *)(f->esp);
}

/** From interrupt frame, locate the argument(s) to syscall.
   The return value can be directly passed to syscall executor.
 */
static void *
syscall_args (struct intr_frame *f)
{
  /** The same as we did in syscall_id. */
  return (f->esp + 4);
}

int 
halt_executor (void *args) 
{
  printf ("system call halt!\n");
  return 0;
}
int 
exit_executor (void *args) 
{
  printf ("system call exit!\n");
  return 0;
}

/** list of implemented system calls */
static syscall_executor_t syscall_executors[] = 
  {
    [SYS_HALT] halt_executor,
    [SYS_EXIT] exit_executor,
  };

/** Number of implemented system calls(to detect overflow) */
static const unsigned int
syscall_executor_cnt = (sizeof (syscall_executors)
                      / sizeof (syscall_executors[0]));

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call %d!\n", syscall_id (f));
  thread_exit ();
}
