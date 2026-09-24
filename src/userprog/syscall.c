#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_ptr (const void *uaddr);
static int get_syscall_arg (struct intr_frame *f, int index);

/* it returns true if UADDR is a valid user virtual address */
static bool
is_valid_ptr (const void *uaddr)
{
  return uaddr != NULL && is_user_vaddr (uaddr);
}

/* Reads the INDEXth argument (0-based) from the interrupt */
static int
get_syscall_arg (struct intr_frame *f, int index)
{
  int *arg_ptr = (int *) f->esp + 1 + index;
  if (!is_valid_ptr (arg_ptr))
    thread_exit ();
  return *arg_ptr;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if (!is_valid_ptr (f->esp))
    thread_exit ();

  int syscall_number = *(int *) f->esp;

  /* halt syscall - shuts down pintos */
  if (syscall_number == SYS_HALT)
    {
      shutdown_power_off ();
    }

  /* exit syscall - prints exit message and terminates thread */
  else if (syscall_number == SYS_EXIT)
    {
      int status = get_syscall_arg (f, 0);
      printf ("%s: exit(%d)\n", thread_current ()->name, status);
      thread_exit ();
    }

  /* write syscall - only fd 1 (console) supported for now */
  else if (syscall_number == SYS_WRITE)
    {
      int fd = get_syscall_arg (f, 0);
      const void *buffer = (const void *) get_syscall_arg (f, 1);
      unsigned size = (unsigned) get_syscall_arg (f, 2);

      if (!is_valid_ptr (buffer))
        thread_exit ();

      if (fd == 1)
        {
          putbuf (buffer, size);
          f->eax = size;
        }
      else
        {
          f->eax = -1;
        }
    }

  /* unknown syscall - kill the process */
  else
    {
      thread_exit ();
    }
}
