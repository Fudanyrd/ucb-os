#include <debug.h>
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/vaddr.h"

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                       Memory Mapped File Impl 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/** Allocate a file map id, returns -1 if failure. */
static int
mapid_alloc (void)
{
  struct thread *cur = thread_current ();
  struct process_meta *meta = cur->meta;
  for (int i = 0; i < NMMAP; ++i) {
    if (meta->mmaptb[i] == NULL) /* Success */
      return i;
  }
  return -1;
}

static void *
mapid_lookup (int md)
{
  if (md < 0 || md >= NMMAP)
    return NULL;
  struct thread *cur = thread_current ();
  struct process_meta *meta = cur->meta;
  return meta->mmaptb[md];
}

static void
mapid_free (int md)
{
  /* validate map id. */
  ASSERT (md >= 0 && md < NMMAP);
  struct thread *cur = thread_current ();
  struct process_meta *meta = cur->meta;

  /* Zero out the slot for future use. */
  meta->mmaptb[md] = NULL;
}

static void unmap_from (void *map_file_rt, void *upage, 
                        unsigned bytes, int writeback);

/** Map a file object mf at address upage.
 * @return the mapping id. -1 if failure.
 */
int 
vm_mmap (struct file *fobj, void *upage)
{
  if (fobj == NULL || upage == NULL)
    return -1;

  /* No. must be page aligned. */ 
  if (pg_ofs (upage) != 0U)
    return -1;
  
  unsigned int left = file_length (fobj);
  if (left == 0U) /* do not create for zero-len files. */
    return -1;

  int md = mapid_alloc ();
  /* Maximum allowed exceed */
  if (md < 0)
    return -1;
  ASSERT (md < NMMAP);
  /* Record mmap */
  struct thread *cur = thread_current ();
  struct process_meta *meta = cur->meta;
  meta->mmaptb[md] = upage;

  /* record start of mapping. */
  void *mstart = upage;
  /* record total bytes mapped(plus offset of a file). */
  off_t offset = 0;

  /* Execute mapping. */
  while (left > 0U)
    {
      if (vm_is_present (upage)) /* Overlapping detected! */
        goto mmap_fail;

      struct map_file *mf = malloc (sizeof (struct map_file));
      if (mf == NULL) /* Fail to allocate slot! */
        goto mmap_fail;

      /* initialize data members */
      mf->fobj = file_reopen (fobj);
      mf->offset = offset;
      mf->read_bytes = left >= PGSIZE ? PGSIZE : left;
      mf->mmap = 1;
      mf->writable = 1;

      /* Install the page in the table. */
      if (!map_file (meta->map_file_rt, mf, upage))
        PANIC ("Should not fail map file");

      /* Advance. */
      upage += PGSIZE;
      offset += mf->read_bytes; // offset is incremented only if a file 
                                // mapping succeeded.
      left -= mf->read_bytes;
    }

  /* Success. */
  return md;
mmap_fail:
  /* Free map id. */
  mapid_free (md);
  /* From start to last mapped, unmap all pages! */
  unmap_from (meta->map_file_rt, mstart, offset, 0);
  return -1; 
}

int 
vm_unmap (int md)
{
  void *upage = mapid_lookup (md);
  if (upage == NULL)
    return -1;

  struct thread *cur = thread_current (); 
  struct process_meta *meta = cur->meta;
  struct map_file *mf = map_file_lookup (meta->map_file_rt, upage); 
  ASSERT (mf != NULL);  /* Must not be null. */
  off_t flen = file_length (mf->fobj);
  unmap_from (meta->map_file_rt, upage, flen, 1);

  /* Free mapid */
  meta->mmaptb[md] = NULL;
  return 0;
}

  /* No. no overlap with code, data, stack, other mapping, etc. */

/** In the mapping, unmap from an address. 
 * This includes unmap in the table, close fobj, free map_file struct.
 * @param writeback if nonzero, write back [dirty] pages.
 */
static void 
unmap_from (void *map_file_rt, void *upage, 
            unsigned int bytes, int writeback)
{
  /* validate parameters */
  if (bytes == 0)
    return;
  struct map_file *first = map_file_lookup (map_file_rt, upage);
  if (first == NULL) /* Not mapped at all(or fail at first alloc )*/
    return;

  /* page directory. */
  struct thread *cur = thread_current ();
  uint32_t *pgtbl = cur->pagedir;
  struct process_meta *meta = cur->meta;
  struct frame_table *ftb = &(meta->frametb);
  
  /* First mapped fobj */
  unsigned int left = bytes;
  off_t offset = 0;

  /* Start unmapping. */
  while (left > 0U)
    {
      struct map_file **mfpp = map_file_walk (map_file_rt, upage);

      /* Validate return value. */
      if (mfpp == NULL)
        PANIC ("unmapping encounted dir page missing");
      struct map_file *mf = *mfpp;
      if (mf == NULL)
        PANIC ("unmapping encounted mfobj missing");
      if (!mf->mmap)
        PANIC ("not mapped by mmap!");
      /* Validate offset consistency. */
      ASSERT (offset == mf->offset);
      
      /* If writeback, present and dirty, writeback. */
      if (writeback && pagedir_is_dirty (pgtbl, upage)) {
        /* Hint: if upage is dirty, then it must exist in memory! */
        void *kpage = pagedir_get_page (thread_current ()->pagedir, upage);
        if (kpage != NULL)
          file_write_at (mf->fobj, kpage, mf->read_bytes, mf->offset);
      }
      
      /* If writeback = 0, then it must be from mmap; else from munmap. */
      if (writeback) {
        /* If page exist, unmap from memory. */
        pagedir_clear_page (pgtbl, upage);
        /* Also unmap from frame table!(why?) */
        for (int i = 0; i < NFRAME; ++i) {
          if (ftb->upages[i] == upage)
            ftb->upages[i] = NULL;
        }
      }

      /* Advance. */
      offset += mf->read_bytes;
      ASSERT (left >= mf->read_bytes);
      left -= mf->read_bytes;
      upage += PGSIZE;

      /* Close fobj, free mf. */
      file_close (mf->fobj);
      free (mf);
      
      /* Zero out this slot */
      *mfpp = NULL;
    }
  /* All jobs done. */
}

/** +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *                     Memory Mapped File Checklist 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/*
  1. mmap fd must be valid;
  2. should written back on munmap if data is modified.
  3. mapping persist after close.
  4. write back on exit.
  5. mapping is NOT inherited
  6. mapping addr is page aligned(else fail)
  7. NOT mapping to nullptr.
  8. NOT mapping over code segment
  9. NOT mapping over data segment
  10. NOT mapping over stack
  11. NO overlapping mapping
  12. mapping should exist on close and delete.
  13. ALLOWED to map a file twice.
  14. Mapping a file of len 0 should either success or fail, but the address
      should be not useable(i.e. not mapped).
*/
