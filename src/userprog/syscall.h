#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <stdio.h>
#include "lib/user/syscall.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/init.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "lib/string.h"
#include "pagedir.h"
#include "threads/vaddr.h"

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

/* Lab 4 */
#define MAX_SIZE_ARG 32*4;

/* Lab 5*/
bool is_ptr_valid (void* esp);
bool is_str_valid (char* str);
bool is_bufr_valid (void* buffer, unsigned size);
bool is_fd_valid (int fd);  

/* Lab 6 */
void seek (int fd, unsigned position);
unsigned tell (int fd);
int filesize (int fd);
bool remove (const char *file_name);

#endif /* userprog/syscall.h */