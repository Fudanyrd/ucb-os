# Project 3a: Virtual Memory

## Preliminaries

>Fill in your name and email address.

Rundong Yang <a href="mailto:3096842671@qq.com">email</a>

>If you have any preliminary comments on your submission, notes for the TAs, please give them here.

(void)

>Please cite any offline or online sources you consulted while preparing your submission, other than the Pintos documentation, course text, lecture notes, and course staff.

(void)

## Page Table Management

#### DATA STRUCTURES

>A1: Copy here the declaration of each new or changed struct or struct member, global or static variable, typedef, or enumeration.  Identify the purpose of each in 25 words or less.

In [vm/vm-util.h](../src/vm/vm-util.h):
```c
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
    short writable;       /**< is the mapped file read-only? */
    short mmap;           /**< 1 if created via syscall mmap */
    /**< see examples below! */
  };
```
The `map_file` struct contains suficient information of how to get and initialize a page from
executable file(and later memory mapped file).

In [vm/vm-util.h](../src/vm/vm-util.h):
```c
#define NFRAME 16
/**< Frame table. */
struct frame_table
  {
    void      *pages[NFRAME];    /**< number of private frames(pages) */
    void      *upages[NFRAME];   /**< record user page mappings */
    int        free_ptr;         /**< index to the next uninitialized frame */
  };
```
Frame table organizes frames used by a process and controls eviction of frames.

In [vm/vm-util.h](../src/vm/vm-util.h):
```c
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
```
Swap table records the position of each evicted dirty frame in the swap device(disk).

#### ALGORITHMS

>A2: In a few paragraphs, describe your code for accessing the data
>stored in the SPT about a given page.

The process of accessing a particular page is described in `vm_fetch_page` in
[vm/vm_util.c](../src/vm/vm_util.c). It does the following:
<ol>
  <li>Check that required page is not null;</li>
  <li>Query the process's page table for the frame; </li>
  <li>Query the process's swap table for the frame; </li>
  <li>Query the process's memory-mapped file table for the frame; </li>
  <li>In the process, a frame may be eviced. If all attempts failed, return null. </li>
</ol>

The reason why we did so in this order is well described in the comments of `vm_fetch_page`.
`vm_is_present` follows similar procedure except that it does not evict and fetch frames.


>A3: How does your code coordinate accessed and dirty bits between
>kernel and user virtual addresses that alias a single frame, or
>alternatively how do you avoid the issue?

Our frame table only maintains user virtual page, which means that kernel page won't be tracked or evicted.

#### SYNCHRONIZATION

>A4: When two user processes both need a new frame at the same time,
>how are races avoided?

Two user processes have separate frame table. This shall avoid any race conditions.

#### RATIONALE

>A5: Why did you choose the data structure(s) that you did for
>representing virtual-to-physical mappings?

Our data structure is organized like a tree. This probably won't consume too many kernel frames.

## Paging To And From Disk

#### DATA STRUCTURES

>B1: Copy here the declaration of each new or changed struct or struct member, global or static variable, typedef, or enumeration.  Identify the purpose of each in 25 words or less.

(void)

#### ALGORITHMS

>B2: When a frame is required but none is free, some frame must be
>evicted.  Describe your code for choosing a frame to evict.

We randomly select a frame in use to evict. We confirm this is a major drawback of
our vm implementation.

>B3: When a process P obtains a frame that was previously used by a
>process Q, how do you adjust the page table (and any other data
>structures) to reflect the frame Q no longer has?

This won't happen at all for we use separate frame table for process P and Q.

#### SYNCHRONIZATION

>B5: Explain the basics of your VM synchronization design.  In
>particular, explain how it prevents deadlock.  (Refer to the
>textbook for an explanation of the necessary conditions for
>deadlock.)

We avoid synchronization problems by creating a frame table, swap table and mmap table
for each separate process, i.e. these data structures is not shared, hence does not incur 
race conditions.

>B6: A page fault in process P can cause another process Q's frame
>to be evicted.  How do you ensure that Q cannot access or modify
>the page during the eviction process?  How do you avoid a race
>between P evicting Q's frame and Q faulting the page back in?

This won't happen at all for we use separate frame table for process P and Q.

>B7: Suppose a page fault in process P causes a page to be read from
>the file system or swap.  How do you ensure that a second process Q
>cannot interfere by e.g. attempting to evict the frame while it is
>still being read in?

This won't happen at all for we use separate frame table for process P and Q.

>B8: Explain how you handle access to paged-out pages that occur
>during system calls.  Do you use page faults to bring in pages (as
>in user programs), or do you have a mechanism for "locking" frames
>into physical memory, or do you use some other design?  How do you
>gracefully handle attempted accesses to invalid virtual addresses?

We handle access to paged-out pages in the same way as `vm_fetch_page` described earlier.
We use page faults to bring in pages in user programs. If `vm_fetch_page` returns null, then
it should be an invalid address. 

#### RATIONALE

>B9: A single lock for the whole VM system would make
>synchronization easy, but limit parallelism.  On the other hand,
>using many locks complicates synchronization and raises the
>possibility for deadlock but allows for high parallelism.  Explain
>where your design falls along this continuum and why you chose to
>design it this way.

Again, we avoid synchronization problems by creating a frame table, swap table and mmap table
for each separate process, i.e. these data structures is not shared, hence does not incur 
race conditions. This method is simple and requires no locks.