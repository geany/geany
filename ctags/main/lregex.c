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

#include <ctype.h>
#include <stddef.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>  /* declare off_t (not known to regex.h on FreeBSD) */
#endif
#include <regex.h>

#include <inttypes.h>

#include "debug.h"
#include "colprint_p.h"
#include "entry_p.h"
#include "field_p.h"
#include "flags_p.h"
#include "htable.h"
#include "kind.h"
#include "options.h"
#include "parse_p.h"
#include "promise.h"
#include "read.h"
#include "read_p.h"
#include "routines.h"
#include "routines_p.h"
#include "trashbox.h"
#include "xtag_p.h"

static bool regexAvailable = false;

/*
*   MACROS
*/

/* Back-references \0 through \9 */
#define BACK_REFERENCE_COUNT 10

/* The max depth of taction=enter/leave stack */
#define MTABLE_STACK_MAX_DEPTH 64

/* How many times ctags allows a mtable parser
   stays at the same input position across table switching.

   The value is derived from MTABLE_STACK_MAX_DEPTH.
   No deep meaning is in that. It just for simplifying
   Tmain cases. */
#define MTABLE_MOTIONLESS_MAX (MTABLE_STACK_MAX_DEPTH + 1)


/*
*   DATA DECLARATIONS
*/

enum pType { PTRN_TAG, PTRN_CALLBACK };

enum scopeAction {
	SCOPE_REF     = 1UL << 0,
	SCOPE_POP     = 1UL << 1,
	SCOPE_PUSH    = 1UL << 2,
	SCOPE_CLEAR   = 1UL << 3,
	SCOPE_PLACEHOLDER = 1UL << 4,
};

enum tableAction {
	TACTION_NOP,
	TACTION_ENTER,				/* {tenter=N} */
	TACTION_LEAVE,				/* {tleave} */
	TACTION_JUMP,					/* {tjump=N} */
	TACTION_RESET,				/* {treset=N} */
	TACTION_QUIT,					/* {tquit} */
};

struct fieldPattern {
	fieldType ftype;
	const char *template;
};

struct boundarySpec {
	int patternGroup;
	bool fromStartOfGroup;
	bool placeholder;
};

struct guestLangSpec {
	enum guestLangSpecType {
		GUEST_LANG_UNKNOWN,
		GUEST_LANG_PLACEHOLDER,			   /* _ */
		GUEST_LANG_STATIC_LANGNAME,		   /* C, Python,... */
		GUEST_LANG_PTN_GROUP_FOR_LANGNAME, /* \1, \2, ..., \9 */
		GUEST_LANG_PTN_GROUP_FOR_FILEMAP, /* *1, *2, ... *9 */
	} type;
	union {
		langType lang;
		int patternGroup;
	} spec;
};

struct guestSpec {
	struct guestLangSpec lang;
#define BOUNDARY_START 0
#define BOUNDARY_END  1
	struct boundarySpec boundary[2];
};

struct mGroupSpec {
#define NO_MULTILINE -1
	int forLineNumberDetermination;
	int forNextScanning;
	/* true => start, false => end */
	bool nextFromStart;
};

struct mTableActionSpec {
	enum tableAction action;
	struct regexTable *table;

	/* used when action == TACTION_ENTER */
	struct regexTable *continuation_table;
};

typedef struct {
	regex_t *pattern;
	enum pType type;
	bool exclusive;
	bool accept_empty_name;
	union {
		struct {
			int kindIndex;
			roleBitsType roleBits;
			char *name_pattern;
		} tag;
		struct {
			regexCallback function;
			void *userData;
		} callback;
	} u;
	unsigned int scopeActions;
	bool *disabled;

	enum regexParserType regptype;
	struct mGroupSpec mgroup;
	struct guestSpec guest;
	struct mTableActionSpec taction;

	int   xtagType;
	ptrArray *fieldPatterns;

	char *pattern_string;

	char *anonymous_tag_prefix;

	struct {
		errorSelection selection;
		char *message_string;
	} message;

	int refcount;
} regexPattern;


typedef struct {
	/* the pattern can be shared among entries using a refcount */
	regexPattern *pattern;

	/* but the statistics are per-table-entry */
	struct {
		unsigned int match;
		unsigned int unmatch;
	} statistics;
} regexTableEntry;


#define TABLE_INDEX_UNUSED -1
struct regexTable {
	char *name;
	ptrArray *entries;
};

struct boundaryInRequest {
	bool offset_set;
	off_t offset;
};

struct guestRequest {
	bool lang_set;
	langType lang;

	struct boundaryInRequest boundary[2];
};

struct lregexControlBlock {
	int currentScope;
	ptrArray *entries [2];

	ptrArray *tables;
	ptrArray *tstack;

	struct guestRequest *guest_req;

	langType owner;
};

/*
*   DATA DEFINITIONS
*/

/*
*   FUNCTION DEFINITIONS
*/
static int getTableIndexForName (const struct lregexControlBlock *const lcb, const char *name);
static void deletePattern (regexPattern *p);
static int  makePromiseForAreaSpecifiedWithOffsets (const char *parser,
													off_t startOffset,
													off_t endOffset);

static struct guestRequest *guestRequestNew (void);
static void   guestRequestDelete (struct guestRequest *);
static bool   guestRequestIsFilled(struct guestRequest *);
static void   guestRequestClear (struct guestRequest *);
static void   guestRequestSubmit (struct guestRequest *);

static void deleteTable (void *ptrn)
{
	struct regexTable *t = ptrn;

	ptrArrayDelete (t->entries);
	eFree (t->name);
	eFree (t);
}

static void deleteTableEntry (void *ptrn)
{
	regexTableEntry *e = ptrn;
	Assert (e && e->pattern);
	deletePattern (e->pattern);
	eFree (e);
}

static void deletePattern (regexPattern *p)
{
	p->refcount--;

	if (p->refcount > 0)
		return;

	regfree (p->pattern);
	eFree (p->pattern);
	p->pattern = NULL;

	if (p->type == PTRN_TAG)
	{
		eFree (p->u.tag.name_pattern);
		p->u.tag.name_pattern = NULL;
	}

	if (p->fieldPatterns)
	{
		ptrArrayDelete (p->fieldPatterns);
		p->fieldPatterns = NULL;
	}

	eFree (p->pattern_string);

	if (p->message.message_string)
		eFree (p->message.message_string);

	if (p->anonymous_tag_prefix)
		eFree (p->anonymous_tag_prefix);

	eFree (p);
}

static void clearPatternSet (struct lregexControlBlock *lcb)
{
	ptrArrayClear (lcb->entries [REG_PARSER_SINGLE_LINE]);
	ptrArrayClear (lcb->entries [REG_PARSER_MULTI_LINE]);
	ptrArrayClear (lcb->tables);
}

extern struct lregexControlBlock* allocLregexControlBlock (parserDefinition *parser)
{
	struct lregexControlBlock *lcb = xCalloc (1, struct lregexControlBlock);

	lcb->entries[REG_PARSER_SINGLE_LINE] = ptrArrayNew(deleteTableEntry);
	lcb->entries[REG_PARSER_MULTI_LINE] = ptrArrayNew(deleteTableEntry);
	lcb->tables = ptrArrayNew(deleteTable);
	lcb->tstack = ptrArrayNew(NULL);
	lcb->guest_req = guestRequestNew ();
	lcb->owner = parser->id;

	return lcb;
}

extern void freeLregexControlBlock (struct lregexControlBlock* lcb)
{
	clearPatternSet (lcb);

	ptrArrayDelete (lcb->entries [REG_PARSER_SINGLE_LINE]);
	lcb->entries [REG_PARSER_SINGLE_LINE] = NULL;
	ptrArrayDelete (lcb->entries [REG_PARSER_MULTI_LINE]);
	lcb->entries [REG_PARSER_MULTI_LINE] = NULL;

	ptrArrayDelete (lcb->tables);
	lcb->tables = NULL;

	ptrArrayDelete (lcb->tstack);
	lcb->tstack = NULL;

	guestRequestDelete (lcb->guest_req);
	lcb->guest_req = NULL;

	eFree (lcb);
}

/*
*   Regex pseudo-parser
*/

static void initRegexTag (tagEntryInfo *e,
		const vString* const name, int kindIndex, int roleIndex, int scopeIndex, int placeholder,
		unsigned long line, MIOPos *pos, int xtag_type)
{
	Assert (name != NULL  &&  ((vStringLength (name) > 0) || placeholder));
	initRefTagEntry (e, vStringValue (name), kindIndex, roleIndex);
	e->extensionFields.scopeIndex = scopeIndex;
	e->placeholder = !!placeholder;
	if (line)
	{
		e->lineNumber = line;
		e->filePosition = *pos;
	}

	if (xtag_type != XTAG_UNKNOWN)
		markTagExtraBit (e, xtag_type);
}

/*
*   Regex pattern definition
*/

/* Take a string like "/blah/" and turn it into "blah", making sure
 * that the first and last characters are the same, and handling
 * quoted separator characters.  Actually, stops on the occurrence of
 * an unquoted separator.  Also turns "\t" into a Tab character.
 * Turns "\n" into a Newline character if MULTILINE is true.
 * Returns pointer to terminating separator.  Works in place.  Null
 * terminates name string.
 */
static char* scanSeparators (char* name, bool multiline)
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
			else if (multiline && *name == 'n')
				*copyto++ = '\n';
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
		enum regexParserType regptype,
		char* const regexp, char** const name,
		char** const kinds, char** const flags)
{
	bool result = false;
	const int separator = (unsigned char) regexp [0];

	*name = scanSeparators (regexp, (regptype == REG_PARSER_MULTI_LINE
									 || regptype == REG_PARSER_MULTI_TABLE));
	if (*regexp == '\0')
		error (WARNING, "empty regexp");
	else if (**name != separator)
		error (WARNING, "%s: incomplete regexp", regexp);
	else
	{
		char* const third = scanSeparators (*name, false);
		if (**name != '\0' && (*name) [strlen (*name) - 1] == '\\')
			error (WARNING, "error in name pattern: \"%s\"", *name);
		if (*third != separator)
			error (WARNING, "%s: regexp missing final separator", regexp);
		else
		{
			/*
			 * first----------V third------------V
			 * --regex-<LANG>=/regexp/replacement/[kind-spec/][flags]
			 * second----------------^ fourth---------------^
			 */

			char* const fourth = scanSeparators (third, false);
			if (*fourth == separator)
			{
				*kinds = third;
				scanSeparators (fourth, false);
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


static void pre_ptrn_flag_exclusive_short (char c CTAGS_ATTR_UNUSED, void* data)
{
	bool *exclusive = data;
	*exclusive = true;
}

static void pre_ptrn_flag_exclusive_long (const char* const s CTAGS_ATTR_UNUSED, const char* const unused CTAGS_ATTR_UNUSED, void* data)
{
	pre_ptrn_flag_exclusive_short ('x', data);
}

static flagDefinition prePtrnFlagDef[] = {
	{ 'x',  "exclusive", pre_ptrn_flag_exclusive_short, pre_ptrn_flag_exclusive_long ,
	  NULL, "skip testing the other patterns if a line is matched to this pattern"},
};

static void scope_ptrn_flag_eval (const char* const f  CTAGS_ATTR_UNUSED,
				  const char* const v, void* data)
{
	unsigned int *bfields = data;

	if (strcmp (v, "ref") == 0)
		*bfields |= SCOPE_REF;
	else if (strcmp (v, "push") == 0)
		*bfields |= (SCOPE_PUSH | SCOPE_REF);
	else if (strcmp (v, "pop") == 0)
		*bfields |= SCOPE_POP;
	else if (strcmp (v, "clear") == 0)
		*bfields |= SCOPE_CLEAR;
	else if (strcmp (v, "set") == 0)
		*bfields |= (SCOPE_CLEAR | SCOPE_PUSH);
	else
		error (FATAL, "Unexpected value for scope flag in regex definition: scope=%s", v);
}

static void placeholder_ptrn_flag_eval (const char* const f  CTAGS_ATTR_UNUSED,
				     const char* const v  CTAGS_ATTR_UNUSED, void* data)
{
	unsigned int *bfields = data;
	*bfields |= SCOPE_PLACEHOLDER;
}

static flagDefinition scopePtrnFlagDef[] = {
	{ '\0', "scope",     NULL, scope_ptrn_flag_eval,
	  "ACTION", "use scope stack: ACTION = ref|push|pop|clear|set"},
	{ '\0', "placeholder",  NULL, placeholder_ptrn_flag_eval,
	  NULL, "don't put this tag to tags file."},
};

static kindDefinition *kindNew (char letter, const char *name, const char *description)
{
	kindDefinition *kdef = xCalloc (1, kindDefinition);
	kdef->letter        = letter;
	kdef->name = eStrdup (name);
	kdef->description = eStrdup(description? description: kdef->name);
	kdef->enabled = true;
	return kdef;
}

static void kindFree (kindDefinition *kind)
{
	kind->letter = '\0';
	eFree ((void *)kind->name);
	kind->name = NULL;
	eFree ((void *)kind->description);
	kind->description = NULL;
	eFree (kind);
}

static void initMgroup(struct mGroupSpec *mgroup)
{
	mgroup->forLineNumberDetermination = NO_MULTILINE;
	mgroup->forNextScanning = NO_MULTILINE;
	mgroup->nextFromStart = false;
}

static void initGuestSpec (struct guestSpec *guest)
{
	guest->lang.type = GUEST_LANG_UNKNOWN;
}

static void initTaction(struct mTableActionSpec *taction)
{
	taction->action = TACTION_NOP;
	taction->table = NULL;
}

static regexPattern * refPattern (regexPattern * ptrn)
{
	ptrn->refcount++;
	return ptrn;
}

static regexPattern * newPattern (regex_t* const pattern,
								  enum regexParserType regptype)
{
	regexPattern *ptrn = xCalloc(1, regexPattern);

	ptrn->pattern = pattern;
	ptrn->exclusive = false;
	ptrn->accept_empty_name = false;
	ptrn->regptype = regptype;
	ptrn->xtagType = XTAG_UNKNOWN;

	if (regptype == REG_PARSER_MULTI_LINE)
		initMgroup(&ptrn->mgroup);
	if (regptype == REG_PARSER_MULTI_TABLE)
		initTaction(&ptrn->taction);
	initGuestSpec (&ptrn->guest);

	ptrn->u.tag.roleBits = 0;
	ptrn->refcount = 1;
	return ptrn;
}

static regexTableEntry * newRefPatternEntry (regexTableEntry * other)
{
	regexTableEntry *entry = xCalloc (1, regexTableEntry);

	Assert (other && other->pattern);

	entry->pattern = refPattern(other->pattern);
	return entry;
}

static regexTableEntry * newEntry (regex_t* const pattern,
								   enum regexParserType regptype)
{
	regexTableEntry *entry = xCalloc (1, regexTableEntry);
	entry->pattern = newPattern (pattern, regptype);
	return entry;
}

static regexPattern* addCompiledTagCommon (struct lregexControlBlock *lcb,
										   int table_index,
										   regex_t* const pattern,
										   enum regexParserType regptype)
{
	regexTableEntry *entry = newEntry (pattern, regptype);

	if (regptype == REG_PARSER_MULTI_TABLE)
	{
		struct regexTable *table = ptrArrayItem (lcb->tables, table_index);
		Assert(table);

		ptrArrayAdd (table->entries, entry);
	}
	else
		ptrArrayAdd (lcb->entries[regptype], entry);

	useRegexMethod(lcb->owner);

	return entry->pattern;
}

static void pre_ptrn_flag_mgroup_long (const char* const s, const char* const v, void* data)
{
	struct mGroupSpec *mgroup = data;
	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}
	if (!strToInt (v, 10, &mgroup->forLineNumberDetermination))
	{
		error (WARNING, "wrong %s specification: %s", s, v);
		mgroup->forLineNumberDetermination = NO_MULTILINE;
	}
	else if (mgroup->forLineNumberDetermination < 0
			 || mgroup->forLineNumberDetermination >= BACK_REFERENCE_COUNT)
	{
		error (WARNING, "out of range(0 ~ %d) %s specification: %s",
			   (BACK_REFERENCE_COUNT - 1),
			   s, v);
		mgroup->forLineNumberDetermination = NO_MULTILINE;
	}
}

static void pre_ptrn_flag_advanceTo_long (const char* const s, const char* const v, void* data)
{
	struct mGroupSpec *mgroup = data;
	char *vdup;
	char *tmp;


	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}

	vdup = eStrdup (v);

	mgroup->nextFromStart = false;
	if ((tmp = strstr(vdup, "start")))
	{
		mgroup->nextFromStart = true;
		*tmp = '\0';
	}
	else if ((tmp = strstr(vdup, "end")))
		*tmp = '\0';

	if (!strToInt (vdup, 10, &(mgroup->forNextScanning)))
	{
		error (WARNING, "wrong %s specification: %s", s, vdup);
		mgroup->nextFromStart = false;
	}
	else if (mgroup->forNextScanning < 0 || mgroup->forNextScanning >= BACK_REFERENCE_COUNT)
	{
		error (WARNING, "out of range(0 ~ %d) %s specification: %s",
			   (BACK_REFERENCE_COUNT - 1), s, vdup);
		mgroup->nextFromStart = false;
	}

	eFree (vdup);
}

struct guestPtrnFlagData {
	enum regexParserType type;
	struct guestSpec *guest;
};

static void pre_ptrn_flag_guest_long (const char* const s, const char* const v, void* data)
{
	struct guestPtrnFlagData *flagData = data;
	enum regexParserType type = flagData->type;
	struct guestSpec *guest = flagData->guest;
	struct boundarySpec *current;

	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}

	char *tmp = strchr (v, ',');
	if (tmp == NULL)
	{
		error (WARNING, "no terminator found for parser name: %s", s);
		return;
	}

	if ((tmp - v) == 0)
	{
		if (type == REG_PARSER_MULTI_LINE)
		{
			error (WARNING,
				   "using placeholder for guest name field is not allowed in multiline regex spec: %s", v);
			goto err;
		}

		guest->lang.type = GUEST_LANG_PLACEHOLDER;
	}
	else if (*v == '\\' || *v == '*')
	{
		const char *n_tmp = v + 1;
		const char *n = n_tmp;
		for (; isdigit (*n_tmp); n_tmp++);
		char c = *n_tmp;
		*(char *)n_tmp = '\0';
		if (!strToInt (n, 10, &(guest->lang.spec.patternGroup)))
		{
			error (WARNING, "wrong guest name specification: %s", v);
			goto err;
		}
		else if (guest->lang.spec.patternGroup >= BACK_REFERENCE_COUNT)
		{
			error (WARNING, "wrong guest name specification (back reference count is too large): %d",
				   guest->lang.spec.patternGroup);
			goto err;
		}

		*(char *)n_tmp = c;
		if (*n_tmp != ',')
		{
			error (WARNING, "wrong guest specification (garbage at the end of end guest spec): %s", v);
			goto err;
		}

		guest->lang.type = (*v == '\\')
			? GUEST_LANG_PTN_GROUP_FOR_LANGNAME
			: GUEST_LANG_PTN_GROUP_FOR_FILEMAP;
	}
	else
	{
		guest->lang.spec.lang = getNamedLanguage (v, (tmp - v));
		if (guest->lang.spec.lang == LANG_IGNORE)
		{
			error (WARNING, "no parser found for the guest spec: %s", v);
			goto err;
		}
		guest->lang.type = GUEST_LANG_STATIC_LANGNAME;
	}

	tmp++;
	if (*tmp == '\0')
	{
		error (WARNING, "no area spec found in the guest spec: %s", v);
		goto err;
	}

	for (int i = 0; i < 2; i++)
	{
		current = guest->boundary + i;
		const char *current_field_str = (i == BOUNDARY_START? "start": "end");

		if (tmp [0] == ((i == BOUNDARY_START)? ',': '\0'))
		{
			if (type == REG_PARSER_MULTI_LINE)
				error (WARNING,
					   "using placeholder for %s field is not allowed in multiline regex spec: %s",
					   current_field_str, v);

			current->placeholder = true;
		}
		else
		{
			char *n = tmp;

			for (; isdigit (*tmp); tmp++);
			char c = *tmp;
			*tmp = '\0';
			if (!strToInt (n, 10, &(current->patternGroup)))
			{
				error (WARNING, "wrong guest area specification (patternGroup of %s, number expected): %s:%s",
					   current_field_str, v, n);
				goto err;
			}
			*tmp = c;
			if (*tmp == '\0')
			{
				error (WARNING, "wrong guest area specification (patternGroup of %s, nether start nor end given): %s",
					   current_field_str, v);
				goto err;
			}
			else if (strncmp (tmp, "start", 5) == 0)
			{
				current->fromStartOfGroup = true;
				tmp += 5;
			}
			else if (strncmp (tmp, "end", 3) == 0)
			{
				current->fromStartOfGroup = false;
				tmp += 3;
			}
			else
			{
				error (WARNING, "wrong guest area specification (%s): %s",
					   current_field_str, v);
				goto err;
			}
		}

		if (i == 0)
		{
			if (*tmp != ',')
			{
				error (WARNING,
					   "wrong guest area specification (separator between start and end boundaries): %s", v);
				goto err;
			}
			tmp++;
		}
		else if (i == 1 && (*tmp != '\0'))
		{
			error (WARNING, "wrong guest area specification (garbage at the end of end boundary spec): %s", v);
			goto err;
		}
	}
	return;
 err:
	guest->lang.type = GUEST_LANG_UNKNOWN;
}

static flagDefinition multilinePtrnFlagDef[] = {
	{ '\0',  "mgroup", NULL, pre_ptrn_flag_mgroup_long,
	  "N", "a group in pattern determining the line number of tag"},
	{ '\0',  "_advanceTo", NULL, pre_ptrn_flag_advanceTo_long,
	  "N[start|end]", "a group in pattern from where the next scan starts [0end]"},
};

static flagDefinition guestPtrnFlagDef[] = {
#define EXPERIMENTAL "_"
	{ '\0',  EXPERIMENTAL "guest", NULL, pre_ptrn_flag_guest_long,
	  "PARSERSPEC,N0[start|end],N1[start|end]", "run guest parser on the area"},
};

static bool hasMessage(const regexPattern *const ptrn)
{
	return (ptrn->message.selection > 0 && ptrn->message.message_string);
}

struct commonFlagData {
	const langType owner;
	const struct lregexControlBlock *const lcb;
	regexPattern *ptrn;
};

static void common_flag_msg_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData *cdata = data;
	regexPattern *ptrn = cdata->ptrn;

	Assert (ptrn);

	if (hasMessage(ptrn))
	{
		error (WARNING, "only one message flag may be given per regex (already set to '%s')",
			   ptrn->message.message_string);
		return;
	}

	if (strcmp (s, "fatal") == 0)
	{
		ptrn->message.selection = FATAL;
	}
	else if (strcmp (s, "warning") == 0)
	{
		ptrn->message.selection = WARNING;
	}

	Assert (ptrn->message.selection != 0);

	if (!v || !*v)
	{
		error (WARNING, "no message value is given for {%s}", s);
		return;
	}

	const char* begin = v;
	const char* end = v + strlen (v);
	--end;

	if (*begin != '"' || *end != '"' || begin == end)
	{
		error (WARNING, "argument for {%s} must be in double-quotes", s);
		return;
	}

	++begin;

	if (begin < end)
		ptrn->message.message_string = eStrndup (begin, end - begin);
}

static void common_flag_extra_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData * cdata = data;

	Assert (cdata->ptrn);

	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}

	cdata->ptrn->xtagType = getXtagTypeForNameAndLanguage (v, cdata->owner);
	if (cdata->ptrn->xtagType == XTAG_UNKNOWN)
		error (WARNING, "no such extra \"%s\" in %s", v, getLanguageName(cdata->owner));
}


static struct fieldPattern * fieldPatternNew (fieldType ftype, const char *template)
{
	struct fieldPattern *fp;

	fp = xMalloc(1, struct fieldPattern);
	fp->ftype = ftype;
	fp->template = eStrdup(template);

	return fp;
}

static void fieldPatternDelete (struct fieldPattern *fp)
{
	eFree ((void *)fp->template);
	eFree (fp);
}

static void common_flag_field_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData * cdata = data;
	regexPattern *ptrn = cdata->ptrn;

	Assert (ptrn);

	struct fieldPattern *fp;
	fieldType ftype;
	char *fname;
	const char* template;
	char *tmp;

	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}

	tmp = strchr (v, ':');
	if (tmp == NULL || tmp == v)
	{
		error (WARNING, "no field name is given for: %s", s);
		return;
	}

	fname = eStrndup (v, tmp - v);
	ftype = getFieldTypeForNameAndLanguage (fname, cdata->owner);
	if (ftype == FIELD_UNKNOWN)
	{
		error (WARNING, "no such field \"%s\" in %s", fname, getLanguageName(cdata->owner));
		eFree (fname);
		return;
	}

	if (ptrn->fieldPatterns)
	{
		for (unsigned int i = 0; i < ptrArrayCount(ptrn->fieldPatterns); i++)
		{
			fp = ptrArrayItem(ptrn->fieldPatterns, i);
			if (fp->ftype == ftype)
			{
				error (WARNING, "duplicated field specification \"%s\" in %s", fname, getLanguageName(cdata->owner));
				eFree (fname);
				return;
			}
		}
	}
	eFree (fname);

	template = tmp + 1;
	fp = fieldPatternNew (ftype, template);

	if (ptrn->fieldPatterns == NULL)
		ptrn->fieldPatterns = ptrArrayNew((ptrArrayDeleteFunc)fieldPatternDelete);
	ptrArrayAdd(ptrn->fieldPatterns, fp);
}

static void common_flag_role_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData * cdata = data;
	regexPattern *ptrn = cdata->ptrn;
	roleDefinition * role;

	Assert (ptrn);

	if (!v)
	{
		error (WARNING, "no value is given for: %s", s);
		return;
	}

	role = getLanguageRoleForName(cdata->owner,
								  ptrn->u.tag.kindIndex, v);
	if (!role)
	{
		error (WARNING, "no such role: %s", v);
		return;
	}

	ptrn->u.tag.roleBits |= makeRoleBit(role->id);
}

static void common_flag_anonymous_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData * cdata = data;
	regexPattern *ptrn = cdata->ptrn;

	Assert (ptrn);

	if (ptrn->anonymous_tag_prefix)
	{
		error (WARNING, "an anonymous tag prefix for this pattern (%s) is already given: %s",
			   ptrn->pattern_string? ptrn->pattern_string: "",
			   ptrn->anonymous_tag_prefix);
		return;
	}

	if (!v)
	{
		error (WARNING, "no PREFIX for anonymous regex flag is given (pattern == %s)",
			   ptrn->pattern_string? ptrn->pattern_string: "");
		return;
	}

	if (ptrn->u.tag.kindIndex == KIND_GHOST_INDEX)
	{
		error (WARNING, "use \"%s\" regex flag only with an explicitly defined kind", s);
		return;
	}

	ptrn->anonymous_tag_prefix = eStrdup (v);
}

static flagDefinition commonSpecFlagDef[] = {
	{ '\0',  "fatal", NULL, common_flag_msg_long ,
	  "\"MESSAGE\"", "print the given MESSAGE and exit"},
	{ '\0',  "warning", NULL, common_flag_msg_long ,
	  "\"MESSAGE\"", "print the given MESSAGE at WARNING level"},
#define EXPERIMENTAL "_"
	{ '\0',  EXPERIMENTAL "extra", NULL, common_flag_extra_long ,
	  "EXTRA", "record the tag only when the extra is enabled"},
	{ '\0',  EXPERIMENTAL "field", NULL, common_flag_field_long ,
	  "FIELD:VALUE", "record the matched string(VALUE) to parser own FIELD of the tag"},
	{ '\0',  EXPERIMENTAL "role", NULL, common_flag_role_long,
	  "ROLE", "set the given ROLE to the roles field"},
	{ '\0',  EXPERIMENTAL "anonymous", NULL, common_flag_anonymous_long,
	  "PREFIX", "make an anonymous tag with PREFIX"},
};


static void pre_ptrn_flag_mtable_long (const char* const s, const char* const v, void* data)
{
	struct commonFlagData * cdata = data;
	regexPattern *ptrn = cdata->ptrn;
	struct mTableActionSpec *taction;
	bool taking_table = true;

	Assert (ptrn);
	Assert (cdata->lcb);

	taction = &ptrn->taction;

	if (strcmp (s, "tenter") == 0)
		taction->action = TACTION_ENTER;
	else if (strcmp (s, "tleave") == 0)
	{
		taction->action = TACTION_LEAVE;
		taking_table = false;
	}
	else if (strcmp (s, "tjump") == 0)
		taction->action = TACTION_JUMP;
	else if (strcmp (s, "treset") == 0)
		taction->action = TACTION_RESET;
	else if (strcmp (s, "tquit") == 0)
	{
		taction->action = TACTION_QUIT;
		taking_table = false;
	}

	if (taking_table)
	{
		int t;
		char *continuation = NULL;


		if (!v || (!*v))
			error (FATAL, "no table is given for table action: %s", s);

		if (taction->action == TACTION_ENTER
			&& (continuation = strchr (v, ',')))
		{
			char *tableEnterTo;

			tableEnterTo = eStrndup (v, continuation - v);
			t = getTableIndexForName (cdata->lcb, tableEnterTo);
			if (t < 0)
				error (FATAL, "table is not defined: %s", tableEnterTo);
			taction->table = ptrArrayItem (cdata->lcb->tables, t);
			eFree (tableEnterTo);

			if (!*(continuation + 1))
				error (FATAL, "no continuation table is given for: %s", v);

			int t_cont = getTableIndexForName (cdata->lcb, continuation + 1);
			if (t_cont < 0)
				error (FATAL, "table for continuation is not defined: %s", continuation + 1);
			taction->continuation_table = ptrArrayItem (cdata->lcb->tables, t_cont);
		}
		else
		{
			t = getTableIndexForName (cdata->lcb, v);
			if (t < 0)
				error (FATAL, "table is not defined: %s", v);
			taction->table = ptrArrayItem (cdata->lcb->tables, t);
			taction->continuation_table = NULL;
		}
	}
}

static flagDefinition multitablePtrnFlagDef[] = {
	{ '\0',  "tenter", NULL, pre_ptrn_flag_mtable_long ,
	  "TABLE[,CONT]", "enter to given regext table (with specifying continuation)"},
	{ '\0',  "tleave", NULL, pre_ptrn_flag_mtable_long ,
	  NULL, "leave from the current regext table"},
	{ '\0',  "tjump", NULL, pre_ptrn_flag_mtable_long ,
	  "TABLE", "jump to another regext table(don't push the current table to state stack)"},
	{ '\0',  "treset", NULL, pre_ptrn_flag_mtable_long ,
	  "TABLE", "clear the state stack and jump to given regex table"},
	{ '\0',  "tquit", NULL, pre_ptrn_flag_mtable_long ,
	  NULL, "stop the parsing with this parser"},
};


static void setKind(regexPattern * ptrn, const langType owner,
					const char kindLetter, const char* kindName,
					const char *const description,
					bool kind_explicitly_defined)
{
	Assert (ptrn);
	Assert (ptrn->u.tag.name_pattern);
	Assert (kindName);
	kindDefinition *kdef = getLanguageKindForLetter (owner, kindLetter);

	if (kdef)
	{
		if (strcmp (kdef->name, kindName) && (strcmp(kindName, KIND_REGEX_DEFAULT_NAME)))
			/* When using a same kind letter for multiple regex patterns, the name of kind
			   should be the same. */
			error  (WARNING, "Don't reuse the kind letter `%c' in a language %s (old: \"%s\", new: \"%s\")",
					kdef->letter, getLanguageName (owner),
					kdef->name, kindName);
		ptrn->u.tag.kindIndex = kdef->id;
	}
	else if (*ptrn->u.tag.name_pattern == '\0' &&
			 kindLetter == KIND_REGEX_DEFAULT_LETTER &&
			 (strcmp(kindName, KIND_REGEX_DEFAULT_NAME) == 0) &&
			 (!kind_explicitly_defined))
		ptrn->u.tag.kindIndex = KIND_GHOST_INDEX;
	else
	{
		kdef = kindNew (kindLetter, kindName, description);
		defineLanguageKind (owner, kdef, kindFree);
		ptrn->u.tag.kindIndex = kdef->id;
	}
}

static void patternEvalFlags (struct lregexControlBlock *lcb,
							  regexPattern * ptrn,
							  enum regexParserType regptype,
							  const char* flags)
{
	struct commonFlagData commonFlagData = {
		.owner = lcb->owner,
		.lcb = lcb,
		.ptrn = ptrn
	};

	if (regptype == REG_PARSER_SINGLE_LINE)
		flagsEval (flags, prePtrnFlagDef, ARRAY_SIZE(prePtrnFlagDef), &ptrn->exclusive);

	flagsEval (flags, commonSpecFlagDef, ARRAY_SIZE(commonSpecFlagDef), &commonFlagData);

	if (regptype == REG_PARSER_SINGLE_LINE || regptype == REG_PARSER_MULTI_TABLE)
		flagsEval (flags, scopePtrnFlagDef, ARRAY_SIZE(scopePtrnFlagDef), &ptrn->scopeActions);

	if (regptype == REG_PARSER_MULTI_LINE || regptype == REG_PARSER_MULTI_TABLE)
	{
		ptrn->mgroup.forNextScanning = 0;
		/* ptrn->mgroup.nextFromStart is initialized in initMgroup() already. */
		flagsEval (flags, multilinePtrnFlagDef, ARRAY_SIZE(multilinePtrnFlagDef), &ptrn->mgroup);
	}

	struct guestPtrnFlagData guestPtrnFlagData = {
		.type = regptype,
		.guest = &ptrn->guest,
	};
	flagsEval (flags, guestPtrnFlagDef, ARRAY_SIZE(guestPtrnFlagDef), &guestPtrnFlagData);

	if (regptype == REG_PARSER_MULTI_TABLE)
		flagsEval (flags, multitablePtrnFlagDef, ARRAY_SIZE(multitablePtrnFlagDef), &commonFlagData);
}

static regexPattern *addCompiledTagPattern (struct lregexControlBlock *lcb,
											int table_index,
											enum regexParserType regptype, regex_t* const pattern,
					    const char* const name, char kindLetter, const char* kindName,
					    char *const description, const char* flags,
					    bool kind_explicitly_defined,
					    bool *disabled)
{
	regexPattern * ptrn = addCompiledTagCommon(lcb, table_index, pattern, regptype);

	ptrn->type = PTRN_TAG;
	ptrn->u.tag.name_pattern = eStrdup (name);
	ptrn->disabled = disabled;

	setKind(ptrn, lcb->owner, kindLetter, kindName, description, kind_explicitly_defined);
	patternEvalFlags (lcb, ptrn, regptype, flags);

	return ptrn;
}

static regexPattern *addCompiledCallbackPattern (struct lregexControlBlock *lcb, regex_t* const pattern,
					const regexCallback callback, const char* flags,
					bool *disabled,
					void *userData)
{
	regexPattern * ptrn;
	bool exclusive = false;
	flagsEval (flags, prePtrnFlagDef, ARRAY_SIZE(prePtrnFlagDef), &exclusive);
	ptrn = addCompiledTagCommon(lcb, TABLE_INDEX_UNUSED, pattern, REG_PARSER_SINGLE_LINE);
	ptrn->type    = PTRN_CALLBACK;
	ptrn->u.callback.function = callback;
	ptrn->u.callback.userData = userData;
	ptrn->exclusive = exclusive;
	ptrn->disabled = disabled;
	return ptrn;
}


static void regex_flag_basic_short (char c CTAGS_ATTR_UNUSED, void* data)
{
	int* cflags = data;
	*cflags &= ~REG_EXTENDED;
}

static void regex_flag_basic_long (const char* const s CTAGS_ATTR_UNUSED, const char* const unused CTAGS_ATTR_UNUSED, void* data)
{
	regex_flag_basic_short ('b', data);
}

static void regex_flag_extend_short (char c CTAGS_ATTR_UNUSED, void* data)
{
	int* cflags = data;
	*cflags |= REG_EXTENDED;
}

static void regex_flag_extend_long (const char* const c CTAGS_ATTR_UNUSED, const char* const unused CTAGS_ATTR_UNUSED, void* data)
{
	regex_flag_extend_short('e', data);
}

static void regex_flag_icase_short (char c CTAGS_ATTR_UNUSED, void* data)
{
	int* cflags = data;
	*cflags |= REG_ICASE;
}

static void regex_flag_icase_long (const char* s CTAGS_ATTR_UNUSED, const char* const unused CTAGS_ATTR_UNUSED, void* data)
{
	regex_flag_icase_short ('i', data);
}


static flagDefinition regexFlagDefs[] = {
	{ 'b', "basic",  regex_flag_basic_short,  regex_flag_basic_long,
	  NULL, "interpreted as a Posix basic regular expression."},
	{ 'e', "extend", regex_flag_extend_short, regex_flag_extend_long,
	  NULL, "interpreted as a Posix extended regular expression (default)"},
	{ 'i', "icase",  regex_flag_icase_short,  regex_flag_icase_long,
	  NULL, "applied in a case-insensitive manner"},
};

static regex_t* compileRegex (enum regexParserType regptype,
							  const char* const regexp, const char* const flags)
{
	int cflags = REG_EXTENDED | REG_NEWLINE;

	if (regptype == REG_PARSER_MULTI_TABLE)
		cflags &= ~REG_NEWLINE;

	regex_t *result;
	int errcode;

	flagsEval (flags,
		   regexFlagDefs,
		   ARRAY_SIZE(regexFlagDefs),
		   &cflags);

	result = xMalloc (1, regex_t);
	errcode = regcomp (result, regexp, cflags);
	if (errcode != 0)
	{
		char errmsg[256];
		regerror (errcode, result, errmsg, 256);
		error (WARNING, "regcomp %s: %s", regexp, errmsg);
		regfree (result);
		eFree (result);
		result = NULL;
	}
	return result;
}


/* If a letter and/or a name are defined in kindSpec, return true. */
static bool parseKinds (
		const char* const kindSpec, char* const kindLetter, char** const kindName,
		char **description)
{
	*description = NULL;

	if (kindSpec == NULL  ||  kindSpec [0] == '\0')
	{
		*kindLetter = KIND_REGEX_DEFAULT_LETTER;
		*kindName = eStrdup (KIND_REGEX_DEFAULT_NAME);
		return false;
	}
	else
	{
		bool explicitly_defined = false;
		const char* k = kindSpec;

		if (k [0] != ','  &&  (k [1] == ','  ||  k [1] == '\0'))
		{
			*kindLetter = *k++;
			explicitly_defined = true;
		}
		else
			*kindLetter = KIND_REGEX_DEFAULT_LETTER;

		if (*k == ',')
			++k;

		if (k [0] == '\0')
			*kindName = eStrdup (KIND_REGEX_DEFAULT_NAME);
		else
		{
			const char *const comma = strchr (k, ',');

			if (comma == NULL)
			{
				if (strlen (k) == 0)
					*kindName = eStrdup (KIND_REGEX_DEFAULT_NAME);
				else
				{
					*kindName = eStrdup (k);
					explicitly_defined = true;
				}
			}
			else
			{
				if (comma - k == 0)
					*kindName = eStrdup (KIND_REGEX_DEFAULT_NAME);
				else
				{
					*kindName = eStrndup (k, comma - k );
					explicitly_defined = true;
				}
				k = comma + 1;
				if (k [0] != '\0')
					*description = eStrdup (k);
			}
		}
		return explicitly_defined;
	}
}

/*
*   Regex pattern matching
*/


static vString* substitute (
		const char* const in, const char* out,
		const int nmatch, const regmatch_t* const pmatch)
{
	vString* result = vStringNew ();
	const char* p;
	for (p = out  ;  *p != '\0'  ;  p++)
	{
		if (*p == '\\'  &&  isdigit ((int) *++p))
		{
			const int dig = *p - '0';
			if (0 < dig  &&  dig < nmatch  &&  pmatch [dig].rm_so != -1)
			{
				const int diglen = pmatch [dig].rm_eo - pmatch [dig].rm_so;
				vStringNCatS (result, in + pmatch [dig].rm_so, diglen);
			}
		}
		else if (*p != '\n'  &&  *p != '\r')
			vStringPut (result, *p);
	}
	return result;
}

static unsigned long getInputLineNumberInRegPType (enum regexParserType regptype,
												   off_t offset)
{
	return (regptype == REG_PARSER_MULTI_LINE || regptype == REG_PARSER_MULTI_TABLE)
		? getInputLineNumberForFileOffset (offset)
		: getInputLineNumber ();
}

static void fillEndLineFieldOfUpperScopes (struct lregexControlBlock *lcb, unsigned long endline)
{
	tagEntryInfo *entry;
	int n = lcb->currentScope;

	while ((entry = getEntryInCorkQueue (n))
		   && (entry->extensionFields.endLine == 0))
	{
		entry->extensionFields.endLine = endline;
		n = entry->extensionFields.scopeIndex;
	}
}

static void matchTagPattern (struct lregexControlBlock *lcb,
		const char* line,
		const regexPattern* const patbuf,
		const regmatch_t* const pmatch,
			     off_t offset)
{
	vString *const name =
		(patbuf->u.tag.name_pattern[0] != '\0') ? substitute (line,
															  patbuf->u.tag.name_pattern,
															  BACK_REFERENCE_COUNT, pmatch):
		(patbuf->anonymous_tag_prefix) ? anonGenerateNew (patbuf->anonymous_tag_prefix,
														  patbuf->u.tag.kindIndex):
		vStringNewInit ("");
	bool placeholder = !!((patbuf->scopeActions & SCOPE_PLACEHOLDER) == SCOPE_PLACEHOLDER);
	int scope = CORK_NIL;
	int n;

	vStringStripLeading (name);
	vStringStripTrailing (name);

	if (patbuf->scopeActions & SCOPE_REF)
	{
		tagEntryInfo *entry;

		scope = lcb->currentScope;
		while ((entry = getEntryInCorkQueue (scope)) && entry->placeholder)
			/* Look at parent */
			scope = entry->extensionFields.scopeIndex;
	}
	if (patbuf->scopeActions & SCOPE_CLEAR)
	{
		unsigned long endline = getInputLineNumberInRegPType(patbuf->regptype, offset);
		fillEndLineFieldOfUpperScopes (lcb, endline);
		lcb->currentScope = CORK_NIL;
	}
	if (patbuf->scopeActions & SCOPE_POP)
	{
		tagEntryInfo *entry = getEntryInCorkQueue (lcb->currentScope);

		if (entry && (entry->extensionFields.endLine == 0))
			entry->extensionFields.endLine = getInputLineNumberInRegPType(patbuf->regptype, offset);

		lcb->currentScope = entry? entry->extensionFields.scopeIndex: CORK_NIL;
	}

	if (vStringLength (name) == 0 && (placeholder == false))
	{
		if (patbuf->accept_empty_name == false)
			error (WARNING, "%s:%lu: null expansion of name pattern \"%s\"",
			       getInputFileName (),
				   getInputLineNumberInRegPType(patbuf->regptype, offset),
			       patbuf->u.tag.name_pattern);
		n = CORK_NIL;
	}
	else
	{
		static TrashBox* field_trashbox;
		unsigned long ln = 0;
		MIOPos pos;
		tagEntryInfo e;
		int kind;
		roleBitsType roleBits;

		if ((patbuf->regptype == REG_PARSER_MULTI_LINE)
			|| (patbuf->regptype == REG_PARSER_MULTI_TABLE))
		{
			ln = getInputLineNumberForFileOffset (offset);
			pos = getInputFilePositionForLine (ln);
		}

		n = CORK_NIL;
		kind = patbuf->u.tag.kindIndex;
		roleBits = patbuf->u.tag.roleBits;

		initRegexTag (&e, name, kind, ROLE_DEFINITION_INDEX, scope, placeholder,
					  ln, ln == 0? NULL: &pos, patbuf->xtagType);

		if (field_trashbox == NULL)
		{
			field_trashbox = trashBoxNew();
			DEFAULT_TRASH_BOX (field_trashbox, trashBoxDelete);
		}

		if (patbuf->fieldPatterns)
		{
			for (unsigned int i = 0; i < ptrArrayCount(patbuf->fieldPatterns); i++)
			{
				struct fieldPattern *fp = ptrArrayItem(patbuf->fieldPatterns, i);
				if (isFieldEnabled (fp->ftype))
				{
					vString * const value = substitute (line, fp->template,
														BACK_REFERENCE_COUNT, pmatch);
					attachParserField (&e, false, fp->ftype, vStringValue (value));
					trashBoxPut (field_trashbox, value,
								 (TrashBoxDestroyItemProc)vStringDelete);
				}
			}
		}

		if (roleBits)
		{
			unsigned int roleIndex;

			for (roleIndex = 0;
				 roleIndex < countLanguageRoles(e.langType, kind);
				 roleIndex++)
			{
				if (roleBits & makeRoleBit(roleIndex))
					assignRole (&e, roleIndex);
			}
		}

		if (patbuf->anonymous_tag_prefix)
			markTagExtraBit (&e, XTAG_ANONYMOUS);

		n = makeTagEntry (&e);

		trashBoxMakeEmpty(field_trashbox);
	}

	if (patbuf->scopeActions & SCOPE_PUSH)
		lcb->currentScope = n;

	vStringDelete (name);
}

static bool matchCallbackPattern (
		const vString* const line, const regexPattern* const patbuf,
		const regmatch_t* const pmatch)
{
	regexMatch matches [BACK_REFERENCE_COUNT];
	unsigned int count = 0;
	int i;
	for (i = 0  ;  i < BACK_REFERENCE_COUNT  ;  ++i)
	{
		matches [i].start  = pmatch [i].rm_so;
		matches [i].length = pmatch [i].rm_eo - pmatch [i].rm_so;
		/* a valid match may have both offsets == -1,
		 * e.g. (foo)*(bar) matching "bar" - see CTags bug 271.
		 * As POSIX regex doesn't seem to have a way to count matches,
		 * we return the count up to the last non-empty match. */
		if (pmatch [i].rm_so != -1)
			count = i + 1;
	}
	return patbuf->u.callback.function (vStringValue (line), matches, count,
				     patbuf->u.callback.userData);
}


static void printMessage(const langType language,
						 const regexPattern *const ptrn,
						 const off_t offset,
						 const char *const line,
						 const regmatch_t* const pmatch)
{
	vString *msg;

	Assert (ptrn);
	Assert (ptrn->message.selection > 0);
	Assert (ptrn->message.message_string);

	msg = substitute (line, ptrn->message.message_string, BACK_REFERENCE_COUNT, pmatch);

	error (ptrn->message.selection, "%sMessage from regex<%s>: %s (%s:%lu)",
		   (ptrn->message.selection == FATAL ? "Fatal: " : ""),
		   getLanguageName (language),
		   vStringValue (msg),
		   getInputFileName (),
		   getInputLineNumberInRegPType (ptrn->regptype, offset));

	vStringDelete (msg);
}

static bool isGuestRequestConsistent (struct guestRequest *guest_req)
{
	return (guest_req->lang != LANG_IGNORE)
		&& (guest_req->boundary[BOUNDARY_START].offset < guest_req->boundary[BOUNDARY_END].offset);
}

static bool fillGuestRequest (const char *start,
							  const char *current,
							  regmatch_t pmatch [BACK_REFERENCE_COUNT],
							  struct guestSpec *guest_spec,
							  struct guestRequest *guest_req)
{
	if (guest_spec->lang.type == GUEST_LANG_UNKNOWN)
		return false;
	else if (guest_spec->lang.type == GUEST_LANG_PLACEHOLDER)
		;
	else if (guest_spec->lang.type == GUEST_LANG_STATIC_LANGNAME)
	{
		guest_req->lang = guest_spec->lang.spec.lang;
		guest_req->lang_set = true;
	}
	else if (guest_spec->lang.type == GUEST_LANG_PTN_GROUP_FOR_LANGNAME)
	{
		const char * name = current + pmatch [guest_spec->lang.spec.patternGroup].rm_so;
		int size = pmatch [guest_spec->lang.spec.patternGroup].rm_eo
			- pmatch [guest_spec->lang.spec.patternGroup].rm_so;
		if (size > 0)
		{
			guest_req->lang = getNamedLanguage (name, size);
			guest_req->lang_set = true;
		}
	}
	else if (guest_spec->lang.type == GUEST_LANG_PTN_GROUP_FOR_FILEMAP)
	{
		const char * name = current + pmatch [guest_spec->lang.spec.patternGroup].rm_so;
		int size = pmatch [guest_spec->lang.spec.patternGroup].rm_eo
			- pmatch [guest_spec->lang.spec.patternGroup].rm_so;
		char *fname = (size > 0)? eStrndup (name, size): NULL;

		if (fname)
		{
			guest_req->lang = getLanguageForFilename (fname, LANG_AUTO);
			guest_req->lang_set = true;
			eFree (fname);
		}
	}

	for (int i = 0; i < 2; i++)
	{
		struct boundarySpec *boundary_spec = guest_spec->boundary + i;
		struct boundaryInRequest *boundary = guest_req->boundary + i;
		if (!boundary_spec->placeholder)
		{
			boundary->offset =  current - start + (boundary_spec->fromStartOfGroup
												   ? pmatch [boundary_spec->patternGroup].rm_so
												   : pmatch [boundary_spec->patternGroup].rm_eo);
			boundary->offset_set = true;
		}
	}
	return guestRequestIsFilled (guest_req);
}

static bool matchRegexPattern (struct lregexControlBlock *lcb,
							   const vString* const line,
							   regexTableEntry *entry)
{
	bool result = false;
	regmatch_t pmatch [BACK_REFERENCE_COUNT];
	int match;
	regexPattern* patbuf = entry->pattern;
	struct guestSpec  *guest = &patbuf->guest;

	if (patbuf->disabled && *(patbuf->disabled))
		return false;

	match = regexec (patbuf->pattern, vStringValue (line),
			 BACK_REFERENCE_COUNT, pmatch, 0);
	if (match == 0)
	{
		result = true;
		entry->statistics.match++;

		if (hasMessage(patbuf))
			printMessage(lcb->owner, patbuf, 0, vStringValue (line), pmatch);

		if (patbuf->type == PTRN_TAG)
		{
			matchTagPattern (lcb, vStringValue (line), patbuf, pmatch, 0);

			if (guest->lang.type != GUEST_LANG_UNKNOWN)
			{
				unsigned long ln = getInputLineNumber ();
				long current = getInputFileOffsetForLine (ln);
				if (fillGuestRequest (vStringValue (line) - current,
									  vStringValue (line), pmatch, guest, lcb->guest_req))
				{
					Assert (lcb->guest_req->lang != LANG_AUTO);
					if (isGuestRequestConsistent(lcb->guest_req))
						guestRequestSubmit (lcb->guest_req);
					guestRequestClear (lcb->guest_req);
				}
			}
		}
		else if (patbuf->type == PTRN_CALLBACK)
			result = matchCallbackPattern (line, patbuf, pmatch);
		else
		{
			Assert ("invalid pattern type" == NULL);
			result = false;
		}
	}
	else
		entry->statistics.unmatch++;
	return result;
}

static bool matchMultilineRegexPattern (struct lregexControlBlock *lcb,
										const vString* const allLines,
										regexTableEntry *entry)
{
	const char *start;
	const char *current;
	off_t offset = 0;
	regexPattern* patbuf = entry->pattern;
	struct mGroupSpec *mgroup = &patbuf->mgroup;
	struct guestSpec  *guest = &patbuf->guest;

	bool result = false;
	regmatch_t pmatch [BACK_REFERENCE_COUNT];
	int match = 0;
	unsigned int delta = 1;

	Assert (patbuf);

	if (patbuf->disabled && *(patbuf->disabled))
		return false;

	current = start = vStringValue (allLines);
	do
	{
		match = regexec (patbuf->pattern, current,
						 BACK_REFERENCE_COUNT, pmatch, 0);
		if (match != 0)
		{
			entry->statistics.unmatch++;
			break;
		}

		if (hasMessage(patbuf))
			printMessage(lcb->owner, patbuf, (current + pmatch[0].rm_so) - start, current, pmatch);

		offset = (current + pmatch [mgroup->forLineNumberDetermination].rm_so)
				 - start;

		entry->statistics.match++;
		if (patbuf->type == PTRN_TAG)
		{
			matchTagPattern (lcb, current, patbuf, pmatch, offset);
			result = true;
		}
		else if (patbuf->type == PTRN_CALLBACK)
			;	/* Not implemented yet */
		else
		{
			Assert ("invalid pattern type" == NULL);
			result = false;
			break;
		}

		if (fillGuestRequest (start, current, pmatch, guest, lcb->guest_req))
		{
			Assert (lcb->guest_req->lang != LANG_AUTO);
			if (isGuestRequestConsistent(lcb->guest_req))
				guestRequestSubmit (lcb->guest_req);
			guestRequestClear (lcb->guest_req);
		}

		delta = (mgroup->nextFromStart
				 ? pmatch [mgroup->forNextScanning].rm_so
				 : pmatch [mgroup->forNextScanning].rm_eo);
		if (delta == 0)
		{
			unsigned int pos = current - start;
			error (WARNING,
				   "a multi line regex pattern doesn't advance the input cursor: %s",
				   patbuf->pattern_string);
			error (WARNING, "Language: %s, input file: %s, pos: %u",
				   getLanguageName (lcb->owner), getInputFileName(), pos);
			break;
		}
		current += delta;

	} while (current < start + vStringLength (allLines));

	return result;
}

/* PUBLIC INTERFACE */

/* Match against all patterns for specified language. Returns true if at least
 * on pattern matched.
 */
extern bool matchRegex (struct lregexControlBlock *lcb, const vString* const line)
{
	bool result = false;
	unsigned int i;
	for (i = 0  ;  i < ptrArrayCount(lcb->entries[REG_PARSER_SINGLE_LINE])  ;  ++i)
	{
		regexTableEntry *entry = ptrArrayItem(lcb->entries[REG_PARSER_SINGLE_LINE], i);
		regexPattern *ptrn = entry->pattern;

		Assert (ptrn);

		if ((ptrn->xtagType != XTAG_UNKNOWN)
			&& (!isXtagEnabled (ptrn->xtagType)))
				continue;

		if (matchRegexPattern (lcb, line, entry))
		{
			result = true;
			if (ptrn->exclusive)
				break;
		}
	}
	return result;
}

extern void notifyRegexInputStart (struct lregexControlBlock *lcb)
{
	lcb->currentScope = CORK_NIL;

	ptrArrayClear (lcb->tstack);
	guestRequestClear (lcb->guest_req);
}

extern void notifyRegexInputEnd (struct lregexControlBlock *lcb)
{
	unsigned long endline = getInputLineNumber ();
	fillEndLineFieldOfUpperScopes (lcb, endline);
}

extern void findRegexTagsMainloop (int (* driver)(void))
{
	/* merely read all lines of the file */
	while (driver () != EOF)
		;
}

static int fileReadLineDriver(void)
{
	return (readLineFromInputFile () == NULL)? EOF: 1;
}

extern void findRegexTags (void)
{
	findRegexTagsMainloop (fileReadLineDriver);
}

static bool doesExpectCorkInRegex0(ptrArray *entries)
{
	for (unsigned int i = 0; i < ptrArrayCount(entries); i++)
	{
		regexTableEntry *entry = ptrArrayItem(entries, i);
		Assert (entry && entry->pattern);
		if (entry->pattern->scopeActions)
			return true;
	}
	return false;
}

extern bool doesExpectCorkInRegex (struct lregexControlBlock *lcb)
{
	ptrArray *entries;

	entries = lcb->entries[REG_PARSER_SINGLE_LINE];
	if (doesExpectCorkInRegex0 (entries))
		return true;

	entries = lcb->entries[REG_PARSER_MULTI_LINE];
	if (doesExpectCorkInRegex0 (entries))
		return true;

	for (unsigned int i = 0; i < ptrArrayCount(lcb->tables); i++)
	{
		struct regexTable *table = ptrArrayItem(lcb->tables, i);
		if (doesExpectCorkInRegex0 (table->entries))
			return true;
	}

	return false;
}

static char *escapeRegexPattern (const char* pattern)
{
	vString *p = vStringNew ();

	while (*pattern != '\0')
	{
		char c = *pattern;
		if (c == '\n')
			vStringCatS(p, "\\n");
		else if (c == '\t')
			vStringCatS(p, "\\t");
		else if (c == '\\')
			vStringCatS(p, "\\\\");
		else
			vStringPut(p, c);

		pattern++;
	}

	return vStringDeleteUnwrap (p);
}

static regexPattern *addTagRegexInternal (struct lregexControlBlock *lcb,
										  int table_index,
					  enum regexParserType regptype,
					  const char* const regex,
					  const char* const name,
					  const char* const kinds,
					  const char* const flags,
					  bool *disabled)
{
	Assert (regex != NULL);
	Assert (name != NULL);

	if (!regexAvailable)
		return NULL;

	regex_t* const cp = compileRegex (regptype, regex, flags);
	if (cp == NULL)
		return NULL;

	char kindLetter;
	char* kindName;
	char* description;
	kindDefinition* fileKind;

	bool explictly_defined =  parseKinds (kinds, &kindLetter, &kindName, &description);
	fileKind = getLanguageKind (lcb->owner, KIND_FILE_INDEX);
	if (kindLetter == fileKind->letter)
		error (FATAL,
			   "Kind letter \'%c\' used in regex definition \"%s\" of %s language is reserved in ctags main",
			   kindLetter,
			   regex,
			   getLanguageName (lcb->owner));
	else if (!isalpha ((unsigned char)kindLetter))
		error (FATAL,
			   "Kind letter must be an alphabetical character: \"%c\"",
			   kindLetter);

	if (strcmp (kindName, fileKind->name) == 0)
		error (FATAL,
			   "Kind name \"%s\" used in regex definition \"%s\" of %s language is reserved in ctags main",
			   kindName,
			   regex,
			   getLanguageName (lcb->owner));

	const char *option_bsae = (regptype == REG_PARSER_SINGLE_LINE? "regex"        :
							   regptype == REG_PARSER_MULTI_LINE ? "mline-regex"  :
							   regptype == REG_PARSER_MULTI_TABLE? "_mtable-regex":
							   NULL);
	Assert (option_bsae);

	for (const char * p = kindName; *p; p++)
	{
		if (p == kindName)
		{
			if (!isalpha(*p))
				error (FATAL,
					   "A kind name doesn't start with an alphabetical character: "
					   "'%s' in \"--%s-%s\" option",
					   kindName,
					   option_bsae,
					   getLanguageName (lcb->owner));
		}
		else
		{
			/*
			 * People may object to this error.
			 * Searching github repositories, I found not a few .ctags files
			 * in which Exuberant-ctags users define kind names with whitespaces.
			 * "FATAL" error breaks the compatibility.
			 */
			if (!isalnum(*p))
				error (/* regptype == REG_PARSER_SINGLE_LINE? WARNING: */ FATAL,
					   "Non-alphanumeric char is used in kind name: "
					   "'%s' in \"--%s-%s\" option",
					   kindName,
					   option_bsae,
					   getLanguageName (lcb->owner));

		}
	}

	regexPattern *rptr = addCompiledTagPattern (lcb, table_index,
												regptype, cp, name,
												kindLetter, kindName, description, flags,
												explictly_defined,
												disabled);
	rptr->pattern_string = escapeRegexPattern(regex);

	eFree (kindName);
	if (description)
		eFree (description);

	if (*name == '\0')
	{
		if (rptr->exclusive || rptr->scopeActions & SCOPE_PLACEHOLDER
			|| rptr->anonymous_tag_prefix
			|| regptype == REG_PARSER_MULTI_TABLE
			|| rptr->guest.lang.type != GUEST_LANG_UNKNOWN
			)
			rptr->accept_empty_name = true;
		else
			error (WARNING, "%s: regexp missing name pattern", regex);
	}

	return rptr;
}

extern void addTagRegex (struct lregexControlBlock *lcb,
			 const char* const regex,
			 const char* const name,
			 const char* const kinds,
			 const char* const flags,
			 bool *disabled)
{
	addTagRegexInternal (lcb, TABLE_INDEX_UNUSED,
						 REG_PARSER_SINGLE_LINE, regex, name, kinds, flags, disabled);
}

extern void addTagMultiLineRegex (struct lregexControlBlock *lcb, const char* const regex,
								  const char* const name, const char* const kinds, const char* const flags,
								  bool *disabled)
{
	addTagRegexInternal (lcb, TABLE_INDEX_UNUSED,
						 REG_PARSER_MULTI_LINE, regex, name, kinds, flags, disabled);
}

extern void addTagMultiTableRegex(struct lregexControlBlock *lcb,
								  const char* const table_name,
								  const char* const regex,
								  const char* const name, const char* const kinds, const char* const flags,
								  bool *disabled)
{
	int table_index = getTableIndexForName (lcb, table_name);

	if (table_index < 0)
		error (FATAL, "unknown table name: %s", table_name);

	addTagRegexInternal (lcb, table_index, REG_PARSER_MULTI_TABLE, regex, name, kinds, flags,
						 disabled);
}

extern void addCallbackRegex (struct lregexControlBlock *lcb,
			      const char* const regex,
			      const char* const flags,
			      const regexCallback callback,
			      bool *disabled,
			      void * userData)
{
	Assert (regex != NULL);

	if (!regexAvailable)
		return;


	regex_t* const cp = compileRegex (REG_PARSER_SINGLE_LINE, regex, flags);
	if (cp != NULL)
	{
		regexPattern *rptr = addCompiledCallbackPattern (lcb, cp, callback, flags,
														 disabled, userData);
		rptr->pattern_string = escapeRegexPattern(regex);
	}
}

static void addTagRegexOption (struct lregexControlBlock *lcb,
							   enum regexParserType regptype,
							   const char* const pattern)
{
	if (!regexAvailable)
		return;

	int table_index = TABLE_INDEX_UNUSED;
	char * regex_pat = NULL;
	char *name, *kinds, *flags;


	if (regptype == REG_PARSER_MULTI_TABLE)
	{
		const char *c;
		for (c = pattern; *c; c++)
		{
			if (! (isalnum(*c) || *c == '_'))
			{
				if (*c &&  (*(c + 1) != '^'))
				{
					vString *tmp = vStringNew ();

					/* Put '^' as prefix for the pattern */
					vStringPut(tmp, *c);
					vStringPut(tmp, '^');
					vStringCatS(tmp, c + 1);
					regex_pat = vStringDeleteUnwrap(tmp);
				}
				else
					regex_pat = eStrdup (c);
				break;
			}
		}

		if (regex_pat == NULL || *regex_pat == '\0')
			error (FATAL, "wrong mtable pattern specification: %s", pattern);

		char *table_name = eStrndup(pattern, c - pattern);
		table_index = getTableIndexForName (lcb, table_name);
		if (table_index < 0)
			error (FATAL, "unknown table name: %s (in %s)", table_name, pattern);
		eFree(table_name);
	}
	else
		regex_pat = eStrdup (pattern);

	if (parseTagRegex (regptype, regex_pat, &name, &kinds, &flags))
		addTagRegexInternal (lcb, table_index, regptype, regex_pat, name, kinds, flags,
							 NULL);

	eFree (regex_pat);
}

extern void processTagRegexOption (struct lregexControlBlock *lcb,
								   enum regexParserType regptype,
								   const char* const parameter)
{
	if (parameter == NULL  ||  parameter [0] == '\0')
		clearPatternSet (lcb);
	else if (parameter [0] != '@')
		addTagRegexOption (lcb, regptype, parameter);
	else if (! doesFileExist (parameter + 1))
		error (WARNING, "cannot open regex file");
	else
	{
		const char* regexfile = parameter + 1;

		verbose ("open a regex file: %s\n", regexfile);
		MIO* const mio = mio_new_file (regexfile, "r");
		if (mio == NULL)
			error (WARNING | PERROR, "%s", regexfile);
		else
		{
			vString* const regex = vStringNew ();
			while (readLineRaw (regex, mio))
			{
				if (vStringLength (regex) > 1 && vStringValue (regex)[0] != '\n')
					addTagRegexOption (lcb, regptype, vStringValue (regex));
			}
			mio_unref (mio);
			vStringDelete (regex);
		}
	}
}

/*
*   Regex option parsing
*/

extern void printRegexFlags (bool withListHeader, bool machinable, FILE *fp)
{
	struct colprintTable * table;

	table = flagsColprintTableNew ();

	flagsColprintAddDefinitions (table, regexFlagDefs,  ARRAY_SIZE (regexFlagDefs));
	flagsColprintAddDefinitions (table, prePtrnFlagDef, ARRAY_SIZE (prePtrnFlagDef));
	flagsColprintAddDefinitions (table, guestPtrnFlagDef, ARRAY_SIZE (guestPtrnFlagDef));
	flagsColprintAddDefinitions (table, scopePtrnFlagDef, ARRAY_SIZE (scopePtrnFlagDef));
	flagsColprintAddDefinitions (table, commonSpecFlagDef, ARRAY_SIZE (commonSpecFlagDef));

	flagsColprintTablePrint (table, withListHeader, machinable, fp);
	colprintTableDelete(table);
}

extern void printMultilineRegexFlags (bool withListHeader, bool machinable, FILE *fp)
{
	struct colprintTable * table;

	table = flagsColprintTableNew ();

	flagsColprintAddDefinitions (table, regexFlagDefs,  ARRAY_SIZE (regexFlagDefs));
	flagsColprintAddDefinitions (table, multilinePtrnFlagDef, ARRAY_SIZE (multilinePtrnFlagDef));
	flagsColprintAddDefinitions (table, guestPtrnFlagDef, ARRAY_SIZE (guestPtrnFlagDef));
	flagsColprintAddDefinitions (table, commonSpecFlagDef, ARRAY_SIZE (commonSpecFlagDef));

	flagsColprintTablePrint (table, withListHeader, machinable, fp);
	colprintTableDelete(table);
}

extern void printMultitableRegexFlags (bool withListHeader, bool machinable, FILE *fp)
{
	struct colprintTable * table;

	table = flagsColprintTableNew ();

	flagsColprintAddDefinitions (table, regexFlagDefs,  ARRAY_SIZE (regexFlagDefs));
	flagsColprintAddDefinitions (table, multilinePtrnFlagDef, ARRAY_SIZE (multilinePtrnFlagDef));
	flagsColprintAddDefinitions (table, multitablePtrnFlagDef, ARRAY_SIZE (multitablePtrnFlagDef));
	flagsColprintAddDefinitions (table, guestPtrnFlagDef, ARRAY_SIZE (guestPtrnFlagDef));
	flagsColprintAddDefinitions (table, scopePtrnFlagDef, ARRAY_SIZE (scopePtrnFlagDef));
	flagsColprintAddDefinitions (table, commonSpecFlagDef, ARRAY_SIZE (commonSpecFlagDef));

	flagsColprintTablePrint (table, withListHeader, machinable, fp);
	colprintTableDelete(table);
}

extern void freeRegexResources (void)
{
	/* TODO: SHOULD BE REMOVED */
}

extern bool regexNeedsMultilineBuffer (struct lregexControlBlock *lcb)
{
	if  (ptrArrayCount(lcb->entries [REG_PARSER_MULTI_LINE]) > 0)
		return true;
	else if (ptrArrayCount(lcb->tables) > 0)
		return true;
	else
		return false;
}

extern bool matchMultilineRegex (struct lregexControlBlock *lcb, const vString* const allLines)
{
	bool result = false;

	unsigned int i;

	for (i = 0; i < ptrArrayCount(lcb->entries [REG_PARSER_MULTI_LINE]); ++i)
	{
		regexTableEntry *entry = ptrArrayItem(lcb->entries [REG_PARSER_MULTI_LINE], i);
		Assert (entry && entry->pattern);

		if ((entry->pattern->xtagType != XTAG_UNKNOWN)
			&& (!isXtagEnabled (entry->pattern->xtagType)))
			continue;

		result = matchMultilineRegexPattern (lcb, allLines, entry) || result;
	}
	return result;
}

static int getTableIndexForName (const struct lregexControlBlock *const lcb, const char *name)
{
	unsigned int i;

	for (i = 0; i < ptrArrayCount(lcb->tables); i++)
	{
		struct regexTable *table = ptrArrayItem(lcb->tables, i);
		if (strcmp (table->name, name) == 0)
			return (int)i;
	}

	return TABLE_INDEX_UNUSED;
}

extern void addRegexTable (struct lregexControlBlock *lcb, const char *name)
{
	const char *c;
	for (c = name; *c; c++)
		if (! (isalnum(*c) || *c == '_'))
			error (FATAL, "`%c' in \"%s\" is not acceptable as part of table name", *c, name);

	if (getTableIndexForName(lcb, name) >= 0)
	{
		error (WARNING, "regex table \"%s\" is already defined", name);
		return;
	}

	struct regexTable *table = xCalloc(1, struct regexTable);
	table->name = eStrdup (name);
	table->entries = ptrArrayNew(deleteTableEntry);

	ptrArrayAdd (lcb->tables, table);
}

static void dumpSstack(FILE* fp, int scope)
{
	tagEntryInfo *entry;
	fprintf (fp, "scope : ");
	while ((entry = getEntryInCorkQueue (scope)))
	{
		fprintf(fp, "%s", entry->name);

		scope = entry->extensionFields.scopeIndex;
		if (scope != CORK_NIL)
			fprintf(fp, "%c", '/');
	}
	fprintf (fp, "\n");
}

static void dumpTstack(FILE* fp, ptrArray *tstack)
{
	for (unsigned int i = ptrArrayCount(tstack); i > 0; i--)
	{
		char tmp[2];
		struct regexTable *t = ptrArrayItem(tstack, i - 1);
		if (i == 1)
			tmp[0] = '\0';
		else
		{
			tmp[0] = '/';
			tmp[1] = '\0';
		}
		fprintf(fp, "%s%s", t->name, tmp);
	}
	fprintf(fp, "\n");
}

static void printInputLine(FILE* vfp, const char *c, const off_t offset)
{
	vString *v = vStringNew ();

	for (; *c && (*c != '\n'); c++)
		vStringPut(v, *c);

	if (vStringLength (v) == 0 && *c == '\n')
		vStringCatS (v, "\\n");

	fprintf (vfp, "\ninput : \"%s\" L%lu\n",
			 vStringValue (v),
			 getInputLineNumberForFileOffset(offset));
	vStringDelete(v);
}

static void printMultitableMessage(const langType language,
								   const char *const tableName,
								   const unsigned int index,
								   const regexPattern *const ptrn,
								   const off_t offset,
								   const char *const current,
								   const regmatch_t* const pmatch)
{
	vString *msg;

	Assert (ptrn);
	Assert (ptrn->message.selection > 0);
	Assert (ptrn->message.message_string);

	msg = substitute (current, ptrn->message.message_string, BACK_REFERENCE_COUNT, pmatch);

	error (ptrn->message.selection, "%sMessage from mtable<%s/%s[%2u]>: %s (%s:%lu)",
		   (ptrn->message.selection == FATAL ? "Fatal: " : ""),
		   getLanguageName (language),
		   tableName,
		   index,
		   vStringValue (msg),
		   getInputFileName (),
		   getInputLineNumberForFileOffset (offset));

	vStringDelete (msg);
}

static struct regexTable * matchMultitableRegexTable (struct lregexControlBlock *lcb,
													  struct regexTable *table, const vString *const start, unsigned int *offset)
{
	struct regexTable *next = NULL;
	const char *current;
	regmatch_t pmatch [BACK_REFERENCE_COUNT];
	const char *cstart = vStringValue(start);
	unsigned int delta;


 restart:
	current = cstart + *offset;

	/* Accept the case *offset == vStringLength(start)
	   because we want an empty regex // still matches empty input. */
	if (*offset > vStringLength(start))
	{
		*offset = vStringLength(start);
		goto out;
	}

	BEGIN_VERBOSE(vfp);
	{
		printInputLine(vfp, current, *offset);
	}
	END_VERBOSE();

	for (unsigned int i = 0; i < ptrArrayCount(table->entries); i++)
	{
		regexTableEntry *entry = ptrArrayItem(table->entries, i);
		if ((entry->pattern->xtagType != XTAG_UNKNOWN)
			&& (!isXtagEnabled (entry->pattern->xtagType)))
			continue;

		regexPattern *ptrn = entry->pattern;
		struct guestSpec  *guest = &ptrn->guest;

		Assert (ptrn);

		BEGIN_VERBOSE(vfp);
		{
			char s[3];
			if (*current == '\n')
			{
				s [0] = '\\';
				s [1] = 'n';
				s [2] = '\0';
			}
			else if (*current == '\t')
			{
				s [0] = '\\';
				s [1] = 't';
				s [2] = '\0';
			}
			else if (*current == '\\')
			{
				s [0] = '\\';
				s [1] = '\\';
				s [2] = '\0';
			}
			else
			{
				s[0] = *current;
				s[1] = '\0';
			}

			if (s[1] == '\0')
				fprintf (vfp, "match : '%s' %15s[%2u] /", s, table->name, i);
			else if (s[0] == '\0')
				fprintf (vfp, "match :  '' %15s[%2u] /", table->name, i);
			else
				fprintf (vfp, "match :'%s' %15s[%2u] / ", s, table->name, i);
			fprintf (vfp, "%s/\n", ptrn->pattern_string);
		}
		END_VERBOSE();

		int match = 0;

		if (ptrn->disabled && *(ptrn->disabled))
			continue;

		match = regexec (ptrn->pattern, current,
						 BACK_REFERENCE_COUNT, pmatch, 0);

		if (match == 0)
		{
			entry->statistics.match++;

			if (ptrn->type == PTRN_TAG)
			{
				struct mTableActionSpec *taction = &(ptrn->taction);

				matchTagPattern (lcb, current, ptrn, pmatch,
								 (current
								  + pmatch [ptrn->mgroup.forLineNumberDetermination].rm_so)
								 - cstart);
				BEGIN_VERBOSE(vfp);
				{
					fprintf(vfp, "result: matched %d bytes\n", (int)(pmatch[0].rm_eo));
					dumpSstack (vfp, lcb->currentScope);
				}
				END_VERBOSE();

				if (hasMessage(ptrn))
					printMultitableMessage (lcb->owner, table->name, i, ptrn,
											*offset, current, pmatch);

				if (fillGuestRequest (cstart, current, pmatch, guest, lcb->guest_req))
				{
					Assert (lcb->guest_req->lang != LANG_AUTO);
					if (isGuestRequestConsistent(lcb->guest_req))
						guestRequestSubmit (lcb->guest_req);
					guestRequestClear (lcb->guest_req);
				}

				delta = (ptrn->mgroup.nextFromStart
						 ? pmatch [ptrn->mgroup.forNextScanning].rm_so
						 : pmatch [ptrn->mgroup.forNextScanning].rm_eo);
				*offset += delta;

				switch (taction->action)
				{
				case TACTION_NOP:
					BEGIN_VERBOSE(vfp);
					{
						fprintf(vfp, "action: NOP in {%s}, stack: /", table->name);
						dumpTstack(vfp, lcb->tstack);
					}
					END_VERBOSE();
					break;
				case TACTION_ENTER:
					/* TODO: Limit the depth of tstack.  */
					ptrArrayAdd (lcb->tstack,
								 taction->continuation_table
								 ? taction->continuation_table
								 : table);
					next = taction->table;
					BEGIN_VERBOSE(vfp);
					{
						if (taction->continuation_table)
							fprintf(vfp, "action: [enter] to {%s}, cont: {%s}, stack: /",
									next->name,
									taction->continuation_table->name);
						else
							fprintf(vfp, "action: [enter] to {%s}, stack: /", next->name);
						dumpTstack(vfp, lcb->tstack);
					}
					END_VERBOSE();
					break;
				case TACTION_LEAVE:
					BEGIN_VERBOSE(vfp);
					{
						fprintf(vfp, "action: [leave] from {%s}, stack: /", table->name);
						dumpTstack(vfp, lcb->tstack);
					}
					END_VERBOSE();
					if (ptrArrayCount (lcb->tstack) == 0)
					{
						error (WARNING, "leave is specified as regex table action but the table stack is empty");
						return NULL;
					}
					next = ptrArrayLast(lcb->tstack);
					ptrArrayRemoveLast (lcb->tstack);
					break;
				case TACTION_JUMP:
					next = taction->table;
					BEGIN_VERBOSE(vfp);
					{
						fprintf(vfp, "action: [jump] from {%s} to {%s}, stack: /", table->name, next->name);
						dumpTstack(vfp, lcb->tstack);
					}
					END_VERBOSE();

					break;
				case TACTION_RESET:
					next = taction->table;
					BEGIN_VERBOSE(vfp);
					{
						fprintf(vfp, "action: [reset] to {%s}, stack: /", next->name);
					}
					END_VERBOSE();

					ptrArrayClear (lcb->tstack);
					break;
				case TACTION_QUIT:
					BEGIN_VERBOSE(vfp);
					{
						fprintf(vfp, "action: [quit], stack: /");
						dumpTstack(vfp, lcb->tstack);
					}
					END_VERBOSE();
					return NULL;
				}

				if (next)
					break;

				if (delta == 0)
				{
					error (WARNING, "Forcefully advance the input pos because");
					error (WARNING, "following conditions for entering infinite loop are satisfied:");
					error (WARNING, "+ matching the pattern succeeds,");
					error (WARNING, "+ the next table is not given, and");
					error (WARNING, "+ the input file pos doesn't advance.");
					error (WARNING, "Language: %s, input file: %s, pos: %u",
						   getLanguageName (lcb->owner), getInputFileName(), *offset);
					++*offset;
				}
			}
			else if (ptrn->type == PTRN_CALLBACK)
				;	/* Not implemented yet */
			else
			{
				Assert ("invalid pattern type" == NULL);
				break;
			}
			goto restart;
		}
		else
			entry->statistics.unmatch++;
	}
 out:
	if (next == NULL && ptrArrayCount (lcb->tstack) > 0)
	{
		static int apop_count = 0;
		next = ptrArrayLast(lcb->tstack);
		verbose("result: no match - autopop<%d> from {%s} to {%s} @ %lu\n", apop_count++, table->name, next->name,
				getInputLineNumberForFileOffset(*offset));
		ptrArrayRemoveLast (lcb->tstack);
	}
	return next;
}

extern void extendRegexTable (struct lregexControlBlock *lcb, const char *src, const char *dist)
{

	int i;
	struct regexTable * src_table;
	struct regexTable * dist_table;

	verbose ("extend regex table  \"%s\" with \"%s\"\n", dist, src);

	i = getTableIndexForName (lcb, src);
	if (i < 0)
		error (FATAL, "no such regex table in %s: %s", getLanguageName(lcb->owner), src);
	src_table = ptrArrayItem(lcb->tables, i);

	i = getTableIndexForName (lcb, dist);
	if (i < 0)
		error (FATAL, "no such regex table in %s: %s", getLanguageName(lcb->owner), dist);
	dist_table = ptrArrayItem(lcb->tables, i);

	for (i = 0; i < (int)ptrArrayCount(src_table->entries); i++)
	{
		regexTableEntry *entry = ptrArrayItem (src_table->entries, i);
		ptrArrayAdd(dist_table->entries, newRefPatternEntry(entry));
	}
}

extern void printMultitableStatistics (struct lregexControlBlock *lcb)
{
	if (ptrArrayCount(lcb->tables) == 0)
		return;

	fprintf(stderr, "\nMTABLE REGEX STATISTICS of %s\n", getLanguageName (lcb->owner));
	fputs("==============================================\n", stderr);
	for (unsigned int i = 0; i < ptrArrayCount(lcb->tables); i++)
	{
		struct regexTable *table = ptrArrayItem (lcb->tables, i);
		fprintf(stderr, "%s\n", table->name);
		fputs("-----------------------\n", stderr);
		for (unsigned int j = 0; j < ptrArrayCount(table->entries); j++)
		{
			regexTableEntry *entry = ptrArrayItem (table->entries, j);
			Assert (entry && entry->pattern);
			fprintf(stderr, "%10u/%-10u%-40s ref: %d\n",
					entry->statistics.match,
					entry->statistics.unmatch + entry->statistics.match,
					entry->pattern->pattern_string,
					entry->pattern->refcount);
		}
		fputc('\n', stderr);
	}
}

extern bool matchMultitableRegex (struct lregexControlBlock *lcb, const vString* const allLines)
{
	if (ptrArrayCount (lcb->tables) == 0)
		return false;

	struct regexTable *table = ptrArrayItem (lcb->tables, 0);
	unsigned int offset = 0;

	int motionless_counter = 0;
	unsigned int last_offset;


	while (table)
	{
		last_offset = offset;
		table = matchMultitableRegexTable(lcb, table, allLines, &offset);

		if (last_offset == offset)
			motionless_counter++;
		else
			motionless_counter = 0;

		if (motionless_counter > MTABLE_MOTIONLESS_MAX)
		{
			error (WARNING, "mtable<%s/%s>: the input cursor stays at %u in %s so long though the tables are switched",
				   getLanguageName (lcb->owner),
				   table->name, offset, getInputFileName ());
			break;
		}

		if (table && (ptrArrayCount (lcb->tstack) > MTABLE_STACK_MAX_DEPTH))
		{
			unsigned int i;
			struct regexTable *t;

			error (WARNING, "mtable<%s/%s>: the tenter/tleave stack overflows at %u in %s",
				   getLanguageName (lcb->owner),
				   table->name, offset, getInputFileName ());
			error (WARNING, "DUMP FROM THE TOP:");
			/* TODO: use dumpTstack */
			for (i = ptrArrayCount(lcb->tstack); 0 < i; --i)
			{
				t = ptrArrayItem (lcb->tstack, i - 1);
				error (WARNING, "%3u %s", i - 1, t->name);
			}

			break;
		}
	}

	return true;
}

static int  makePromiseForAreaSpecifiedWithOffsets (const char *parser,
													off_t startOffset,
													off_t endOffset)
{
	unsigned long startLine = getInputLineNumberForFileOffset(startOffset);
	unsigned long endLine = getInputLineNumberForFileOffset(endOffset);
	unsigned long startLineOffset = getInputFileOffsetForLine (startLine);
	unsigned long endLineOffset = getInputFileOffsetForLine (endLine);

	return makePromise (parser,
						startLine, startOffset - startLineOffset,
						endLine, endOffset - endLineOffset,
						startOffset - startLineOffset);
}

static struct guestRequest *guestRequestNew (void)
{
	struct guestRequest *r = xMalloc (1, struct guestRequest);


	guestRequestClear (r);
	return r;
}

static void   guestRequestDelete (struct guestRequest *r)
{
	eFree (r);
}

static bool   guestRequestIsFilled(struct guestRequest *r)
{
	return (r->lang_set && (r->boundary + 0)->offset_set && (r->boundary + 1)->offset_set);
}

static void   guestRequestClear (struct guestRequest *r)
{
	r->lang_set = false;
	r->boundary[BOUNDARY_START].offset_set = false;
	r->boundary[BOUNDARY_END].offset_set = false;
}

static void   guestRequestSubmit (struct guestRequest *r)
{
	const char *langName = getLanguageName (r->lang);
	verbose ("guestRequestSubmit: %s; "
			 "range: %"PRId64" - %"PRId64"\n",
			 langName,
			 (int64_t)r->boundary[BOUNDARY_START].offset,
			 (int64_t)r->boundary[BOUNDARY_END].offset);
	makePromiseForAreaSpecifiedWithOffsets (langName,
											r->boundary[BOUNDARY_START].offset,
											r->boundary[BOUNDARY_END].offset);
}

/* Return true if available. */
extern bool checkRegex (void)
{
#if defined (CHECK_REGCOMP)
	{
		/* Check for broken regcomp() on Cygwin */
		regex_t patbuf;
		int errcode;
		if (regcomp (&patbuf, "/hello/", 0) != 0)
			error (WARNING, "Disabling broken regex");
		else
			regexAvailable = true;
	}
#else
	/* We are using bundled regex engine. */
	regexAvailable = true;
#endif
	return regexAvailable;
}
