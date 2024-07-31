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

/**< Number of memory pages to be stored in swap dev */
#define SWAP_PAGES 4096

void vm_init (void);

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                         Data Structures 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

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

/**< Directory page of swap table */
struct swap_table_dir
  {
    /** Layout of swap table entry
      +----------------+----------+
      |  block number  | aux bits |
      +----------------+----------+
      ^32              ^3         ^0
     */
    unsigned int     entries[1024];  /**< Entries(i.e. number of block ids) */
  };

/**< Root of swap table. */
struct swap_table_root
  {
    struct swap_table_dir  *dirs[1024];  /**< Pointer to directories */
  };

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                      Memory Mapping files
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

struct map_file *map_file_lookup (void *, void *);
bool map_file (void *rt, struct map_file *mf, void *uaddr);
void *map_file_init (void);
void map_file_clear (void *);
int map_file_fill_page (struct map_file *, void *);

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                          Swap Tables
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/**< To validate an entry, use: (STE_V & entry) */
#define STE_V 0x1

/** Given an swap table entry, return the block number. */
static inline unsigned int
ste_get_blockno (unsigned int ste)
{
  /* A primary benefit of doing so, is that 
    memory_page_size = 8 * disk_block_size. */
  return ste & (~0x00000007);
}

struct swap_table_root *swaptb_create (void);
void swaptb_free (struct swap_table_root *rt);
unsigned int *swaptb_lookup (struct swap_table_root *rt, void *uaddr);
int swaptb_map (struct swap_table_root *rt, void *uaddr, unsigned int blk);

#endif /**< vm/vm-util.h */
