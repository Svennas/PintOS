#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct parent_child {
    int exit_status;
    int alive_count;
    int tid;
    struct list_elem child;
    struct parent_child* parent;
};

#endif /* userprog/process.h */