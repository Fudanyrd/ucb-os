#include "bio.h"
#include "threads/synch.h"

#include <stdint.h>
#include <stdio.h>

/**< number of bp caches */
#define BIO_CACHE 32

/**< buffer pool ticks */
static uint64_t bio_ticks;

/**< metadata of a buffer cache. */
struct buffer_meta 
  {
    uint64_t timestamp;   /**< timestamp, only with non-zero is valid */
    short    dirty;       /**< 1 if page is modified */
    short    pin_cnt;     /**< Pin count */
  };

/**< Initialize metadata of buffer cache */
static inline void 
bm_init (struct buffer_meta *bm) {
  bm->timestamp = 0;
  bm->dirty = 0;
  bm->pin_cnt = 0;
}

/**< Buffer pool(can hold 32 sectors at once) */
static char buffer_pool[BIO_CACHE][BLOCK_SECTOR_SIZE];

/**< Metadata of 32 caches */
static struct buffer_meta bmeta[BIO_CACHE];

/**< Lock for the entire buffer pool */
static struct lock bplock;

void bio_init (void) {
  bio_ticks = 1UL;
  /* Initialize bcache lock */
  lock_init (&bplock);

  /* Initialize b caches */
  for (int i = 0; i < BIO_CACHE; ++i)
    {
      bm_init (&(bmeta[i]));
    }
  /* Done */
}
