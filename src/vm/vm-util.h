#ifndef VM_UTIL_H
#define VM_UTIL_H

#include <stdbool.h>
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/mode.h"
#include "userprog/process.h"

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                      Special Settings
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/**< use a lot of asserts to find bug */
#define ROBUST


/**< Entry of supplemental page table. The members it contains
   allow for easy lookup from user page(address) to the corresponding
   files.

   The map_file table is organized the same way as the page table; it has
   a root page and several directory pages:
   +----------+---------------+--------------+
   | root idx | directory idx | map_file ptr |
   +----------+---------------+--------------+
   ^32        ^22             ^12            ^0

   Note that a map_file struct must be allocated by malloc, so there will
   exist a safe way to free the mapping table.
  */
struct map_file 
  {
    struct file *fobj;  /**< file object to map, must be safe to close */
    off_t offset;       /**< offset */
    int read_bytes;     /**< read some bytes, the rest set to 0 */
    int writable;       /**< is the mapped file read-only? */
  };


/**< Frame table. */
struct frame_table
  {
    void      *pages[NFRAME];    /**< number of private frames(pages) */
    int        free_ptr;         /**< index to the next uninitialized frame */
  };

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                      Memory Mapping files
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

struct map_file *map_file_lookup (void *, void *);
bool map_file (void *rt, struct map_file *mf, void *uaddr);
void *map_file_init (void);
void map_file_clear (void *);
int map_file_fill_page (struct map_file *, void *);

#endif /**< vm/vm-util.h */
