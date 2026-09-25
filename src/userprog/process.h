#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"

/* Shared record between a parent and one of its children.
   It is freed by whoever of the two finishes last, so the parent
   can still read the exit status after the child is gone. */
struct child_status
  {
    tid_t tid;                          /* Child's thread id. */
    int exit_status;                    /* Child's exit status. */
    bool load_success;                  /* Did the child load its program? */
    bool waited;                        /* Has the parent already waited? */
    int ref_cnt;                        /* 2 while both alive, then 1, then 0. */
    struct semaphore load_sema;         /* Upped once load() finishes. */
    struct semaphore exit_sema;         /* Upped when the child exits. */
    struct list_elem elem;              /* Elem in parent's children list. */
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
