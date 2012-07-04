/*
*
*   Copyright (c) 1996-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions to sort the tag entries.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#if defined (HAVE_STDLIB_H)
# include <stdlib.h>	/* to declare malloc () */
#endif
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "entry.h"
#include "main.h"
#include "options.h"
#include "read.h"
#include "sort.h"

#ifdef TRAP_MEMORY_CALLS
# include "safe_malloc.h"
#endif

/*
*   FUNCTION DEFINITIONS
*/

extern void catFile (const char *const name)
{
    FILE *const fp = g_fopen (name, "r");

    if (fp != NULL)
    {
	int c;

	while ((c = getc (fp)) != EOF)
	    putchar (c);
	fflush (stdout);
	fclose (fp);
    }
}

#ifdef EXTERNAL_SORT

#ifdef NON_CONST_PUTENV_PROTOTYPE
# define PE_CONST
#else
# define PE_CONST const
#endif

extern void externalSortTags (const boolean toStdout)
{
    const char *const sortCommand = "sort -u -o";
    PE_CONST char *const sortOrder1 = "LC_COLLATE=C";
    PE_CONST char *const sortOrder2 = "LC_ALL=C";
    const size_t length	= 4 + strlen (sortOrder1) + strlen (sortOrder2) +
	    strlen (sortCommand) + (2 * strlen (tagFileName ()));
    char *const cmd = (char *) g_malloc (length + 1);
    int ret = -1;

    if (cmd != NULL)
    {
	/*  Ensure ASCII value sort order.
	 */
#ifdef HAVE_SETENV
	setenv ("LC_COLLATE", "C", 1);
	setenv ("LC_ALL", "C", 1);
	sprintf (cmd, "%s %s %s", sortCommand, tagFileName (), tagFileName ());
#else
# ifdef HAVE_PUTENV
	putenv (sortOrder1);
	putenv (sortOrder2);
	sprintf (cmd, "%s %s %s", sortCommand, tagFileName (), tagFileName ());
# else
	sprintf (cmd, "%s %s %s %s %s", sortOrder1, sortOrder2, sortCommand,
		tagFileName (), tagFileName ());
# endif
#endif
	verbose ("system (\"%s\")\n", cmd);
	ret = system (cmd);
	g_free (cmd);

    }
    if (ret != 0)
	error (FATAL | PERROR, "cannot sort tag file");
    else if (toStdout)
	catFile (tagFileName ());
}

#else

/*
 *  These functions provide a basic internal sort. No great memory
 *  optimization is performed (e.g. recursive subdivided sorts),
 *  so have lots of memory if you have large tag files.
 */

static void failedSort (MIO *const mio, const char* msg)
{
    const char* const cannotSort = "cannot sort tag file";
    if (mio != NULL)
	mio_free (mio);
    if (msg == NULL)
	error (FATAL | PERROR, "%s", cannotSort);
    else
	error (FATAL, "%s: %s", msg, cannotSort);
}

static int compareTags (const void *const one, const void *const two)
{
    const char *const line1 = *(const char* const*) one;
    const char *const line2 = *(const char* const*) two;

    return strcmp (line1, line2);
}

static void writeSortedTags (char **const table, const size_t numTags,
			     const boolean toStdout)
{
    MIO *mio;
    size_t i;

    /*	Write the sorted lines back into the tag file.
     */
    if (toStdout)
	mio = mio_new_fp (stdout, NULL);
    else
    {
	mio = mio_new_file_full (tagFileName (), "w", g_fopen, fclose);
	if (mio == NULL)
	    failedSort (mio, NULL);
    }
    for (i = 0 ; i < numTags ; ++i)
    {
	/*  Here we filter out identical tag *lines* (including search
	 *  pattern) if this is not an xref file.
	 */
	if (i == 0  ||  Option.xref  ||  strcmp (table [i], table [i-1]) != 0)
	    if (mio_puts (mio, table [i]) == EOF)
		failedSort (mio, NULL);
    }
    if (toStdout)
	fflush (mio_file_get_fp (mio));
    mio_free (mio);
}

extern void internalSortTags (const boolean toStdout)
{
    vString *vLine = vStringNew ();
    MIO *mio = NULL;
    const char *line;
    size_t i;

    /*	Allocate a table of line pointers to be sorted.
     */
    size_t numTags = TagFile.numTags.added + TagFile.numTags.prev;
    const size_t tableSize = numTags * sizeof (char *);
    char **const table = (char **) g_malloc (tableSize);	/* line pointers */
    DebugStatement ( size_t mallocSize = tableSize; )	/* cumulative total */

    if (table == NULL)
	failedSort (mio, "out of memory");

    /*	Open the tag file and place its lines into allocated buffers.
     */
    mio = mio_new_file_full (tagFileName (), "r", g_fopen, fclose);
    if (mio == NULL)
	failedSort (mio, NULL);
    for (i = 0  ;  i < numTags  &&  ! mio_eof (mio)  ;  )
    {
	line = readLine (vLine, mio);
	if (line == NULL)
	{
	    if (! mio_eof (mio))
		failedSort (mio, NULL);
	    break;
	}
	else if (*line == '\0'  ||  strcmp (line, "\n") == 0)
	    ;		/* ignore blank lines */
	else
	{
	    const size_t stringSize = strlen (line) + 1;

	    table [i] = (char *) g_malloc (stringSize);
	    if (table [i] == NULL)
		failedSort (mio, "out of memory");
	    DebugStatement ( mallocSize += stringSize; )
	    strcpy (table [i], line);
	    ++i;
	}
    }
    numTags = i;
    mio_free (mio);
    vStringDelete (vLine);

    /*	Sort the lines.
     */
    qsort (table, numTags, sizeof (*table), compareTags);

    writeSortedTags (table, numTags, toStdout);

    PrintStatus (("sort memory: %ld bytes\n", (long) mallocSize));
    for (i = 0 ; i < numTags ; ++i)
	free (table [i]);
    free (table);
}

#endif

/* vi:set tabstop=8 shiftwidth=4: */
