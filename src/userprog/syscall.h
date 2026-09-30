#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>
#include "threads/synch.h"

/*one lock around the whole file system,since it isn't safe for many threads at once*/
extern struct lock filesys_lock;

void syscall_init (void);
void sys_exit (int status) NO_RETURN;
void close_all_files (void);

#endif /* userprog/syscall.h */
