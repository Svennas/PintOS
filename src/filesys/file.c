#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"

/* An open file. */
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */

    /* Added in lab 6 */
    int readers_count;          /* Keep check of the amount current readers. */
    struct semaphore access;         /* Controll access to file. */

    struct lock readers;        /* Controll changes to readers_count. */
    struct semaphore queue;     /* Keep check of who is next. FIFO.*/
    bool occupied;              /* If the file is taken by write or read. */

    struct thread* prio_queue[];
  };

/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;

      file->readers_count = 0;
      sema_init(&(file->access), 1);    // Binary sema
      lock_init(&(file->readers));
      sema_init(&(file->queue), 0);
      list_init(file->prio_queue);
      file->occupied = false;

      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}

/* Opens and returns a new file for the same inode as FILE.
   Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) 
{
  return file_open (inode_reopen (file->inode));
}

/* Closes FILE. */
void
file_close (struct file *file) 
{
  if (file != NULL)
    {
      file_allow_write (file);
      inode_close (file->inode);
      free (file); 
    }
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) 
{
  return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at the file's current position.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size) 
{
  //printf("In file_read\n");
  // ---- Entry section ---- //
  off_t bytes_read;
  //printf("read: sema_down(&(file->queue));\n");
  //sema_down(&(file->queue));        // Add this read request to the queue
  //printf("read: add to queue\n");
  //list_push_back(file->prio_queue, thread_current()->elem);

  //printf("read: before lock_acquire(&(file->readers))\n");
  lock_acquire(&(file->readers));   // Protect readers count
  //printf("readers count: %i\n", file->readers_count);
  file->readers_count++;  
  //printf("read: lock_release(&(file->readers)); \n");
  lock_release(&(file->readers)); 

  if (file->readers_count == 1)
  {
    //printf("read: sema file->access value = %i\n", file->access.value);
    // Now check if the file is being written to or not
    // Try to down sema. Will be true if the file is not in use
    //if (sema_try_down(&(file->access)))   
    //if (file->access.value == 1)
    if (!file->occupied)
    {
      //printf("read: before sema_up\n");
      sema_up(&(file->queue));    // Let the thread with highest prio run
      //printf("read: after sema_up\n");
      file->occupied = true;      // So no other thread can run this file

      // ---- Critical section ---- //
      //sema_down(&(file->access));
      //printf("read: Crit\n");
      bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
      file->pos += bytes_read;

      //printf("read: sema file->access value = %i\n", file->access.value);
      //printf("read: lock_acquire(&(file->access));\n");
      //lock_acquire(&(file->access));  // Take access of the file
      //printf("read: sema_up(&(file->queue));\n");
      //sema_up(&(file->queue));        // Let next in queue read
      //printf("read: sema_down(&(file->queue));\n");
      //sema_down(&(file->queue));  // Sleep the current thread
      //list_pop_front(file->prio_queue);   // Pop thread with highest prio
      //printf("read: sema_up(&(file->queue));\n");
      //sema_up(&(file->queue));    // Let the thread with highest prio run

    }
    else  // If the file is occupied, put current thread to sleep
    {
      //printf("read: the file is occupied\n");
      sema_down(&(file->queue));  // Sleep the current thread
    }
  }

  // ---- Exit section ---- //
  //printf("read: Exit\n");
  lock_acquire(&(file->readers));   // Protect readers count
  //printf("read: readers count: %i\n", file->readers_count);
  file->readers_count--;
  if (file->readers_count == 0)
  {
    //sema_up(&(file->access));  // Release access to file. Can know write or read again.
    //printf("read: Release access to file.\n");
    file->occupied = false;
  }
  lock_release(&(file->readers));

  return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) 
{
  return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at the file's current position.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size) 
{
  //printf("file write\n");
  //printf("write: readers count: %i\n", file->readers_count);
  // ---- Entry section ---- //
  off_t bytes_written;
  //printf("write: sema_down(&(file->queue));\n");
  //sema_down(&(file->queue));        // Add this write request to the queue

  //printf("write: add to queue\n");
  //list_push_back(file->prio_queue, thread_current()->elem);

  //printf("write: sema file->access value = %i\n", file->access.value);
  //if (file->access.value == 1)
  if (!file->occupied)
  {
    //printf("write: before sema_up\n");
    sema_up(&(file->queue));    // Let the thread with highest prio run
    //printf("write: after sema_up\n");
    file->occupied = true;      // So no other thread can run0xc010724a 0xc010c83f 0xc01002cc 0xc0100728 this file

    // ---- Critical section ---- //
    //sema_down(&(file->access));
    //printf("write: Crit\n");
    bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
    file->pos += bytes_written;

    //printf("sema file->access value = %i\n", file->access.value);
    //printf("write: lock_acquire(&(file->access));\n");
    //lock_acquire(&(file->access));
    //printf("write: sema_up(&(file->queue));\n");
    //sema_up(&(file->queue));

    //printf("write: sema_down(&(file->queue));\n");
    //sema_down(&(file->queue));  // Sleep the current thread
    //list_pop_front(file->prio_queue);   // Pop thread with highest prio
    //printf("write: sema_up(&(file->queue));\n");
    //sema_up(&(file->queue));    // Let the thread with highest prio run

  }
  else
  {
    //printf("write: the file is occupied\n");
    sema_down(&(file->queue));
  }

  // ---- Exit section ---- //
  //printf("write: Release access to file.\n");
  file->occupied = false;
  //printf("write: Exit\n");
  //sema_up(&(file->access));

  return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs) 
{
  return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (!file->deny_write) 
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (file->deny_write) 
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) 
{
  ASSERT (file != NULL);
  return inode_length (file->inode);
}

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file. */
void
file_seek (struct file *file, off_t new_pos)
{
  ASSERT (file != NULL);
  ASSERT (new_pos >= 0);
  file->pos = new_pos;
}

/* Returns the current position in FILE as a byte offset from the
   start of the file. */
off_t
file_tell (struct file *file) 
{
  ASSERT (file != NULL);
  return file->pos;
}
