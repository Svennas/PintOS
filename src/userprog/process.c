#include "userprog/process.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmd_line) 
{
  //printf("In process_execute, current thread ID: %d\n",thread_current()->tid);

  char *fn_copy;
  tid_t tid;

  struct parent_child* status = (struct parent_child*) malloc(sizeof(struct parent_child));
  struct thread* curr = thread_current();

  sema_init(&(curr->wait), 0);    // Init sema for waiting while creating new process
  sema_init(&(status->sleep), 0); // Sema to wait until child has exited

  /* Allocate page for fn_copy, which here is empty. */
  fn_copy = palloc_get_page (0);

  if (fn_copy == NULL) 
  {
    //printf("fn_copy == NULL\n");
    status->exit_status = -1; // So we know that it failed
    status->alive_count = 1;  // Parent is still alive
    return TID_ERROR;

  /* Make a copy of cmd_line.
     Otherwise there's a race between the caller and load(). */
  strlcpy (fn_copy, cmd_line, PGSIZE);
  
  status->fn_copy = fn_copy;
  status->parent = curr;
  status->has_waited = false;
  status->load_success = true;

  /* Add status to the list in the current thread (parent) */
  list_push_front(&(curr->children), &(status->child)); 

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (cmd_line, PRI_DEFAULT, start_process, status);

  /* Make current thread wait while child start executing. */
  sema_down(&(curr->wait)); 

  /* Couldn't allocate thread. */  
  if (tid == TID_ERROR) 
  {     
    //printf("TID_ERROR!\n");
    palloc_free_page (fn_copy);

    status->exit_status = -1; // So we know that it failed
    status->alive_count = 1;  // Parent is still alive

    /* Child thread is finished, let parent continue executing. */
    sema_up(&(status->parent->wait));
  }
  status->child_tid = tid;

  if ((!status->load_success)) return -1;

  //printf("\nend of process exec: Current thread ID: %d\n\n",thread_current()->tid);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *aux)
{ 
  //printf("in start_process\n");
  struct parent_child* status = aux;

  char *file_name = status->fn_copy;

  struct intr_frame if_; 
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Loads the program by: 
       - Allocating and activating page directory
       - Setting up the stack.*/
  success = load (file_name, &if_.eip, &if_.esp);

  /* Free the allocated page for fn_copy. */
  palloc_free_page (file_name);

  status->exit_status = 0;    // Initial value 
  status->alive_count = 2;    // Initial value, both child and parent are alive
  
  /* If load failed, quit. */
  if (!success) {
    //printf("no success\n");
    status->load_success = false;
    status->exit_status = -1; // So we know that it failed
    status->alive_count = 1;  // Parent is still alive
    sema_up(&(status->parent->wait));
    thread_exit ();
  }
  /* Not in critical section anymore. Let parent continue executing. */
  sema_up(&(status->parent->wait));

  thread_current()->parent_info = status;
  //printf("end of start process\n"); 

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED (); 
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.
*/
int
process_wait (tid_t child_tid) //Return the childs exit status
{
  //printf("\n(In process_wait()) Current thread ID: %d\n",thread_current()->tid);

  struct thread* curr = thread_current();
  
  struct list_elem *e;

  for (e = list_begin (&curr->children); e != list_end (&curr->children);
        e = list_next (e))
  {
      struct parent_child *status = list_entry (e, struct parent_child, child);
      
      if (status->child_tid == child_tid)   // Need to get the specified child process
      {
        // Return -1 immediately if child has already called wait()
        if (status->has_waited) return -1;

        status->has_waited = true; // Make this child can only wait once

        if (status->alive_count == 1)       // Child has exited
        {
          //printf("\nChild has exited, alive count = 1\n\n");
          return status->exit_status; 
        }
        else if (status->alive_count == 2)  // Child has not exited, wait for it
        { 
          //printf("Child has not exited, will wait\n");
          sema_down(&(status->sleep));  // Wait until child has exited
          //printf("\nPArent done sleeping\n\n");

          if (status->alive_count == 1) 
          {
            //printf("\nParent done sleeping and Alive count == 1\n\n");
            return status->exit_status;
          }   
        }
        else // Something is wrong, should never get here 
        {
          //printf("\nSomething is wrong\n\n");
          status->exit_status = -1;
          return status->exit_status; 
        }
      }
  }
  //printf("\nFound no child with that tid\n\n");
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{ 
  //printf("Process_exit\n");
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Changed in lab 4. Sets up the stack with the initial function and its
   arguments. */ 
static bool setup_stack (void **esp, int argc, char* argv[]);


static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage, uint32_t uoffset,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *cmd_line, void (**eip) (void), void **esp) 
{
  //printf("in load\n");
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  char *token, *save_ptr;
  char* argv[MAX_NR_ARGS] = {NULL};
  int argc = 0;

  for (token = strtok_r (cmd_line, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
  {
    argv[argc] = token;
    argc++;
    if (argc == MAX_NR_ARGS - 1) break;
  }
  argv[argc] = NULL;    // Last elem is NULL

  // Get file_name 
  char* file_name;
  file_name = argv[0]; 

  strlcpy (t->name, file_name, sizeof t->name);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Set up stack. */
  if (!setup_stack (esp, argc, argv)){
    goto done;
  }

   /* Uncomment the following line to print some debug
     information. This will be useful when you debug the program
     stack.*/
//#define STACK_DEBUG

#ifdef STACK_DEBUG
  printf("*esp is %p\nstack contents:\n", *esp);
  hex_dump((int)*esp , *esp, PHYS_BASE-*esp+16, true);
  printf("-------------END OF HEX_DUMP-------------\n");
  /* The same information, only more verbose: */
  /* It prints every byte as if it was a char and every 32-bit aligned
     data as if it was a pointer. */
  void * ptr_save = PHYS_BASE;
  i=-15;  // 16/4 = 4, 4 frames
  while(ptr_save - i >= *esp) {
    char *whats_there = (char *)(ptr_save - i);
    // show the address ...
    printf("%x\t", (uint32_t)whats_there);
    // ... printable byte content ...
    if(*whats_there >= 32 && *whats_there < 127)
      printf("%c\t", *whats_there);
    else
      printf(" \t");
    // ... and 32-bit aligned content 
    if(i % 4 == 0) {
      uint32_t *wt_uint32 = (uint32_t *)(ptr_save - i);
      printf("%x\t", *wt_uint32);
      printf("\n-------");
      if(i != 0)
        printf("------------------------------------------------");
      else
        printf(" the border between KERNEL SPACE and USER SPACE ");
      printf("-------");
    }
    printf("\n");
    i++;
  }
#endif
  
  /* Open executable file. */
  file = filesys_open (file_name); 
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_offset = phdr.p_offset;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */

                  read_bytes = phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes - page_offset);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE) - page_offset;
                }
              if (!load_segment (file, file_offset, (void *) mem_page, page_offset,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage, uint32_t page_offset,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((page_offset + read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);

  struct thread *t = thread_current();
  
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = page_offset + read_bytes;
      if (page_read_bytes > PGSIZE)
            page_read_bytes = PGSIZE;

      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      bool new_kpage = false;
      uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
      if (!kpage)
      {
            new_kpage = true;
            kpage = palloc_get_page (PAL_USER);
      }

      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage + page_offset, page_read_bytes - page_offset) != (int) (page_read_bytes - page_offset))
      {
        if (new_kpage)
		palloc_free_page (kpage);
        return false;

      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */       
      if (new_kpage)
      {
            if (!install_page (upage, kpage, writable))
            {
                  palloc_free_page (kpage);
                  return false;
            }
      }

      /* Advance. */
      read_bytes -= page_read_bytes - page_offset;
      zero_bytes -= page_zero_bytes;
      page_offset = 0;		
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, int argc, char* argv[])
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        *esp = PHYS_BASE; 
        void* s_ptr = *esp; 

        void** arg_ptrs[argc];    // To save pointers to arguments
        char* curr_arg;           // To easier handle the current argument

        /* Push arguments to stack in reverse order. 
           From testing in lab 5, need to add null somewhere. */ 
        for (int c = argc-1; c >= 0; c--)
        { 
          int size = strlen(argv[c]);
          curr_arg = argv[c];
          char *next_arg = curr_arg - 1;
          curr_arg += size;
          for(; curr_arg != next_arg; curr_arg--)
          {
            s_ptr--;  // Move address one step up the stack
            *((char*)s_ptr) = *curr_arg;    // Push char to stack
          }
          arg_ptrs[c] = s_ptr;  // Save the address to the first char in every arg
        }

        /* Align the stack pointer to a multiple of 4 bytes */ 
        s_ptr -= ((int)s_ptr % 4) + 4;  // Make sure there is atleast 4 addresses between

        /* Push argv[argc] (NULL) to the stack. */
        s_ptr -= sizeof(char*);
        *((char**)s_ptr) = (char*)(argv[argc]);
        //memcpy((char*)s_ptr, ((char*)(arg_ptrs[argc])), sizeof(char*));

        /* Push the address of each string on the stack, in right-to-left order. */
        char** arg_adrs;
        for (int c = argc-1; c >= 0; c--)
        {
          s_ptr -= sizeof(char*);
          memcpy(s_ptr, &(arg_ptrs[c]), sizeof(char*));
          //printf("%p\n", (char*)s_ptr);
          /* After argv[0], push argv (the address of argv[0]) on the stack. */
          if (c == 0)
          {
            arg_adrs = s_ptr;
            s_ptr -= ((argc * 2) * 4);  // Move stack pointer as shown in Lesson 2 pdf
            memcpy(s_ptr, &arg_adrs, sizeof(char**));
          }
        }

        /* Push argc on the stack */
        s_ptr -= sizeof(int);     // Point to new address
        memcpy(s_ptr, &argc, sizeof(int));

        /* Push a fake "return address" on the stack. */
        void* fake;
        s_ptr -= sizeof(void*);
        memcpy(s_ptr, &fake, sizeof(void*));

        /* Assign the stack pointer to esp. */
        *esp = s_ptr;
      }
      

      else {
        palloc_free_page (kpage);
      }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
