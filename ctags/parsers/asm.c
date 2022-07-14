/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for assembly language
*   files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "cpreprocessor.h"
#include "debug.h"
#include "entry.h"
#include "keyword.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "selectors.h"
#include "vstring.h"

/*
*   DATA DECLARATIONS
*/
typedef enum {
	K_PSUEDO_MACRO_END = -2,
	K_NONE = -1, K_DEFINE, K_LABEL, K_MACRO, K_TYPE,
	K_SECTION,
} AsmKind;

typedef enum {
	OP_UNDEFINED = -1,
	OP_ALIGN,
	OP_COLON_EQUAL,
	OP_END,
	OP_ENDM,
	OP_ENDMACRO,
	OP_ENDP,
	OP_ENDS,
	OP_EQU,
	OP_EQUAL,
	OP_LABEL,
	OP_MACRO,
	OP_PROC,
	OP_RECORD,
	OP_SECTIONS,
	OP_SECTION,
	OP_SET,
	OP_STRUCT,
	OP_LAST
} opKeyword;

typedef enum {
	ASM_SECTION_PLACEMENT,
} asmSectionRole;

typedef struct {
	opKeyword keyword;
	AsmKind kind;
} opKind;

/*
*   DATA DEFINITIONS
*/
static langType Lang_asm;

static roleDefinition asmSectionRoles [] = {
	{ true, "placement", "placement where the assembled code goes" },
};

static kindDefinition AsmKinds [] = {
	{ true, 'd', "define", "defines" },
	{ true, 'l', "label",  "labels"  },
	{ true, 'm', "macro",  "macros"  },
	{ true, 't', "type",   "types (structs and records)"   },
	{ true, 's', "section",   "sections",
	  .referenceOnly = true, ATTACH_ROLES(asmSectionRoles)},
};

static const keywordTable AsmKeywords [] = {
	{ "align",    OP_ALIGN       },
	{ "endmacro", OP_ENDMACRO    },
	{ "endm",     OP_ENDM        },
	{ "end",      OP_END         },
	{ "endp",     OP_ENDP        },
	{ "ends",     OP_ENDS        },
	{ "equ",      OP_EQU         },
	{ "label",    OP_LABEL       },
	{ "macro",    OP_MACRO       },
	{ ":=",       OP_COLON_EQUAL },
	{ "=",        OP_EQUAL       },
	{ "proc",     OP_PROC        },
	{ "record",   OP_RECORD      },
	{ "sections", OP_SECTIONS    },

	/* These are used in GNU as. */
	{ "section",  OP_SECTION     },
	{ "equiv",    OP_EQU         },
	{ "eqv",      OP_EQU         },

	{ "set",      OP_SET         },
	{ "struct",   OP_STRUCT      }
};

static const opKind OpKinds [] = {
	/* must be ordered same as opKeyword enumeration */
	{ OP_ALIGN,       K_NONE   },
	{ OP_COLON_EQUAL, K_DEFINE },
	{ OP_END,         K_NONE   },
	{ OP_ENDM,        K_PSUEDO_MACRO_END },
	{ OP_ENDMACRO,    K_NONE   },
	{ OP_ENDP,        K_NONE   },
	{ OP_ENDS,        K_NONE   },
	{ OP_EQU,         K_DEFINE },
	{ OP_EQUAL,       K_DEFINE },
	{ OP_LABEL,       K_LABEL  },
	{ OP_MACRO,       K_MACRO  },
	{ OP_PROC,        K_LABEL  },
	{ OP_RECORD,      K_TYPE   },
	{ OP_SECTIONS,    K_NONE   },
	{ OP_SECTION,     K_SECTION },
	{ OP_SET,         K_DEFINE },
	{ OP_STRUCT,      K_TYPE   }
};

/*
*   FUNCTION DEFINITIONS
*/
static opKeyword analyzeOperator (const vString *const op)
{
	vString *keyword = vStringNew ();
	opKeyword result;

	vStringCopyToLower (keyword, op);
	result = (opKeyword) lookupKeyword (vStringValue (keyword), Lang_asm);
	vStringDelete (keyword);
	return result;
}

static bool isInitialSymbolCharacter (int c)
{
	return (bool) (c != '\0' && (isalpha (c) || strchr ("_$", c) != NULL));
}

static bool isSymbolCharacter (int c)
{
	/* '?' character is allowed in AMD 29K family */
	return (bool) (c != '\0' && (isalnum (c) || strchr ("_$?", c) != NULL));
}

static AsmKind operatorKind (
		const vString *const operator,
		bool *const found)
{
	AsmKind result = K_NONE;
	const opKeyword kw = analyzeOperator (operator);
	*found = (bool) (kw != OP_UNDEFINED);
	if (*found)
	{
		result = OpKinds [kw].kind;
		Assert (OpKinds [kw].keyword == kw);
	}
	return result;
}

/*  We must check for "DB", "DB.L", "DCB.W" (68000)
 */
static bool isDefineOperator (const vString *const operator)
{
	const unsigned char *const op =
		(unsigned char*) vStringValue (operator);
	const size_t length = vStringLength (operator);
	const bool result = (bool) (length > 0  &&
		toupper ((int) *op) == 'D'  &&
		(length == 2 ||
		 (length == 4  &&  (int) op [2] == '.') ||
		 (length == 5  &&  (int) op [3] == '.')));
	return result;
}

static void makeAsmTag (
		const vString *const name,
		const vString *const operator,
		const bool labelCandidate,
		const bool nameFollows,
		const bool directive,
		int *lastMacroCorkIndex)
{
	if (vStringLength (name) > 0)
	{
		bool found;
		const AsmKind kind = operatorKind (operator, &found);
		if (found)
		{
			if (kind > K_NONE)
				makeSimpleTag (name, kind);
		}
		else if (isDefineOperator (operator))
		{
			if (! nameFollows)
				makeSimpleTag (name, K_DEFINE);
		}
		else if (labelCandidate)
		{
			operatorKind (name, &found);
			if (! found)
				makeSimpleTag (name, K_LABEL);
		}
		else if (directive)
		{
			bool found_dummy;
			const AsmKind kind_for_directive = operatorKind (name, &found_dummy);
			tagEntryInfo *macro_tag;

			switch (kind_for_directive)
			{
			case K_NONE:
				break;
			case K_MACRO:
				*lastMacroCorkIndex = makeSimpleTag (operator,
													 kind_for_directive);
				if (*lastMacroCorkIndex != CORK_NIL)
					registerEntry (*lastMacroCorkIndex);
				break;
			case K_PSUEDO_MACRO_END:
				macro_tag = getEntryInCorkQueue (*lastMacroCorkIndex);
				if (macro_tag)
					macro_tag->extensionFields.endLine = getInputLineNumber ();
				*lastMacroCorkIndex = CORK_NIL;
				break;
			case K_SECTION:
				makeSimpleRefTag (operator,
								  kind_for_directive,
								  ASM_SECTION_PLACEMENT);
				break;
			default:
				makeSimpleTag (operator, kind_for_directive);
			}
		}
	}
}

static const unsigned char *readSymbol (
		const unsigned char *const start,
		vString *const sym)
{
	const unsigned char *cp = start;
	vStringClear (sym);
	if (isInitialSymbolCharacter ((int) *cp))
	{
		while (isSymbolCharacter ((int) *cp))
		{
			vStringPut (sym, *cp);
			++cp;
		}
	}
	return cp;
}

static const unsigned char *readOperator (
		const unsigned char *const start,
		vString *const operator)
{
	const unsigned char *cp = start;
	vStringClear (operator);
	while (*cp != '\0'  &&  ! isspace ((int) *cp) && *cp != ',')
	{
		vStringPut (operator, *cp);
		++cp;
	}
	return cp;
}

static const unsigned char *asmReadLineFromInputFile (void)
{
	static vString *line;
	int c;

	line = vStringNewOrClear (line);

	while ((c = cppGetc()) != EOF)
	{
		if (c == '\n')
			break;
		else if (c == STRING_SYMBOL || c == CHAR_SYMBOL)
		{
			/* We cannot store these values to vString
			 * Store a whitespace as a dummy value for them.
			 */
			vStringPut (line, ' ');
		}
		else
			vStringPut (line, c);
	}

	if ((vStringLength (line) == 0)&& (c == EOF))
		return NULL;
	else
		return (unsigned char *)vStringValue (line);
}

static void findAsmTags (void)
{
	vString *name = vStringNew ();
	vString *operator = vStringNew ();
	const unsigned char *line;

	cppInit (false, false, false, false,
			 KIND_GHOST_INDEX, 0, KIND_GHOST_INDEX, KIND_GHOST_INDEX, 0, 0,
			 FIELD_UNKNOWN);

	 int lastMacroCorkIndex = CORK_NIL;

	while ((line = asmReadLineFromInputFile ()) != NULL)
	{
		const unsigned char *cp = line;
		bool labelCandidate = (bool) (! isspace ((int) *cp));
		bool nameFollows = false;
		bool directive = false;
		const bool isComment = (bool)
				(*cp != '\0' && strchr (";*@", *cp) != NULL);

		/* skip comments */
		if (isComment)
			continue;

		/* skip white space */
		while (isspace ((int) *cp))
			++cp;

		/* read symbol */
		if (*cp == '.')
		{
			directive = true;
			labelCandidate = false;
			++cp;
		}

		cp = readSymbol (cp, name);
		if (vStringLength (name) > 0)
		{
			if (*cp == ':')
			{
				labelCandidate = true;
				++cp;
			}
			else if (anyKindEntryInScope (CORK_NIL,
										  vStringValue (name),
										  K_MACRO))
				labelCandidate = false;
		}

		if (! isspace ((int) *cp)  &&  *cp != '\0')
			continue;

		/* skip white space */
		while (isspace ((int) *cp))
			++cp;

		/* skip leading dot */
#if 0
		if (*cp == '.')
			++cp;
#endif

		cp = readOperator (cp, operator);

		/* attempt second read of symbol */
		if (vStringLength (name) == 0)
		{
			while (isspace ((int) *cp))
				++cp;
			cp = readSymbol (cp, name);
			nameFollows = true;
		}
		makeAsmTag (name, operator, labelCandidate, nameFollows, directive,
					&lastMacroCorkIndex);
	}

	cppTerminate ();

	vStringDelete (name);
	vStringDelete (operator);
}

static void initialize (const langType language)
{
	Lang_asm = language;
}

extern parserDefinition* AsmParser (void)
{
	static const char *const extensions [] = {
		"asm", "ASM", "s", "S", NULL
	};
	static const char *const patterns [] = {
		"*.A51",
		"*.29[kK]",
		"*.[68][68][kKsSxX]",
		"*.[xX][68][68]",
		NULL
	};
	static selectLanguage selectors[] = { selectByArrowOfR,
					      NULL };

	parserDefinition* def = parserNew ("Asm");
	def->kindTable      = AsmKinds;
	def->kindCount  = ARRAY_SIZE (AsmKinds);
	def->extensions = extensions;
	def->patterns   = patterns;
	def->parser     = findAsmTags;
	def->initialize = initialize;
	def->keywordTable = AsmKeywords;
	def->keywordCount = ARRAY_SIZE (AsmKeywords);
	def->selectLanguage = selectors;
	def->useCork = CORK_QUEUE | CORK_SYMTAB;
	return def;
}
