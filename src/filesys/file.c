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
    //struct semaphore access;         /* Controll access to file. */
    struct lock rw_mutex;
    //struct semaphore mutex;

    struct lock readers;        /* Controll changes to readers_count. */
    //struct semaphore queue;     /* Keep check of who is next. FIFO.*/
    //bool is_writing;            /* True if the file is being used by write. */
    //bool is_reading;            /* True if the file is being used by read. */

    //struct thread* prio_queue[];
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
      //sema_init(&(file->access), 1);    // Binary sema
      lock_init(&(file->readers));
      //sema_init(&(file->queue), 0);
      lock_init(&(file->rw_mutex));
      //sema_init(&(file->mutex), 0);
      //list_init(file->prio_queue);
      //file->is_writing = false;
      //file->is_reading = false;

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

  //printf("read: before lock_acquire(&(file->readers))\n");
  lock_acquire(&(file->readers));   // Protect readers count
  //printf("readers count: %i\n", file->readers_count);
  file->readers_count++;  
   
  if (file->readers_count == 1) // If this is the first reader
  {
    //sema_down(&(file->access));
    lock_acquire(&(file->rw_mutex));
    //if (!file->is_writing)  // Request access to file
    //{
      //file->is_reading = true;      // So no write can't use this file
    //}
  }
  //printf("read: lock_release(&(file->readers)); \n");
  lock_release(&(file->readers));

  // If file_write is running, put this thread to sleep and put it in the queue
  //if (file->is_writing)
  
    // If write is running, thread gets stuck here until it's done.
    // Write call sema_up when it's done????
    //sema_down(&(file->queue));
  //}
  // file_write is not running, let the next thread run (read or write)
  //else 
  //{
    //sema_up(&(file->queue)); // This can be both read and write
    // If write, go to file_write. file_read is stuck here
    // As soon as write is done, is_writing will be false

    /* If the next file is read, just continue. */
  //}
  
  // ---- Critical section ---- //
  //printf("read: Crit\n");
  bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;

  // ---- Exit section ---- //
  //printf("read: Exit\n");
  lock_acquire(&(file->readers));   // Protect readers count
  //printf("read: readers count: %i\n", file->readers_count);
  file->readers_count--;
  /* As soon as the counter reaches 0, give up access to the file to let write run. */
  if (file->readers_count == 0)
  {
    //printf("read: Release access to file.\n");
    //file->is_reading = false;
    //sema_up(&(file->queue));
    //sema_up(&(file->access));
    lock_release(&(file->rw_mutex));
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
  // ---- Entry section ---- //
  //off_t bytes_written;
  //sema_down(&(file->access));

  lock_acquire(&(file->rw_mutex));

  //if (!file->is_reading && !file->is_writing)
  //{ // Request access <----
    //file->is_writing = true;

    //printf("write: before sema_up\n");
    //sema_up(&(file->queue));    // Let the thread with highest prio run
  //}
  //else
  //{
    //printf("write: the file is occupied\n");
    //sema_down(&(file->queue));  // Add this thread to the queue
  //}

   // ---- Critical section ---- //
  //sema_down(&(file->access));
  //printf("write: Crit\n");
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;

  // ---- Exit section ---- //
  //printf("write: Release access to file.\n");
  //file->is_writing = false;
  //printf("write: Exit\n");
  //sema_up(&(file->access));

  lock_release(&(file->rw_mutex));

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
