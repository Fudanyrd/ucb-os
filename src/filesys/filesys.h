#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/off_t.h"

/** Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /**< Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /**< Root directory file inode sector. */
#define INVALID_SECTOR -1       /**< Used to denote a sector not exists. */

/** Block device that contains the file system. */
struct block *fs_device;

/** Force serial execution of filesys operations */
struct lock *filesys_lock;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

int filesys_walk (int from, const char *path, char *tmp);
int filesys_leave (int from, const char *path, char *tmp);

int fs_create (const char *name, off_t initial_size);
int fs_mkdir (const char *name, off_t initial_size);
struct file *fs_open (const char *name);
int fs_remove (const char *name);
int fs_chdir (const char *name);

#endif /**< filesys/filesys.h */
