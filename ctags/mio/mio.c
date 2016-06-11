/*
 *  MIO, an I/O abstraction layer replicating C file I/O API.
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 */

/* Hack to force ANSI compliance by not using va_copy() even if present.
 * This relies on the fact G_VA_COPY is maybe defined as va_copy in
 * glibconfig.h, so we undef it, but gutils.h takes care of defining a
 * compiler-specific implementation if not already defined.
 * This needs to come before any other GLib inclusion. */
#ifdef MIO_FORCE_ANSI
# include <glibconfig.h>
# undef G_VA_COPY
#endif

#include "mio.h"
#include "mio-file.c"
#include "mio-memory.c"

#include <glib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>



/**
 * SECTION:mio
 * @short_description: The MIO object
 * @include: mio/mio.h
 * 
 * The #MIO object replicates the C file I/O API with support of both standard
 * file based operations and in-memory operations. Its goal is to ease the port
 * of an application that uses C file I/O API to perform in-memory operations.
 * 
 * A #MIO object is created using mio_new_file() or mio_new_memory(), depending
 * on whether you want file or in-memory operations, and destroyed using
 * mio_free(). There is also some other convenient API to create file-based
 * #MIO objects for more complex cases, such as mio_new_file_full() and
 * mio_new_fp().
 * 
 * Once the #MIO object is created, you can perform standard I/O operations on
 * it transparently without the need to care about the effective underlying
 * operations.
 * 
 * The I/O API is almost exactly a replication of the standard C file I/O API
 * with the significant difference that the first parameter is always the #MIO
 * object to work on.
 */


/**
 * mio_new_file_full:
 * @filename: Filename to open, passed as-is to @open_func as the first argument
 * @mode: Mode in which open the file, passed as-is to @open_func as the second
 *        argument
 * @open_func: A function with the fopen() semantic to use to open the file
 * @close_func: A function with the fclose() semantic to close the file when
 *              the #MIO object is destroyed, or %NULL not to close the #FILE
 *              object
 * 
 * Creates a new #MIO object working on a file, from a filename and an opening
 * function. See also mio_new_file().
 * 
 * This function is generally overkill and mio_new_file() should often be used
 * instead, but it allows to specify a custom function to open a file, as well
 * as a close function. The former is useful e.g. if you need to wrap fopen()
 * for some reason (like filename encoding conversion for example), and the
 * latter allows you both to match your custom open function and to choose
 * whether the underlying #FILE object should or not be closed when mio_free()
 * is called on the returned object.
 * 
 * Free-function: mio_free()
 * 
 * Returns: A new #MIO on success, or %NULL on failure.
 */
MIO *
mio_new_file_full (const gchar  *filename,
                   const gchar  *mode,
                   MIOFOpenFunc  open_func,
                   MIOFCloseFunc close_func)
{
  MIO *mio;
  
  /* we need to create the MIO object first, because we may not be able to close
   * the opened file if the user passed NULL as the close function, which means
   * that everything must succeed if we've opened the file successfully */
  mio = g_slice_alloc (sizeof *mio);
  if (mio) {
    FILE *fp = open_func (filename, mode);
    
    if (! fp) {
      g_slice_free1 (sizeof *mio, mio);
      mio = NULL;
    } else {
      mio->type = MIO_TYPE_FILE;
      mio->impl.file.fp = fp;
      mio->impl.file.close_func = close_func;
      /* function table filling */
      FILE_SET_VTABLE (mio);
    }
  }
  
  return mio;
}

/**
 * mio_new_file:
 * @filename: Filename to open, same as the fopen()'s first argument
 * @mode: Mode in which open the file, fopen()'s second argument
 * 
 * Creates a new #MIO object working on a file from a filename; wrapping
 * fopen().
 * This function simply calls mio_new_file_full() with the libc's fopen() and
 * fclose() functions.
 * 
 * Free-function: mio_free()
 * 
 * Returns: A new #MIO on success, or %NULL on failure.
 */
MIO *
mio_new_file (const gchar *filename,
              const gchar *mode)
{
  return mio_new_file_full (filename, mode, fopen, fclose);
}

/**
 * mio_new_fp:
 * @fp: An opened #FILE object
 * @close_func: (allow-none): Function used to close @fp when the #MIO object
 *              gets destroyed, or %NULL not to close the #FILE object
 * 
 * Creates a new #MIO object working on a file, from an already opened #FILE
 * object.
 * 
 * <example>
 * <title>Typical use of this function</title>
 * <programlisting>
 * MIO *mio = mio_new_fp (fp, fclose);
 * </programlisting>
 * </example>
 * 
 * Free-function: mio_free()
 * 
 * Returns: A new #MIO on success or %NULL on failure.
 */
MIO *
mio_new_fp (FILE         *fp,
            MIOFCloseFunc close_func)
{
  MIO *mio;
  
  mio = g_slice_alloc (sizeof *mio);
  if (mio) {
    mio->type = MIO_TYPE_FILE;
    mio->impl.file.fp = fp;
    mio->impl.file.close_func = close_func;
    /* function table filling */
    FILE_SET_VTABLE (mio);
  }
  
  return mio;
}

/**
 * mio_new_memory:
 * @data: Initial data (may be %NULL)
 * @size: Length of @data in bytes
 * @realloc_func: A function with the realloc() semantic used to grow the
 *                buffer, or %NULL to disable buffer growing
 * @free_func: A function with the free() semantic to destroy the data together
 *             with the object, or %NULL not to destroy the data
 * 
 * Creates a new #MIO object working on memory.
 * 
 * To allow the buffer to grow, you must provide a @realloc_func, otherwise
 * trying to write after the end of the current data will fail.
 * 
 * If you want the buffer to be freed together with the #MIO object, you must
 * give a @free_func; otherwise the data will still live after #MIO object
 * termination.
 * 
 * <example>
 * <title>Basic creation of a non-growable, freeable #MIO object</title>
 * <programlisting>
 * MIO *mio = mio_new_memory (data, size, NULL, g_free);
 * </programlisting>
 * </example>
 * 
 * <example>
 * <title>Basic creation of an empty growable and freeable #MIO object</title>
 * <programlisting>
 * MIO *mio = mio_new_memory (NULL, 0, g_try_realloc, g_free);
 * </programlisting>
 * </example>
 * 
 * Free-function: mio_free()
 * 
 * Returns: A new #MIO on success, or %NULL on failure.
 */
MIO *
mio_new_memory (guchar         *data,
                gsize           size,
                MIOReallocFunc  realloc_func,
                GDestroyNotify  free_func)
{
  MIO  *mio;
  
  mio = g_slice_alloc (sizeof *mio);
  if (mio) {
    mio->type = MIO_TYPE_MEMORY;
    mio->impl.mem.buf = data;
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos = 0;
    mio->impl.mem.size = size;
    mio->impl.mem.allocated_size = size;
    mio->impl.mem.realloc_func = realloc_func;
    mio->impl.mem.free_func = free_func;
    mio->impl.mem.eof = FALSE;
    mio->impl.mem.error = FALSE;
    /* function table filling */
    MEM_SET_VTABLE (mio);
  }
  
  return mio;
}

/**
 * mio_file_get_fp:
 * @mio: A #MIO object
 * 
 * Gets the underlying #FILE object associated with a #MIO file stream.
 * 
 * <warning><para>The returned object may become invalid after a call to
 * mio_free() if the stream was configured to close the file when
 * destroyed.</para></warning>
 * 
 * Returns: The underlying #FILE object of the given stream, or %NULL if the
 *          stream is not a file stream.
 */
FILE *
mio_file_get_fp (MIO *mio)
{
  FILE *fp = NULL;
  
  if (mio->type == MIO_TYPE_FILE) {
    fp = mio->impl.file.fp;
  }
  
  return fp;
}

/**
 * mio_memory_get_data:
 * @mio: A #MIO object
 * @size: (allow-none) (out): Return location for the length of the returned
 *        memory, or %NULL
 * 
 * Gets the underlying memory buffer associated with a #MIO memory stream.
 * 
 * <warning><para>The returned pointer and size may become invalid after a
 * successful write on the stream or after a call to mio_free() if the stream
 * was configured to free the memory when destroyed.</para></warning>
 * 
 * Returns: The memory buffer of the given #MIO stream, or %NULL if the stream
 *          is not a memory stream.
 */
guchar *
mio_memory_get_data (MIO   *mio,
                     gsize *size)
{
  guchar *ptr = NULL;
  
  if (mio->type == MIO_TYPE_MEMORY) {
    ptr = mio->impl.mem.buf;
    if (size) *size = mio->impl.mem.size;
  }
  
  return ptr;
}

/**
 * mio_free:
 * @mio: A #MIO object
 * 
 * Destroys a #MIO object.
 */
void
mio_free (MIO *mio)
{
  if (mio) {
    mio->v_free (mio);
    g_slice_free1 (sizeof *mio, mio);
  }
}

/**
 * mio_read:
 * @mio: A #MIO object
 * @ptr: Pointer to the memory to fill with the read data
 * @size: Size of each block to read
 * @nmemb: Number o blocks to read
 * 
 * Reads raw data from a #MIO stream. This function behave the same as fread().
 * 
 * Returns: The number of actually read blocks. If an error occurs or if the
 *          end of the stream is reached, the return value may be smaller than
 *          the requested block count, or even 0. This function doesn't
 *          distinguish between end-of-stream and an error, you should then use
 *          mio_eof() and mio_error() to determine which occurred.
 */
gsize
mio_read (MIO    *mio,
          void   *ptr,
          gsize   size,
          gsize   nmemb)
{
  return mio->v_read (mio, ptr, size, nmemb);
}

/**
 * mio_write:
 * @mio: A #MIO object
 * @ptr: Pointer to the memory to write on the stream
 * @size: Size of each block to write
 * @nmemb: Number of block to write
 * 
 * Writes raw data to a #MIO stream. This function behaves the same as fwrite().
 * 
 * Returns: The number of blocks actually written to the stream. This might be
 *          smaller than the requested count if a write error occurs.
 */
gsize
mio_write (MIO         *mio,
           const void  *ptr,
           gsize        size,
           gsize        nmemb)
{
  return mio->v_write (mio, ptr, size, nmemb);
}

/**
 * mio_putc:
 * @mio: A #MIO object
 * @c: The character to write
 * 
 * Writes a character to a #MIO stream. This function behaves the same as
 * fputc().
 * 
 * Returns: The written wharacter, or %EOF on error.
 */
gint
mio_putc (MIO  *mio,
          gint  c)
{
  return mio->v_putc (mio, c);
}

/**
 * mio_puts:
 * @mio: A #MIO object
 * @s: The string to write
 * 
 * Writes a string to a #MIO object. This function behaves the same as fputs().
 * 
 * Returns: A non-negative integer on success or %EOF on failure.
 */
gint
mio_puts (MIO          *mio,
          const gchar  *s)
{
  return mio->v_puts (mio, s);
}

/**
 * mio_vprintf:
 * @mio: A #MIO object
 * @format: A printf fomrat string
 * @ap: The variadic argument list for the format
 * 
 * Writes a formatted string into a #MIO stream. This function behaves the same
 * as vfprintf().
 * 
 * Returns: The number of bytes written in the stream, or a negative value on
 *          failure.
 */
gint
mio_vprintf (MIO         *mio,
             const gchar *format,
             va_list      ap)
{
  return mio->v_vprintf (mio, format, ap);
}

/**
 * mio_printf:
 * @mio: A #MIO object
 * @format: A print format string
 * @...: Arguments of the format
 * 
 * Writes a formatted string to a #MIO stream. This function behaves the same as
 * fprintf().
 * 
 * Returns: The number of bytes written to the stream, or a negative value on
 *          failure.
 */
gint
mio_printf (MIO         *mio,
            const gchar *format,
            ...)
{
  gint    rv;
  va_list ap;
  
  va_start (ap, format);
  rv = mio->v_vprintf (mio, format, ap);
  va_end (ap);
  
  return rv;
}

/**
 * mio_getc:
 * @mio: A #MIO object
 * 
 * Gets the current character from a #MIO stream. This function behaves the same
 * as fgetc().
 * 
 * Returns: The read character as a #gint, or %EOF on error.
 */
gint
mio_getc (MIO *mio)
{
  return mio->v_getc (mio);
}

/**
 * mio_ungetc:
 * @mio: A #MIO object
 * @ch: Character to put back in the stream
 * 
 * Puts a character back in a #MIO stream. This function behaves the sames as
 * ungetc().
 * 
 * <warning><para>It is only guaranteed that one character can be but back at a
 * time, even if the implementation may allow more.</para></warning>
 * <warning><para>Using this function while the stream cursor is at offset 0 is
 * not guaranteed to function properly. As the C99 standard says, it is "an
 * obsolescent feature".</para></warning>
 * 
 * Returns: The character put back, or %EOF on error.
 */
gint
mio_ungetc (MIO  *mio,
            gint  ch)
{
  return mio->v_ungetc (mio, ch);
}

/**
 * mio_gets:
 * @mio: A #MIO object
 * @s: A string to fill with the read data
 * @size: The maximum number of bytes to read
 * 
 * Reads a string from a #MIO stream, stopping after the first new-line
 * character or at the end of the stream. This function behaves the same as
 * fgets().
 * 
 * Returns: @s on success, %NULL otherwise.
 */
gchar *
mio_gets (MIO    *mio,
          gchar  *s,
          gsize   size)
{
  return mio->v_gets (mio, s, size);
}

/**
 * mio_clearerr:
 * @mio: A #MIO object
 * 
 * Clears the error and end-of-stream indicators of a #MIO stream. This function
 * behaves the same as clearerr().
 */
void
mio_clearerr (MIO *mio)
{
  mio->v_clearerr (mio);
}

/**
 * mio_eof:
 * @mio: A #MIO object
 * 
 * Checks whether the end-of-stream indicator of a #MIO stream is set. This
 * function behaves the same as feof().
 * 
 * Returns: A non-null value if the stream reached its end, 0 otherwise.
 */
gint
mio_eof (MIO *mio)
{
  return mio->v_eof (mio);
}

/**
 * mio_error:
 * @mio: A #MIO object
 * 
 * Checks whether the error indicator of a #MIO stream is set. This function
 * behaves the same as ferror().
 * 
 * Returns: A non-null value if the stream have an error set, 0 otherwise.
 */
gint
mio_error (MIO *mio)
{
  return mio->v_error (mio);
}

/**
 * mio_seek:
 * @mio: A #MIO object
 * @offset: Offset of the new place, from @whence
 * @whence: Move origin. SEEK_SET moves relative to the start of the stream,
 *          SEEK_CUR from the current position and SEEK_SET from the end of the
 *          stream.
 * 
 * Sets the curosr position on a #MIO stream. This functions behaves the same as
 * fseek(). See also mio_tell() and mio_setpos().
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno should be set to
 *          indicate the error.
 */
gint
mio_seek (MIO  *mio,
          glong offset,
          gint  whence)
{
  return mio->v_seek (mio, offset, whence);
}

/**
 * mio_tell:
 * @mio: A #MIO object
 * 
 * Gets the current cursor position of a #MIO stream. This function behaves the
 * same as ftell().
 * 
 * Returns: The current offset from the start of the stream, or -1 or error, in
 *          which case errno is set to indicate the error.
 */
glong
mio_tell (MIO *mio)
{
  return mio->v_tell (mio);
}

/**
 * mio_rewind:
 * @mio: A #MIO object
 * 
 * Resets the cursor position to 0, and also the end-of-stream and the error
 * indicators of a #MIO stream.
 * See also mio_seek() and mio_clearerr().
 */
void
mio_rewind (MIO *mio)
{
  mio->v_rewind (mio);
}

/**
 * mio_getpos:
 * @mio: A #MIO stream
 * @pos: (out): A #MIOPos object to fill-in
 * 
 * Stores the current position (and maybe other informations about the stream
 * state) of a #MIO stream in order to restore it later with mio_setpos(). This
 * function behaves the same as fgetpos().
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno is set to indicate
 *          the error.
 */
gint
mio_getpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  pos->type = mio->type;
  rv = mio->v_getpos (mio, pos);
  #ifdef MIO_DEBUG
  if (rv != -1) {
    pos->tag = mio;
  }
  #endif /* MIO_DEBUG */
  
  return rv;
}

/**
 * mio_setpos:
 * @mio: A #MIO object
 * @pos: (in): A #MIOPos object filled-in by a previous call of mio_getpos() on
 *       the same stream
 * 
 * Restores the position and state indicators of a #MIO stream previously saved
 * by mio_getpos().
 * 
 * <warning><para>The #MIOPos object must have been initialized by a previous
 * call to mio_getpos() on the same stream.</para></warning>
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno is set to indicate
 *          the error.
 */
gint
mio_setpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  #ifdef MIO_DEBUG
  if (pos->tag != mio) {
    g_critical ("mio_setpos((MIO*)%p, (MIOPos*)%p): "
                "Given MIOPos was not set by a previous call to mio_getpos() "
                "on the same MIO object, which means there is a bug in "
                "someone's code.",
                (void *)mio, (void *)pos);
    errno = EINVAL;
    return -1;
  }
  #endif /* MIO_DEBUG */
  rv = mio->v_setpos (mio, pos);
  
  return rv;
}
