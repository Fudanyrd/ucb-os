#ifndef BIO_H
#define BIO_H

/** Buffer cache implementation, use LRU eviction policy. */

#include "devices/block.h"
#include "filesys.h"

/** Block cache package */
struct bio_pack
  {
    char          *cache; /**< cache space, 512 bytes */
    block_sector_t sec;   /**< Sector number */
  };

void bio_init (void);
int bio_pin (block_sector_t sec);
int bio_pin_sec (char *sec);
int bio_unpin_sec (char *sec);
int bio_unpin (block_sector_t sec);
void bio_flush (void);
struct bio_pack bio_new (void);
const char *bio_read (block_sector_t sec);
char *bio_write (block_sector_t sec);
int bio_free_sec (char *sec);

#endif  /**< filesys/bio.h */
