#include "bio.h"
#include "free-map.h"
#include "filesys.h"
#include "threads/synch.h"

#include <stdint.h>
#include <stdio.h>

/**< number of bp caches */
#define BIO_CACHE 48

/**< buffer pool ticks */
static uint64_t bio_ticks;

/**< metadata of a buffer cache. */
struct buffer_meta 
  {
    uint64_t timestamp;   /**< timestamp, only with non-zero is valid */
    short    dirty;       /**< 1 if page is modified */
    uint16_t pin_cnt;     /**< Pin count */
    block_sector_t sec;   /**< Sector number */
  };

/**< Initialize metadata of buffer cache */
static inline void 
bm_init (struct buffer_meta *bm) {
  bm->timestamp = 0;
  bm->dirty = 0;
  bm->pin_cnt = 0;
  bm->sec = -1;
}

/**< Buffer pool(can hold 32 sectors at once) */
static char buffer_pool[BIO_CACHE][BLOCK_SECTOR_SIZE];

/**< Buffer cache base pointer */
static const char *bio_base = (char *)buffer_pool;

/**< Buffer cache bound(end) pointer */
static const char *bio_end = (char *)buffer_pool + sizeof (buffer_pool);

/**< Metadata of 32 caches */
static struct buffer_meta bmeta[BIO_CACHE];

/**< Lock for the entire buffer pool */
static struct lock bplock;

/** Allocate a sector for buffer cache. 
 * @param write: set to 1 if cache is writable
 * @param sec: sector of this cache.
 * @return -1 if all caches is allocated and pinned. 
 */
static int
bio_alloc (block_sector_t sec, short write)
{
  /* Must take the lock when executing bio_alloc. */
  ASSERT (lock_held_by_current_thread (&bplock));

  int ret = -1;
  /** Smallest time stamp */
  uint64_t sts = -1;

  for (int i = 0; i < BIO_CACHE; ++i) 
    {
      if (bmeta[i].timestamp == 0) {
        /* Always use free frame first. */
        ret = i; 
        break;
      }
      if (bmeta[i].pin_cnt > 0) {
        /* Do not evict pinned page */
        continue;
      }
      if (bmeta[i].timestamp < sts) {
        ret = i;
        sts = bmeta[i].timestamp;
      }
    }

  /* If evicting valid cache, write back. */
  if (ret >= 0 && bmeta[ret].dirty != 0)
    {
      ASSERT (bmeta[ret].timestamp != 0);
      block_write (fs_device, bmeta[ret].sec, 
                   bio_base + (BLOCK_SECTOR_SIZE * ret));
    }

  /* Update metadata */
  if (ret >= 0)
    {
      bmeta[ret].dirty = write;
      if (bmeta[ret].pin_cnt != 0) {
        PANIC ("evicting pinned page");
      }
      bmeta[ret].sec = sec;
      bmeta[ret].timestamp = bio_ticks;
    }

  /* Done. */
  return ret;
}

/** Fetch a page so that it appears in the cache:
 * - if already in the cache, return it;
 * - if not in the cache and have empty line, return empty liine.
 * - if not in the cache and cache is full, evict and return.
 * - if all lines are pinned, return -1.
 */
static int
bio_fetch (block_sector_t sec, short write)
{
  /* Must take the lock when executing bio_alloc. */
  ASSERT (lock_held_by_current_thread (&bplock));

  int ret = -1;
  /** Smallest time stamp */
  uint64_t sts = -1;

  for (int i = 0; i < BIO_CACHE; ++i) 
    {
      if (bmeta[i].sec == sec) {
        ret = i;
        break;
      }
      if (bmeta[i].timestamp == 0) {
        /* Always use free frame first. */
        ret = i; 
      }
      if (bmeta[i].pin_cnt > 0) {
        /* Do not evict pinned page */
        continue;
      }
      if (bmeta[i].timestamp < sts) {
        /* Always use free frame first. */
        ret = (ret >= 0 && bmeta[ret].timestamp == 0) ? ret : i;
        sts = bmeta[i].timestamp;
      }
    }
  
  if (ret >= 0) {

    if (bmeta[ret].sec == sec) {
      /** Cache hit! */
      if (write) /* A dirty page shall remain dirty. */
        bmeta[ret].dirty = 1;
      ASSERT (bmeta[ret].timestamp != 0);
      /* Reset timestamp */
      bmeta[ret].timestamp = bio_ticks;
      return ret;
    }

    /* Cache miss, check and flush dirty page */
    ASSERT (bmeta[ret].pin_cnt == 0);
    if (bmeta[ret].dirty) {
      ASSERT (bmeta[ret].timestamp != 0);
      /* Flush page to disk. */
      block_write (fs_device, bmeta[ret].sec, 
                   bio_base + (BLOCK_SECTOR_SIZE * ret));
    }

    bmeta[ret].sec = sec;
    /* Read the page into cache */
    block_read (fs_device, sec, bio_base + (BLOCK_SECTOR_SIZE * ret));
    /* Reset timestamp */
    bmeta[ret].timestamp = bio_ticks;
    bmeta[ret].dirty = write;
  }

  return ret;
}

/** Initialize buffer cache */
void bio_init (void) {

  /* validate buffer cache size(<= 64) */
  STATIC_ASSERT (BIO_CACHE <= 64);

  STATIC_ASSERT (sizeof (struct buffer_meta) == 16);

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

/** Allocate a new sector and a cache */
struct bio_pack 
bio_new (void)
{
  /* Acquire the lock, ad incur bio ticks. */
  lock_acquire (&bplock);
  ++bio_ticks;

  struct bio_pack pack = { .cache = NULL, .sec = 0};

  /** allocate sector first. */
  if (!free_map_allocate (1U, &pack.sec)) {
    /**< failure! */
    lock_release (&bplock);
    return pack;
  }

  int cno = bio_alloc (pack.sec, 1);
  if (cno == -1) {
    /** free the sector, return. */
    free_map_release (pack.sec, 1U);
    pack.sec = 0;

    lock_release (&bplock);
    return pack;
  }

  pack.cache = bio_base + (BLOCK_SECTOR_SIZE * cno);
  lock_release (&bplock);
  return pack;
}

/** Fetch a sector for reading and pin the page. */
static const char *
bio_read_exec (block_sector_t sec)
{
  lock_acquire (&bplock);
  ++bio_ticks;
  int line = bio_fetch (sec, 0);
  if (line < 0) {
    /* Failure */
    lock_release (&bplock);
    return NULL;
  }

  /* Help you pin the page. */
  bmeta[line].pin_cnt++;
  lock_release (&bplock);
  return bio_base + (BLOCK_SECTOR_SIZE * line);
}

/** Fetch a sector for reading and pin the page. */
const char *
bio_read (block_sector_t sec) 
{
  const char *ret = bio_read_exec (sec);
  if (ret == NULL)
    PANIC ("buffer full");
  return ret;
}

/** Fetch a sector for writing and pin it. */
static char *
bio_write_exec (block_sector_t sec)
{
  lock_acquire (&bplock);
  ++bio_ticks;
  int line = bio_fetch (sec, 1);

  /* Help you pin the page. */
  if (line >= 0)
    bmeta[line].pin_cnt++;
  lock_release (&bplock);
  return line < 0 ? NULL : bio_base + (line * BLOCK_SECTOR_SIZE);
}

/** Fetch a sector for writing and pin it. */
char *
bio_write (block_sector_t sec)
{
  char *ret = bio_write_exec (sec);
  if (ret == NULL)
    PANIC ("buffer full");
  return ret;
}

/** Flush all dirty pages back to disk. */
void 
bio_flush (void)
{
  free_map_flush ();
  char *pt = bio_base;
  lock_acquire (&bplock);
  for (int i = 0; i < BIO_CACHE; ++i)
    {
      if (bmeta[i].dirty) {
        ASSERT (bmeta[i].timestamp != 0);
        block_write (fs_device, bmeta[i].sec, pt);
        /* Clear dirty tag */
        bmeta[i].dirty = 0;
      }

      /* Advance */
      pt += BLOCK_SECTOR_SIZE;
    }
  lock_release (&bplock);
}

/** Pin a page in the buffer that will probably be used soon.
 * @return 1 if pin is successful.
 */
int 
bio_pin (block_sector_t sec)
{
  lock_acquire (&bplock);
  for (int i = 0; i < BIO_CACHE; ++i)
    {
      if (bmeta[i].sec == sec) {
        ASSERT (bmeta[i].timestamp != 0);
        bmeta[i].pin_cnt += 1;
        lock_release (&bplock);
        return 1;
      }
    }

  lock_release (&bplock);
  return 0;
}

/** Unpin a page in the buffer. 
 * @return 1 if successful, 0 if sector not in buffer.
 */
int 
bio_unpin (block_sector_t sec)
{
  lock_acquire (&bplock);
  for (int i = 0; i < BIO_CACHE; ++i)
    {
      if (bmeta[i].sec == sec) {
        ASSERT (bmeta[i].timestamp != 0);
        ASSERT (bmeta[i].pin_cnt > 0);
        bmeta[i].pin_cnt -= 1;
        lock_release (&bplock);
        return 1;
      }
    }

  lock_release (&bplock);
  return 0;
}

/** Pin the sector given in sec. */
int 
bio_pin_sec (const char *sec)
{
  if (sec < bio_base || sec >= bio_end)
    {
      /* Invalid pointer. */
      return 0;
    }

  lock_acquire (&bplock); 
  int idx = (sec - bio_base) / BLOCK_SECTOR_SIZE;
  ASSERT (bmeta[idx].timestamp != 0);
  bmeta[idx].pin_cnt += 1;
  lock_release (&bplock); 

  /* Done. */
  return 1;
}

/** Unpin the sector given in sec. */
int 
bio_unpin_sec (const char *sec)
{
  if (sec < bio_base || sec >= bio_end)
    {
      /* Invalid pointer. */
      return 0;
    }

  lock_acquire (&bplock); 
  int idx = (sec - bio_base) / BLOCK_SECTOR_SIZE;
  ASSERT (bmeta[idx].timestamp > 0);
  ASSERT (bmeta[idx].pin_cnt > 0);
  bmeta[idx].pin_cnt -= 1;
  lock_release (&bplock); 

  /* Done. */
  return 1;
}

/** Remove the sector given in sec. Do not write back. */
int 
bio_free_sec (char *sec)
{
  if (sec < bio_base || sec >= bio_end)
    {
      /* Invalid pointer. */
      return 0;
    }

  lock_acquire (&bplock); 
  int idx = (sec - bio_base) / BLOCK_SECTOR_SIZE;
  ASSERT (bmeta[idx].timestamp > 0);
  bmeta[idx].pin_cnt -= 1;
  if (bmeta[idx].pin_cnt != 0)
    PANIC ("freeing pinned sector, "
           "maybe you forget to unpin the page somewhere else?"
          );
  /* Why panic? for only one thread can access the inode at any time
  (for inode->lk), hence there's only one thread pinning and unpinning 
  the sector, hence normally the pin count must be zero now. */
  bmeta[idx].dirty = 0;
  bmeta[idx].timestamp = 0;
  bmeta[idx].sec = -1;

  /* Finished. */
  lock_release (&bplock); 
  return 1;
}
