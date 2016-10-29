/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for applying regular expression matching.
*
*   The code for utilizing the Gnu regex package with regards to processing the
*   regex option and checking for regex matches was adapted from routines in
*   Gnu etags.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <glib.h>

#ifdef HAVE_REGCOMP
# include <ctype.h>
# include <stddef.h>
# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>  /* declare off_t (not known to regex.h on FreeBSD) */
# endif
#endif

#include "mio.h"
#include "entry.h"
#include "parse.h"
#include "read.h"
#include "kind.h"
#include "routines.h"

#ifdef HAVE_REGEX

/*
*   MACROS
*/

/* Back-references \0 through \9 */
#define BACK_REFERENCE_COUNT 10

#if defined (HAVE_REGCOMP) && !defined (REGCOMP_BROKEN)
# define POSIX_REGEX
#endif

#define REGEX_NAME "Regex"

/*
*   DATA DECLARATIONS
*/
#if defined (POSIX_REGEX)

enum pType { PTRN_TAG, PTRN_CALLBACK };

typedef struct {
	GRegex *pattern;
	enum pType type;
	union {
		struct {
			char *name_pattern;
			kindOption kind;
		} tag;
		struct {
			regexCallback function;
		} callback;
	} u;
} regexPattern;

#endif

typedef struct {
	regexPattern *patterns;
	unsigned int count;
} patternSet;

/*
*   DATA DEFINITIONS
*/

static bool regexBroken = false;

/* Array of pattern sets, indexed by language */
static patternSet* Sets = NULL;
static int SetUpper = -1;  /* upper language index in list */

/*
*   FUNCTION DEFINITIONS
*/


static void clearPatternSet (const langType language)
{
	if (language <= SetUpper)
	{
		patternSet* const set = Sets + language;
		unsigned int i;
		for (i = 0  ;  i < set->count  ;  ++i)
		{
			regexPattern *p = &set->patterns [i];
			g_regex_unref(p->pattern);
			p->pattern = NULL;

			if (p->type == PTRN_TAG)
			{
				eFree (p->u.tag.name_pattern);
				p->u.tag.name_pattern = NULL;
				eFree ((char *)p->u.tag.kind.name);
				p->u.tag.kind.name = NULL;
				if (p->u.tag.kind.description != NULL)
				{
					eFree ((char *)p->u.tag.kind.description);
					p->u.tag.kind.description = NULL;
				}
			}
		}
		if (set->patterns != NULL)
			eFree (set->patterns);
		set->patterns = NULL;
		set->count = 0;
	}
}

/*
*   Regex psuedo-parser
*/

static void makeRegexTag (
		const vString* const name, const kindOption* const kind)
{
	Assert (kind != NULL);
	if (kind->enabled)
	{
		tagEntryInfo e;
		Assert (name != NULL  &&  vStringLength (name) > 0);
		initTagEntry (&e, vStringValue (name), kind);
		makeTagEntry (&e);
	}
}

/*
*   Regex pattern definition
*/

/* Take a string like "/blah/" and turn it into "blah", making sure
 * that the first and last characters are the same, and handling
 * quoted separator characters.  Actually, stops on the occurrence of
 * an unquoted separator.  Also turns "\t" into a Tab character.
 * Returns pointer to terminating separator.  Works in place.  Null
 * terminates name string.
 */
static char* scanSeparators (char* name)
{
	char sep = name [0];
	char *copyto = name;
	bool quoted = false;

	for (++name ; *name != '\0' ; ++name)
	{
		if (quoted)
		{
			if (*name == sep)
				*copyto++ = sep;
			else if (*name == 't')
				*copyto++ = '\t';
			else
			{
				/* Something else is quoted, so preserve the quote. */
				*copyto++ = '\\';
				*copyto++ = *name;
			}
			quoted = false;
		}
		else if (*name == '\\')
			quoted = true;
		else if (*name == sep)
		{
			break;
		}
		else
			*copyto++ = *name;
	}
	*copyto = '\0';
	return name;
}

/* Parse `regexp', in form "/regex/name/[k,Kind/]flags" (where the separator
 * character is whatever the first character of `regexp' is), by breaking it
 * up into null terminated strings, removing the separators, and expanding
 * '\t' into tabs. When complete, `regexp' points to the line matching
 * pattern, a pointer to the name matching pattern is written to `name', a
 * pointer to the kinds is written to `kinds' (possibly NULL), and a pointer
 * to the trailing flags is written to `flags'. If the pattern is not in the
 * correct format, a false value is returned.
 */
static bool parseTagRegex (
		char* const regexp, char** const name,
		char** const kinds, char** const flags)
{
	bool result = false;
	const int separator = (unsigned char) regexp [0];

	*name = scanSeparators (regexp);
	if (*regexp == '\0')
		printf ("regex: empty regexp\n");
	else if (**name != separator)
		printf ("regex: %s: incomplete regexp\n", regexp);
	else
	{
		char* const third = scanSeparators (*name);
		if (**name == '\0')
			printf ("regex: %s: regexp missing name pattern\n", regexp);
		if ((*name) [strlen (*name) - 1] == '\\')
			printf ("regex: error in name pattern: \"%s\"\n", *name);
		if (*third != separator)
			printf ("regex: %s: regexp missing final separator\n", regexp);
		else
		{
			char* const fourth = scanSeparators (third);
			if (*fourth == separator)
			{
				*kinds = third;
				scanSeparators (fourth);
				*flags = fourth;
			}
			else
			{
				*flags = third;
				*kinds = NULL;
			}
			result = true;
		}
	}
	return result;
}

static void addCompiledTagPattern (
		const langType language, GRegex* const pattern,
		char* const name, const char kind, char* const kindName,
		char *const description)
{
	patternSet* set;
	regexPattern *ptrn;
	if (language > SetUpper)
	{
		int i;
		Sets = xRealloc (Sets, (language + 1), patternSet);
		for (i = SetUpper + 1  ;  i <= language  ;  ++i)
		{
			Sets [i].patterns = NULL;
			Sets [i].count = 0;
		}
		SetUpper = language;
	}
	set = Sets + language;
	set->patterns = xRealloc (set->patterns, (set->count + 1), regexPattern);
	ptrn = &set->patterns [set->count];
	set->count += 1;

	ptrn->pattern = pattern;
	ptrn->type    = PTRN_TAG;
	ptrn->u.tag.name_pattern = name;
	ptrn->u.tag.kind.enabled = true;
	ptrn->u.tag.kind.letter  = kind;
	ptrn->u.tag.kind.name    = kindName;
	ptrn->u.tag.kind.description = description;
}

static void addCompiledCallbackPattern (
		const langType language, GRegex* const pattern,
		const regexCallback callback)
{
	patternSet* set;
	regexPattern *ptrn;
	if (language > SetUpper)
	{
		int i;
		Sets = xRealloc (Sets, (language + 1), patternSet);
		for (i = SetUpper + 1  ;  i <= language  ;  ++i)
		{
			Sets [i].patterns = NULL;
			Sets [i].count = 0;
		}
		SetUpper = language;
	}
	set = Sets + language;
	set->patterns = xRealloc (set->patterns, (set->count + 1), regexPattern);
	ptrn = &set->patterns [set->count];
	set->count += 1;

	ptrn->pattern = pattern;
	ptrn->type    = PTRN_CALLBACK;
	ptrn->u.callback.function = callback;
}

#if defined (POSIX_REGEX)

static GRegex* compileRegex (const char* const regexp, const char* const flags)
{
	int cflags = G_REGEX_MULTILINE;
	GRegex *result = NULL;
	GError *error = NULL;
	int i;
	for (i = 0  ; flags != NULL  &&  flags [i] != '\0'  ;  ++i)
	{
		switch ((int) flags [i])
		{
			case 'b': g_warning("CTags 'b' flag not supported by Geany!"); break;
			case 'e': break;
			case 'i': cflags |= G_REGEX_CASELESS; break;
			default: printf ("regex: unknown regex flag: '%c'\n", *flags); break;
		}
	}
	result = g_regex_new(regexp, cflags, 0, &error);
	if (error)
	{
		printf ("regex: regcomp %s: %s\n", regexp, error->message);
		g_error_free(error);
	}
	return result;
}

#endif

static void parseKinds (
		const char* const kinds, char* const kind, char** const kindName,
		char **description)
{
	*kind = '\0';
	*kindName = NULL;
	*description = NULL;
	if (kinds == NULL  ||  kinds [0] == '\0')
	{
		*kind = 'r';
		*kindName = eStrdup ("regex");
	}
	else if (kinds [0] != '\0')
	{
		const char* k = kinds;
		if (k [0] != ','  &&  (k [1] == ','  ||  k [1] == '\0'))
			*kind = *k++;
		else
			*kind = 'r';
		if (*k == ',')
			++k;
		if (k [0] == '\0')
			*kindName = eStrdup ("regex");
		else
		{
			const char *const comma = strchr (k, ',');
			if (comma == NULL)
				*kindName = eStrdup (k);
			else
			{
				*kindName = (char*) eMalloc (comma - k + 1);
				strncpy (*kindName, k, comma - k);
				(*kindName) [comma - k] = '\0';
				k = comma + 1;
				if (k [0] != '\0')
					*description = eStrdup (k);
			}
		}
	}
}

static void printRegexKind (const regexPattern *pat, unsigned int i, bool indent)
{
	const kindOption *const kind = &pat [i].u.tag.kind;
	const char *const indentation = indent ? "    " : "";
	Assert (pat [i].type == PTRN_TAG);
	printf ("%s%c  %s %s\n", indentation,
			kind->letter != '\0' ? kind->letter : '?',
			kind->description != NULL ? kind->description : kind->name,
			kind->enabled ? "" : " [off]");
}

static void processLanguageRegex (const langType language,
		const char* const parameter)
{
	if (parameter == NULL  ||  parameter [0] == '\0')
		clearPatternSet (language);
	else if (parameter [0] != '@')
		addLanguageRegex (language, parameter);
	else if (! doesFileExist (parameter + 1))
		printf ("regex: cannot open regex file\n");
	else
	{
		const char* regexfile = parameter + 1;
		MIO* const mio = mio_new_file (regexfile, "r");
		if (mio == NULL)
			printf ("regex: %s\n", regexfile);
		else
		{
			vString* const regex = vStringNew ();
			while (readLineRaw (regex, mio))
				addLanguageRegex (language, vStringValue (regex));
			mio_free (mio);
			vStringDelete (regex);
		}
	}
}

/*
*   Regex pattern matching
*/

#if defined (POSIX_REGEX)

static vString* substitute (
		const char* const in, const char* out,
		const int nmatch, const GMatchInfo* const minfo)
{
	vString* result = vStringNew ();
	const char* p;
	for (p = out  ;  *p != '\0'  ;  p++)
	{
		if (*p == '\\'  &&  isdigit ((int) *++p))
		{
			const int dig = *p - '0';
			int so, eo;
			if (0 < dig  &&  dig < nmatch  &&
				g_match_info_fetch_pos(minfo, dig, &so, &eo) && so != -1)
			{
				const int diglen = eo - so;
				vStringNCatS (result, in + so, diglen);
			}
		}
		else if (*p != '\n'  &&  *p != '\r')
			vStringPut (result, *p);
	}
	return result;
}

static void matchTagPattern (const vString* const line,
		const regexPattern* const patbuf,
		const GMatchInfo* const minfo)
{
	vString *const name = substitute (vStringValue (line),
			patbuf->u.tag.name_pattern, BACK_REFERENCE_COUNT, minfo);
	vStringStripLeading (name);
	vStringStripTrailing (name);
	if (vStringLength (name) > 0)
		makeRegexTag (name, &patbuf->u.tag.kind);
	else
		error (WARNING, "%s:%ld: null expansion of name pattern \"%s\"",
			getInputFileName (), getInputLineNumber (),
			patbuf->u.tag.name_pattern);
	vStringDelete (name);
}

static void matchCallbackPattern (
		const vString* const line, const regexPattern* const patbuf,
		const GMatchInfo* const minfo)
{
	regexMatch matches [BACK_REFERENCE_COUNT];
	unsigned int count = 0;
	int i;
	for (i = 0  ;  i < BACK_REFERENCE_COUNT  ;  ++i)
	{
		int so = -1, eo = -1;
		/* with GRegex we could get the real match count, but that might
		 * cause incompatibilities with CTags */
		g_match_info_fetch_pos(minfo, i, &so, &eo);
		matches [i].start  = so;
		matches [i].length = eo - so;
		/* a valid match may have both offsets == -1,
		 * e.g. (foo)*(bar) matching "bar" - see CTags bug 2970274.
		 * As POSIX regex doesn't seem to have a way to count matches,
		 * we return the count up to the last non-empty match. */
		if (so != -1)
			count = i + 1;
	}
	patbuf->u.callback.function (vStringValue (line), matches, count);
}

static bool matchRegexPattern (const vString* const line,
		const regexPattern* const patbuf)
{
	bool result = false;
	GMatchInfo *minfo;
	if (g_regex_match(patbuf->pattern, vStringValue(line), 0, &minfo))
	{
		result = true;
		if (patbuf->type == PTRN_TAG)
			matchTagPattern (line, patbuf, minfo);
		else if (patbuf->type == PTRN_CALLBACK)
			matchCallbackPattern (line, patbuf, minfo);
		else
		{
			Assert ("invalid pattern type" == NULL);
			result = false;
		}
	}
	g_match_info_free(minfo);
	return result;
}

#endif

/* PUBLIC INTERFACE */

/* Match against all patterns for specified language. Returns true if at least
 * on pattern matched.
 */
extern bool matchRegex (const vString* const line, const langType language)
{
	bool result = false;
	if (language != LANG_IGNORE  &&  language <= SetUpper  &&
		Sets [language].count > 0)
	{
		const patternSet* const set = Sets + language;
		unsigned int i;
		for (i = 0  ;  i < set->count  ;  ++i)
			if (matchRegexPattern (line, set->patterns + i))
				result = true;
	}
	return result;
}

extern void findRegexTags (void)
{
	/* merely read all lines of the file */
	while (readLineFromInputFile () != NULL)
		;
}

#endif  /* HAVE_REGEX */

extern void addTagRegex (
		const langType language CTAGS_ATTR_UNUSED,
		const char* const regex CTAGS_ATTR_UNUSED,
		const char* const name CTAGS_ATTR_UNUSED,
		const char* const kinds CTAGS_ATTR_UNUSED,
		const char* const flags CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	Assert (regex != NULL);
	Assert (name != NULL);
	if (! regexBroken)
	{
		GRegex* const cp = compileRegex (regex, flags);
		if (cp != NULL)
		{
			char kind;
			char* kindName;
			char* description;
			parseKinds (kinds, &kind, &kindName, &description);
			addCompiledTagPattern (language, cp, eStrdup (name),
					kind, kindName, description);
		}
	}
#endif
}

extern void addCallbackRegex (
		const langType language CTAGS_ATTR_UNUSED,
		const char* const regex CTAGS_ATTR_UNUSED,
		const char* const flags CTAGS_ATTR_UNUSED,
		const regexCallback callback CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	Assert (regex != NULL);
	if (! regexBroken)
	{
		GRegex* const cp = compileRegex (regex, flags);
		if (cp != NULL)
			addCompiledCallbackPattern (language, cp, callback);
	}
#endif
}

extern void addLanguageRegex (
		const langType language CTAGS_ATTR_UNUSED, const char* const regex CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	if (! regexBroken)
	{
		char *const regex_pat = eStrdup (regex);
		char *name, *kinds, *flags;
		if (parseTagRegex (regex_pat, &name, &kinds, &flags))
		{
			addTagRegex (language, regex_pat, name, kinds, flags);
			eFree (regex_pat);
		}
	}
#endif
}

/*
*   Regex option parsing
*/

extern bool processRegexOption (const char *const option,
								   const char *const parameter CTAGS_ATTR_UNUSED)
{
	bool handled = false;
	const char* const dash = strchr (option, '-');
	if (dash != NULL  &&  strncmp (option, "regex", dash - option) == 0)
	{
#ifdef HAVE_REGEX
		langType language;
		language = getNamedLanguage (dash + 1);
		if (language == LANG_IGNORE)
			printf ("regex: unknown language \"%s\" in --%s option\n", (dash + 1), option);
		else
			processLanguageRegex (language, parameter);
#else
		printf ("regex: regex support not available; required for --%s option\n",
		   option);
#endif
		handled = true;
	}
	return handled;
}

extern void disableRegexKinds (const langType language CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	if (language <= SetUpper  &&  Sets [language].count > 0)
	{
		patternSet* const set = Sets + language;
		unsigned int i;
		for (i = 0  ;  i < set->count  ;  ++i)
			if (set->patterns [i].type == PTRN_TAG)
				set->patterns [i].u.tag.kind.enabled = false;
	}
#endif
}

extern bool enableRegexKind (
		const langType language CTAGS_ATTR_UNUSED,
		const int kind CTAGS_ATTR_UNUSED, const bool mode CTAGS_ATTR_UNUSED)
{
	bool result = false;
#ifdef HAVE_REGEX
	if (language <= SetUpper  &&  Sets [language].count > 0)
	{
		patternSet* const set = Sets + language;
		unsigned int i;
		for (i = 0  ;  i < set->count  ;  ++i)
			if (set->patterns [i].type == PTRN_TAG &&
				set->patterns [i].u.tag.kind.letter == kind)
			{
				set->patterns [i].u.tag.kind.enabled = mode;
				result = true;
			}
	}
#endif
	return result;
}

extern void printRegexKinds (const langType language CTAGS_ATTR_UNUSED, bool indent CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	if (language <= SetUpper  &&  Sets [language].count > 0)
	{
		patternSet* const set = Sets + language;
		unsigned int i;
		for (i = 0  ;  i < set->count  ;  ++i)
			if (set->patterns [i].type == PTRN_TAG)
				printRegexKind (set->patterns, i, indent);
	}
#endif
}

extern void freeRegexResources (void)
{
#ifdef HAVE_REGEX
	int i;
	for (i = 0  ;  i <= SetUpper  ;  ++i)
		clearPatternSet (i);
	if (Sets != NULL)
		eFree (Sets);
	Sets = NULL;
	SetUpper = -1;
#endif
}

/* Check for broken regcomp() on Cygwin */
extern void checkRegex (void)
{
	/* not needed now we have GRegex */
}
