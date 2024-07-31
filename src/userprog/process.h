#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "mode.h"
#include "threads/thread.h"
#include "threads/palloc.h"

#ifdef VM
#include "vm/vm-util.h"
#endif

extern struct list waiting_process;
extern struct list exec_process;

#if 0
/**< Frame table entry. */
typedef struct frame_entry
  {
    void           *frame;  /**< Frame address(NULL) if invalid */
    /**< Other data members */
  } frame_entry;
#endif

struct file;
struct map_file;

/** Metadata of a user process(put on stack) */
struct process_meta
  { 
    char           *argv;       /**< Position of argv */
    struct file    *ofile[MAX_FILE];
                                /**< File descriptor table */
    struct file    *executable; /**< Executable; must close on process_exit. */
#ifdef VM  /**< Virtual memory is implemented! */
    void           *map_file_rt;     /**< Root of file mapping table. */
#endif
  };

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                         Process Operators 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_terminate (int);
void process_activate (void);
void process_unblock (struct list*, tid_t, int);
void *process_get_page (enum palloc_flags);

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                          File Operators 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

int fdalloc (void);
int fdfree (int);
int fdseek (int, unsigned int);
int fdtell (int);
int fdsize (int);
struct file *filealloc (const char *fn);

#endif /**< userprog/process.h */
