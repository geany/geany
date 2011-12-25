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

#ifndef H_MIO_H
#define H_MIO_H

#include <glib.h>
#include <stdio.h>
#include <stdarg.h>


/**
 * MIOType:
 * @MIO_TYPE_FILE: #MIO object works on a file
 * @MIO_TYPE_MEMORY: #MIO object works in-memory
 * 
 * Existing implementations.
 */
enum _MIOType {
  MIO_TYPE_FILE,
  MIO_TYPE_MEMORY
};

typedef enum _MIOType   MIOType;
typedef struct _MIO     MIO;
typedef struct _MIOPos  MIOPos;
/**
 * MIOReallocFunc:
 * @ptr: Pointer to the memory to resize
 * @size: New size of the memory pointed by @ptr
 * 
 * A function following the realloc() semantic.
 * 
 * Returns: A pointer to the start of the new memory, or %NULL on failure.
 */
/* should be GReallocFunc but it's only defined by GIO */
typedef gpointer (* MIOReallocFunc) (gpointer ptr,
                                     gsize    size);

/**
 * MIOFOpenFunc:
 * @filename: The filename to open
 * @mode: fopen() modes for opening @filename
 * 
 * A function following the fclose() semantic, used to close a #FILE
 * object.
 * 
 * Returns: A new #FILE object, or %NULL on failure
 */
typedef FILE    *(* MIOFOpenFunc)   (const gchar *filename,
                                     const gchar *mode);

/**
 * MIOFCloseFunc:
 * @fp: An opened #FILE object
 * 
 * A function following the fclose() semantic, used to close a #FILE
 * object.
 * 
 * Returns: 0 on success, EOF otherwise.
 */
typedef gint     (* MIOFCloseFunc)  (FILE *fp);

/**
 * MIOPos:
 * 
 * An object representing the state of a #MIO stream. This object can be
 * statically allocated but all its fields are private and should not be
 * accessed directly.
 */
struct _MIOPos {
  /*< private >*/
  guint type;
#ifdef MIO_DEBUG
  void *tag;
#endif
  union {
    fpos_t file;
    gsize  mem;
  } impl;
};

/**
 * MIO:
 * 
 * An object representing a #MIO stream. No assumptions should be made about
 * what compose this object, and none of its fields should be accessed directly.
 */
struct _MIO {
  /*< private >*/
  guint type;
  union {
    struct {
      FILE           *fp;
      MIOFCloseFunc   close_func;
    } file;
    struct {
      guchar         *buf;
      gint            ungetch;
      gsize           pos;
      gsize           size;
      gsize           allocated_size;
      MIOReallocFunc  realloc_func;
      GDestroyNotify  free_func;
      gboolean        error;
      gboolean        eof;
    } mem;
  } impl;
  /* virtual function table */
  void    (*v_free)     (MIO *mio);
  gsize   (*v_read)     (MIO     *mio,
                         void    *ptr,
                         gsize    size,
                         gsize    nmemb);
  gsize   (*v_write)    (MIO         *mio,
                         const void  *ptr,
                         gsize        size,
                         gsize        nmemb);
  gint    (*v_getc)     (MIO *mio);
  gchar  *(*v_gets)     (MIO   *mio,
                         gchar *s,
                         gsize  size);
  gint    (*v_ungetc)   (MIO *mio,
                         gint ch);
  gint    (*v_putc)     (MIO *mio,
                         gint c);
  gint    (*v_puts)     (MIO         *mio,
                         const gchar *s);
  gint    (*v_vprintf)  (MIO         *mio,
                         const gchar *format,
                         va_list      ap);
  void    (*v_clearerr) (MIO *mio);
  gint    (*v_eof)      (MIO *mio);
  gint    (*v_error)    (MIO *mio);
  gint    (*v_seek)     (MIO   *mio,
                         glong  offset,
                         gint   whence);
  glong   (*v_tell)     (MIO *mio);
  void    (*v_rewind)   (MIO *mio);
  gint    (*v_getpos)   (MIO     *mio,
                         MIOPos  *pos);
  gint    (*v_setpos)   (MIO     *mio,
                         MIOPos  *pos);
};


MIO        *mio_new_file            (const gchar *filename,
                                     const gchar *mode);
MIO        *mio_new_file_full       (const gchar   *filename,
                                     const gchar   *mode,
                                     MIOFOpenFunc   open_func,
                                     MIOFCloseFunc  close_func);
MIO        *mio_new_fp              (FILE          *fp,
                                     MIOFCloseFunc  close_func);
MIO        *mio_new_memory          (guchar        *data,
                                     gsize          size,
                                     MIOReallocFunc realloc_func,
                                     GDestroyNotify free_func);
void        mio_free                (MIO *mio);
FILE       *mio_file_get_fp         (MIO *mio);
guchar     *mio_memory_get_data     (MIO   *mio,
                                     gsize *size);
gsize       mio_read                (MIO     *mio,
                                     void    *ptr,
                                     gsize    size,
                                     gsize    nmemb);
gsize       mio_write               (MIO         *mio,
                                     const void  *ptr,
                                     gsize        size,
                                     gsize        nmemb);
gint        mio_getc                (MIO *mio);
gchar      *mio_gets                (MIO   *mio,
                                     gchar *s,
                                     gsize  size);
gint        mio_ungetc              (MIO *mio,
                                     gint ch);
gint        mio_putc                (MIO *mio,
                                     gint c);
gint        mio_puts                (MIO         *mio,
                                     const gchar *s);

gint        mio_vprintf             (MIO         *mio,
                                     const gchar *format,
                                     va_list      ap);
gint        mio_printf              (MIO         *mio,
                                     const gchar *format,
                                     ...) G_GNUC_PRINTF (2, 3);

void        mio_clearerr            (MIO *mio);
gint        mio_eof                 (MIO *mio);
gint        mio_error               (MIO *mio);
gint        mio_seek                (MIO   *mio,
                                     glong  offset,
                                     gint   whence);
glong       mio_tell                (MIO *mio);
void        mio_rewind              (MIO *mio);
gint        mio_getpos              (MIO     *mio,
                                     MIOPos  *pos);
gint        mio_setpos              (MIO     *mio,
                                     MIOPos  *pos);



#endif /* guard */
