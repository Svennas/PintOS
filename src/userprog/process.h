#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

<<<<<<< HEAD
struct parent_child {
    int exit_status;
    int alive_count;
    int tid;
    struct list_elem child;
    struct parent_child* parent;
};

#endif /* userprog/process.h */
=======
/* Struct to replace fn_copy when using thread_create. Contains char *copy
   to replace fn_copy to setup stack and semaphore block to block the
   parent thread during setup of child thread. */
/*struct synch*/
/* <<<< Added for lab 3 >>>> */
/*{
  char *copy;
  bool child_load_success;       /* Used to check if load of child process was
                                    successful. */
  //struct semaphore parent_block; /* Used to block parent thread while starting
                                    //child thread. */
  //struct thread *parent;         /* Keeps track of parent thread. */
  //struct thread *child;          /* Keeps track of child thread. */
//};

#endif /* userprog/process.h */
>>>>>>> 9b76f7d931fc2adbce2821f2ce898fbb19acc3cf
