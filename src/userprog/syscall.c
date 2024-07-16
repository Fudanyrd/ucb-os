#include "devices/shutdown.h"
#include "mode.h"
#include "userprog/pagedir.h"
#include "process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <list.h>
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

/** List of all processes that is waiting for others to finish */
struct list waiting_process;

/** Copy at most bytes from user space, return actual bytes copied.
 * @param pagetable pagetable to lookup.
 * @param uaddr start of user address to copy from.
 * @param kbuf kernel buffer.
 */
static unsigned int
copy_from_user (uint32_t *pagetable, void *uaddr, void *kbuf, 
                unsigned int bytes) 
{
  /* validate parameters */
  ASSERT (pagetable != NULL && is_user_vaddr(uaddr) && kbuf != NULL);
  if (bytes == 0U) {
    return 0U;
  }

  /* Once loopup a page, and copy only relevant part. */
  void *ptr = pg_round_down (uaddr);  /**< page-aligned address */
  unsigned int left = bytes;  /**< number of bytes left */
  unsigned int pgleft;        /**< bytes left in a page */
  for (; ptr < PHYS_BASE; )
    {
      void *kaddr = pagedir_get_page (pagetable, ptr);
      if (kaddr == NULL) {
        /* encounter page fault, abort(else will crash!) */
        break;
      }

      kaddr += uaddr - ptr;
      pgleft = PGSIZE - (uaddr - ptr);
      /* HINT: current memory layout:
               |<   pgleft  >|
        +------+------+------+
        |     userpage       |
        +------+------+------+
        ^ptr   ^uaddr

               |< bytes >|
        +------+------+------+
        |     kernelpage     |
        +------+------+------+
               ^kbuf
       */
      /* copy min(pgleft, left) bytes into kernel buf */
      if (pgleft < left) {
        memmove (kbuf, kaddr, pgleft);
        left -= pgleft;
        kbuf += pgleft;
      } else {
        memmove (kbuf, kaddr, left);
        left = 0;
        /* ignore kbuf for now */
        break;
      }

      /* look into next page; update uaddr */
      ptr += PGSIZE;
      uaddr = ptr;
    }

  return bytes - left;
}

/** Copy to user buffer. 
 * @param pagetable user page table
 * @param kbuf start of kernel address
 * @param uaddr user buffer
 * @return number of bytes copied to user buffer
 */
static unsigned int
copy_to_user (uint32_t *pagetable, void *kbuf, void *uaddr, 
              unsigned int bytes)
{
  /* validate parameters */
  ASSERT (pagetable != NULL && is_user_vaddr(uaddr) && kbuf != NULL);
  if (bytes == 0U) {
    return 0U;
  }

  /* Once loopup a page, and copy only relevant part. */
  void *ptr = pg_round_down (uaddr);  /**< page-aligned address */
  unsigned int left = bytes;  /**< number of bytes left */
  unsigned int pgleft;        /**< bytes left in a page */
  for (; ptr < PHYS_BASE; )
    {
      void *kaddr = pagedir_get_page (pagetable, ptr);
      if (kaddr == NULL) {
        /* encounter page fault, abort(else will crash!) */
        break;
      }

      kaddr += uaddr - ptr;
      pgleft = PGSIZE - (uaddr - ptr);
      /* copy min(pgleft, left) bytes into kernel buf */
      if (pgleft < left) {
        memmove (kaddr, kbuf, pgleft);
        left -= pgleft;
      } else {
        memmove (kaddr, kbuf, left);
        left = 0;
        /* ignore kbuf for now */
        break;
      }

      /* look into next page; update uaddr */
      ptr += PGSIZE;
      uaddr = ptr;
    }

  return bytes - left;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init (&waiting_process);
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

     Ans: this is one line of output from <syscall_handler+15>:
     "intr frame f is c010afb0, f->esp is bffffe88"
     obviosly intr-frame is in kernel space, and f->esp is in user space.
     Hence *(int *)f->esp is safe.
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

static int 
halt_executor (void *args UNUSED) 
{
  /* Shut down the machine */
  shutdown_power_off ();

  /* Will not return */
  return 0;
}

int 
exit_executor (void *args) 
{
  struct thread *cur = thread_current ();
  int code = -1;  /**< exit code */
  unsigned ret = copy_from_user (cur->pagedir, args, &code, sizeof(int));
  ASSERT (ret == sizeof(int));
#ifdef TEST
  printf ("system call exit got %x!\n", code);
#endif
  /* Weird idea: use ticks to store the exit code! */
  cur->ticks = code;
  process_exit ();
  thread_exit ();
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
  /* Check if the system call is implemented. */
  int id = syscall_id (f);
  if (id >= 0 && id < syscall_executor_cnt) 
    {
      syscall_executor_t executor = syscall_executors[id];
      int ret = executor (syscall_args (f));
      /* set the return value of intr_frame. */
      f->eax = ret;
      return;
    }

  /* pintos -p ../../examples/echo -a echo  -- -f -q run 'echo' */
  /* In the process of implementing system calls, there must be some
    you have not implemented! */
#ifdef TEST  
  printf ("intr frame f is %x, f->esp is %x\n", (unsigned)f, 
                                                (unsigned)(f->esp));
  ASSERT (is_kernel_vaddr ((void *)f));
  ASSERT (is_user_vaddr (f->esp));
  printf ("system call %d!\n", syscall_id (f));
#endif
  thread_exit ();
}
