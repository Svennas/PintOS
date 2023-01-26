#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "lib/user/syscall.h"

/* Lab 1 */
void syscall_init (void);

void halt (void);
bool create (const char *file, unsigned initialsize);
int open (const char *file);
void close (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);


/* Lab 3 (Started again 2023)*/
void exit (int status);
pid_t exec (const char *cmd_line);


#endif /* userprog/syscall.h */