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
    struct lock access;    /* Controll access to file. */
    struct lock readers;        /* Controll changes to readers_count. */
    struct semaphore queue;     /* Keep check of who is next. FIFO.*/
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
      lock_init(&(file->access));
      lock_init(&(file->readers));
      sema_init(&(file->queue), 0);

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
  printf("In file_read\n");
  // ---- Entry section ---- //
  off_t bytes_read;

  sema_down(&(file->queue));        // Add this read request to the queue

  lock_acquire(&(file->readers));   // Protect readers count
  file->readers_count++;

  if (file->readers_count == 1)
  {
    // Now check if the file is being written to or not
    if (lock_try_acquire(&(file->access)))
    {
      lock_acquire(&(file->access));  // Take access of the file
      sema_up(&(file->queue));        // Let next in queue read

      lock_release(&(file->readers)); 

      // ---- Critical section ---- //
      printf("Crit\n");
      bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
      file->pos += bytes_read;
    }
  }

  // ---- Exit section ---- //
  printf("Exit\n");
  lock_acquire(&(file->readers));   // Protect readers count
  file->readers_count--;
  if (file->readers_count == 0)
  {
    lock_release(&(file->access));  // Release access to file. Can know write or read again.
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
  printf("file write\n");
  // ---- Entry section ---- //
  off_t bytes_written;

  sema_down(&(file->queue));        // Add this write request to the queue

  if (lock_try_acquire(&(file->access)))
  {
    lock_acquire(&(file->access));
    sema_up(&(file->queue));

    // ---- Critical section ---- //
    printf("Crit\n");
    bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
    file->pos += bytes_written;
  }

  // ---- Exit section ---- //
  printf("Exit\n");
  lock_release(&(file->access));

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
