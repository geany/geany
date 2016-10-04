/*
*   Copyright (c) 2002-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains a lose assortment of shared functions.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>  /* to declare malloc (), realloc () */
#endif
#include <ctype.h>
#include <string.h>
#include <stdio.h>  /* to declare tempnam(), and SEEK_SET (hopefully) */

#ifdef HAVE_FCNTL_H
# include <fcntl.h>  /* to declare O_RDWR, O_CREAT, O_EXCL */
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>  /* to declare mkstemp () */
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>  /* to declare MB_LEN_MAX */
#endif
#ifndef MB_LEN_MAX
# define MB_LEN_MAX 6
#endif

/*  To declare "struct stat" and stat ().
 */
#if defined (HAVE_SYS_TYPES_H)
# include <sys/types.h>
#else
# if defined (HAVE_TYPES_H)
#  include <types.h>
# endif
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#else
# ifdef HAVE_STAT_H
#  include <stat.h>
# endif
#endif

#ifdef HAVE_DIRECT_H
# include <direct.h>  /* to _getcwd */
#endif
#ifdef HAVE_DIR_H
# include <dir.h>  /* to declare findfirst() and findnext() */
#endif
#ifdef HAVE_IO_H
# include <io.h>  /* to declare open() */
#endif
#include "debug.h"
#include "routines.h"
#ifdef HAVE_ICONV
# include "mbcs.h"
#endif
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#include "options.h"

/*
*   MACROS
*/
#ifndef TMPDIR
# define TMPDIR "/tmp"
#endif

/*  File type tests.
 */
#ifndef S_ISREG
# if defined (S_IFREG)
#  define S_ISREG(mode)		((mode) & S_IFREG)
# endif
#endif

#ifndef S_ISLNK
# ifdef S_IFLNK
#  define S_ISLNK(mode)		(((mode) & S_IFMT) == S_IFLNK)
# else
#  define S_ISLNK(mode)		false  /* assume no soft links */
# endif
#endif

#ifndef S_ISDIR
# ifdef S_IFDIR
#  define S_ISDIR(mode)		(((mode) & S_IFMT) == S_IFDIR)
# else
#  define S_ISDIR(mode)		false  /* assume no soft links */
# endif
#endif

#ifndef S_IFMT
# define S_IFMT 0
#endif

#ifndef S_IXUSR
# define S_IXUSR 0
#endif
#ifndef S_IXGRP
# define S_IXGRP 0
#endif
#ifndef S_IXOTH
# define S_IXOTH 0
#endif

#ifndef S_IRUSR
# define S_IRUSR 0400
#endif
#ifndef S_IWUSR
# define S_IWUSR 0200
#endif

#ifndef S_ISUID
# define S_ISUID 0
#endif

#ifndef S_ISGID
# define S_ISGID 0
#endif

/*  Hack for ridiculous practice of Microsoft Visual C++.
 */
#if defined (WIN32)
# if defined (_MSC_VER)
#  define stat    _stat
#  define getcwd  _getcwd
#  define PATH_MAX  _MAX_PATH
# endif
#endif

#ifndef PATH_MAX
# define PATH_MAX 256
#endif

/*
 *  Miscellaneous macros
 */

#define selected(var,feature)   (((int)(var) & (int)(feature)) == (int)feature)


/*
*   DATA DEFINITIONS
*/
#if defined (MSDOS_STYLE_PATH)
const char *const PathDelimiters = ":/\\";
#endif

char *CurrentDirectory;

static const char *ExecutableProgram = NULL;
static const char *ExecutableName = "geany";

/*
*   FUNCTION PROTOTYPES
*/
#ifdef NEED_PROTO_STAT
extern int stat (const char *, struct stat *);
#endif
#ifdef NEED_PROTO_LSTAT
extern int lstat (const char *, struct stat *);
#endif


/*
*   FUNCTION DEFINITIONS
*/


extern void setExecutableName (const char *const path)
{
	ExecutableProgram = path;
	ExecutableName = baseFilename (path);
}

extern const char *getExecutableName (void)
{
	return ExecutableName;
}


extern void *eMalloc (const size_t size)
{
	void *buffer = g_malloc (size);

	if (buffer == NULL)
		error (FATAL, "out of memory");

	return buffer;
}

extern void *eCalloc (const size_t count, const size_t size)
{
	void *buffer = calloc (count, size);

	if (buffer == NULL)
		error (FATAL, "out of memory");

	return buffer;
}

extern void *eRealloc (void *const ptr, const size_t size)
{
	void *buffer;
	if (ptr == NULL)
		buffer = eMalloc (size);
	else
	{
		buffer = g_realloc (ptr, size);
		if (buffer == NULL)
			error (FATAL, "out of memory");
	}
	return buffer;
}

extern void eFree (void *const ptr)
{
	if (ptr != NULL)
		free (ptr);
}

#ifndef HAVE_STRSTR
extern char* strstr (const char *str, const char *substr)
{
	const size_t length = strlen (substr);
	const char *match = NULL;
	const char *p;

	for (p = str  ;  *p != '\0'  &&  match == NULL  ;  ++p)
		if (strncmp (p, substr, length) == 0)
			match = p;
	return (char*) match;
}
#endif

extern char* eStrdup (const char* str)
{
	char* result = xMalloc (strlen (str) + 1, char);
	strcpy (result, str);
	return result;
}

extern void toLowerString (char* str)
{
	while (*str != '\0')
	{
		*str = tolower ((int) *str);
		++str;
	}
}

extern void toUpperString (char* str)
{
	while (*str != '\0')
	{
		*str = toupper ((int) *str);
		++str;
	}
}

/*  Newly allocated string containing lower case conversion of a string.
 */
extern char* newLowerString (const char* str)
{
	char* const result = xMalloc (strlen (str) + 1, char);
	int i = 0;
	do
		result [i] = tolower ((int) str [i]);
	while (str [i++] != '\0');
	return result;
}

/*  Newly allocated string containing upper case conversion of a string.
 */
extern char* newUpperString (const char* str)
{
	char* const result = xMalloc (strlen (str) + 1, char);
	int i = 0;
	do
		result [i] = toupper ((int) str [i]);
	while (str [i++] != '\0');
	return result;
}


#if 0
static void setCurrentDirectory (void)
{
	char* const cwd = getcwd (NULL, PATH_MAX);
	CurrentDirectory = xMalloc (strlen (cwd) + 2, char);
	if (cwd [strlen (cwd) - (size_t) 1] == PATH_SEPARATOR)
		strcpy (CurrentDirectory, cwd);
	else
		sprintf (CurrentDirectory, "%s%c", cwd, OUTPUT_PATH_SEPARATOR);
	free (cwd);
}
#endif


extern bool doesFileExist (const char *const fileName)
{
	GStatBuf fileStatus;

	return (bool) (g_stat (fileName, &fileStatus) == 0);
}

extern bool isRecursiveLink (const char* const dirName)
{
	bool result = false;
	char* const path = absoluteFilename (dirName);
	while (path [strlen (path) - 1] == PATH_SEPARATOR)
		path [strlen (path) - 1] = '\0';
	while (! result  &&  strlen (path) > (size_t) 1)
	{
		char *const separator = strrchr (path, PATH_SEPARATOR);
		if (separator == NULL)
			break;
		else if (separator == path)  /* backed up to root directory */
			*(separator + 1) = '\0';
		else
			*separator = '\0';
		result = isSameFile (path, dirName);
	}
	eFree (path);
	return result;
}

extern bool isSameFile (const char *const name1, const char *const name2)
{
	bool result = false;
#ifdef HAVE_STAT_ST_INO
	GStatBuf stat1, stat2;

	if (g_stat (name1, &stat1) == 0  &&  g_stat (name2, &stat2) == 0)
		result = (bool) (stat1.st_ino == stat2.st_ino);
#endif
	return result;
}

/*
 *  Pathname manipulation (O/S dependent!!!)
 */

extern const char *baseFilename (const char *const filePath)
{
#if defined (MSDOS_STYLE_PATH)
	const char *tail = NULL;
	unsigned int i;

	/*  Find whichever of the path delimiters is last.
	 */
	for (i = 0  ;  i < strlen (PathDelimiters)  ;  ++i)
	{
		const char *sep = strrchr (filePath, PathDelimiters [i]);

		if (sep > tail)
			tail = sep;
	}
#else
	const char *tail = strrchr (filePath, PATH_SEPARATOR);
#endif
	if (tail == NULL)
		tail = filePath;
	else
		++tail;  /* step past last delimiter */

	return tail;
}

/*
 *  File extension and language mapping
 */
extern const char *fileExtension (const char *const fileName)
{
	const char *extension;
	const char *pDelimiter = NULL;

	pDelimiter = strrchr (fileName, '.');

	if (pDelimiter == NULL)
		extension = "";
	else
		extension = pDelimiter + 1;     /* skip to first char of extension */

	return extension;
}

extern bool isAbsolutePath (const char *const path)
{
	bool result = false;
#if defined (MSDOS_STYLE_PATH)
	if (strchr (PathDelimiters, path [0]) != NULL)
		result = true;
	else if (isalpha (path [0])  &&  path [1] == ':')
	{
		if (strchr (PathDelimiters, path [2]) != NULL)
			result = true;
		else
			/*  We don't support non-absolute file names with a drive
			 *  letter, like `d:NAME' (it's too much hassle).
			 */
			error (FATAL,
				"%s: relative file names with drive letters not supported",
				path);
	}
#else
	result = (bool) (path [0] == PATH_SEPARATOR);
#endif
	return result;
}

extern vString *combinePathAndFile (const char *const path,
									const char *const file)
{
	vString *const filePath = vStringNew ();
	const int lastChar = path [strlen (path) - 1];
# ifdef MSDOS_STYLE_PATH
	bool terminated = (bool) (strchr (PathDelimiters, lastChar) != NULL);
# else
	bool terminated = (bool) (lastChar == PATH_SEPARATOR);
# endif

	vStringCopyS (filePath, path);
	if (! terminated)
		vStringPut (filePath, OUTPUT_PATH_SEPARATOR);
	vStringCatS (filePath, file);

	return filePath;
}

/* Return a newly-allocated string whose contents concatenate those of
 * s1, s2, s3.
 * Routine adapted from Gnu etags.
 */
static char* concat (const char *s1, const char *s2, const char *s3)
{
	int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
	char *result = xMalloc (len1 + len2 + len3 + 1, char);

	strcpy (result, s1);
	strcpy (result + len1, s2);
	strcpy (result + len1 + len2, s3);
	result [len1 + len2 + len3] = '\0';

	return result;
}

/* Return a newly allocated string containing the absolute file name of FILE
 * given CWD (which should end with a slash).
 * Routine adapted from Gnu etags.
 */
extern char* absoluteFilename (const char *file)
{
	char *slashp, *cp;
	char *res = NULL;
	if (isAbsolutePath (file))
		res = eStrdup (file);
	else
		res = concat (CurrentDirectory, file, "");

	/* Delete the "/dirname/.." and "/." substrings. */
	slashp = strchr (res, '/');
	while (slashp != NULL  &&  slashp [0] != '\0')
	{
		if (slashp[1] == '.')
		{
			if (slashp [2] == '.' && (slashp [3] == '/' || slashp [3] == '\0'))
			{
				cp = slashp;
				do
					cp--;
				while (cp >= res  &&  ! isAbsolutePath (cp));
				if (cp < res)
					cp = slashp;/* the absolute name begins with "/.." */
#ifdef MSDOS_STYLE_PATH
				/* Under MSDOS and NT we get `d:/NAME' as absolute file name,
				 * so the luser could say `d:/../NAME'. We silently treat this
				 * as `d:/NAME'.
				 */
				else if (cp [0] != '/')
					cp = slashp;
#endif
				memmove (cp, slashp + 3, strlen (slashp + 3) + 1);
				slashp = cp;
				continue;
			}
			else if (slashp [2] == '/'  ||  slashp [2] == '\0')
			{
				memmove (slashp, slashp + 2, strlen (slashp + 2) + 1);
				continue;
			}
		}
		slashp = strchr (slashp + 1, '/');
	}

	if (res [0] == '\0')
		return eStrdup ("/");
	else
	{
#ifdef MSDOS_STYLE_PATH
		/* Canonicalize drive letter case. */
		if (res [1] == ':'  &&  islower (res [0]))
			res [0] = toupper (res [0]);
#endif

		return res;
	}
}

/* Return a newly allocated string containing the absolute file name of dir
 * where FILE resides given CWD (which should end with a slash).
 * Routine adapted from Gnu etags.
 */
extern char* absoluteDirname (char *file)
{
	char *slashp, *res;
	char save;
#ifdef MSDOS_STYLE_PATH
	char *p;
	for (p = file  ;  *p != '\0'  ;  p++)
		if (*p == '\\')
			*p = '/';
#endif
	slashp = strrchr (file, '/');
	if (slashp == NULL)
		res = eStrdup (CurrentDirectory);
	else
	{
		save = slashp [1];
		slashp [1] = '\0';
		res = absoluteFilename (file);
		slashp [1] = save;
	}
	return res;
}

/* Return a newly allocated string containing the file name of FILE relative
 * to the absolute directory DIR (which should end with a slash).
 * Routine adapted from Gnu etags.
 */
extern char* relativeFilename (const char *file, const char *dir)
{
	const char *fp, *dp;
	char *absdir, *res;
	int i;

	/* Find the common root of file and dir (with a trailing slash). */
	absdir = absoluteFilename (file);
	fp = absdir;
	dp = dir;
	while (*fp++ == *dp++)
		continue;
	fp--;
	dp--;  /* back to the first differing char */
	do
	{                           /* look at the equal chars until '/' */
		if (fp == absdir)
			return absdir;  /* first char differs, give up */
		fp--;
		dp--;
	} while (*fp != '/');

	/* Build a sequence of "../" strings for the resulting relative file name.
	 */
	i = 0;
	while ((dp = strchr (dp + 1, '/')) != NULL)
		i += 1;
	res = xMalloc (3 * i + strlen (fp + 1) + 1, char);
	res [0] = '\0';
	while (i-- > 0)
		strcat (res, "../");

	/* Add the file name relative to the common root of file and dir. */
	strcat (res, fp + 1);
	free (absdir);

	return res;
}

extern long unsigned int getFileSize (const char *const name)
{
	GStatBuf fileStatus;
	unsigned long size = 0;

	if (g_stat (name, &fileStatus) == 0)
		size = fileStatus.st_size;

	return size;
}

#if 0
static bool isSymbolicLink (const char *const name)
{
#if defined (WIN32)
	return false;
#else
	GStatBuf fileStatus;
	bool result = false;

	if (g_lstat (name, &fileStatus) == 0)
		result = (bool) (S_ISLNK (fileStatus.st_mode));

	return result;
#endif
}

static bool isNormalFile (const char *const name)
{
	GStatBuf fileStatus;
	bool result = false;

	if (g_stat (name, &fileStatus) == 0)
		result = (bool) (S_ISREG (fileStatus.st_mode));

	return result;
}
#endif

extern bool isExecutable (const char *const name)
{
	GStatBuf fileStatus;
	bool result = false;

	if (g_stat (name, &fileStatus) == 0)
		result = (bool) ((fileStatus.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) != 0);

	return result;
}

#ifdef HAVE_MKSTEMP

static bool isSetUID (const char *const name)
{
#if defined (WIN32)
	return false;
#else
	GStatBuf fileStatus;
	bool result = false;

	if (g_stat (name, &fileStatus) == 0)
		result = (bool) ((fileStatus.st_mode & S_ISUID) != 0);

	return result;
#endif
}

#endif

#if 0
static bool isDirectory (const char *const name)
{
	bool result = false;
	GStatBuf fileStatus;

	if (g_stat (name, &fileStatus) == 0)
		result = (bool) S_ISDIR (fileStatus.st_mode);
	return result;
}
#endif

/*#ifndef HAVE_FGETPOS*/
/*
extern int fgetpos ( stream, pos )
	FILE *const stream;
	fpos_t *const pos;
{
	int result = 0;

	*pos = ftell (stream);
	if (*pos == -1L)
		result = -1;

	return result;
}

extern int fsetpos ( stream, pos )
	FILE *const stream;
	fpos_t *const pos;
{
	return fseek (stream, *pos, SEEK_SET);
}
*/
/*#endif*/

extern FILE *tempFile (const char *const mode, char **const pName)
{
	char *name;
	FILE *fp;
	int fd;
#ifdef HAVE_MKSTEMP
	const char *const template = "tags.XXXXXX";
	const char *tmpdir = NULL;
	if (! isSetUID (ExecutableProgram))
		tmpdir = getenv ("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = TMPDIR;
	name = xMalloc (strlen (tmpdir) + 1 + strlen (template) + 1, char);
	sprintf (name, "%s%c%s", tmpdir, OUTPUT_PATH_SEPARATOR, template);
	fd = mkstemp(name);
#else
	name = xMalloc (L_tmpnam, char);
	if (tmpnam (name) != name)
		error (FATAL | PERROR, "cannot assign temporary file name");
	fd = open (name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif
	if (fd == -1)
		error (FATAL | PERROR, "cannot open temporary file");
	fp = fdopen (fd, mode);
	if (fp == NULL)
		error (FATAL | PERROR, "cannot open temporary file");
	DebugStatement (
		debugPrintf (DEBUG_STATUS, "opened temporary file %s\n", name); )
	Assert (*pName == NULL);
	*pName = name;
	return fp;
}

extern void error (const errorSelection selection,
				   const char *const format, ...)
{
	va_list ap;

	va_start (ap, format);
	fprintf (errout, "%s: %s", getExecutableName (),
			selected (selection, WARNING) ? "Warning: " : "");
	vfprintf (errout, format, ap);
	if (selected (selection, PERROR))
		fprintf (errout, " : %s", g_strerror (errno));
	fputs ("\n", errout);
	va_end (ap);
	if (selected (selection, FATAL))
		exit (1);
}

#ifndef HAVE_STRICMP
extern int stricmp (const char *s1, const char *s2)
{
	int result;
	do
	{
		result = toupper ((int) *s1) - toupper ((int) *s2);
	} while (result == 0  &&  *s1++ != '\0'  &&  *s2++ != '\0');
	return result;
}
#endif

#ifndef HAVE_STRNICMP
extern int strnicmp (const char *s1, const char *s2, size_t n)
{
	int result;
	do
	{
		result = toupper ((int) *s1) - toupper ((int) *s2);
	} while (result == 0  &&  --n > 0  &&  *s1++ != '\0'  &&  *s2++ != '\0');
	return result;
}
#endif
