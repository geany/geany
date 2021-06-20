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
#include "entry_p.h"
#include "field.h"
#include "field_p.h"
#include "kind.h"
#include "options_p.h"
#include "parse_p.h"
#include "read.h"
#include "routines.h"
#include "trashbox.h"
#include "writer_p.h"
#include "xtag_p.h"

#define FIELD_NULL_LETTER_CHAR '-'
#define FIELD_NULL_LETTER_STRING "-"

typedef struct sFieldObject {
	fieldDefinition *def;
	vString     *buffer;
	const char* nameWithPrefix;
	langType language;
	fieldType sibling;
} fieldObject;

static const char *renderFieldName (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldNameNoEscape (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b);
static const char *renderFieldInput (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldInputNoEscape (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldCompactInputLine (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldSignature (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldSignatureNoEscape (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldScope (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldScopeNoEscape (const tagEntryInfo *const tag, const char *value, vString* b);
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
static const char *renderFieldRoles (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldRefMarker (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldExtras (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldXpath (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldScopeKindName(const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldEnd (const tagEntryInfo *const tag, const char *value, vString* b);
static const char *renderFieldEpoch (const tagEntryInfo *const tag, const char *value, vString* b);

static bool doesContainAnyCharInName (const tagEntryInfo *const tag, const char *value, const char *chars);
static bool doesContainAnyCharInInput (const tagEntryInfo *const tag, const char*value, const char *chars);
static bool doesContainAnyCharInFieldScope (const tagEntryInfo *const tag, const char *value, const char *chars);
static bool doesContainAnyCharInSignature (const tagEntryInfo *const tag, const char *value, const char *chars);

static bool     isLanguageFieldAvailable  (const tagEntryInfo *const tag);
static bool     isTyperefFieldAvailable   (const tagEntryInfo *const tag);
static bool     isFileFieldAvailable      (const tagEntryInfo *const tag);
static bool     isInheritsFieldAvailable  (const tagEntryInfo *const tag);
static bool     isAccessFieldAvailable    (const tagEntryInfo *const tag);
static bool     isImplementationFieldAvailable (const tagEntryInfo *const tag);
static bool     isSignatureFieldAvailable (const tagEntryInfo *const tag);
static bool     isExtrasFieldAvailable    (const tagEntryInfo *const tag);
static bool     isXpathFieldAvailable     (const tagEntryInfo *const tag);
static bool     isEndFieldAvailable       (const tagEntryInfo *const tag);
static bool     isEpochAvailable           (const tagEntryInfo *const tag);


#define DEFINE_FIELD(L, N, V, H, DT, RE)				\
	DEFINE_FIELD_FULL (L, N, V, H, NULL, DT, RE, NULL, NULL)
#define DEFINE_FIELD_FULL(L, N, V, H, A, DT, RE, RN, DCAC)	\
	{					\
		.letter        = L,		\
		.name          = N,		\
		.description   = H,		\
		.enabled       = V,		\
		.render        = RE,		\
		.renderNoEscaping= RN,		\
		.doesContainAnyChar = DCAC, \
		.isValueAvailable = A,		\
		.dataType = DT, \
	}

#define WITH_DEFUALT_VALUE(str) ((str)?(str):FIELD_NULL_LETTER_STRING)

static fieldDefinition fieldDefinitionsFixed [] = {
        /* FIXED FIELDS */
	DEFINE_FIELD_FULL ('N', "name",     true,
			  "tag name",
			  NULL,
			  FIELDTYPE_STRING,
			  renderFieldName, renderFieldNameNoEscape,
			  doesContainAnyCharInName),
	DEFINE_FIELD_FULL ('F', "input",    true,
			   "input file",
			   NULL,
			   FIELDTYPE_STRING,
			   renderFieldInput, renderFieldInputNoEscape,
			   doesContainAnyCharInInput),
	DEFINE_FIELD ('P', "pattern",  true,
			   "pattern",
			   FIELDTYPE_STRING|FIELDTYPE_BOOL,
			   renderFieldPattern),
};

static fieldDefinition fieldDefinitionsExuberant [] = {
	DEFINE_FIELD ('C', "compact",        false,
			   "compact input line (used only in xref output)",
			   FIELDTYPE_STRING,
			   renderFieldCompactInputLine),

	/* EXTENSION FIELDS */
	DEFINE_FIELD_FULL ('a', "access",         false,
		      "Access (or export) of class members",
			  isAccessFieldAvailable,
			  FIELDTYPE_STRING,
		      renderFieldAccess, NULL, NULL),
	DEFINE_FIELD_FULL ('f', "file",           true,
		      "File-restricted scoping",
			  isFileFieldAvailable,
			  FIELDTYPE_BOOL,
		      renderFieldFile, NULL, NULL),
	DEFINE_FIELD_FULL ('i', "inherits",       false,
		      "Inheritance information",
			  isInheritsFieldAvailable,
			  FIELDTYPE_STRING|FIELDTYPE_BOOL,
		      renderFieldInherits, NULL, NULL),
	DEFINE_FIELD ('K', NULL,             false,
		      "Kind of tag in long-name form",
		      FIELDTYPE_STRING,
		      renderFieldKindName),
	DEFINE_FIELD ('k', NULL,             true,
			   "Kind of tag in one-letter form",
			   FIELDTYPE_STRING,
			   renderFieldKindLetter),
	DEFINE_FIELD_FULL ('l', "language",       false,
			   "Language of input file containing tag",
			   isLanguageFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldLanguage, NULL, NULL),
	DEFINE_FIELD_FULL ('m', "implementation", false,
			   "Implementation information",
			   isImplementationFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldImplementation, NULL, NULL),
	DEFINE_FIELD ('n', "line",           false,
			   "Line number of tag definition",
			   FIELDTYPE_INTEGER,
			   renderFieldLineNumber),
	DEFINE_FIELD_FULL ('S', "signature",     false,
			   "Signature of routine (e.g. prototype or parameter list)",
			   isSignatureFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldSignature, renderFieldSignatureNoEscape,
			   doesContainAnyCharInSignature),
	DEFINE_FIELD_FULL ('s', NULL,             true,
			   "[tags output] scope (kind:name) of tag definition, [xref and json output] name of scope",
			   NULL,
			   FIELDTYPE_STRING,
			   renderFieldScope, renderFieldScopeNoEscape,
			   doesContainAnyCharInFieldScope),
	DEFINE_FIELD_FULL ('t', "typeref",        true,
			   "Type and name of a variable or typedef",
			   isTyperefFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldTyperef, NULL, NULL),
	DEFINE_FIELD ('z', "kind",           false,
			   "[tags output] prepend \"kind:\" to k/ (or K/) field output, [xref and json output] kind in long-name form",
			   FIELDTYPE_STRING,
			   /* Following renderer is for handling --_xformat=%{kind};
			      and is not for tags output. */
			   renderFieldKindName),
};

static fieldDefinition fieldDefinitionsUniversal [] = {
	DEFINE_FIELD ('r', "roles",    false,
			   "Roles",
			   FIELDTYPE_STRING,
			   renderFieldRoles),
	DEFINE_FIELD ('R',  NULL,     false,
			   "Marker (R or D) representing whether tag is definition or reference",
			   FIELDTYPE_STRING,
			   renderFieldRefMarker),
	DEFINE_FIELD_FULL ('Z', "scope",   false,
			  "[tags output] prepend \"scope:\" key to s/scope field output, [xref and json output] the same as s/ field",
			   NULL,
			   FIELDTYPE_STRING,
			   /* Following renderer is for handling --_xformat=%{scope};
			      and is not for tags output. */
			   renderFieldScope, renderFieldScopeNoEscape,
			   doesContainAnyCharInFieldScope),
	DEFINE_FIELD_FULL ('E', "extras",   false,
			   "Extra tag type information",
			   isExtrasFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldExtras, NULL, NULL),
	DEFINE_FIELD_FULL ('x', "xpath",   false,
			   "xpath for the tag",
			   isXpathFieldAvailable,
			   FIELDTYPE_STRING,
			   renderFieldXpath, NULL, NULL),
	DEFINE_FIELD ('p', "scopeKind", false,
			   "[tags output] no effect, [xref and json output] kind of scope in long-name form",
			   FIELDTYPE_STRING,
			   renderFieldScopeKindName),
	DEFINE_FIELD_FULL ('e', "end", false,
			   "end lines of various items",
			   isEndFieldAvailable,
			   FIELDTYPE_INTEGER,
			   renderFieldEnd, NULL, NULL),
	DEFINE_FIELD_FULL ('T', "epoch", true,
					   "the last modified time of the input file (only for F/file kind tag)",
					   isEpochAvailable,
					   FIELDTYPE_INTEGER,
					   renderFieldEpoch, NULL, NULL),
};


static unsigned int       fieldObjectUsed = 0;
static unsigned int       fieldObjectAllocated = 0;
static fieldObject* fieldObjects = NULL;

extern void initFieldObjects (void)
{
	unsigned int i;
	fieldObject *fobj;

	Assert (fieldObjects == NULL);

	fieldObjectAllocated
	  = ARRAY_SIZE (fieldDefinitionsFixed)
	  + ARRAY_SIZE (fieldDefinitionsExuberant)
	  + ARRAY_SIZE (fieldDefinitionsUniversal);
	fieldObjects = xMalloc (fieldObjectAllocated, fieldObject);
	DEFAULT_TRASH_BOX(&fieldObjects, eFreeIndirect);

	fieldObjectUsed = 0;

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsFixed); i++)
	{
		fobj = fieldObjects + i + fieldObjectUsed;
		fobj->def = fieldDefinitionsFixed + i;
		fobj->buffer = NULL;
		fobj->nameWithPrefix = fobj->def->name;
		fobj->language = LANG_IGNORE;
		fobj->sibling  = FIELD_UNKNOWN;
	}
	fieldObjectUsed += ARRAY_SIZE (fieldDefinitionsFixed);

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsExuberant); i++)
	{
		fobj = fieldObjects + i + fieldObjectUsed;
		fobj->def = fieldDefinitionsExuberant +i;
		fobj->buffer = NULL;
		fobj->nameWithPrefix = fobj->def->name;
		fobj->language = LANG_IGNORE;
		fobj->sibling  = FIELD_UNKNOWN;
	}
	fieldObjectUsed += ARRAY_SIZE (fieldDefinitionsExuberant);

	for (i = 0; i < ARRAY_SIZE (fieldDefinitionsUniversal); i++)
	{
		char *nameWithPrefix;

		fobj = fieldObjects + i + fieldObjectUsed;
		fobj->def = fieldDefinitionsUniversal + i;
		fobj->buffer = NULL;

		if (fobj->def->name)
		{
			nameWithPrefix = eMalloc (sizeof CTAGS_FIELD_PREFIX + strlen (fobj->def->name) + 1);
			nameWithPrefix [0] = '\0';
			strcat (nameWithPrefix, CTAGS_FIELD_PREFIX);
			strcat (nameWithPrefix, fobj->def->name);
			fobj->nameWithPrefix = nameWithPrefix;
			DEFAULT_TRASH_BOX(nameWithPrefix, eFree);
		}
		else
			fobj->nameWithPrefix = NULL;
		fobj->language = LANG_IGNORE;
		fobj->sibling  = FIELD_UNKNOWN;
	}
	fieldObjectUsed += ARRAY_SIZE (fieldDefinitionsUniversal);

	Assert ( fieldObjectAllocated == fieldObjectUsed );
}

static fieldObject* getFieldObject(fieldType type)
{
	Assert ((0 <= type) && ((unsigned int)type < fieldObjectUsed));
	return fieldObjects + type;
}

extern fieldType getFieldTypeForOption (char letter)
{
	unsigned int i;

	for (i = 0; i < fieldObjectUsed; i++)
	{
		if (fieldObjects [i].def->letter == letter)
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

	for (i = 0; i < fieldObjectUsed; i++)
	{
		if (fieldObjects [i].def->name
		    && strcmp (fieldObjects [i].def->name, fieldName) == 0
		    && ((language == LANG_AUTO)
			|| (fieldObjects [i].language == language)))
			return i;
	}

	return FIELD_UNKNOWN;
}

extern const char* getFieldDescription (fieldType type)
{
	fieldObject* fobj;

	fobj = getFieldObject (type);
	return fobj->def->description;
}

extern const char* getFieldName(fieldType type)
{
	fieldObject* fobj;

	fobj = getFieldObject (type);
	if (Option.putFieldPrefix)
		return fobj->nameWithPrefix;
	else
		return fobj->def->name;
}

extern unsigned char getFieldLetter (fieldType type)
{
	fieldObject* fobj = getFieldObject (type);

	return fobj->def->letter == '\0'
		? FIELD_NULL_LETTER_CHAR
		: fobj->def->letter;
}

extern bool doesFieldHaveValue (fieldType type, const tagEntryInfo *tag)
{
	if (getFieldObject(type)->def->isValueAvailable)
		return getFieldObject(type)->def->isValueAvailable(tag);
	else
		return true;
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

static const char *renderEscapedName (const bool isTagName,
				      const char* s,
				      const tagEntryInfo *const tag,
				      vString* b)
{
	int unexpected_byte = 0;

	if (isTagName && (!tag->isPseudoTag) &&  (*s == ' ' || *s == '!'))
	{
		/* Don't allow a leading space or exclamation mark as it conflicts with
		 * pseudo-tags when sorting.  Anything with a lower byte value is
		 * escaped by renderEscapedString() already. */
		unexpected_byte = *s;
		switch (*s)
		{
			case ' ': vStringCatS (b, "\\x20"); s++; break;
			case '!': vStringCatS (b, "\\x21"); s++; break;
			default: AssertNotReached();
		}
	}
	else
	{
		/* Find the first byte needing escaping for the warning message */
		const char *p = s;

		while (*p > 0x1F && *p != 0x7F)
			p++;
		unexpected_byte = *p;
	}

	if (unexpected_byte)
	{
		const kindDefinition *kdef = getTagKind (tag);
		verbose ("Unexpected character %#04x included in a tagEntryInfo: %s\n", unexpected_byte, s);
		verbose ("File: %s, Line: %lu, Lang: %s, Kind: %c\n",
			 tag->inputFileName, tag->lineNumber, getLanguageName(tag->langType), kdef->letter);
		verbose ("Escape the character\n");
	}

	return renderEscapedString (s, tag, b);
}

static const char *renderFieldName (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedName (true, tag->name, tag, b);
}

static const char *renderFieldNameNoEscape (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderAsIs (b, tag->name);
}

static bool doesContainAnyCharInName (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, const char *chars)
{
	return strpbrk (tag->name, chars)? true: false;
}

static const char *renderFieldInput (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char *f = tag->inputFileName;

	if (Option.lineDirectives && tag->sourceFileName)
		f = tag->sourceFileName;
	return renderEscapedString (f, tag, b);
}

static const char *renderFieldInputNoEscape (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char *f = tag->inputFileName;

	if (Option.lineDirectives && tag->sourceFileName)
		f = tag->sourceFileName;

	return renderAsIs (b, f);
}

static bool doesContainAnyCharInInput (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, const char *chars)
{
	const char *f = tag->inputFileName;

	if (Option.lineDirectives && tag->sourceFileName)
		f = tag->sourceFileName;

	return strpbrk (f, chars)? true: false;
}

static const char *renderFieldSignature (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedString (WITH_DEFUALT_VALUE (tag->extensionFields.signature),
				    tag, b);
}

static const char *renderFieldSignatureNoEscape (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderAsIs (b, WITH_DEFUALT_VALUE (tag->extensionFields.signature));
}

static bool doesContainAnyCharInSignature (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, const char *chars)
{
	return (tag->extensionFields.signature && strpbrk(tag->extensionFields.signature, chars))
		? true
		: false;
}

static const char *renderFieldScope (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char* scope;

	getTagScopeInformation ((tagEntryInfo *const)tag, NULL, &scope);
	return scope? renderEscapedName (false, scope, tag, b): NULL;
}

static const char *renderFieldScopeNoEscape (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	const char* scope;

	getTagScopeInformation ((tagEntryInfo *const)tag, NULL, &scope);
	return scope? renderAsIs (b, scope): NULL;
}

static bool doesContainAnyCharInFieldScope (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, const char *chars)
{
	const char* scope;

	getTagScopeInformation ((tagEntryInfo *const)tag, NULL, &scope);
	return (scope && strpbrk (scope, chars));
}


static const char *renderFieldInherits (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	return renderEscapedString (WITH_DEFUALT_VALUE (tag->extensionFields.inheritance),
				    tag, b);
}

static const char *renderFieldTyperef (const tagEntryInfo *const tag, const char *value CTAGS_ATTR_UNUSED, vString* b)
{
	/* Return "-" instead of "-:-". */
	if (tag->extensionFields.typeRef [0] == NULL
		&& tag->extensionFields.typeRef [1] == NULL)
		return renderAsIs (b, FIELD_NULL_LETTER_STRING);

	vStringCatS (b, WITH_DEFUALT_VALUE (tag->extensionFields.typeRef [0]));
	vStringPut  (b, ':');
	return renderEscapedName (false, WITH_DEFUALT_VALUE (tag->extensionFields.typeRef [1]), tag, b);
}


static const char* renderFieldCommon (fieldType type,
									  const tagEntryInfo *tag,
									  int index,
									  bool noEscaping)
{
	fieldObject *fobj = fieldObjects + type;
	const char *value;
	fieldRenderer rfn;

	Assert (tag);
	Assert (index < 0 || ((unsigned int)index) < tag->usedParserFields);

	if (index >= 0)
	{
		const tagField *f = getParserFieldForIndex (tag, index);

		value = f->value;
	}
	else
		value = NULL;

	if (noEscaping)
		rfn = fobj->def->renderNoEscaping;
	else
		rfn = fobj->def->render;
	Assert (rfn);

	fobj->buffer = vStringNewOrClearWithAutoRelease (fobj->buffer);
	return rfn (tag, value, fobj->buffer);
}

extern const char* renderField (fieldType type, const tagEntryInfo *tag, int index)
{
	return renderFieldCommon (type, tag, index, false);
}

extern const char* renderFieldNoEscaping (fieldType type, const tagEntryInfo *tag, int index)
{
	return renderFieldCommon (type, tag, index, true);
}

static bool defaultDoesContainAnyChar (const tagEntryInfo *const tag CTAGS_ATTR_UNUSED, const char* value, const char* chars)
{
	return strpbrk (value, chars)? true: false;
}

extern bool  doesFieldHaveTabOrNewlineChar (fieldType type, const tagEntryInfo *tag, int index)
{
	fieldObject *fobj = fieldObjects + type;
	const char *value;
	bool (* doesContainAnyChar) (const tagEntryInfo *const, const char*, const char*) = fobj->def->doesContainAnyChar;

	Assert (tag);
	Assert (index == NO_PARSER_FIELD || ((unsigned int)index) < tag->usedParserFields);

	if (doesContainAnyChar == NULL)
	{
		if (index == NO_PARSER_FIELD)
			return false;
		else
			doesContainAnyChar = defaultDoesContainAnyChar;
	}

	if (index >= 0)
	{
		const tagField *f = getParserFieldForIndex (tag, index);

		value = f->value;
	}
	else
		value = NULL;

	return (* doesContainAnyChar) (tag, value, "\t\n");
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
	const char* name = getTagKindName (tag);
	return renderAsIs (b, name);
}

static const char *renderFieldCompactInputLine (const tagEntryInfo *const tag,
						const char *value CTAGS_ATTR_UNUSED,
						 vString* b)
{
	const char *line;
	static vString *tmp;

	if (tag->isPseudoTag)
	{
		Assert (tag->pattern);
		return tag->pattern;
	}

	tmp = vStringNewOrClearWithAutoRelease (tmp);

	line = readLineFromBypassForTag (tmp, tag, NULL);
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

static const char *renderFieldRoles (const tagEntryInfo *const tag,
				    const char *value CTAGS_ATTR_UNUSED,
				    vString* b)
{
	roleBitsType rbits = tag->extensionFields.roleBits;
	const roleDefinition * role;
	if (rbits)
	{
		int roleCount = countLanguageRoles (tag->langType, tag->kindIndex);
		int nRoleWritten = 0;

		for (int roleIndex = 0; roleIndex < roleCount; roleIndex++)
		{
			if (((rbits >> roleIndex) & (roleBitsType)1)
				&& isLanguageRoleEnabled (tag->langType, tag->kindIndex, roleIndex))
			{
				if (nRoleWritten > 0)
					vStringPut(b, ',');

				role = getTagRole(tag, roleIndex);
				renderRole (role, b);
				nRoleWritten++;
			}
		}
	}
	else
		vStringCatS (b, ROLE_DEFINITION_NAME);
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
				      const char *value CTAGS_ATTR_UNUSED,
				      vString* b)
{
	return renderAsIs (b, WITH_DEFUALT_VALUE (tag->extensionFields.access));
}

static const char *renderFieldKindLetter (const tagEntryInfo *const tag,
					  const char *value CTAGS_ATTR_UNUSED,
					  vString* b)
{
	static char c[2] = { [1] = '\0' };

	c [0] = getTagKindLetter(tag);

	return renderAsIs (b, c);
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
	return renderAsIs (b, tag->isFileScope? "file": FIELD_NULL_LETTER_STRING);
}

static const char *renderFieldPattern (const tagEntryInfo *const tag,
				       const char *value CTAGS_ATTR_UNUSED,
				       vString* b)
{
	if (tag->isFileEntry)
		return NULL;
	else if (tag->pattern)
		vStringCatS (b, tag->pattern);
	else
	{
		char* tmp;

		tmp = makePatternString (tag);
		vStringCatS (b, tmp);
		eFree (tmp);
	}
	return vStringValue (b);
}

static const char *renderFieldRefMarker (const tagEntryInfo *const tag,
					 const char *value CTAGS_ATTR_UNUSED,
					 vString* b)
{
	static char c[2] = { [1] = '\0' };

	c [0] = (tag->extensionFields.roleBits)? 'R': 'D';

	return renderAsIs (b, c);
}

static const char *renderFieldExtras (const tagEntryInfo *const tag,
				     const char *value CTAGS_ATTR_UNUSED,
				     vString* b)
{
	int i;
	bool hasExtra = false;
	int c = countXtags();

	for (i = 0; i < c; i++)
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
				     const char *value CTAGS_ATTR_UNUSED,
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
					    const char *value CTAGS_ATTR_UNUSED,
					    vString* b)
{
	const char* kind;

	getTagScopeInformation ((tagEntryInfo *const)tag, &kind, NULL);
	return kind? renderAsIs (b, kind): NULL;
}

static const char *renderFieldEnd (const tagEntryInfo *const tag,
				   const char *value CTAGS_ATTR_UNUSED,
				   vString* b)
{
	static char buf[21];

	if (tag->extensionFields.endLine != 0)
	{
		sprintf (buf, "%lu", tag->extensionFields.endLine);
		return renderAsIs (b, buf);
	}
	else
		return NULL;
}

static const char *renderFieldEpoch (const tagEntryInfo *const tag,
									  const char *value, vString* b)
{
#define buf_len 21
	static char buf[buf_len];

	if (snprintf (buf, buf_len, "%lld", (long long)tag->extensionFields.epoch) > 0)
		return renderAsIs (b, buf);
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

static bool     isExtrasFieldAvailable     (const tagEntryInfo *const tag)
{
	unsigned int i;
	for (i = 0; i < sizeof (tag->extra); i++)
	{
		if (tag->extra [i])
			return true;
		else if (tag->extraDynamic)
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

static bool isEpochAvailable (const tagEntryInfo *const tag)
{
	return (tag->kindIndex == KIND_FILE_INDEX)
		? true
		: false;
}

extern bool isFieldEnabled (fieldType type)
{
	return getFieldObject(type)->def->enabled;
}

extern bool enableField (fieldType type, bool state)
{
	fieldDefinition *def = getFieldObject(type)->def;
	bool old = def->enabled;
	getFieldObject(type)->def->enabled = state;

	if (isCommonField (type))
		verbose ("enable field \"%s\": %s\n",
				 getFieldObject(type)->def->name,
				 (state? "yes": "no"));
	else
		verbose ("enable field \"%s\"<%s>: %s\n",
				 getFieldObject(type)->def->name,
				 getLanguageName (getFieldOwner(type)),
				 (state? "yes": "no"));
	return old;
}

extern bool isCommonField (fieldType type)
{
	return (FIELD_BUILTIN_LAST < type)? false: true;
}

extern int     getFieldOwner (fieldType type)
{
	return getFieldObject(type)->language;
}

extern unsigned int getFieldDataType (fieldType type)
{
	return getFieldObject(type)->def->dataType;
}

extern bool doesFieldHaveRenderer (fieldType type, bool noEscaping)
{
	if (noEscaping)
		return getFieldObject(type)->def->renderNoEscaping? true: false;
	else
		return getFieldObject(type)->def->render? true: false;
}

extern int countFields (void)
{
	return fieldObjectUsed;
}

extern fieldType nextSiblingField (fieldType type)
{
	fieldObject *fobj;

	fobj = fieldObjects + type;
	return fobj->sibling;
}

static void updateSiblingField (fieldType type, const char* name)
{
	int i;
	fieldObject *fobj;

	for (i = type; i > 0; i--)
	{
		fobj = fieldObjects + i - 1;
		if (fobj->def->name && (strcmp (fobj->def->name, name) == 0))
		{
			Assert (fobj->sibling == FIELD_UNKNOWN);
			fobj->sibling = type;
			break;
		}
	}
}

static const char* defaultRenderer (const tagEntryInfo *const tag CTAGS_ATTR_UNUSED,
				    const char *value,
				    vString * buffer CTAGS_ATTR_UNUSED)
{
	return renderEscapedString (value, tag, buffer);
}

extern int defineField (fieldDefinition *def, langType language)
{
	fieldObject *fobj;
	char *nameWithPrefix;
	size_t i;

	Assert (def);
	Assert (def->name);
	for (i = 0; i < strlen (def->name); i++)
	{
		Assert ( isalpha (def->name [i]) );
	}
	def->letter = NUL_FIELD_LETTER;

	if (fieldObjectUsed == fieldObjectAllocated)
	{
		fieldObjectAllocated *= 2;
		fieldObjects = xRealloc (fieldObjects, fieldObjectAllocated, fieldObject);
	}
	fobj = fieldObjects + (fieldObjectUsed);
	def->ftype = fieldObjectUsed++;

	if (def->render == NULL)
	{
		def->render = defaultRenderer;
		def->renderNoEscaping = NULL;
		def->doesContainAnyChar = NULL;
	}

	if (! def->dataType)
		def->dataType = FIELDTYPE_STRING;

	fobj->def = def;

	fobj->buffer = NULL;

	nameWithPrefix = eMalloc (sizeof CTAGS_FIELD_PREFIX + strlen (def->name) + 1);
	nameWithPrefix [0] = '\0';
	strcat (nameWithPrefix, CTAGS_FIELD_PREFIX);
	strcat (nameWithPrefix, def->name);
	fobj->nameWithPrefix = nameWithPrefix;
	DEFAULT_TRASH_BOX(nameWithPrefix, eFree);

	fobj->language = language;
	fobj->sibling  = FIELD_UNKNOWN;

	updateSiblingField (def->ftype, def->name);
	return def->ftype;
}

#define FIELD_COL_LETTER      0
#define FIELD_COL_NAME        1
#define FIELD_COL_ENABLED     2
#define FIELD_COL_LANGUAGE    3
#define FIELD_COL_JSTYPE      4
#define FIELD_COL_FIXED       5
#define FIELD_COL_DESCRIPTION 6
extern struct colprintTable * fieldColprintTableNew (void)
{
	return colprintTableNew ("L:LETTER", "L:NAME", "L:ENABLED",
							 "L:LANGUAGE", "L:JSTYPE", "L:FIXED", "L:DESCRIPTION", NULL);
}

static void  fieldColprintAddLine (struct colprintTable *table, int i)
{
	fieldObject *fobj = getFieldObject(i);
	fieldDefinition *fdef = fobj->def;

	struct colprintLine *line = colprintTableGetNewLine(table);

	colprintLineAppendColumnChar (line,
								  (fdef->letter == NUL_FIELD_LETTER)
								  ? FIELD_NULL_LETTER_CHAR
								  : fdef->letter);

	const char *name = getFieldName (i);
	colprintLineAppendColumnCString (line, name? name: RSV_NONE);
	colprintLineAppendColumnBool (line, fdef->enabled);
	colprintLineAppendColumnCString (line,
									 fobj->language == LANG_IGNORE
									 ? RSV_NONE
									 : getLanguageName (fobj->language));

	char  typefields [] = "---";
	{
		unsigned int bmask, offset;
		unsigned int type = getFieldDataType(i);
		for (bmask = 1, offset = 0;
			 bmask < FIELDTYPE_END_MARKER;
			 bmask <<= 1, offset++)
			if (type & bmask)
				typefields[offset] = fieldDataTypeFalgs[offset];
	}
	colprintLineAppendColumnCString (line, typefields);
	colprintLineAppendColumnBool (line, writerDoesTreatFieldAsFixed (i));
	colprintLineAppendColumnCString (line, fdef->description);
}

extern void fieldColprintAddCommonLines (struct colprintTable *table)
{
	for (int i = 0; i <= FIELD_BUILTIN_LAST; i++)
		fieldColprintAddLine(table, i);
}

extern void fieldColprintAddLanguageLines (struct colprintTable *table, langType language)
{
	for (unsigned int i = FIELD_BUILTIN_LAST + 1; i < fieldObjectUsed; i++)
	{
		fieldObject *fobj = getFieldObject(i);
		if (fobj->language == language)
			fieldColprintAddLine (table, i);
	}
}

static int fieldColprintCompareLines (struct colprintLine *a , struct colprintLine *b)
{
	const char *a_fixed  = colprintLineGetColumn (a, FIELD_COL_FIXED);
	const char *b_fixed  = colprintLineGetColumn (b, FIELD_COL_FIXED);
	const char *a_parser = colprintLineGetColumn (a, FIELD_COL_LANGUAGE);
	const char *b_parser = colprintLineGetColumn (b, FIELD_COL_LANGUAGE);

	if ((strcmp (a_fixed, "yes") == 0)
		&& (strcmp (b_fixed, "yes") == 0))
	{
		/* name, input, pattern, compact */
		const char *a_name  = colprintLineGetColumn (a, FIELD_COL_NAME);
		const char *b_name  = colprintLineGetColumn (b, FIELD_COL_NAME);
		const char *ref_name;
		unsigned int a_index = ~0U;
		unsigned int b_index = ~0U;

		for (unsigned int i = 0; i < ARRAY_SIZE(fieldDefinitionsFixed); i++)
		{
			ref_name = fieldDefinitionsFixed [i].name;
			if (strcmp (a_name, ref_name) == 0)
				a_index = i;
			if (strcmp (b_name, ref_name) == 0)
				b_index = i;
			if ((a_index != ~0U) || (b_index != ~0U))
				break;
		}

		if (a_index < b_index)
			return -1;
		else if (a_index == b_index)
			return 0;			/* ??? */
		else
			return 1;
	}
	else if ((strcmp (a_fixed, "yes") == 0)
			  && (strcmp (b_fixed, "yes") != 0))
		return -1;
	else if ((strcmp (a_fixed, "yes") != 0)
			 && (strcmp (b_fixed, "yes") == 0))
		return 1;

	if (strcmp (a_parser, RSV_NONE) == 0
		&& strcmp (b_parser, RSV_NONE) != 0)
		return -1;
	else if (strcmp (a_parser, RSV_NONE) != 0
			 && strcmp (b_parser, RSV_NONE) == 0)
		return 1;
	else if (strcmp (a_parser, RSV_NONE) != 0
			 && strcmp (b_parser, RSV_NONE) != 0)
	{
		int r;
		r = strcmp (a_parser, b_parser);
		if (r != 0)
			return r;

		const char *a_name = colprintLineGetColumn (a, FIELD_COL_NAME);
		const char *b_name = colprintLineGetColumn (b, FIELD_COL_NAME);

		return strcmp(a_name, b_name);
	}
	else
	{
		const char *a_letter = colprintLineGetColumn (a, FIELD_COL_LETTER);
		const char *b_letter = colprintLineGetColumn (b, FIELD_COL_LETTER);

		return strcmp(a_letter, b_letter);
	}
}

extern void fieldColprintTablePrint (struct colprintTable *table,
									 bool withListHeader, bool machinable, FILE *fp)
{
	colprintTableSort (table, fieldColprintCompareLines);
	colprintTablePrint (table, 0, withListHeader, machinable, fp);
}
