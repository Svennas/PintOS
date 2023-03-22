/* Executes and waits for a single child process. */

#include <stdio.h>
#include <syscall.h>
#include "../userprog/syscall.h"

int
main (int argc, char *argv[])
{
  wait (exec ("child-simple"));
}
