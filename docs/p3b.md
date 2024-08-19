# Project 3b: Virtual Memory

## Preliminaries

>Fill in your name and email address.

Rundong Yang <3096842671@qq.com>

>If you have any preliminary comments on your submission, notes for the TAs, please give them here.

We claim to get **91.8** points for testing out of 100 in this project.

>Please cite any offline or online sources you consulted while preparing your submission, other than the Pintos documentation, course text, lecture notes, and course staff.

(void)

## Stack Growth

#### ALGORITHMS

>A1: Explain your heuristic for deciding whether a page fault for an
>invalid virtual address should cause the stack to be extended into
>the page that faulted.

If the faulting address is above the value of `esp-12`, then it is on the stack, and the page
should be installed in the page table.

## Memory Mapped Files

#### DATA STRUCTURES

>B1: Copy here the declaration of each new or changed struct or struct member, global or static variable, typedef, or enumeration.  Identify the purpose of each in 25 words or less.

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

In the `map_file` structure, the `fobj` provide the file mapped in the page, and `offset`
denotes where to read the file when initializing the frame.

#### ALGORITHMS

>B2: Describe how memory mapped files integrate into your virtual
>memory subsystem.  Explain how the page fault and eviction
>processes differ between swap pages and other pages.

As described in [p3a.md](./p3a.md), the process of looking up a frame is:
<ol>
  <li>Check that required page is not null;</li>
  <li>Query the process's page table for the frame; </li>
  <li>Query the process's swap table for the frame; </li>
  <li>Query the process's memory-mapped file table for the frame; </li>
  <li>In the process, a frame may be eviced. If all attempts failed, return null. </li>
</ol>

In our implementation, memory-mapped file, together with executable, is integrated in the
memory mapped table(organized in a page directory like, tree structure).

>B3: Explain how you determine whether a new file mapping overlaps
>any existing segment.

This is done via the `vm_is_present` functionality(in [vm/vm_util.c](../src/vm/vm_util.c)). The
`mmap` syscall executor will first use this functionality to determine whether mapped address causes
confliction, hence any overlaps will be detected.

#### RATIONALE

>B4: Mappings created with "mmap" have similar semantics to those of
>data demand-paged from executables, except that "mmap" mappings are
>written back to their original files, not to swap.  This implies
>that much of their implementation can be shared.  Explain why your
>implementation either does or does not share much of the code for
>the two situations.

We clearly do this sharing. Executables and memory mapped files share the same data structure,
as described above. In this way, executable and memory mapped files can be implemented "almost" 
in the same way.

However, we also spot crucial difference between executable and memory mapped files:
dirty memory-mapped page should be written back on process exit. Hence, we store a member in the
`map_file` structure to avoid writing to executable. 
