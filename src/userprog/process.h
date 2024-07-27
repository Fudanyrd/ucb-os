#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "mode.h"
#include "threads/thread.h"
#include "threads/palloc.h"

extern struct list waiting_process;
extern struct list exec_process;

/**< Frame table entry. */
typedef struct frame_entry
  {
    void           *frame;  /**< Frame address(NULL) if invalid */
    /**< Other data members */
  } frame_entry;

struct file;
/** Metadata of a user process(put on stack) */
struct process_meta
  { 
    char           *argv;       /**< Position of argv */
    struct file    *ofile[MAX_FILE];
                                /**< File descriptor table */
    struct file    *executable; /**< Executable; must close on process_exit. */
#ifdef VM  /**< Virtual memory is implemented! */
    uint32_t       *supple_pagedir;  /**< Supplemental page directory */
    frame_entry     frame_table[NFRAME];
#endif
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_terminate (int);
void process_activate (void);
void process_unblock (struct list*, tid_t, int);
void *process_get_page (enum palloc_flags);

int fdalloc (void);
int fdfree (int);
int fdseek (int, unsigned int);
int fdtell (int);
int fdsize (int);
struct file *filealloc (const char *fn);

#endif /**< userprog/process.h */
