#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

// Under work
struct parent_child {   
    int exit_status;         // -1 if the process crashes for any reason, otherwise 0
    int alive_count;         // Can be 0, 1, or 2, depending on how many threads are alive
    struct thread* parent;   // To be able to reach the parent's semaphore
    struct list_elem child;  // Used by parent thread in the children list
    char* fn_copy;           // Needed for start_process to free the allocated page
};

#endif /* userprog/process.h */
