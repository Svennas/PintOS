#include "userprog/syscall.h"


#define ARG_1 (f->esp+4)    /* First argument from stack (not counting syscall nr) */
#define ARG_2 (f->esp+8)    /* Second argument from stack */
#define ARG_3 (f->esp+12)   /* Third argument from stack */
#define ARG_4 (f->esp+16)   /* Fourth argument from stack */

/* FD 0 and 1 are reserved for the console (stdin/stdout). */
#define FD_START 2          /* First value for FD that can be used */
#define FD_END 130          /* Last value able for FD. 128 files should be able to remain 
                               open at the same time. */

static void syscall_handler (struct intr_frame *f);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf("start of syscall_handler\n");
  if (!(is_ptr_valid(f->esp))) exit(-1);

  int syscall_nr = *((int*)f->esp);

  if (syscall_nr == SYS_HALT) 
  {
    halt();
  }

  else if (syscall_nr == SYS_CREATE) 
  {
    //printf("SYS_CREATE\n");
    if (!(is_ptr_valid(ARG_1)) || !(is_ptr_valid(ARG_1))) exit(-1);

    char *create_arg_1 = *((char**)ARG_1);
    if (!(is_str_valid(create_arg_1))) exit(-1);

    unsigned create_arg_2 = *((unsigned*)ARG_2);
    f->eax = create (create_arg_1, create_arg_2);
  }

  else if (syscall_nr == SYS_OPEN) 
  {
    //printf("SYS_OPEN\n");
    if (!(is_ptr_valid(ARG_1))) exit(-1);

    char *open_arg = *((char**)ARG_1);
    if (!(is_str_valid(open_arg))) exit(-1);

    f->eax = open (open_arg);
  }

  else if (syscall_nr == SYS_CLOSE) 
  {
    //printf("SYS_CLOSE\n");
    if (!(is_ptr_valid(ARG_1))) exit(-1);

    int close_arg = *((int*)ARG_1);
    if (!(is_fd_valid(close_arg))) exit(-1);

    close (close_arg);
  }

  else if (syscall_nr == SYS_READ) 
  {
    //printf("SYS_READ\n");
    if (!(is_ptr_valid(ARG_1)) || !(is_ptr_valid(ARG_1)) 
    || !(is_ptr_valid(ARG_1))) exit(-1);

    int read_arg_1 = *((int*)ARG_1);
    if (!(is_fd_valid(read_arg_1))) exit(-1);

    void *read_arg_2 = *((void**)ARG_2);
    unsigned read_arg_3 = *((unsigned*)ARG_3);
    if (!(is_bufr_valid(read_arg_2, read_arg_3))) exit(-1);

    f->eax = read (read_arg_1, read_arg_2, read_arg_3);
  }

  else if (syscall_nr == SYS_WRITE) 
  {
    //printf("SYS_WRITE\n");
    if (!(is_ptr_valid(ARG_1)) || !(is_ptr_valid(ARG_1)) 
    || !(is_ptr_valid(ARG_1))) exit(-1);

    int write_arg_1 = *((int*)ARG_1);
    if (!(is_fd_valid(write_arg_1))) exit(-1);

    void *write_arg_2 = *((void**)ARG_2);
    unsigned write_arg_3 = *((unsigned*)ARG_3);
    if (!(is_bufr_valid(write_arg_2, write_arg_3))) exit(-1);

    f->eax = write (write_arg_1, write_arg_2, write_arg_3);
  }

  else if (syscall_nr == SYS_EXIT) 
  {
    //printf("SYS_EXIT\n");
    if (!(is_ptr_valid(ARG_1))) exit(-1);

    int exit_arg = *((int*)ARG_1);
    exit (exit_arg);
  }

  else if (syscall_nr == SYS_EXEC)
  {
    //printf("SYS_EXIT\n");
    if (!(is_ptr_valid(ARG_1))) exit(-1);

    char *exec_arg = *((char**)ARG_1);
    if (!(is_str_valid(exec_arg))) exit(-1);

    f->eax = exec (exec_arg);
  }

  else if (syscall_nr == SYS_WAIT)
  {
    //printf("SYS_WAIT\n");
    if (!(is_ptr_valid(ARG_1))) exit(-1);

    pid_t wait_arg = *((pid_t*)ARG_1);
    f->eax = wait (wait_arg);
  }

  else 
  {
    //printf ("Not a valid system call!\n");
    exit(-1);
  }
}

/* Shuts down the whole system by using power_off()
(declared in threads/init.h). */
void halt (void) {
  power_off ();
}

/* Creates a new file with name and size of the file from then
given inputs. Uses filesys_create() from filesys/filesys.h */
bool create (const char *file, unsigned initial_size)
{
  return filesys_create (file, initial_size);
}

/* Opens a file with the given name by using filesys_open()
from filesys/filesys.h. Returns -1 if the file is NULL or
the limit for opened files has been exceeded. Returns
the files fd if successful. */
int open (const char *file)
{
  struct thread *t = thread_current();
  int fd = -1;
  for(int i = FD_START; i < FD_END; i++) {
    fd = i;
    if (t->fd_list[fd] == NULL) {
      t->fd_list[fd] = filesys_open(file);
      if (t->fd_list[fd] == NULL) return -1;
      break;
    }
  }
  return fd;
}

/* Closes the file associated with the given fd.
Uses file_close() from filesys/file.h.*/
void close (int fd)
{
  //printf("fd = %i\n", fd);
  struct thread *t = thread_current ();
  file_close(t->fd_list[fd]);
  t->fd_list[fd] = NULL;
}

/* Reads a given size of the file associated with the given file
descriptor into the given buffer. Uses file_read() from filesys/file.h.
If fd is 0, reads from console using input_getc() from devices/input.h.
Returns -1 if the file is NULL. Returns the files size if successful.*/
int read (int fd, void *buffer, unsigned size)
{
  //printf("fd = %i\n", fd);
  struct thread *t = thread_current ();
  struct file *f = t->fd_list[fd];

  if (fd == STDIN_FILENO)
  {
    for (unsigned int i = 0; i < size; i++)
    {
      *((uint8_t*)buffer + i) = input_getc ();
      putbuf (buffer + i, 1); /* So keypress is shown in console. */
    }
    return size;
  }
  else
  {
    if (f == NULL) return -1;
    return file_read (f, buffer, size);
  }
}

/* Writes a given size of the file associated with the given file
descriptor into the given buffer. Uses file_write() from filesys/file.h.
If fd is 1, writes into console by using putbuf() from lib/kernel/console.h.
Returns -1 if the file is NULL. Returns the size of the bytes written
if successful. */
int write (int fd, const void *buffer, unsigned size)
{
  struct thread *t = thread_current();
  struct file *f = t->fd_list[fd];

  if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);
    return size;
  }
  else
  {
    if (f == NULL) exit(-1);
    return file_write (f, buffer, size);
  }
}

/* Closes all the files related to the thread, the uses thread_exit() from 
threads/thread.h to deschedule the current thread and destroy it. 
*/
void exit (int status)
{
  //printf("In exit()\n");
  thread_current()->parent_info->exit_status = status;

  printf("%s: exit(%d)\n", thread_current()->name, 
    thread_current()->parent_info->exit_status);

  for (int i = FD_START; i < FD_END; i++)
  {
    close(i);
  }
  thread_exit();
}

/* Runs the executable whose name is given in cmd line, passing any given
arguments, and returns the new process’ program id (pid). Must return pid -1
if the program cannot load or run for any reason. For now you may ignore the
arguments in cmd line and use only the program name to execute it. */
pid_t exec (const char *cmd_line)
{ 
  //printf("In exec()\n");
  return process_execute (cmd_line);
}

/* Sleep the parent until child finishes and return the child’s exit status.
   Wait for a child process to die. */
int wait (pid_t pid) 
{
  //printf("In wait()\n");
  int temp = process_wait(pid);
  //printf("Exit status is %i\n", temp);
  return temp;
}


/* ------ The following part is for input validation (Lab 5) ------ */


bool is_ptr_valid (void* esp)
{
  if (esp == NULL) return false;

  if (is_kernel_vaddr(esp)) return false;

  if (pagedir_get_page(thread_current()->pagedir, esp) == NULL) return false;

  return true; 
}

/* Checks if the given string is valid. Every address needs in it needs to be checked for
   bad pointers using is_ptr_valid(). If every pointer is valid and it reaches '\0',
   it's a valid string. */
bool is_str_valid (char* str)
{
  for (; ; str++)
  {
    if (!(is_ptr_valid((void*)str))) return false;

    if (*str == '\0') return true;
  }
}

bool is_bufr_valid (void* buffer, unsigned size)
{
  for (unsigned i = 0; i <= size; i++)
  {
    if (!(is_ptr_valid(buffer + i))) return false;
  }
  return true;
}

bool is_fd_valid (int fd)
{
  if (fd >= 130) return false;

  if (fd < 1) return false;

  return true;
}