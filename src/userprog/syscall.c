#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* one open file of a process, kept in thread's fd_list */
struct file_desc
  {
    int fd;                             /* Number the user sees. */
    struct file *file;                  /* The open file. */
    struct list_elem elem;              /* Elem in thread's fd_list. */
  };

struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static bool is_valid_ptr (const void *uaddr);
static void check_buffer (const void *uaddr, unsigned size);
static void check_string (const char *str);
static int get_syscall_arg (struct intr_frame *f, int index);
static struct file_desc *find_file_desc (int fd);

/* it returns true if UADDR is a valid user virtual address
   that is really mapped in our page directory */
static bool
is_valid_ptr (const void *uaddr)
{
  return uaddr != NULL && is_user_vaddr (uaddr)
         && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL;
}

/* kills the process unless all SIZE bytes at UADDR are valid */
static void
check_buffer (const void *uaddr, unsigned size)
{
  uintptr_t start = (uintptr_t) uaddr;
  uintptr_t end = start + size - 1;
  uintptr_t page;

  if (size == 0)
    return;

  /* first and last byte, and the buffer must not wrap around */
  if (end < start || !is_valid_ptr ((void *) start)
      || !is_valid_ptr ((void *) end))
    sys_exit (-1);

  /* one byte per page in the middle is enough */
  for (page = (uintptr_t) pg_round_up ((void *) start); page < end;
       page += PGSIZE)
    if (!is_valid_ptr ((void *) page))
      sys_exit (-1);
}

/* kills the process unless STR is a valid string, up to its '\0' */
static void
check_string (const char *str)
{
  for (;;)
    {
      if (!is_valid_ptr (str))
        sys_exit (-1);
      if (*str == '\0')
        return;
      str++;
    }
}

/* Reads the INDEXth argument (0-based) from the interrupt */
static int
get_syscall_arg (struct intr_frame *f, int index)
{
  int *arg_ptr = (int *) f->esp + 1 + index;
  check_buffer (arg_ptr, sizeof *arg_ptr);
  return *arg_ptr;
}

/* returns our open file with number FD, or NULL if there is none */
static struct file_desc *
find_file_desc (int fd)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->fd_list); e != list_end (&cur->fd_list);
       e = list_next (e))
    {
      struct file_desc *desc = list_entry (e, struct file_desc, elem);
      if (desc->fd == fd)
        return desc;
    }
  return NULL;
}

/* sets our exit status and terminates. process_exit() prints the
   "name: exit(status)" line, so kills by the kernel print it too */
void
sys_exit (int status)
{
  thread_current ()->exit_status = status;
  thread_exit ();
}

/* closes every file the current process still has open.
   called from process_exit() */
void
close_all_files (void)
{
  struct thread *cur = thread_current ();

  while (!list_empty (&cur->fd_list))
    {
      struct list_elem *e = list_pop_front (&cur->fd_list);
      struct file_desc *desc = list_entry (e, struct file_desc, elem);

      lock_acquire (&filesys_lock);
      file_close (desc->file);
      lock_release (&filesys_lock);
      free (desc);
    }
}

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  check_buffer (f->esp, sizeof (int));

  int syscall_number = *(int *) f->esp;

  /* halt syscall - shuts down pintos */
  if (syscall_number == SYS_HALT)
    {
      shutdown_power_off ();
    }

  /* exit syscall - saves the status and terminates thread */
  else if (syscall_number == SYS_EXIT)
    {
      int status = get_syscall_arg (f, 0);
      sys_exit (status);
    }

  /* exec syscall - runs a new program, -1 if it could not load */
  else if (syscall_number == SYS_EXEC)
    {
      const char *cmd_line = (const char *) get_syscall_arg (f, 0);
      check_string (cmd_line);
      f->eax = process_execute (cmd_line);
    }

  /* wait syscall - waits for a child and gives its exit status */
  else if (syscall_number == SYS_WAIT)
    {
      tid_t pid = get_syscall_arg (f, 0);
      f->eax = process_wait (pid);
    }

  /* create syscall - makes a new file of the given size */
  else if (syscall_number == SYS_CREATE)
    {
      const char *file = (const char *) get_syscall_arg (f, 0);
      unsigned initial_size = (unsigned) get_syscall_arg (f, 1);
      check_string (file);

      lock_acquire (&filesys_lock);
      f->eax = filesys_create (file, initial_size);
      lock_release (&filesys_lock);
    }

  /* remove syscall - deletes a file (open copies keep working) */
  else if (syscall_number == SYS_REMOVE)
    {
      const char *file = (const char *) get_syscall_arg (f, 0);
      check_string (file);

      lock_acquire (&filesys_lock);
      f->eax = filesys_remove (file);
      lock_release (&filesys_lock);
    }

  /* open syscall - opens a file and gives back a new fd, or -1 */
  else if (syscall_number == SYS_OPEN)
    {
      const char *file = (const char *) get_syscall_arg (f, 0);
      struct thread *cur = thread_current ();
      struct file_desc *desc;
      struct file *opened;
      check_string (file);

      lock_acquire (&filesys_lock);
      opened = filesys_open (file);
      lock_release (&filesys_lock);

      desc = opened != NULL ? malloc (sizeof *desc) : NULL;
      if (desc == NULL)
        {
          /* either no such file or out of memory */
          lock_acquire (&filesys_lock);
          file_close (opened);
          lock_release (&filesys_lock);
          f->eax = -1;
        }
      else
        {
          desc->fd = cur->next_fd++;
          desc->file = opened;
          list_push_back (&cur->fd_list, &desc->elem);
          f->eax = desc->fd;
        }
    }

  /* filesize syscall - size of an open file in bytes */
  else if (syscall_number == SYS_FILESIZE)
    {
      struct file_desc *desc = find_file_desc (get_syscall_arg (f, 0));

      if (desc == NULL)
        f->eax = -1;
      else
        {
          lock_acquire (&filesys_lock);
          f->eax = file_length (desc->file);
          lock_release (&filesys_lock);
        }
    }

  /* read syscall - fd 0 reads the keyboard, others read a file */
  else if (syscall_number == SYS_READ)
    {
      int fd = get_syscall_arg (f, 0);
      uint8_t *buffer = (uint8_t *) get_syscall_arg (f, 1);
      unsigned size = (unsigned) get_syscall_arg (f, 2);
      struct file_desc *desc;

      check_buffer (buffer, size);

      if (fd == 0)
        {
          unsigned i;
          for (i = 0; i < size; i++)
            buffer[i] = input_getc ();
          f->eax = size;
        }
      else if ((desc = find_file_desc (fd)) != NULL)
        {
          lock_acquire (&filesys_lock);
          f->eax = file_read (desc->file, buffer, size);
          lock_release (&filesys_lock);
        }
      else
        {
          f->eax = -1;
        }
    }

  /* write syscall - fd 1 writes the console, others write a file */
  else if (syscall_number == SYS_WRITE)
    {
      int fd = get_syscall_arg (f, 0);
      const void *buffer = (const void *) get_syscall_arg (f, 1);
      unsigned size = (unsigned) get_syscall_arg (f, 2);
      struct file_desc *desc;

      check_buffer (buffer, size);

      if (fd == 1)
        {
          putbuf (buffer, size);
          f->eax = size;
        }
      else if ((desc = find_file_desc (fd)) != NULL)
        {
          lock_acquire (&filesys_lock);
          f->eax = file_write (desc->file, buffer, size);
          lock_release (&filesys_lock);
        }
      else
        {
          f->eax = -1;
        }
    }

  /* seek syscall - moves the read/write position of an open file */
  else if (syscall_number == SYS_SEEK)
    {
      struct file_desc *desc = find_file_desc (get_syscall_arg (f, 0));
      unsigned position = (unsigned) get_syscall_arg (f, 1);

      if (desc != NULL)
        {
          lock_acquire (&filesys_lock);
          file_seek (desc->file, position);
          lock_release (&filesys_lock);
        }
    }

  /* tell syscall - current read/write position of an open file */
  else if (syscall_number == SYS_TELL)
    {
      struct file_desc *desc = find_file_desc (get_syscall_arg (f, 0));

      if (desc == NULL)
        f->eax = -1;
      else
        {
          lock_acquire (&filesys_lock);
          f->eax = file_tell (desc->file);
          lock_release (&filesys_lock);
        }
    }

  /* close syscall - closes an fd, bad or already closed fds are ignored */
  else if (syscall_number == SYS_CLOSE)
    {
      struct file_desc *desc = find_file_desc (get_syscall_arg (f, 0));

      if (desc != NULL)
        {
          list_remove (&desc->elem);
          lock_acquire (&filesys_lock);
          file_close (desc->file);
          lock_release (&filesys_lock);
          free (desc);
        }
    }

  /* unknown syscall - kill the process */
  else
    {
      sys_exit (-1);
    }
}
