#include "filesys/filesys.h"
#include "common.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/bio.h"
#include "userprog/process.h"

/** Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/** Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
#ifdef FS_NOT_IMPL
  /* Set this, only check compilation. */
  PANIC ("Not implemented yet. PRs welcome!");
#endif

  bio_init ();
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/** Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  bio_flush ();
  free_map_close ();
}

/** Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, INODE_FILE)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/** Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  struct file *ret = file_open (inode);
  return ret;
}

/** Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/** Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/** Walk down the fs tree to find the inode sector of path. 
 * @param tmp a temp buffer of length at least NAME_MAX + 1.
 * This will avoid stack overflow of deep recursion.
 */
int 
filesys_walk (int from, const char *path, char *tmp)
{
  /* End recursion */
  if (from == INVALID_SECTOR || path == NULL) {
    return INVALID_SECTOR;
  }

  /* End recursion */
  if (*path == '\0')
    return from;
  
  /* Fetch file name. */
  int iter = 0;
  for (; path[iter] != '/' && path[iter] != '\0'; ++iter) {
    tmp[iter] = path[iter];
  }
  /* Set terminator. */
  tmp[iter] = '\0';

  /* Skip multiple dash. */
  for (; path[iter] == '/'; ++iter) {}

  /* Open the inode, and keep recursion. */
  struct inode *ino = inode_open (from);
  if (inode_typ (ino) != INODE_DIR) {
    /* Not a directory, abort. */
    inode_close (ino);
    return INVALID_SECTOR;
  }

  /* Open directory, and find destination. */
  struct dir *dir = dir_open (ino);
  from = dir_sec (dir, tmp);

  /* dir_close shall close the inode too. */
  dir_close (ino);

  /* Recurse, reusing the tmp buffer. */
  return filesys_walk (from, path + iter, tmp);
}

/** Almost the same as fs_walk. The only difference is that it does not
 * proceed to the last file of path instead copy its name in tmp.
 */
int 
filesys_leave (int from, const char *path, char *tmp)
{
  /* End recursion */
  if (from == INVALID_SECTOR || path == NULL) {
    return INVALID_SECTOR;
  }

  /* End recursion */
  if (*path == '\0')
    return from;
  
  /* Fetch file name. */
  int iter = 0;
  for (; path[iter] != '/' && path[iter] != '\0'; ++iter) {
    tmp[iter] = path[iter];
  }
  /* Set terminator. */
  tmp[iter] = '\0';

  /* Skip multiple dash. */
  for (; path[iter] == '/'; ++iter) {}

  if (path[iter] == '\0') {
    /* Terminate */
    return from;
  }

  /* Continue. */
  struct inode *ino = inode_open (from);
  if (inode_typ (ino) != INODE_DIR) {
    /* Not a directory, abort. */
    inode_close (ino);
    return INVALID_SECTOR;
  }

  /* Open directory, and find destination. */
  struct dir *dir = dir_open (ino);
  from = dir_sec (dir, tmp);

  /* dir_close shall close the inode too. */
  dir_close (ino);

  /* Recurse, reusing the tmp buffer. */
  return filesys_leave (from, path + iter, tmp);
}

/** Returns the current working dir of thread. */
static int
fs_get_pwd (void)
{
  struct thread *cur = thread_current ();
  struct process_meta *meta = cur->meta;
  ASSERT (meta != NULL);
  return meta->pwd;
}

/* Remove a file or empty directory. */
int 
fs_remove (const char *name)
{
  /* Not implemented */
  return 0;
}

/**  Create a file or directory, 
 * @param name may be absolute or relative.
 */
int 
fs_create (const char *name, off_t initial_size)
{
  int absolute = 0;
  if (*name == '/') {
    absolute = 1;
    /* Abolute path. */
    while (*name == '/') {
      ++name;
    }
  }

  /* Starting directory. */
  int from = absolute ? ROOT_DIR_SECTOR : fs_get_pwd ();

  /* Walk to destination */
  char tmp[NAME_MAX + 2];  /**< buffer */
  int dest;                /**< sector of destination */
  dest = filesys_leave (from, name, tmp);

  if (dest == INVALID_SECTOR) {
    /* Fail */
    return 0;
  }

  /* Open directory. */
  struct inode *ino = inode_open (dest);
  struct dir *dir = dir_open (ino);
  ASSERT (ino != NULL && dir != NULL && inode_typ (ino) == INODE_DIR);

  block_sector_t sec = 0;
  int ret = (
    free_map_allocate (1U, &sec) &&
    inode_create (sec, initial_size, INODE_FILE) &&
    dir_add (dir, tmp, sec)
  );
  if (ret == 0&& sec != 0) 
    free_map_release (sec, 1);
  dir_close (dir);

  return ret;
}
