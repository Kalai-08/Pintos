#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>
#include "threads/synch.h"

/* One big lock around the whole file system, since the base
   file system is not safe to use from many threads at once. */
extern struct lock filesys_lock;

void syscall_init (void);
void sys_exit (int status) NO_RETURN;
void close_all_files (void);

#endif /* userprog/syscall.h */
