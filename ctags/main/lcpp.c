/*
*   Copyright (c) 1996-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains the high level input read functions (preprocessor
*   directives are handled within this level).
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <glib.h>

#include "debug.h"
#include "entry.h"
#include "lcpp.h"
#include "kind.h"
#include "options.h"
#include "read.h"
#include "vstring.h"

/*
*   MACROS
*/
#define stringMatch(s1,s2)		(strcmp (s1,s2) == 0)
#define isspacetab(c)			((c) == SPACE || (c) == TAB)

/*
*   DATA DECLARATIONS
*/
typedef enum { COMMENT_NONE, COMMENT_C, COMMENT_CPLUS, COMMENT_D } Comment;

enum eCppLimits {
	MaxCppNestingLevel = 20,
	MaxDirectiveName = 10
};

/*  Defines the one nesting level of a preprocessor conditional.
 */
typedef struct sConditionalInfo {
	bool ignoreAllBranches;  /* ignoring parent conditional branch */
	bool singleBranch;       /* choose only one branch */
	bool branchChosen;       /* branch already selected */
	bool ignoring;           /* current ignore state */
} conditionalInfo;

enum eState {
	DRCTV_NONE,    /* no known directive - ignore to end of line */
	DRCTV_DEFINE,  /* "#define" encountered */
	DRCTV_HASH,    /* initial '#' read; determine directive */
	DRCTV_IF,      /* "#if" or "#ifdef" encountered */
	DRCTV_PRAGMA,  /* #pragma encountered */
	DRCTV_UNDEF    /* "#undef" encountered */
};

/*  Defines the current state of the pre-processor.
 */
typedef struct sCppState {
	int		ungetch, ungetch2;   /* ungotten characters, if any */
	bool resolveRequired;     /* must resolve if/else/elif/endif branch */
	bool hasAtLiteralStrings; /* supports @"c:\" strings */
	bool hasCxxRawLiteralStrings; /* supports R"xxx(...)xxx" strings */
	const kindOption  *defineMacroKind;
	struct sDirective {
		enum eState state;       /* current directive being processed */
		bool	accept;          /* is a directive syntactically permitted? */
		vString * name;          /* macro name */
		unsigned int nestLevel;  /* level 0 is not used */
		conditionalInfo ifdef [MaxCppNestingLevel];
	} directive;
} cppState;

/*
*   DATA DEFINITIONS
*/

/*  Use brace formatting to detect end of block.
 */
static bool BraceFormat = false;

static cppState Cpp = {
	'\0', '\0',  /* ungetch characters */
	false,       /* resolveRequired */
	false,       /* hasAtLiteralStrings */
	false,       /* hasCxxRawLiteralStrings */
	NULL,        /* defineMacroKind */
	{
		DRCTV_NONE,  /* state */
		false,       /* accept */
		NULL,        /* tag name */
		0,           /* nestLevel */
		{ {false,false,false,false} }  /* ifdef array */
	}  /* directive */
};

/*
*   FUNCTION DEFINITIONS
*/

extern bool cppIsBraceFormat (void)
{
	return BraceFormat;
}

extern unsigned int cppGetDirectiveNestLevel (void)
{
	return Cpp.directive.nestLevel;
}

extern void cppInit (const bool state, const bool hasAtLiteralStrings,
                     const bool hasCxxRawLiteralStrings,
                     const kindOption *defineMacroKind)
{
	BraceFormat = state;

	Cpp.ungetch         = '\0';
	Cpp.ungetch2        = '\0';
	Cpp.resolveRequired = false;
	Cpp.hasAtLiteralStrings = hasAtLiteralStrings;
	Cpp.hasCxxRawLiteralStrings = hasCxxRawLiteralStrings;
	Cpp.defineMacroKind = defineMacroKind;

	Cpp.directive.state     = DRCTV_NONE;
	Cpp.directive.accept    = true;
	Cpp.directive.nestLevel = 0;

	Cpp.directive.ifdef [0].ignoreAllBranches = false;
	Cpp.directive.ifdef [0].singleBranch = false;
	Cpp.directive.ifdef [0].branchChosen = false;
	Cpp.directive.ifdef [0].ignoring     = false;

	if (Cpp.directive.name == NULL)
		Cpp.directive.name = vStringNew ();
	else
		vStringClear (Cpp.directive.name);
}

extern void cppTerminate (void)
{
	if (Cpp.directive.name != NULL)
	{
		vStringDelete (Cpp.directive.name);
		Cpp.directive.name = NULL;
	}
}

extern void cppBeginStatement (void)
{
	Cpp.resolveRequired = true;
}

extern void cppEndStatement (void)
{
	Cpp.resolveRequired = false;
}

/*
*   Scanning functions
*
*   This section handles preprocessor directives.  It strips out all
*   directives and may emit a tag for #define directives.
*/

/*  This puts a character back into the input queue for the input File.
 *  Up to two characters may be ungotten.
 */
extern void cppUngetc (const int c)
{
	Assert (Cpp.ungetch2 == '\0');
	Cpp.ungetch2 = Cpp.ungetch;
	Cpp.ungetch = c;
}

/*  Reads a directive, whose first character is given by "c", into "name".
 */
static bool readDirective (int c, char *const name, unsigned int maxLength)
{
	unsigned int i;

	for (i = 0  ;  i < maxLength - 1  ;  ++i)
	{
		if (i > 0)
		{
			c = getcFromInputFile ();
			if (c == EOF  ||  ! isalpha (c))
			{
				ungetcToInputFile (c);
				break;
			}
		}
		name [i] = c;
	}
	name [i] = '\0';  /* null terminate */

	return (bool) isspacetab (c);
}

/*  Reads an identifier, whose first character is given by "c", into "tag",
 *  together with the file location and corresponding line number.
 */
static void readIdentifier (int c, vString *const name)
{
	vStringClear (name);
	do
	{
		vStringPut (name, c);
		c = getcFromInputFile ();
	} while (c != EOF && cppIsident (c));
	ungetcToInputFile (c);
}

static conditionalInfo *currentConditional (void)
{
	return &Cpp.directive.ifdef [Cpp.directive.nestLevel];
}

static bool isIgnore (void)
{
	return Cpp.directive.ifdef [Cpp.directive.nestLevel].ignoring;
}

static bool setIgnore (const bool ignore)
{
	return Cpp.directive.ifdef [Cpp.directive.nestLevel].ignoring = ignore;
}

static bool isIgnoreBranch (void)
{
	conditionalInfo *const ifdef = currentConditional ();

	/*  Force a single branch if an incomplete statement is discovered
	 *  en route. This may have allowed earlier branches containing complete
	 *  statements to be followed, but we must follow no further branches.
	 */
	if (Cpp.resolveRequired  &&  ! BraceFormat)
		ifdef->singleBranch = true;

	/*  We will ignore this branch in the following cases:
	 *
	 *  1.  We are ignoring all branches (conditional was within an ignored
	 *        branch of the parent conditional)
	 *  2.  A branch has already been chosen and either of:
	 *      a.  A statement was incomplete upon entering the conditional
	 *      b.  A statement is incomplete upon encountering a branch
	 */
	return (bool) (ifdef->ignoreAllBranches ||
					 (ifdef->branchChosen  &&  ifdef->singleBranch));
}

static void chooseBranch (void)
{
	if (! BraceFormat)
	{
		conditionalInfo *const ifdef = currentConditional ();

		ifdef->branchChosen = (bool) (ifdef->singleBranch ||
										Cpp.resolveRequired);
	}
}

/*  Pushes one nesting level for an #if directive, indicating whether or not
 *  the branch should be ignored and whether a branch has already been chosen.
 */
static bool pushConditional (const bool firstBranchChosen)
{
	const bool ignoreAllBranches = isIgnore ();  /* current ignore */
	bool ignoreBranch = false;

	if (Cpp.directive.nestLevel < (unsigned int) MaxCppNestingLevel - 1)
	{
		conditionalInfo *ifdef;

		++Cpp.directive.nestLevel;
		ifdef = currentConditional ();

		/*  We take a snapshot of whether there is an incomplete statement in
		 *  progress upon encountering the preprocessor conditional. If so,
		 *  then we will flag that only a single branch of the conditional
		 *  should be followed.
		 */
		ifdef->ignoreAllBranches = ignoreAllBranches;
		ifdef->singleBranch      = Cpp.resolveRequired;
		ifdef->branchChosen      = firstBranchChosen;
		ifdef->ignoring = (bool) (ignoreAllBranches || (
				! firstBranchChosen  &&  ! BraceFormat  &&
				(ifdef->singleBranch || !Option.if0)));
		ignoreBranch = ifdef->ignoring;
	}
	return ignoreBranch;
}

/*  Pops one nesting level for an #endif directive.
 */
static bool popConditional (void)
{
	if (Cpp.directive.nestLevel > 0)
		--Cpp.directive.nestLevel;

	return isIgnore ();
}

static void makeDefineTag (const char *const name, bool parameterized)
{
	const bool isFileScope = (bool) (! isInputHeaderFile ());

	if (includingDefineTags () &&
		(! isFileScope  ||  Option.include.fileScope))
	{
		tagEntryInfo e;

		initTagEntry (&e, name, Cpp.defineMacroKind);

		e.lineNumberEntry = (bool) (Option.locate != EX_PATTERN);
		e.isFileScope  = isFileScope;
		e.truncateLine = true;
		if (parameterized)
		{
			e.extensionFields.signature = cppGetArglistFromFilePos(getInputFilePosition()
					, e.name);
		}
		makeTagEntry (&e);
		if (parameterized)
			free((char *) e.extensionFields.signature);
	}
}

static void directiveDefine (const int c)
{
	bool parameterized;
	int nc;

	if (cppIsident1 (c))
	{
		readIdentifier (c, Cpp.directive.name);
		nc = getcFromInputFile ();
		ungetcToInputFile (nc);
		parameterized = (bool) (nc == '(');
		if (! isIgnore ())
			makeDefineTag (vStringValue (Cpp.directive.name), parameterized);
	}
	Cpp.directive.state = DRCTV_NONE;
}

static void directivePragma (int c)
{
	if (cppIsident1 (c))
	{
		readIdentifier (c, Cpp.directive.name);
		if (stringMatch (vStringValue (Cpp.directive.name), "weak"))
		{
			/* generate macro tag for weak name */
			do
			{
				c = getcFromInputFile ();
			} while (c == SPACE);
			if (cppIsident1 (c))
			{
				readIdentifier (c, Cpp.directive.name);
				makeDefineTag (vStringValue (Cpp.directive.name), false);
			}
		}
	}
	Cpp.directive.state = DRCTV_NONE;
}

static bool directiveIf (const int c)
{
	const bool ignore = pushConditional ((bool) (c != '0'));

	Cpp.directive.state = DRCTV_NONE;

	return ignore;
}

static bool directiveHash (const int c)
{
	bool ignore = false;
	char directive [MaxDirectiveName];
	DebugStatement ( const bool ignore0 = isIgnore (); )

	readDirective (c, directive, MaxDirectiveName);
	if (stringMatch (directive, "define"))
		Cpp.directive.state = DRCTV_DEFINE;
	else if (stringMatch (directive, "undef"))
		Cpp.directive.state = DRCTV_UNDEF;
	else if (strncmp (directive, "if", (size_t) 2) == 0)
		Cpp.directive.state = DRCTV_IF;
	else if (stringMatch (directive, "elif")  ||
			stringMatch (directive, "else"))
	{
		ignore = setIgnore (isIgnoreBranch ());
		if (! ignore  &&  stringMatch (directive, "else"))
			chooseBranch ();
		Cpp.directive.state = DRCTV_NONE;
		DebugStatement ( if (ignore != ignore0) debugCppIgnore (ignore); )
	}
	else if (stringMatch (directive, "endif"))
	{
		DebugStatement ( debugCppNest (false, Cpp.directive.nestLevel); )
		ignore = popConditional ();
		Cpp.directive.state = DRCTV_NONE;
		DebugStatement ( if (ignore != ignore0) debugCppIgnore (ignore); )
	}
	else if (stringMatch (directive, "pragma"))
		Cpp.directive.state = DRCTV_PRAGMA;
	else
		Cpp.directive.state = DRCTV_NONE;

	return ignore;
}

/*  Handles a pre-processor directive whose first character is given by "c".
 */
static bool handleDirective (const int c)
{
	bool ignore = isIgnore ();

	switch (Cpp.directive.state)
	{
		case DRCTV_NONE:    ignore = isIgnore ();        break;
		case DRCTV_DEFINE:  directiveDefine (c);         break;
		case DRCTV_HASH:    ignore = directiveHash (c);  break;
		case DRCTV_IF:      ignore = directiveIf (c);    break;
		case DRCTV_PRAGMA:  directivePragma (c);         break;
		case DRCTV_UNDEF:   directiveDefine (c);         break;
	}
	return ignore;
}

/*  Called upon reading of a slash ('/') characters, determines whether a
 *  comment is encountered, and its type.
 */
static Comment isComment (void)
{
	Comment comment;
	const int next = getcFromInputFile ();

	if (next == '*')
		comment = COMMENT_C;
	else if (next == '/')
		comment = COMMENT_CPLUS;
	else if (next == '+')
		comment = COMMENT_D;
	else
	{
		ungetcToInputFile (next);
		comment = COMMENT_NONE;
	}
	return comment;
}

/*  Skips over a C style comment. According to ANSI specification a comment
 *  is treated as white space, so we perform this substitution.
 */
int cppSkipOverCComment (void)
{
	int c = getcFromInputFile ();

	while (c != EOF)
	{
		if (c != '*')
			c = getcFromInputFile ();
		else
		{
			const int next = getcFromInputFile ();

			if (next != '/')
				c = next;
			else
			{
				c = SPACE;  /* replace comment with space */
				break;
			}
		}
	}
	return c;
}

/*  Skips over a C++ style comment.
 */
static int skipOverCplusComment (void)
{
	int c;

	while ((c = getcFromInputFile ()) != EOF)
	{
		if (c == BACKSLASH)
			getcFromInputFile ();  /* throw away next character, too */
		else if (c == NEWLINE)
			break;
	}
	return c;
}

/* Skips over a D style comment.
 * Really we should match nested /+ comments. At least they're less common.
 */
static int skipOverDComment (void)
{
	int c = getcFromInputFile ();

	while (c != EOF)
	{
		if (c != '+')
			c = getcFromInputFile ();
		else
		{
			const int next = getcFromInputFile ();

			if (next != '/')
				c = next;
			else
			{
				c = SPACE;  /* replace comment with space */
				break;
			}
		}
	}
	return c;
}

/*  Skips to the end of a string, returning a special character to
 *  symbolically represent a generic string.
 */
static int skipToEndOfString (bool ignoreBackslash)
{
	int c;

	while ((c = getcFromInputFile ()) != EOF)
	{
		if (c == BACKSLASH && ! ignoreBackslash)
			getcFromInputFile ();  /* throw away next character, too */
		else if (c == DOUBLE_QUOTE)
			break;
	}
	return STRING_SYMBOL;  /* symbolic representation of string */
}

static int isCxxRawLiteralDelimiterChar (int c)
{
	return (c != ' ' && c != '\f' && c != '\n' && c != '\r' && c != '\t' && c != '\v' &&
	        c != '(' && c != ')' && c != '\\');
}

static int skipToEndOfCxxRawLiteralString (void)
{
	int c = getcFromInputFile ();

	if (c != '(' && ! isCxxRawLiteralDelimiterChar (c))
	{
		ungetcToInputFile (c);
		c = skipToEndOfString (false);
	}
	else
	{
		char delim[16];
		unsigned int delimLen = 0;
		bool collectDelim = true;

		do
		{
			if (collectDelim)
			{
				if (isCxxRawLiteralDelimiterChar (c) &&
				    delimLen < (sizeof delim / sizeof *delim))
					delim[delimLen++] = c;
				else
					collectDelim = false;
			}
			else if (c == ')')
			{
				unsigned int i = 0;

				while ((c = getcFromInputFile ()) != EOF && i < delimLen && delim[i] == c)
					i++;
				if (i == delimLen && c == DOUBLE_QUOTE)
					break;
				else
					ungetcToInputFile (c);
			}
		}
		while ((c = getcFromInputFile ()) != EOF);
		c = STRING_SYMBOL;
	}
	return c;
}

/*  Skips to the end of the three (possibly four) 'c' sequence, returning a
 *  special character to symbolically represent a generic character.
 *  Also detects Vera numbers that include a base specifier (ie. 'b1010).
 */
static int skipToEndOfChar (void)
{
	int c;
	int count = 0, veraBase = '\0';

	while ((c = getcFromInputFile ()) != EOF)
	{
	    ++count;
		if (c == BACKSLASH)
			getcFromInputFile ();  /* throw away next character, too */
		else if (c == SINGLE_QUOTE)
			break;
		else if (c == NEWLINE)
		{
			ungetcToInputFile (c);
			break;
		}
		else if (count == 1  &&  strchr ("DHOB", toupper (c)) != NULL)
			veraBase = c;
		else if (veraBase != '\0'  &&  ! isalnum (c))
		{
			ungetcToInputFile (c);
			break;
		}
	}
	return CHAR_SYMBOL;  /* symbolic representation of character */
}

/*  This function returns the next character, stripping out comments,
 *  C pre-processor directives, and the contents of single and double
 *  quoted strings. In short, strip anything which places a burden upon
 *  the tokenizer.
 */
extern int cppGetc (void)
{
	bool directive = false;
	bool ignore = false;
	int c;

	if (Cpp.ungetch != '\0')
	{
		c = Cpp.ungetch;
		Cpp.ungetch = Cpp.ungetch2;
		Cpp.ungetch2 = '\0';
		return c;  /* return here to avoid re-calling debugPutc () */
	}
	else do
	{
		c = getcFromInputFile ();
process:
		switch (c)
		{
			case EOF:
				ignore    = false;
				directive = false;
				break;

			case TAB:
			case SPACE:
				break;  /* ignore most white space */

			case NEWLINE:
				if (directive  &&  ! ignore)
					directive = false;
				Cpp.directive.accept = true;
				break;

			case DOUBLE_QUOTE:
				Cpp.directive.accept = false;
				c = skipToEndOfString (false);
				break;

			case '#':
				if (Cpp.directive.accept)
				{
					directive = true;
					Cpp.directive.state  = DRCTV_HASH;
					Cpp.directive.accept = false;
				}
				break;

			case SINGLE_QUOTE:
				Cpp.directive.accept = false;
				c = skipToEndOfChar ();
				break;

			case '/':
			{
				const Comment comment = isComment ();

				if (comment == COMMENT_C)
					c = cppSkipOverCComment ();
				else if (comment == COMMENT_CPLUS)
				{
					c = skipOverCplusComment ();
					if (c == NEWLINE)
						ungetcToInputFile (c);
				}
				else if (comment == COMMENT_D)
					c = skipOverDComment ();
				else
					Cpp.directive.accept = false;
				break;
			}

			case BACKSLASH:
			{
				int next = getcFromInputFile ();

				if (next == NEWLINE)
				{
					c = getcFromInputFile ();
					goto process;
				}
				else
					ungetcToInputFile (next);
				break;
			}

			case '?':
			{
				int next = getcFromInputFile ();
				if (next != '?')
					ungetcToInputFile (next);
				else
				{
					next = getcFromInputFile ();
					switch (next)
					{
						case '(':          c = '[';       break;
						case ')':          c = ']';       break;
						case '<':          c = '{';       break;
						case '>':          c = '}';       break;
						case '/':          c = BACKSLASH; goto process;
						case '!':          c = '|';       break;
						case SINGLE_QUOTE: c = '^';       break;
						case '-':          c = '~';       break;
						case '=':          c = '#';       goto process;
						default:
							ungetcToInputFile ('?');
							ungetcToInputFile (next);
							break;
					}
				}
			} break;

			/* digraphs:
			 * input:  <:  :>  <%  %>  %:  %:%:
			 * output: [   ]   {   }   #   ##
			 */
			case '<':
			{
				int next = getcFromInputFile ();
				switch (next)
				{
					case ':':	c = '['; break;
					case '%':	c = '{'; break;
					default: ungetcToInputFile (next);
				}
				goto enter;
			}
			case ':':
			{
				int next = getcFromInputFile ();
				if (next == '>')
					c = ']';
				else
					ungetcToInputFile (next);
				goto enter;
			}
			case '%':
			{
				int next = getcFromInputFile ();
				switch (next)
				{
					case '>':	c = '}'; break;
					case ':':	c = '#'; goto process;
					default: ungetcToInputFile (next);
				}
				goto enter;
			}

			default:
				if (c == '@' && Cpp.hasAtLiteralStrings)
				{
					int next = getcFromInputFile ();
					if (next == DOUBLE_QUOTE)
					{
						Cpp.directive.accept = false;
						c = skipToEndOfString (true);
						break;
					}
					else
						ungetcToInputFile (next);
				}
				else if (c == 'R' && Cpp.hasCxxRawLiteralStrings)
				{
					/* OMG!11 HACK!!11  Get the previous character.
					 *
					 * We need to know whether the previous character was an identifier or not,
					 * because "R" has to be on its own, not part of an identifier.  This allows
					 * for constructs like:
					 *
					 * 	#define FOUR "4"
					 * 	const char *p = FOUR"5";
					 *
					 * which is not a raw literal, but a preprocessor concatenation.
					 *
					 * FIXME: handle
					 *
					 * 	const char *p = R\
					 * 	"xxx(raw)xxx";
					 *
					 * which is perfectly valid (yet probably very unlikely). */
					int prev = getNthPrevCFromInputFile (1, '\0');
					int prev2 = getNthPrevCFromInputFile (2, '\0');
					int prev3 = getNthPrevCFromInputFile (3, '\0');

					if (! cppIsident (prev) ||
					    (! cppIsident (prev2) && (prev == 'L' || prev == 'u' || prev == 'U')) ||
					    (! cppIsident (prev3) && (prev2 == 'u' && prev == '8')))
					{
						int next = getcFromInputFile ();
						if (next != DOUBLE_QUOTE)
							ungetcToInputFile (next);
						else
						{
							Cpp.directive.accept = false;
							c = skipToEndOfCxxRawLiteralString ();
							break;
						}
					}
				}
			enter:
				Cpp.directive.accept = false;
				if (directive)
					ignore = handleDirective (c);
				break;
		}
	} while (directive || ignore);

	DebugStatement ( debugPutc (DEBUG_CPP, c); )
	DebugStatement ( if (c == NEWLINE)
				debugPrintf (DEBUG_CPP, "%6ld: ", getInputLineNumber () + 1); )

	return c;
}

typedef enum
{
	st_none_t,
	st_escape_t,
	st_c_comment_t,
	st_cpp_comment_t,
	st_double_quote_t,
	st_single_quote_t
} ParseState;

static void stripCodeBuffer(char *buf)
{
	int i = 0, pos = 0;
	ParseState state = st_none_t, prev_state = st_none_t;

	while (buf[i] != '\0')
	{
		switch(buf[i])
		{
			case '/':
				if (st_none_t == state)
				{
					/* Check if this is the start of a comment */
					if (buf[i+1] == '*') /* C comment */
						state = st_c_comment_t;
					else if (buf[i+1] == '/') /* C++ comment */
						state = st_cpp_comment_t;
					else /* Normal character */
						buf[pos++] = '/';
				}
				else if (st_c_comment_t == state)
				{
					/* Check if this is the end of a C comment */
					if (buf[i-1] == '*')
					{
						if ((pos > 0) && (buf[pos-1] != ' '))
							buf[pos++] = ' ';
						state = st_none_t;
					}
				}
				break;
			case '"':
				if (st_none_t == state)
					state = st_double_quote_t;
				else if (st_double_quote_t == state)
					state = st_none_t;
				break;
			case '\'':
				if (st_none_t == state)
					state = st_single_quote_t;
				else if (st_single_quote_t == state)
					state = st_none_t;
				break;
			default:
				if ((buf[i] == '\\') && (st_escape_t != state))
				{
					prev_state = state;
					state = st_escape_t;
				}
				else if (st_escape_t == state)
				{
					state = prev_state;
					prev_state = st_none_t;
				}
				else if ((buf[i] == '\n') && (st_cpp_comment_t == state))
				{
					if ((pos > 0) && (buf[pos-1] != ' '))
						buf[pos++] = ' ';
					state = st_none_t;
				}
				else if (st_none_t == state)
				{
					if (isspace(buf[i]))
					{
						if ((pos > 0) && (buf[pos-1] != ' '))
							buf[pos++] = ' ';
					}
					else
						buf[pos++] = buf[i];
				}
				break;
		}
		++i;
	}
	buf[pos] = '\0';
	return;
}

static char *getArglistFromStr(char *buf, const char *name)
{
	char *start, *end;
	int level;
	if ((NULL == buf) || (NULL == name) || ('\0' == name[0]))
		return NULL;
	stripCodeBuffer(buf);
	if (NULL == (start = strstr(buf, name)))
		return NULL;
	if (NULL == (start = strchr(start, '(')))
		return NULL;
	for (level = 1, end = start + 1; level > 0; ++end)
	{
		if ('\0' == *end)
			break;
		else if ('(' == *end)
			++ level;
		else if (')' == *end)
			-- level;
	}
	*end = '\0';
	return strdup(start);
}

extern char *cppGetArglistFromFilePos(MIOPos startPosition, const char *tokenName)
{
	MIOPos originalPosition;
	char *result = NULL;
	char *arglist = NULL;
	long pos1, pos2;

	pos2 = mio_tell(File.mio);

	mio_getpos(File.mio, &originalPosition);
	mio_setpos(File.mio, &startPosition);
	pos1 = mio_tell(File.mio);

	if (pos2 > pos1)
	{
		size_t len = pos2 - pos1;

		result = (char *) g_malloc(len + 1);
		if (result != NULL && (len = mio_read(File.mio, result, 1, len)) > 0)
		{
			result[len] = '\0';
			arglist = getArglistFromStr(result, tokenName);
		}
		g_free(result);
	}
	mio_setpos(File.mio, &originalPosition);
	return arglist;
}
