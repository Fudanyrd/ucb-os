#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/** System call executor type */
typedef int (*syscall_executor_t)(void *);

void syscall_init (void);

#endif /**< userprog/syscall.h */
