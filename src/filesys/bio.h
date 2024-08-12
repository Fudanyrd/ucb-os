#ifndef BIO_H
#define BIO_H

/** Buffer cache implementation, use LRU eviction policy. */

#include "devices/block.h"
#include "filesys.h"

void bio_init (void);
void bio_read (block_sector_t sector, void *buf);
void bio_write (block_sector_t sector, const void *buf);
void bio_write_through (block_sector_t sec, const void *buf);
void bio_pin (block_sector_t sec);
void bio_unpin (block_sector_t sec);
void bio_flush (void);

#endif  /**< filesys/bio.h */
