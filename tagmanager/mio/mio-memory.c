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

/* memory IO implementation */

#include <glib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mio.h"


#define MEM_SET_VTABLE(mio)           \
  G_STMT_START {                      \
    mio->v_free     = mem_free;       \
    mio->v_read     = mem_read;       \
    mio->v_write    = mem_write;      \
    mio->v_getc     = mem_getc;       \
    mio->v_gets     = mem_gets;       \
    mio->v_ungetc   = mem_ungetc;     \
    mio->v_putc     = mem_putc;       \
    mio->v_puts     = mem_puts;       \
    mio->v_vprintf  = mem_vprintf;    \
    mio->v_clearerr = mem_clearerr;   \
    mio->v_eof      = mem_eof;        \
    mio->v_error    = mem_error;      \
    mio->v_seek     = mem_seek;       \
    mio->v_tell     = mem_tell;       \
    mio->v_rewind   = mem_rewind;     \
    mio->v_getpos   = mem_getpos;     \
    mio->v_setpos   = mem_setpos;     \
  } G_STMT_END


/* minimal reallocation chunk size */
#define MIO_CHUNK_SIZE 4096


static void
mem_free (MIO *mio)
{
  if (mio->impl.mem.free_func) {
    mio->impl.mem.free_func (mio->impl.mem.buf);
  }
  mio->impl.mem.buf = NULL;
  mio->impl.mem.pos = 0;
  mio->impl.mem.size = 0;
  mio->impl.mem.allocated_size = 0;
  mio->impl.mem.realloc_func = NULL;
  mio->impl.mem.free_func = NULL;
  mio->impl.mem.eof = FALSE;
  mio->impl.mem.error = FALSE;
}

static gsize
mem_read (MIO    *mio,
          void   *ptr,
          gsize   size,
          gsize   nmemb)
{
  gsize n_read = 0;
  
  if (size != 0 && nmemb != 0) {
    if (mio->impl.mem.ungetch != EOF) {
      *((guchar *)ptr) = (guchar)mio->impl.mem.ungetch;
      mio->impl.mem.ungetch = EOF;
      mio->impl.mem.pos++;
      if (size == 1) {
        n_read++;
      } else if (mio->impl.mem.pos + (size - 1) <= mio->impl.mem.size) {
        memcpy (&(((guchar *)ptr)[1]),
                &mio->impl.mem.buf[mio->impl.mem.pos], size - 1);
        mio->impl.mem.pos += size - 1;
        n_read++;
      }
    }
    for (; n_read < nmemb; n_read++) {
      if (mio->impl.mem.pos + size > mio->impl.mem.size) {
        break;
      } else {
        memcpy (&(((guchar *)ptr)[n_read * size]),
                &mio->impl.mem.buf[mio->impl.mem.pos], size);
        mio->impl.mem.pos += size;
      }
    }
    if (mio->impl.mem.pos >= mio->impl.mem.size) {
      mio->impl.mem.eof = TRUE;
    }
  }
  
  return n_read;
}

/*
 * mem_try_resize:
 * @mio: A #MIO object of the type %MIO_TYPE_MEMORY
 * @new_size: Requested new size
 * 
 * Tries to resize the underlying buffer of an in-memory #MIO object.
 * This supports both growing and shrinking.
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static gboolean
mem_try_resize (MIO  *mio,
                gsize new_size)
{
  gboolean success = FALSE;
  
  if (mio->impl.mem.realloc_func) {
    if (G_UNLIKELY (new_size == G_MAXSIZE)) {
      #ifdef EOVERFLOW
      errno = EOVERFLOW;
      #endif
    } else {
      if (new_size > mio->impl.mem.size) {
        if (new_size <= mio->impl.mem.allocated_size) {
          mio->impl.mem.size = new_size;
          success = TRUE;
        } else {
          gsize   newsize;
          guchar *newbuf;
          
          newsize = MAX (mio->impl.mem.allocated_size + MIO_CHUNK_SIZE,
                         new_size);
          newbuf = mio->impl.mem.realloc_func (mio->impl.mem.buf, newsize);
          if (newbuf) {
            mio->impl.mem.buf = newbuf;
            mio->impl.mem.allocated_size = newsize;
            mio->impl.mem.size = new_size;
            success = TRUE;
          }
        }
      } else {
        guchar *newbuf;
        
        newbuf = mio->impl.mem.realloc_func (mio->impl.mem.buf, new_size);
        if (G_LIKELY (newbuf || new_size == 0)) {
          mio->impl.mem.buf = newbuf;
          mio->impl.mem.allocated_size = new_size;
          mio->impl.mem.size = new_size;
          success = TRUE;
        }
      }
    }
  }
  
  return success;
}

/*
 * mem_try_ensure_space:
 * @mio: A #MIO object
 * @n: Requested size from the current (cursor) position
 * 
 * Tries to ensure there is enough space for @n bytes to be written from the
 * current cursor position.
 * 
 * Returns: %TRUE if there is enough space, %FALSE otherwise.
 */
static gboolean
mem_try_ensure_space (MIO  *mio,
                      gsize n)
{
  gboolean success = TRUE;
  
  if (mio->impl.mem.pos + n > mio->impl.mem.size) {
    success = mem_try_resize (mio, mio->impl.mem.pos + n);
  }
  
  return success;
}

static gsize
mem_write (MIO         *mio,
           const void  *ptr,
           gsize        size,
           gsize        nmemb)
{
  gsize n_written = 0;
  
  if (size != 0 && nmemb != 0) {
    if (mem_try_ensure_space (mio, size * nmemb)) {
      memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], ptr, size * nmemb);
      mio->impl.mem.pos += size * nmemb;
      n_written = nmemb;
    }
  }
  
  return n_written;
}

static gint
mem_putc (MIO  *mio,
          gint  c)
{
  gint rv = EOF;
  
  if (mem_try_ensure_space (mio, 1)) {
    mio->impl.mem.buf[mio->impl.mem.pos] = (guchar)c;
    mio->impl.mem.pos++;
    rv = (gint)((guchar)c);
  }
  
  return rv;
}

static gint
mem_puts (MIO          *mio,
          const gchar  *s)
{
  gint  rv = EOF;
  gsize len;
  
  len = strlen (s);
  if (mem_try_ensure_space (mio, len)) {
    memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], s, len);
    mio->impl.mem.pos += len;
    rv = 1;
  }
  
  return rv;
}

static gint
mem_vprintf (MIO         *mio,
             const gchar *format,
             va_list      ap)
{
  gint    rv = -1;
  gsize   n;
  gsize   old_pos;
  gsize   old_size;
  va_list ap_copy;
  
  old_pos = mio->impl.mem.pos;
  old_size = mio->impl.mem.size;
  G_VA_COPY (ap_copy, ap);
  /* compute the size we will need into the buffer */
  n = g_printf_string_upper_bound (format, ap_copy);
  va_end (ap_copy);
  if (mem_try_ensure_space (mio, n)) {
    guchar c;
    
    /* backup character at n+1 that will be overwritten by a \0 ... */
    c = mio->impl.mem.buf[mio->impl.mem.pos + (n - 1)];
    rv = vsprintf ((gchar *)&mio->impl.mem.buf[mio->impl.mem.pos], format, ap);
    /* ...and restore it */
    mio->impl.mem.buf[mio->impl.mem.pos + (n - 1)] = c;
    if (G_LIKELY (rv >= 0 && (gsize)rv == (n - 1))) {
      /* re-compute the actual size since we might have allocated one byte
       * more than needed */
      mio->impl.mem.size = MAX (old_size, old_pos + (guint)rv);
      mio->impl.mem.pos += (guint)rv;
    } else {
      mio->impl.mem.size = old_size;
      rv = -1;
    }
  }
  
  return rv;
}

static gint
mem_getc (MIO *mio)
{
  gint rv = EOF;
  
  if (mio->impl.mem.ungetch != EOF) {
    rv = mio->impl.mem.ungetch;
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos++;
  } else if (mio->impl.mem.pos < mio->impl.mem.size) {
    rv = mio->impl.mem.buf[mio->impl.mem.pos];
    mio->impl.mem.pos++;
  } else {
    mio->impl.mem.eof = TRUE;
  }
  
  return rv;
}

static gint
mem_ungetc (MIO  *mio,
            gint  ch)
{
  gint rv = EOF;
  
  if (ch != EOF && mio->impl.mem.ungetch == EOF) {
    rv = mio->impl.mem.ungetch = ch;
    mio->impl.mem.pos--;
    mio->impl.mem.eof = FALSE;
  }
  
  return rv;
}

static gchar *
mem_gets (MIO    *mio,
          gchar  *s,
          gsize   size)
{
  gchar *rv = NULL;
  
  if (size > 0) {
    gsize i = 0;
    
    if (mio->impl.mem.ungetch != EOF) {
      s[i] = (gchar)mio->impl.mem.ungetch;
      mio->impl.mem.ungetch = EOF;
      mio->impl.mem.pos++;
      i++;
    }
    for (; mio->impl.mem.pos < mio->impl.mem.size && i < (size - 1); i++) {
      s[i] = (gchar)mio->impl.mem.buf[mio->impl.mem.pos];
      mio->impl.mem.pos++;
      if (s[i] == '\n') {
        i++;
        break;
      }
    }
    if (i > 0) {
      s[i] = 0;
      rv = s;
    }
    if (mio->impl.mem.pos >= mio->impl.mem.size) {
      mio->impl.mem.eof = TRUE;
    }
  }
  
  return rv;
}

static void
mem_clearerr (MIO *mio)
{
  mio->impl.mem.error = FALSE;
  mio->impl.mem.eof = FALSE;
}

static gint
mem_eof (MIO *mio)
{
  return mio->impl.mem.eof != FALSE;
}

static gint
mem_error (MIO *mio)
{
  return mio->impl.mem.error != FALSE;
}

/* FIXME: should we support seeking out of bounds like lseek() seems to do? */
static gint
mem_seek (MIO  *mio,
          glong offset,
          gint  whence)
{
  gint rv = -1;
  
  switch (whence) {
    case SEEK_SET:
      if (offset < 0 || (gsize)offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = (gsize)offset;
        rv = 0;
      }
      break;
    
    case SEEK_CUR:
      if ((offset < 0 && (gsize)-offset > mio->impl.mem.pos) ||
          mio->impl.mem.pos + (gsize)offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = (gsize)((gssize)mio->impl.mem.pos + offset);
        rv = 0;
      }
      break;
    
    case SEEK_END:
      if (offset > 0 || (gsize)-offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = mio->impl.mem.size - (gsize)-offset;
        rv = 0;
      }
      break;
    
    default:
      errno = EINVAL;
  }
  if (rv == 0) {
    mio->impl.mem.eof = FALSE;
    mio->impl.mem.ungetch = EOF;
  }
  
  return rv;
}

static glong
mem_tell (MIO *mio)
{
  glong rv = -1;
  
  if (mio->impl.mem.pos > G_MAXLONG) {
    #ifdef EOVERFLOW
    errno = EOVERFLOW;
    #endif
  } else {
    rv = (glong)mio->impl.mem.pos;
  }
  
  return rv;
}

static void
mem_rewind (MIO *mio)
{
  mio->impl.mem.pos = 0;
  mio->impl.mem.ungetch = EOF;
  mio->impl.mem.eof = FALSE;
  mio->impl.mem.error = FALSE;
}

static gint
mem_getpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  if (mio->impl.mem.pos == (gsize)-1) {
    /* this happens if ungetc() was called at the start of the stream */
    #ifdef EIO
    errno = EIO;
    #endif
  } else {
    pos->impl.mem = mio->impl.mem.pos;
    rv = 0;
  }
  
  return rv;
}

static gint
mem_setpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  if (pos->impl.mem > mio->impl.mem.size) {
    errno = EINVAL;
  } else {
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos = pos->impl.mem;
    rv = 0;
  }
  
  return rv;
}
