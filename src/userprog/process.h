#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "mode.h"
#include "threads/thread.h"

extern struct list waiting_process;

struct file;
/** Metadata of a user process(put on stack) */
struct process_meta
  { 
    char           *argv;       /**< Position of argv */
    struct file    *ofile[MAX_FILE];
                                /**< File descriptor table */
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

int fdalloc (void);
int fdfree (int);
int fdseek (int, unsigned int);
int fdtell (int);
struct file *filealloc (const char *fn);

#endif /**< userprog/process.h */
