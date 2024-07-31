#include <string.h>

#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/palloc.h"
#include "vm-util.h"

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                          Helper functions
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/** For swap table to keep track of used disk spaces. */
static struct bitmap *swap_table_bitmap;

/** Read an entire memory page from swap block */
static inline void
swaptb_read_page (unsigned int blockno, void *page)
{
  struct block *blk = block_get_role (BLOCK_SWAP);
  for (int i = 0; i < SECTORS_PER_PAGE; ++i)
    {
      block_read (blk, blockno + i, page);

      /* Advance */
      page += BLOCK_SECTOR_SIZE;
    }
}

static inline void
swaptb_write_page (unsigned int blockno, const void *page)
{
  struct block *blk = block_get_role (BLOCK_SWAP);
  /* Remember, a memory page equals 8 disk blocks */
  for (int i = 0; i < SECTORS_PER_PAGE; ++i)
    {
      block_write (blk, blockno + i, page);

      /* Advance */
      page += BLOCK_SECTOR_SIZE;
    }
}

/** Allocate a consecutive 8 disk blocks to store a memory page,
   Pay attention to how to translate from page index to block no:
   page_idx * 8 => blockno.
 */
static unsigned int
swap_table_alloc_page (void)
{
#ifdef ROBUST
  // validate parameters
  ASSERT (swap_table_bitmap != NULL);
#endif
  for (unsigned i = 0; i < SWAP_PAGES; ++i)
    {
      if (!bitmap_test (swap_table_bitmap, i))
        {
          /* Mark the page as used */
          bitmap_set (swap_table_bitmap, i, 1);
          return i;
        }
    }
  return (unsigned)-1;
}

/** Free a consecutive 8 disk blocks to store a memory page,
   Pay attention to how to translate from page index to block no:
   page_idx * 8 => blockno.
 */
static unsigned int
swap_talbe_free_page (unsigned int page_idx)
{
#ifdef ROBUST
  // validate parameters
  ASSERT (swap_table_bitmap != NULL);
  ASSERT (page_idx < SWAP_PAGES);
  ASSERT (bitmap_test (swap_table_bitmap, page_idx));
#endif
  // free the bit in the map
  bitmap_set (swap_table_bitmap, page_idx, 0);
}

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                          Swap Tables Method
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/** Initialize virtual memory */
void 
vm_init (void)
{
  /* Validate macro SECTORS_PER_PAGE */
  ASSERT (PGSIZE == (SECTORS_PER_PAGE * BLOCK_SECTOR_SIZE));

  /* Create a bitmap that supports a swap disk 
    of 16 MB(= 4 * 1024 memory pages), this bitmap is 0.5 KB */
  swap_table_bitmap = bitmap_create (SWAP_PAGES);
  ASSERT (swap_table_bitmap != NULL);

  /** Initially all swap pages is not used(free) */
  for (unsigned int i = 0; i < SWAP_PAGES; ++i)
    {
      bitmap_set (swap_table_bitmap, i, 0);
    }
}

/** Return an initialized swap table, NULL if failure */
struct swap_table_root *
swaptb_create (void)
{
  void *page = palloc_get_page (0);
  if (page != NULL) {
    memset (page, 0, PGSIZE);
  }

  return page;
}
