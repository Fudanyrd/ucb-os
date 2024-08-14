#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#include "common.h"

#ifndef FILESYS

/** Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/** On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /**< First data sector. */
    off_t length;                       /**< File size in bytes. */
    unsigned magic;                     /**< Magic number. */
    uint32_t unused[125];               /**< Not used. */
  };

/** Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/** In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /**< Element in inode list. */
    block_sector_t sector;              /**< Sector number of disk location. */
    int open_cnt;                       /**< Number of openers. */
    bool removed;                       /**< True if deleted, false otherwise. */
    int deny_write_cnt;                 /**< 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /**< Inode content. */
    struct lock lk;                     /**< inode lock */
  };

/** Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
}

/** List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/** Protect against concurrent access to inode list */
static struct lock inode_list_lock;

/** WARNING: to avoid deadlock, you must hold inode_list_lock BEFORE holding
   locks belonging to an inode. */

/** Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  lock_init (&inode_list_lock);
}

/** Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  lock_acquire (&inode_list_lock);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }

  /* OK. */
  lock_release (&inode_list_lock);
  return success;
}

/** Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  lock_acquire (&inode_list_lock);

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          lock_release (&inode_list_lock);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL) {
    lock_release (&inode_list_lock);
    return NULL;
  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->lk);
  block_read (fs_device, inode->sector, &inode->data);

  /** Release all locks */
  lock_release (&inode_list_lock);
  return inode;
}

/** Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL) {
    lock_acquire (&inode->lk);
    inode->open_cnt++;
    lock_release (&inode->lk);
  }
  return inode;
}

/** Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/** Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /** Why? because of possible list_remove! */
  lock_acquire (&inode_list_lock);
  lock_acquire (&inode->lk);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      lock_release (&inode->lk);
      lock_release (&inode_list_lock);
      free (inode); 
      return;
    }

  lock_release (&inode->lk);
  lock_release (&inode_list_lock);
}

/** Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  lock_acquire (&inode->lk);
  inode->removed = true;
  lock_release (&inode->lk);
}

/** Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  lock_acquire (&inode->lk);
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  lock_release (&inode->lk);
  return bytes_read;
}

/** Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  lock_acquire (&inode->lk);

  if (inode->deny_write_cnt) {
    lock_release (&inode->lk);
    return 0;
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  lock_release (&inode->lk);
  return bytes_written;
}

/** Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  lock_acquire (&inode->lk);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_release (&inode->lk);
}

/** Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  lock_acquire (&inode->lk);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  lock_release (&inode->lk);
}

/** Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

#else  /**< Add your own inode impl! */
#include "bio.h"

/** inode magic number */
#define INODE_MAGIC 0x10203040

/** invalid inode position */
#define INODE_INVALID (-1)

/** 
 * cication: <https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/fs.h> 
 */

enum inode_type {
  /**< Invalid type */
  INODE_NULL = 0,
  /**< File */
  INODE_FILE,
  /**< Directory */
  INODE_DIR,
};

/** On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    short type;           /**< Type of file */
    short nlink;          /**< Number of links to inode */
    off_t size;           /**< Size of file (bytes) */
    int addrs[125];       /**< Data block addresses */
    unsigned magic;       /**< Magic number */
  };

/**< size of sum of direct sectors */
#define DIRECT_SIZE (123 * BLOCK_SECTOR_SIZE)

/**< size of single indirect blocks */
#define SINGLE_INDIR_SIZE (128 * BLOCK_SECTOR_SIZE)

/**< size of doubly indirect blocks */
#define DOUBLY_INDIR_SIZE (128 * SINGLE_INDIR_SIZE)

/**< maximum size of file */
#define MAXFILE (DIRECT_SIZE + SINGLE_INDIR_SIZE + DOUBLY_INDIR_SIZE)

/** In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /**< Element in inode list. */
    block_sector_t sector;              /**< Sector number of disk location. */
    int open_cnt;                       /**< Number of openers. */
    bool removed;                       /**< True if deleted, false otherwise. */
    int deny_write_cnt;                 /**< 0: writes ok, >0: deny writes. */
    struct inode_disk *data;            /**< Inode content. */
    struct lock lk;                     /**< inode lock */
  };

/** Indirect block */
struct indirect_block 
  {
    block_sector_t addrs[128];          /**< Address to other blocks. */
  };

/** Deallocate all sectors occupied by an inode. */
static void
inode_deallocate (struct inode *ino)
{
  if (ino == NULL)
    PANIC ("invalid argument");
  ASSERT (lock_held_by_current_thread (&ino->lk));

  /* Fetch and pin the sector */
  const struct inode_disk *di = bio_read (ino->sector); 
  if (di == NULL) {
    PANIC ("buffer full");
  }
  if (!bio_pin_sec (di)) {
    PANIC ("pin sec");
  }

  /* For each direct page, destroy */
  for (int i = 0; i < 123; ++i)
    {
      if (di->addrs[i] != INODE_INVALID) {
        free_map_release (di->addrs[i], 1U);
      }
    }
  
  /* Free primary indirect page */
  if (di->addrs[123] != INODE_INVALID)
    {
      const struct indirect_block *ind = bio_read (di->addrs[123]);
      if (!bio_pin_sec (ind))
        PANIC ("bio pin");
      for (int i = 0; i < 128; ++i) {
        if (ind->addrs[i] != INODE_INVALID)
          free_map_release (ind->addrs[i], 1U);
      }

      if (!bio_free_sec (ind))
        PANIC ("bio free");
      free_map_release (di->addrs[123], 1U);
    }

  /* Free doubly indirect blocks. */
  if (di->addrs[124] != INODE_INVALID)
    {
      const struct indirect_block *first = bio_read (di->addrs[124]);
      if (!bio_pin_sec (first))
        PANIC ("bio pin");
      const struct indirect_block *second;

      for (int i = 0; i < 128; ++i) {
        if (first->addrs[i] == INODE_INVALID)
          continue;
        
        second = bio_read (first->addrs[i]);
        if (!bio_pin_sec (second))
          PANIC ("bio pin");

        for (int k = 0; k < 128; ++k) {
          if (second->addrs[k] != INODE_INVALID)
            free_map_release (second->addrs[k], 1U);
        }
        
        if (!bio_free_sec (second))
          PANIC ("bio free");
        free_map_release (first->addrs[i], 1U);
      }

      if (!bio_free_sec (first))
        PANIC ("bio free");
      free_map_release (di->addrs[124], 1U);
    }

  /* Finish. */ 
  if (!bio_free_sec (di)) {
    PANIC ("free sec");
  }
  free_map_release (ino->sector, 1U);
}

/** Return the offset in a sector. */
static inline off_t
sec_off (off_t offset)
{
  return offset % BLOCK_SECTOR_SIZE;
}

/** Seek and read a page into buffer. 
 * @param di disk inode representing an inode
 * @param buf buffer to read data to
 * @param offset seek file position
 * @param size maximum bytes read
 * @return number of bytes read into buffer.
*/
static off_t
inode_seek_read (const struct inode_disk *di, char *buf, off_t offset, 
                 off_t size)
{
  if (di->magic != INODE_MAGIC) {
    PANIC ("not inode_disk");
  }

  if (offset >= MAXFILE) /* Seek beyond largest file */
    return 0;  
  if (offset >= di->size) {
    /* Seek outside the file */
    return 0;
  }
  
  if (offset < DIRECT_SIZE) {
    int idx = offset / BLOCK_SECTOR_SIZE;
    ASSERT (idx < 123);
    /* Sector of data page */
    block_sector_t dsec = di->addrs[idx];
    /* bytes to be read. */
    off_t bytes = BLOCK_SECTOR_SIZE - sec_off (offset);
    /* bytes = min(size, bytes); */
    bytes = bytes > size ? size : bytes;

    if (dsec == INODE_INVALID) {
      /* Lazily allocated page is filled with 0. */
      memset (buf, 0, bytes);
      return bytes;
    }

    /* Fetch the data sector and pin it. */
    const char *dat = bio_read (dsec);
    if (!bio_pin_sec (dat))
      PANIC ("bio pin");

    /* Copy the bytes */
    memcpy (buf, dat + sec_off (offset), bytes);

    /* unpin the data page, done. */
    if (!bio_unpin_sec (dat))
      PANIC ("bio unpin");
    return bytes;
  }

  /* Look into singly indirect sectors. */
  offset -= DIRECT_SIZE;
  if (offset < SINGLE_INDIR_SIZE) {
    const struct indirect_block *indir = bio_read (di->addrs[123]);
    if (!bio_pin_sec (indir))
      PANIC ("bio pin");
    /* Get data sector */
    int dsec = indir->addrs[offset / BLOCK_SECTOR_SIZE];
    if (!bio_unpin_sec (indir))
      PANIC ("bio unpin");
    
    /* bytes to be read. */
    off_t bytes = BLOCK_SECTOR_SIZE - sec_off (offset);
    /* bytes = min(size, bytes); */
    bytes = bytes > size ? size : bytes;

    /* validate dsec */
    if (dsec == INODE_INVALID) {
      memset (buf, 0, bytes);
      return bytes;
    }
    
    /* Read data in sector dsec. */
    const char *dat = bio_read (dsec);
    if (!bio_pin_sec (dat))
      PANIC ("bio pin");
    memcpy (buf, dat + sec_off (offset), bytes);
    if (!bio_unpin_sec (dat))
      PANIC ("bio unpin");
    return bytes;
  }

  /* compute indexes. */
  offset -= SINGLE_INDIR_SIZE;
  off_t bytes = BLOCK_SECTOR_SIZE - sec_off (offset);  /* Bytes read. */
  /* bytes = min(size, bytes); */
  bytes = bytes > size ? size : bytes;
  const int ind1 = offset / SINGLE_INDIR_SIZE; /* index into first directory */
  const int ind2 = (offset % SINGLE_INDIR_SIZE) / BLOCK_SECTOR_SIZE;
                                        /* index into second directory */ 
  ASSERT (ind1 < 128 && ind2 < 128);

  /* pin first level directory block, read and unpin. */
  const struct indirect_block *isec1 = bio_read (di->addrs[124]);
  if (!bio_pin_sec (isec1)) {
    PANIC ("bio pin");
  }
  const int isec2id = isec1->addrs[ind1];
  if (!bio_unpin_sec (isec1)) {
    PANIC ("bio unpin");
  }
  if (isec2id == INODE_INVALID) { /* not found, fill 0. */
    goto sec_not_found;
  }

  /* pin second level directory block, read and unpin. */
  const struct indirect_block *isec2 = bio_read (isec2id); 
  if (!bio_pin_sec (isec2)) {
    PANIC ("bio pin");
  }
  const int dsec = isec2->addrs[ind2];
  if (!bio_unpin_sec (isec2)) {
    PANIC ("bio unpin");
  }
  if (dsec == INODE_INVALID) { /* not found, fill 0. */
    goto sec_not_found;
  }

  /* pin data block, read and unpin. */
  const char *idat = bio_read (dsec);
  if (!bio_pin_sec (idat)) {
    PANIC ("bio pin");
  }
  memcpy (buf, idat + sec_off (offset), bytes);
  if (!bio_unpin_sec (idat)) {
    PANIC ("bio unpin");
  }

  /* sector found, done. */
  return bytes;

sec_not_found: /* Sector not found, fill with zero. */
  memset (buf, 0, bytes);
  return bytes;
}

/** Seek and read a page into buffer. 
 * @param di disk inode representing an inode
 * @param buf buffer to read data to
 * @param offset seek file position
 * @param size maximum bytes read
 * @return number of bytes read into buffer.
*/
static off_t
inode_seek_write (struct inode_disk *di, const char *buf, off_t offset, 
                  off_t size)
{
  if (offset >= MAXFILE) {
    /* Cannot write outside of maxfile. */
    return 0;
  }
  if (di->magic != INODE_MAGIC) {
    PANIC ("not inode_disk");
  }
  const off_t sec_of = sec_off (offset);
  /* bytes = min(bytes, size); */
  const off_t bytes = (size >= (BLOCK_SECTOR_SIZE - sec_of)) 
                    ? (BLOCK_SECTOR_SIZE - sec_of) : size;

  /* Try direct block first. */
  if (offset < DIRECT_SIZE) {
    /* Index into the page */
    const int isec = offset / BLOCK_SECTOR_SIZE;
    ASSERT (isec <= 123);
    /* Test the sector. */
    if (di->addrs[isec] == INODE_INVALID) {
      /* create and cache the page. */
      int new_sec;
      if (!free_map_allocate (1, &new_sec)) {
        /* failure */
        return 0;
      }

      di->addrs[isec] = new_sec;
    }

    /* fetch the page and write. */
    char *dat = bio_write (di->addrs[isec]);
    if (!bio_pin_sec (dat)) {
      PANIC ("bio pin");
    }
    memcpy (dat + sec_of, buf, bytes);
    if (!bio_unpin_sec (dat)) {
      PANIC ("bio unpin");
    }

    return bytes;
  }

  /* Then try single indirect block. */
  offset -= DIRECT_SIZE;
  if (offset < SINGLE_INDIR_SIZE) {
    PANIC ("not implemented");
  }

  /* Then try doubly indirect block */
  PANIC ("not implemented");

  return bytes;
}

/** List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/** Protect against concurrent access to inode list */
static struct lock inode_list_lock;

/** WARNING: to avoid deadlock, you must hold inode_list_lock BEFORE holding
   locks belonging to an inode. */

/** Initializes the inode module. */
void
inode_init (void) 
{
  /* dinode must be exactly BLOCK_SECTOR_SIZE. */
  STATIC_ASSERT (sizeof (struct inode_disk) == BLOCK_SECTOR_SIZE);
  /* Indirect block must be exactly block sector size. */
  STATIC_ASSERT (sizeof (struct indirect_block) == BLOCK_SECTOR_SIZE);
  /* Must support file of 8MB or larger */
  STATIC_ASSERT (MAXFILE >= (8 * 1024 * 1024));

  /* Inode sizes should be sector-aligned. */
  STATIC_ASSERT (DIRECT_SIZE % BLOCK_SECTOR_SIZE == 0);
  STATIC_ASSERT (SINGLE_INDIR_SIZE % BLOCK_SECTOR_SIZE == 0);
  STATIC_ASSERT (DOUBLY_INDIR_SIZE % BLOCK_SECTOR_SIZE == 0);

  list_init (&open_inodes);
  lock_init (&inode_list_lock);
}

/** Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool 
inode_create (block_sector_t sec, off_t size)
{
  /* Fetch and pin the sector. */
  struct inode_disk *di = bio_write (sec);
  if (di == NULL)
    return false;
  if (!bio_pin_sec (di)) {
    PANIC ("inode_create pin");
  }

  di->type = INODE_FILE;
  di->size = size;
  /* TODO: set a reasonable nlink. */
  di->nlink = 1;

  for (int i = 0; i < 125; ++i)
    {
      di->addrs[i] = INODE_INVALID;
    }
  di->magic = INODE_MAGIC;

  /* Unpin the page, done. */
  if (!bio_unpin_sec (di)) {
    PANIC ("inode_create unpin");
  }
  return true;
}

/** Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  lock_acquire (&inode_list_lock);

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          lock_release (&inode_list_lock);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL) {
    lock_release (&inode_list_lock);
    return NULL;
  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->lk);

  /* Do not fetch the sector for now. */
#if 0
  block_read (fs_device, inode->sector, &inode->data);
#endif

  /** Release all locks */
  lock_release (&inode_list_lock);
  return inode;
}

/** Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL) {
    lock_acquire (&inode->lk);
    inode->open_cnt++;
    lock_release (&inode->lk);
  }
  return inode;
}

/** Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/** Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /** Why? because of possible list_remove! */
  lock_acquire (&inode_list_lock);
  lock_acquire (&inode->lk);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          inode_deallocate (inode);
        }

      lock_release (&inode->lk);
      lock_release (&inode_list_lock);
      free (inode); 
      return;
    }

  lock_release (&inode->lk);
  lock_release (&inode_list_lock);
}

/** Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  lock_acquire (&inode->lk);
  inode->removed = true;
  lock_release (&inode->lk);
}

/** Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  /* Fetch and pin the page(DO NOT WRITE inode->data) */
  const struct inode_disk *sec = bio_read (inode->sector);
  if (!bio_pin_sec (sec))
    PANIC ("bio pin");

  /** Hint: enter critical section */
  lock_acquire (&inode->lk);

  /* If seek outof range, return. */
  if (offset >= sec->size)
    goto read_done;

  /* Seek offset */
  while (size > 0) {
    /* call reader. */
    const off_t bread = inode_seek_read (sec, buffer, offset, size);
    ASSERT (bread <= size);

    /* Advance. */
    offset += bread;
    buffer += bread;
    size -= bread;
  }

  /* Done. */
read_done:
  if (!bio_unpin_sec (sec))
    PANIC ("bio unpin");
  lock_release (&inode->lk);
  return bytes_read;
}

/** Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  /* Not implemented. */
  return 0;
}

/** Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  lock_acquire (&inode->lk);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_release (&inode->lk);
}

/** Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  lock_acquire (&inode->lk);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  lock_release (&inode->lk);
}

/** Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  ASSERT (inode != NULL);
  off_t ret = 0;

  /* Create critical section */
  lock_acquire (&inode->lk);
  const struct inode_disk *di = bio_read (inode->sector);
  if (!bio_pin_sec (di))
    PANIC ("bio pin");

  /* read the size data */ 
  if (di->magic != INODE_MAGIC)
    PANIC ("not inode sector");
  ret = di->size;

length_done:
  if (!bio_unpin_sec (di))
    PANIC ("bio unpin");
  lock_release (&inode->lk);
  return ret;
}

#endif /**< FILESYS */
