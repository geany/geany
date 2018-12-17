/*
 *
 *  Copyright (c) 2015, Red Hat, Inc.
 *  Copyright (c) 2015, Masatake YAMATO
 *
 *  Author: Masatake YAMATO <yamato@redhat.com>
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 */

#include "general.h"  /* must always come first */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "ctags.h"
#include "debug.h"
#include "entry.h"
#include "field.h"
#include "kind.h"
#include "options.h"
#include "read.h"
#include "routines.h"


struct sFieldDesc {
	fieldDefinition *spec;
	unsigned int fixed:   1;   /* fields which cannot be disabled. */
	vString     *buffer;
	const char* nameWithPrefix;
	langType language;
	fieldType sibling;
};

static const char *renderFieldName (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldInput (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldCompactInputLine (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldSignature (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldScope (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldTyperef (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldInherits (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldKindName (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldLineNumber (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldLanguage (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldAccess (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldKindLetter (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldImplementation (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldFile (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldPattern (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldRole (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldRefMarker (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldExtra (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldXpath (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldScopeKindName(const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldEnd (const tagEntryInfo *const tag, const char *value, vString* b);

static bool     isLanguageFieldAvailable  (const tagEntryInfo *const tag);
static bool     isTyperefFieldAvailable   (const tagEntryInfo *const tag);
static bool     isFileFieldAvailable      (const tagEntryInfo *const tag);
static bool     isInheritsFieldAvailable  (const tagEntryInfo *const tag);
static bool     isAccessFieldAvailable    (const tagEntryInfo *const tag);
static bool     isImplementationFieldAvailable (const tagEntryInfo *const tag);
static bool     isSignatureFieldAvailable (const tagEntryInfo *const tag);
static bool     isRoleFieldAvailable      (const tagEntryInfo *const tag);
static bool     isExtraFieldAvailable     (const tagEntryInfo *const tag);
static bool     isXpathFieldAvailable     (const tagEntryInfo *const tag);
static bool     isEndFieldAvailable       (const tagEntryInfo *const tag);


#define DEFINE_FIELD_SPEC(L, N, V, H, F)		\
	DEFINE_FIELD_SPEC_FULL (L, N, V, H, F, NULL)
#define DEFINE_FIELD_SPEC_FULL(L, N, V, H, F, A)\
	{					\
		.letter        = L,		\
		.name          = N,		\
		.description   = H,		\
		.enabled       = V,		\
		.renderEscaped = F,		\
		.isValueAvailable = A,		\
	}

#define WITH_DEFUALT_VALUE(str) ((str)?(str):"-")

static fieldDefinition fieldDefinitionsFixed [] = {
        /* FIXED FIELDS */
	DEFINE_FIELD_SPEC ('N', "name",     true,
			  "tag name (fixed field)",
			  renderFieldName),
	DEFINE_FIELD_SPEC ('F', "input",    true,
			   "input file (fixed field)",
			   renderFieldInput),
	DEFINE_FIELD_SPEC ('P', "pattern",  true,
			   "pattern (fixed field)",
			   renderFieldPattern),
};

static fieldDefinition fieldDefinitionsExuberant [] = {
	DEFINE_FIELD_SPEC ('C', "compact",        false,
			   "compact input line (fixed field, only used in -x option)",
			   renderFieldCompactInputLine),

	/* EXTENSION FIELDS */
	DEFINE_FIELD_SPEC_FULL ('a', "access",         false,
		      "Access (or export) of class members",
		      renderFieldAccess, isAccessFieldAvailable),
	DEFINE_FIELD_SPEC_FULL ('f', "file",           true,
		      "File-restricted scoping",
		      renderFieldFile, isFileFieldAvailable),
	DEFINE_FIELD_SPEC_FULL ('i', "inherits",       false,
		      "Inheritance information",
		      renderFieldInherits, isInheritsFieldAvailable),
	DEFINE_FIELD_SPEC ('K', NULL,             false,
		      "Kind of tag as full name",
		      renderFieldKindName),
	DEFINE_FIELD_SPEC ('k', NULL,             true,
			   "Kind of tag as a single letter",
			   renderFieldKindLetter),
	DEFINE_FIELD_SPEC_FULL ('l', "language",       false,
			   "Language of input file containing tag",
			   renderFieldLanguage, isLanguageFieldAvailable),
	DEFINE_FIELD_SPEC_FULL ('m', "implementation", false,
			   "Implementation information",
			   renderFieldImplementation, isImplementationFieldAvailable),
	DEFINE_FIELD_SPEC ('n', "line",           false,
			   "Line number of tag definition",
			   renderFieldLineNumber),
	DEFINE_FIELD_SPEC_FULL ('S', "signature",	     false,
			   "Signature of routine (e.g. prototype or parameter list)",
			   renderFieldSignature, isSignatureFieldAvailable),
	DEFINE_FIELD_SPEC ('s', NULL,             true,
			   "Scope of tag definition (`p' can be used for printing its kind)",
			   renderFieldScope),
	DEFINE_FIELD_SPEC_FULL ('t', "typeref",        true,
			   "Type and name of a variable or typedef",
			   renderFieldTyperef, isTyperefFieldAvailable),
	DEFINE_FIELD_SPEC ('z', "kind",           false,
			   "Include the \"kind:\" key in kind field (use k or K) in tags output, kind full name in xref output",
			   /* Following renderer is for handling --_xformat=%{kind};
			      and is not for tags output. */
			   renderFieldKindName),
};

static fieldDefinition fieldDefinitionsUniversal [] = {
	DEFINE_FIELD_SPEC_FULL ('r', "role",    false,
			   "Role",
			   renderFieldRole, isRoleFieldAvailable),
	DEFINE_FIELD_SPEC ('R',  NULL,     false,
			   "Marker (R or D) representing whether tag is definition or reference",
			   renderFieldRefMarker),
	DEFINE_FIELD_SPEC ('Z', "scope",   false,
			  "Include the \"scope:\" key in scope field (use s) in tags output, scope name in xref output",
			   /* Following renderer is for handling --_xformat=%{scope};
			      and is not for tags output. */
			   renderFieldScope),
	DEFINE_FIELD_SPEC_FULL ('E', "extra",   false,
			   "Extra tag type information",
			   renderFieldExtra, isExtraFieldAvailable),
	DEFINE_FIELD_SPEC_FULL ('x', "xpath",   false,
			   "xpath for the tag",
			   renderFieldXpath, isXpathFieldAvailable),
	DEFINE_FIELD_SPEC ('p', "scopeKind", false,
			   "Kind of scope as full name",
			   renderFieldScopeKindName),
	DEFINE_FIELD_SPEC_FULL ('e', "end", false,
			   "end lines of various items",
			   renderFieldEnd, isEndFieldAvailable),
};


static unsigned int       fieldDescUsed = 0;
static unsigned int       fieldDescAllocated = 0;
static fieldDesc* fieldDescs = NULL;

extern void initFieldDescs (void)
{
	int i;
	fieldDesc *fdesc;

	Assert (fieldDescs == NULL);

	fieldDescAllocated
	  = ARRAY_SIZE (fieldDefinitionsFixed)
	  + ARRAY_SIZE (fieldDefinitionsExuberant)
	  + ARRAY_SIZE (fieldDefinitionsUniversal);
	fieldDescs = xMalloc (fieldDescAllocated, fieldDesc);

	fieldDescUsed = 0;

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsFixed); i++)
	{
		fdesc = fieldDescs + i + fieldDescUsed;
		fdesc->spec   = fieldDefinitionsFixed + i;
		fdesc->fixed  = 1;
		fdesc->buffer = NULL;
		fdesc->nameWithPrefix = fdesc->spec->name;
		fdesc->language = LANG_IGNORE;
		fdesc->sibling  = FIELD_UNKNOWN;
	}
	fieldDescUsed += ARRAY_SIZE (fieldDefinitionsFixed);

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsExuberant); i++)
	{
		fdesc = fieldDescs + i + fieldDescUsed;
		fdesc->spec = fieldDefinitionsExuberant +i;
		fdesc->fixed = 0;
		fdesc->buffer = NULL;
		fdesc->nameWithPrefix = fdesc->spec->name;
		fdesc->language = LANG_IGNORE;
		fdesc->sibling  = FIELD_UNKNOWN;
	}
	fieldDescUsed += ARRAY_SIZE (fieldDefinitionsExuberant);

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsUniversal); i++)
	{
		char *nameWithPrefix;

		fdesc = fieldDescs + i + fieldDescUsed;
		fdesc->spec = fieldDefinitionsUniversal + i;
		fdesc->fixed = 0;
		fdesc->buffer = NULL;

		if (fdesc->spec->name)
		{
			nameWithPrefix = eMalloc (sizeof CTAGS_FIELD_PREFIX + strlen (fdesc->spec->name) + 1);
			nameWithPrefix [0] = '\0';
			strcat (nameWithPrefix, CTAGS_FIELD_PREFIX);
			strcat (nameWithPrefix, fdesc->spec->name);
			fdesc->nameWithPrefix = nameWithPrefix;
		}
		else
			fdesc->nameWithPrefix = NULL;
		fdesc->language = LANG_IGNORE;
		fdesc->sibling  = FIELD_UNKNOWN;
	}
	fieldDescUsed += ARRAY_SIZE (fieldDefinitionsUniversal);

	Assert ( fieldDescAllocated == fieldDescUsed );
}

static fieldDesc* getFieldDesc(fieldType type)
{
	Assert ((0 <= type) && (type < fieldDescUsed));
	return fieldDescs + type;
}

extern fieldType getFieldTypeForOption (char letter)
{
	unsigned int i;

	for (i = 0; i < fieldDescUsed; i++)
	{
		if (fieldDescs [i].spec->letter == letter)
			return i;
	}
	return FIELD_UNKNOWN;
}

extern fieldType getFieldTypeForName (const char *name)
{
	return getFieldTypeForNameAndLanguage (name, LANG_IGNORE);
}

extern fieldType getFieldTypeForNameAndLanguage (const char *fieldName, langType language)
{
	static bool initialized = false;
	unsigned int i;

	if (fieldName == NULL)
		return FIELD_UNKNOWN;

	if (language == LANG_AUTO && (initialized == false))
	{
		initialized = true;
		initializeParser (LANG_AUTO);
	}
	else if (language != LANG_IGNORE && (initialized == false))
		initializeParser (language);

	for (i = 0; i < fieldDescUsed; i++)
	{
		if (fieldDescs [i].spec->name
		    && strcmp (fieldDescs [i].spec->name, fieldName) == 0
		    && ((language == LANG_AUTO)
			|| (fieldDescs [i].language == language)))
			return i;
	}

	return FIELD_UNKNOWN;
}

extern const char* getFieldName(fieldType type)
{
	fieldDesc* fdesc;

	fdesc = getFieldDesc (type);
	if (Option.putFieldPrefix)
		return fdesc->nameWithPrefix;
	else
		return fdesc->spec->name;
}

extern bool doesFieldHaveValue (fieldType type, const tagEntryInfo *tag)
{
	if (getFieldDesc(type)->spec->isValueAvailable)
		return getFieldDesc(type)->spec->isValueAvailable(tag);
	else
		return true;
}

#define PR_FIELD_WIDTH_LETTER     7
#define PR_FIELD_WIDTH_NAME      15
#define PR_FIELD_WIDTH_LANGUAGE  16
#define PR_FIELD_WIDTH_DESC      30
#define PR_FIELD_WIDTH_XFMT      6
#define PR_FIELD_WIDTH_ENABLED   7

#define PR_FIELD_STR(X) PR_FIELD_WIDTH_##X
#define PR_FIELD_FMT(X,T) "%-" STRINGIFY(PR_FIELD_STR(X)) STRINGIFY(T)

#define MAKE_FIELD_FMT(LETTER_SPEC)		\
	PR_FIELD_FMT (LETTER,LETTER_SPEC)	\
	" "					\
	PR_FIELD_FMT (NAME,s)			\
	" "					\
	PR_FIELD_FMT (ENABLED,s)		\
	" "					\
	PR_FIELD_FMT (LANGUAGE,s)		\
	" "					\
	PR_FIELD_FMT (XFMT,s)		\
	" "					\
	PR_FIELD_FMT (DESC,s)			\
	"\n"

static void printField (fieldType i)
{
	unsigned char letter = fieldDescs[i].spec->letter;
	const char *name;
	const char *language;

	if (letter == FIELD_LETTER_NO_USE)
		letter = '-';

	if (! fieldDescs[i].spec->name)
		name = "NONE";
	else
		name = getFieldName (i);

	if (fieldDescs[i].language == LANG_IGNORE)
		language = "NONE";
	else
		language = getLanguageName (fieldDescs[i].language);

	printf((Option.machinable? "%c\t%s\t%s\t%s\t%s\t%s\n": MAKE_FIELD_FMT(c)),
	       letter,
	       name,
	       isFieldEnabled (i)? "on": "off",
	       language,
	       getFieldDesc (i)->spec->renderEscaped? "TRUE": "FALSE",
	       fieldDescs[i].spec->description? fieldDescs[i].spec->description: "NONE");
}

extern void printFields (int language)
{
	unsigned int i;

	if (Option.withListHeader)
		printf ((Option.machinable? "%s\t%s\t%s\t%s\t%s\t%s\n": MAKE_FIELD_FMT(s)),
			"#LETTER", "NAME", "ENABLED", "LANGUAGE", "XFMT", "DESCRIPTION");

	for (i = 0; i < fieldDescUsed; i++)
	{
		if (language == LANG_AUTO || getFieldOwner (i) == language)
			printField (i);
	}
}

static const char *renderAsIs (vString* b CTAGS_ATTR_UNUSED, const char *s)
{
	return s;
}

static const char *renderEscapedString (const char *s,
					const tagEntryInfo *const tag CTAGS_ATTR_UNUSED,
					vString* b)
{
	vStringCatSWithEscaping (b, s);
	return vStringValue (b);
}

static const char *renderEscapedName (const char* s,
				      const tagEntryInfo *const tag,
				      vString* b)
{
	const char* base = s;

	for (; *s; s++)
	{
		int c = *s;
		if ((c > 0x00 && c <= 0x1F) || c == 0x7F)
		{
			char letter = getLanguageKind(tag->langType, tag->kindIndex)->letter;
			verbose ("Unexpected character (0 < *c && *c < 0x20) included in a tagEntryInfo: %s\n", base);
			verbose ("File: %s, Line: %lu, Lang: %s, Kind: %c\n",
				 tag->inputFileName, tag->lineNumber, getLanguageName(tag->langType), letter);
			verbose ("Escape the character\n");
			break;
		}
		else if (c == '\\')
			break;
		else
			continue;
	}

	if (*s == '\0')
		return base;

	vStringNCatS (b, base, s - base);

	return renderEscapedString (s, tag, b);
}

static const char *renderFieldName (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedName (tag->name, tag, b);
}

static const char *renderFieldInput (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char *f = tag->inputFileName;

	if (Option.lineDirectives && tag->sourceFileName)
		f = tag->sourceFileName;
	return renderEscapedString (f, tag, b);
}

static const char *renderFieldSignature (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedString (WITH_DEFUALT_VALUE (tag->extensionFields.signature),
				    tag, b);
}

static const char *renderFieldScope (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char* scope;

	getTagScopeInformation ((tagEntryInfo *const)tag, NULL, &scope);
	return scope? renderEscapedName (scope, tag, b): NULL;
}

static const char *renderFieldInherits (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedString (WITH_DEFUALT_VALUE (tag->extensionFields.inheritance),
				    tag, b);
}

static const char *renderFieldTyperef (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedName (WITH_DEFUALT_VALUE (tag->extensionFields.typeRef [1]), tag, b);
}


extern const char* renderFieldEscaped (fieldType type,
				       const tagEntryInfo *tag,
				       int index)
{
	fieldDesc *fdesc = fieldDescs + type;
	const char *value;

	Assert (tag);
	Assert (fdesc->spec->renderEscaped);

	fdesc->buffer = vStringNewOrClear (fdesc->buffer);

	if (index >= 0)
	{
		Assert ( tag->usedParserFields > index );
		value = tag->parserFields[ index ].value;
	}
	else
		value = NULL;

	return fdesc->spec->renderEscaped (tag, value, fdesc->buffer);
}

/*  Writes "line", stripping leading and duplicate white space.
 */
static const char* renderCompactInputLine (vString *b,  const char *const line)
{
	bool lineStarted = false;
	const char *p;
	int c;

	/*  Write everything up to, but not including, the newline.
	 */
	for (p = line, c = *p  ;  c != NEWLINE  &&  c != '\0'  ;  c = *++p)
	{
		if (lineStarted  || ! isspace (c))  /* ignore leading spaces */
		{
			lineStarted = true;
			if (isspace (c))
			{
				int next;

				/*  Consume repeating white space.
				 */
				while (next = *(p+1) , isspace (next)  &&  next != NEWLINE)
					++p;
				c = ' ';  /* force space character for any white space */
			}
			if (c != CRETURN  ||  *(p + 1) != NEWLINE)
				vStringPut (b, c);
		}
	}
	return vStringValue (b);
}

static const char *renderFieldKindName (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	kindDefinition *kdef = getLanguageKind(tag->langType, tag->kindIndex);
	return renderAsIs (b, kdef->name);
}

static const char *renderFieldCompactInputLine (const tagEntryInfo *const tag,
						const char *value CTAGS_ATTR_UNUSED,
						 vString* b)
{
	const char *line;
	static vString *tmp;

	tmp = vStringNewOrClear (tmp);

	line = readLineFromBypassAnyway (tmp, tag, NULL);
	if (line)
		renderCompactInputLine (b, line);
	else
	{
		/* If no associated line for tag is found, we cannot prepare
		 * parameter to writeCompactInputLine(). In this case we
		 * use an empty string as LINE.
		 */
		vStringClear (b);
	}

	return vStringValue (b);
}

static const char *renderFieldLineNumber (const tagEntryInfo *const tag,
					  const char *value CTAGS_ATTR_UNUSED,
					  vString* b)
{
	long ln = tag->lineNumber;
	char buf[32] = {[0] = '\0'};

	if (Option.lineDirectives && (tag->sourceLineNumberDifference != 0))
		ln += tag->sourceLineNumberDifference;
	snprintf (buf, sizeof(buf), "%ld", ln);
	vStringCatS (b, buf);
	return vStringValue (b);
}

static const char *renderFieldRole (const tagEntryInfo *const tag,
				    const char *value CTAGS_ATTR_UNUSED,
				    vString* b)
{
	int rindex = tag->extensionFields.roleIndex;

	if (rindex == ROLE_INDEX_DEFINITION)
		vStringClear (b);
	else
	{
		return "TODO";
	}

	return vStringValue (b);
}

static const char *renderFieldLanguage (const tagEntryInfo *const tag,
					const char *value CTAGS_ATTR_UNUSED,
					vString* b)
{
	const char *l;

	if (Option.lineDirectives && (tag->sourceLangType != LANG_IGNORE))
		l = getLanguageName(tag->sourceLangType);
	else
		l = getLanguageName(tag->langType);

	return renderAsIs (b, WITH_DEFUALT_VALUE(l));
}

static const char *renderFieldAccess (const tagEntryInfo *const tag,
				      const char *value,
				      vString* b)
{
	return renderAsIs (b, WITH_DEFUALT_VALUE (tag->extensionFields.access));
}

static const char *renderFieldKindLetter (const tagEntryInfo *const tag,
					  const char *value CTAGS_ATTR_UNUSED,
					  vString* b)
{
	return "TODO";
}

static const char *renderFieldImplementation (const tagEntryInfo *const tag,
					      const char *value CTAGS_ATTR_UNUSED,
					      vString* b)
{
	return renderAsIs (b, WITH_DEFUALT_VALUE (tag->extensionFields.implementation));
}

static const char *renderFieldFile (const tagEntryInfo *const tag,
				    const char *value CTAGS_ATTR_UNUSED,
				    vString* b)
{
	return renderAsIs (b, tag->isFileScope? "file": "-");
}

static const char *renderFieldPattern (const tagEntryInfo *const tag,
				       const char *value CTAGS_ATTR_UNUSED,
				       vString* b)
{
	char* tmp = makePatternString (tag);
	vStringCatS (b, tmp);
	eFree (tmp);
	return vStringValue (b);
}

static const char *renderFieldRefMarker (const tagEntryInfo *const tag,
					 const char *value CTAGS_ATTR_UNUSED,
					 vString* b)
{
	static char c[2] = { [1] = '\0' };

	c [0] = tag->extensionFields.roleIndex == ROLE_INDEX_DEFINITION? 'D': 'R';

	return renderAsIs (b, c);
}

static const char *renderFieldExtra (const tagEntryInfo *const tag,
				     const char *value CTAGS_ATTR_UNUSED,
				     vString* b)
{
	int i;
	bool hasExtra = false;

	for (i = 0; i < XTAG_COUNT; i++)
	{
		const char *name = getXtagName (i);

		if (!name)
			continue;

		if (isTagExtraBitMarked (tag, i))
		{

			if (hasExtra)
				vStringPut (b, ',');
			vStringCatS (b, name);
			hasExtra = true;
		}
	}

	if (hasExtra)
		return vStringValue (b);
	else
		return NULL;
}

static const char *renderFieldXpath (const tagEntryInfo *const tag,
				     const char *value,
				     vString* b)
{
#ifdef HAVE_LIBXML
	if (tag->extensionFields.xpath)
		return renderEscapedString (tag->extensionFields.xpath,
					    tag, b);
#endif
	return NULL;
}

static const char *renderFieldScopeKindName(const tagEntryInfo *const tag,
					    const char *value,
					    vString* b)
{
	const char* kind;

	getTagScopeInformation ((tagEntryInfo *const)tag, &kind, NULL);
	return kind? renderAsIs (b, kind): NULL;
}

static const char *renderFieldEnd (const tagEntryInfo *const tag,
				   const char *value,
				   vString* b)
{
	static char buf[16];

	if (tag->extensionFields.endLine != 0)
	{
		sprintf (buf, "%ld", tag->extensionFields.endLine);
		return renderAsIs (b, buf);
	}
	else
		return NULL;
}

static bool     isLanguageFieldAvailable (const tagEntryInfo *const tag)
{
	return (tag->langType == LANG_IGNORE)? false: true;
}

static bool     isTyperefFieldAvailable  (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.typeRef [0] != NULL
		&& tag->extensionFields.typeRef [1] != NULL)? true: false;
}

static bool     isFileFieldAvailable  (const tagEntryInfo *const tag)
{
	return tag->isFileScope? true: false;
}

static bool     isInheritsFieldAvailable (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.inheritance != NULL)? true: false;
}

static bool     isAccessFieldAvailable   (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.access != NULL)? true: false;
}

static bool     isImplementationFieldAvailable (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.implementation != NULL)? true: false;
}

static bool     isSignatureFieldAvailable (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.signature != NULL)? true: false;
}

static bool     isRoleFieldAvailable      (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.roleIndex != ROLE_INDEX_DEFINITION)? true: false;
}

static bool     isExtraFieldAvailable     (const tagEntryInfo *const tag)
{
	int i;
	for (i = 0; i < sizeof (tag->extra); i++)
	{
		if (tag->extra [i])
			return true;
	}

	return false;
}

static bool     isXpathFieldAvailable      (const tagEntryInfo *const tag)
{
#ifdef HAVE_LIBXML
	return (tag->extensionFields.xpath != NULL)? true: false;
#else
	return false;
#endif
}

static bool     isEndFieldAvailable       (const tagEntryInfo *const tag)
{
	return (tag->extensionFields.endLine != 0)? true: false;
}

extern bool isFieldEnabled (fieldType type)
{
	return getFieldDesc(type)->spec->enabled;
}

static bool isFieldFixed (fieldType type)
{
	return getFieldDesc(type)->fixed? true: false;
}

extern bool enableField (fieldType type, bool state, bool warnIfFixedField)
{
	fieldDefinition *spec = getFieldDesc(type)->spec;
	bool old = spec->enabled? true: false;
	if (isFieldFixed (type))
	{
		if ((!state) && warnIfFixedField)
		{
			if (spec->name && spec->letter != NUL_FIELD_LETTER)
				error(WARNING, "Cannot disable fixed field: '%c'{%s}",
				      spec->letter, spec->name);
			else if (spec->name)
				error(WARNING, "Cannot disable fixed field: {%s}",
				      spec->name);
			else if (spec->letter != NUL_FIELD_LETTER)
				error(WARNING, "Cannot disable fixed field: '%c'",
				      getFieldDesc(type)->spec->letter);
			else
				AssertNotReached();
		}
	}
	else
	{
		getFieldDesc(type)->spec->enabled = state;

		if (isCommonField (type))
			verbose ("enable field \"%s\": %s\n",
				 getFieldDesc(type)->spec->name,
				 (state? "TRUE": "FALSE"));
		else
			verbose ("enable field \"%s\"<%s>: %s\n",
				 getFieldDesc(type)->spec->name,
				 getLanguageName (getFieldOwner(type)),
				 (state? "TRUE": "FALSE"));
	}
	return old;
}

extern bool isCommonField (fieldType type)
{
	return (FIELD_BUILTIN_LAST < type)? false: true;
}

extern int     getFieldOwner (fieldType type)
{
	return getFieldDesc(type)->language;
}

extern bool isFieldRenderable (fieldType type)
{
	return getFieldDesc(type)->spec->renderEscaped? true: false;
}

extern int countFields (void)
{
	return fieldDescUsed;
}

extern fieldType nextSiblingField (fieldType type)
{
	fieldDesc *fdesc;

	fdesc = fieldDescs + type;
	return fdesc->sibling;
}

static void updateSiblingField (fieldType type, const char* name)
{
	int i;
	fieldDesc *fdesc;

	for (i = type; i > 0; i--)
	{
		fdesc = fieldDescs + i - 1;
		if (fdesc->spec->name && (strcmp (fdesc->spec->name, name) == 0))
		{
			Assert (fdesc->sibling == FIELD_UNKNOWN);
			fdesc->sibling = type;
			break;
		}
	}
}

static const char* defaultRenderer (const tagEntryInfo *const tag,
				    const char *value,
				    vString * buffer)
{
	return value;
}

extern int defineField (fieldDefinition *spec, langType language)
{
	fieldDesc *fdesc;
	char *nameWithPrefix;
	size_t i;

	Assert (spec);
	Assert (spec->name);
	for (i = 0; i < strlen (spec->name); i++)
	{
		Assert ( isalnum (spec->name [i]) );
	}
	spec->letter = NUL_FIELD_LETTER;

	if (fieldDescUsed == fieldDescAllocated)
	{
		fieldDescAllocated *= 2;
		fieldDescs = xRealloc (fieldDescs, fieldDescAllocated, fieldDesc);
	}
	fdesc = fieldDescs + (fieldDescUsed);
	spec->ftype = fieldDescUsed++;

	if (spec->renderEscaped == NULL)
		spec->renderEscaped = defaultRenderer;

	fdesc->spec = spec;

	fdesc->fixed =  0;
	fdesc->buffer = NULL;

	nameWithPrefix = eMalloc (sizeof CTAGS_FIELD_PREFIX + strlen (spec->name) + 1);
	nameWithPrefix [0] = '\0';
	strcat (nameWithPrefix, CTAGS_FIELD_PREFIX);
	strcat (nameWithPrefix, spec->name);
	fdesc->nameWithPrefix = nameWithPrefix;

	fdesc->language = language;
	fdesc->sibling  = FIELD_UNKNOWN;

	updateSiblingField (spec->ftype, spec->name);
	return spec->ftype;
}
