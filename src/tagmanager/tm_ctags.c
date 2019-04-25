/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/

#include "general.h"  /* must always come first */

#include "tm_ctags.h"
#include "types.h"
#include "routines.h"
#include "error.h"
#include "mio.h"
#include "writer_p.h"
#include "parse_p.h"
#include "options_p.h"
#include "trashbox.h"
#include "field.h"
#include "xtag.h"
#include "entry_p.h"

#include "tm_tag.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define CTAGS_LANG(x) ((x) >= 0 ? (x) + 1 : (x))
#define GEANY_LANG(x) ((x) >= 1 ? (x) - 1 : (x))

static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag);
static void rescanFailed (tagWriter *writer, unsigned long validTagNum);

tagWriter geanyWriter = {
	.writeEntry = writeEntry,
	.writePtagEntry = NULL, /* no pseudo-tags */
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.rescanFailedEntry = rescanFailed,
	.buildFqTagCache = NULL,
	.defaultFileName = "geany_tags_file_which_should_never_appear_anywhere",
	.private = NULL,
	.type = WRITER_U_CTAGS /* not really but we must use some of the builtin types */
};

static TMSourceFile *current_source_file = NULL;


static bool nofatalErrorPrinter (const errorSelection selection,
					  const char *const format,
					  va_list ap, void *data CTAGS_ATTR_UNUSED)
{
	fprintf (stderr, "%s: ", (selection & WARNING) ? "Warning: " : "Error");
	vfprintf (stderr, format, ap);
	if (selection & PERROR)
#ifdef HAVE_STRERROR
		fprintf (stderr, " : %s", strerror (errno));
#else
		perror (" ");
#endif
	fputs ("\n", stderr);

	return false;
}


static void enableAllLangKinds()
{
	unsigned int lang;

	for (lang = 0; lang < countParsers(); lang++)
	{
		unsigned int kindNum = countLanguageKinds(lang);
		unsigned int kind;

		for (kind = 0; kind < kindNum; kind++)
		{
			kindDefinition *def = getLanguageKind(lang, kind);
			enableKind(def, true);
		}
	}
}


/*
 Initializes a TMTag structure with information from a ctagsTag struct
 used by the ctags parsers. Note that the TMTag structure must be malloc()ed
 before calling this function.
 @param tag The TMTag structure to initialize
 @param file Pointer to a TMSourceFile struct (it is assigned to the file member)
 @param tag_entry Tag information gathered by the ctags parser
 @return TRUE on success, FALSE on failure
*/
static gboolean init_tag(TMTag *tag, TMSourceFile *file, const tagEntryInfo *tag_entry)
{
	TMTagType type;
	guchar kind_letter;
	TMParserType lang;

	if (!tag_entry)
		return FALSE;

	lang = GEANY_LANG(tag_entry->langType);
	kind_letter = getLanguageKind(tag_entry->langType, tag_entry->kindIndex)->letter;
	type = tm_parser_get_tag_type(kind_letter, lang);
	if (file->lang != lang)  /* this is a tag from a subparser */
	{
		/* check for possible re-definition of subparser type */
		type = tm_parser_get_subparser_type(file->lang, lang, type);
	}

	if (!tag_entry->name || type == tm_tag_undef_t)
		return FALSE;

	tag->name = g_strdup(tag_entry->name);
	tag->type = type;
	tag->local = tag_entry->isFileScope;
	tag->pointerOrder = 0;	/* backward compatibility (use var_type instead) */
	tag->line = tag_entry->lineNumber;
	if (NULL != tag_entry->extensionFields.signature)
		tag->arglist = g_strdup(tag_entry->extensionFields.signature);
	if ((NULL != tag_entry->extensionFields.scopeName) &&
		(0 != tag_entry->extensionFields.scopeName[0]))
		tag->scope = g_strdup(tag_entry->extensionFields.scopeName);
	if (tag_entry->extensionFields.inheritance != NULL)
		tag->inheritance = g_strdup(tag_entry->extensionFields.inheritance);
	if (tag_entry->extensionFields.typeRef[1] != NULL)
		tag->var_type = g_strdup(tag_entry->extensionFields.typeRef[1]);
	if (tag_entry->extensionFields.access != NULL)
		tag->access = tm_source_file_get_tag_access(tag_entry->extensionFields.access);
	if (tag_entry->extensionFields.implementation != NULL)
		tag->impl = tm_source_file_get_tag_impl(tag_entry->extensionFields.implementation);
	if ((tm_tag_macro_t == tag->type) && (NULL != tag->arglist))
		tag->type = tm_tag_macro_with_arg_t;
	tag->file = file;
	/* redefine lang also for subparsers because the rest of Geany assumes that
	 * tags from a single file are from a single language */
	tag->lang = file->lang;
	return TRUE;
}


/* add argument list of __init__() Python methods to the class tag */
static void update_python_arglist(const TMTag *tag, TMSourceFile *current_source_file)
{
	guint i;
	const char *parent_tag_name;

	if (tag->type != tm_tag_method_t || tag->scope == NULL ||
		g_strcmp0(tag->name, "__init__") != 0)
		return;

	parent_tag_name = strrchr(tag->scope, '.');
	if (parent_tag_name)
		parent_tag_name++;
	else
		parent_tag_name = tag->scope;

	/* going in reverse order because the tag was added recently */
	for (i = current_source_file->tags_array->len; i > 0; i--)
	{
		TMTag *prev_tag = (TMTag *) current_source_file->tags_array->pdata[i - 1];
		if (g_strcmp0(prev_tag->name, parent_tag_name) == 0)
		{
			g_free(prev_tag->arglist);
			prev_tag->arglist = g_strdup(tag->arglist);
			break;
		}
	}
}


static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag)
{
	TMTag *tm_tag = tm_tag_new();

	getTagScopeInformation((tagEntryInfo *)tag, NULL, NULL);

	if (!init_tag(tm_tag, current_source_file, tag))
	{
		tm_tag_unref(tm_tag);
		return 0;
	}

	if (tm_tag->lang == TM_PARSER_PYTHON)
		update_python_arglist(tm_tag, current_source_file);

	g_ptr_array_add(current_source_file->tags_array, tm_tag);

	/* output length - we don't write anything to the MIO */
	return 0;
}


static void rescanFailed (tagWriter *writer, unsigned long valid_tag_num)
{
	GPtrArray *tags_array = current_source_file->tags_array;

	if (tags_array->len > valid_tag_num)
	{
		guint i;
		for (i = valid_tag_num; i < tags_array->len; i++)
			tm_tag_unref(tags_array->pdata[i]);
		g_ptr_array_set_size(tags_array, valid_tag_num);
	}
}


/* keep in sync with ctags main() - use only things interesting for us */
extern void ctagsInit(void)
{
	initDefaultTrashBox ();

	setErrorPrinter (nofatalErrorPrinter, NULL);
	geanySetTagWriter (&geanyWriter);

	checkRegex ();
	initFieldObjects ();
	initXtagObjects ();

	initializeParsing ();
	initOptions ();

	/* make sure all parsers are initialized */
	initializeParser (LANG_AUTO);

	/* change default value which is false */
	enableXtag(XTAG_TAGS_GENERATED_BY_GUEST_PARSERS, true);

	/* some kinds we are interested in are disabled by default */
	enableAllLangKinds();
}


extern void ctagsParse(unsigned char *buffer, size_t bufferSize,
	const char *fileName, const int language, TMSourceFile *source_file)
{
	if (buffer == NULL && fileName == NULL)
	{
		error(FATAL, "Neither buffer nor file provided to ctagsParse()");
		return;
	}

	current_source_file = source_file;

	geanyCreateTags(buffer, bufferSize, fileName, CTAGS_LANG(language));
}


extern const char *ctagsGetLangName(int lang)
{
	return getLanguageName(CTAGS_LANG(lang));
}


extern int ctagsGetNamedLang(const char *name)
{
	return GEANY_LANG(getNamedLanguage(name, 0));
}


extern const char *ctagsGetLangKinds(int lang)
{
	unsigned int kindNum = countLanguageKinds(CTAGS_LANG(lang));
	static char kinds[257];
	unsigned int i;

	for (i = 0; i < kindNum; i++)
		kinds[i] = getLanguageKind(CTAGS_LANG(lang), i)->letter;
	kinds[i] = '\0';

	return kinds;
}


extern const char *ctagsGetKindName(char kind, int lang)
{
	kindDefinition *def = getLanguageKindForLetter (CTAGS_LANG(lang), kind);
	return def ? def->name : "unknown";
}


extern char ctagsGetKindFromName(const char *name, int lang)
{
	kindDefinition *def = getLanguageKindForName (CTAGS_LANG(lang), name);
	return def ? def->letter : '-';
}


extern unsigned int ctagsGetLangCount(void)
{
	return GEANY_LANG(countParsers());
}
