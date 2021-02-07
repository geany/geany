/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Encapsulates ctags so it is isolated from the rest of Geany.
*/

#include "tm_ctags.h"
#include "tm_tag.h"

#include "general.h"  /* must always come before the rest of ctags headers */
#include "entry_p.h"
#include "error_p.h"
#include "field_p.h"
#include "options_p.h"
#include "parse_p.h"
#include "trashbox_p.h"
#include "writer_p.h"
#include "xtag_p.h"

#include <string.h>


static gint write_entry(tagWriter *writer, MIO * mio, const tagEntryInfo *const tag, void *user_data);
static void rescan_failed(tagWriter *writer, gulong valid_tag_num, void *user_data);

tagWriter geanyWriter = {
	.writeEntry = write_entry,
	.writePtagEntry = NULL, /* no pseudo-tags */
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.rescanFailedEntry = rescan_failed,
	.treatFieldAsFixed = NULL,
	.defaultFileName = "geany_tags_file_which_should_never_appear_anywhere",
	.private = NULL,
	.type = WRITER_CUSTOM
};


static bool nonfatal_error_printer(const errorSelection selection,
					  const gchar *const format,
					  va_list ap, void *data CTAGS_ATTR_UNUSED)
{
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, ap);

	return false;
}


static void enable_all_lang_kinds()
{
	TMParserType lang;

	for (lang = 0; lang < countParsers(); lang++)
	{
		guint kind_num = countLanguageKinds(lang);
		guint kind;

		for (kind = 0; kind < kind_num; kind++)
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

	lang = tag_entry->langType;
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
static void update_python_arglist(const TMTag *tag, TMSourceFile *source_file)
{
	guint i;
	const gchar *parent_tag_name;

	if (tag->type != tm_tag_method_t || tag->scope == NULL ||
		g_strcmp0(tag->name, "__init__") != 0)
		return;

	parent_tag_name = strrchr(tag->scope, '.');
	if (parent_tag_name)
		parent_tag_name++;
	else
		parent_tag_name = tag->scope;

	/* going in reverse order because the tag was added recently */
	for (i = source_file->tags_array->len; i > 0; i--)
	{
		TMTag *prev_tag = (TMTag *) source_file->tags_array->pdata[i - 1];
		if (g_strcmp0(prev_tag->name, parent_tag_name) == 0)
		{
			g_free(prev_tag->arglist);
			prev_tag->arglist = g_strdup(tag->arglist);
			break;
		}
	}
}


static gint write_entry(tagWriter *writer, MIO * mio, const tagEntryInfo *const tag, void *user_data)
{
	TMSourceFile *source_file = user_data;
	TMTag *tm_tag = tm_tag_new();

	getTagScopeInformation((tagEntryInfo *)tag, NULL, NULL);

	if (!init_tag(tm_tag, source_file, tag))
	{
		tm_tag_unref(tm_tag);
		return 0;
	}

	if (tm_tag->lang == TM_PARSER_PYTHON)
		update_python_arglist(tm_tag, source_file);

	g_ptr_array_add(source_file->tags_array, tm_tag);

	/* output length - we don't write anything to the MIO */
	return 0;
}


static void rescan_failed(tagWriter *writer, gulong valid_tag_num, void *user_data)
{
	TMSourceFile *source_file = user_data;
	GPtrArray *tags_array = source_file->tags_array;

	if (tags_array->len > valid_tag_num)
	{
		guint i;
		for (i = valid_tag_num; i < tags_array->len; i++)
			tm_tag_unref(tags_array->pdata[i]);
		g_ptr_array_set_size(tags_array, valid_tag_num);
	}
}


/* keep in sync with ctags main() - use only things interesting for us */
void tm_ctags_init(void)
{
	initDefaultTrashBox();

	setErrorPrinter(nonfatal_error_printer, NULL);
	setTagWriter(WRITER_CUSTOM, &geanyWriter);

	checkRegex();
	initFieldObjects();
	initXtagObjects();

	initializeParsing();
	initOptions();

	/* make sure all parsers are initialized */
	initializeParser(LANG_AUTO);

	/* change default values which are false */
	enableXtag(XTAG_TAGS_GENERATED_BY_GUEST_PARSERS, true);
	enableXtag(XTAG_REFERENCE_TAGS, true);

	/* some kinds we are interested in are disabled by default */
	enable_all_lang_kinds();
}


void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType language, TMSourceFile *source_file)
{
	g_return_if_fail(buffer != NULL || file_name != NULL);

	parseRawBuffer(file_name, buffer, buffer_size, language, source_file);
}


const gchar *tm_ctags_get_lang_name(TMParserType lang)
{
	return getLanguageName(lang);
}


TMParserType tm_ctags_get_named_lang(const gchar *name)
{
	return getNamedLanguage(name, 0);
}


const gchar *tm_ctags_get_lang_kinds(TMParserType lang)
{
	guint kind_num = countLanguageKinds(lang);
	static gchar kinds[257];
	guint i;

	for (i = 0; i < kind_num; i++)
		kinds[i] = getLanguageKind(lang, i)->letter;
	kinds[i] = '\0';

	return kinds;
}


const gchar *tm_ctags_get_kind_name(gchar kind, TMParserType lang)
{
	kindDefinition *def = getLanguageKindForLetter(lang, kind);
	return def ? def->name : "unknown";
}


gchar tm_ctags_get_kind_from_name(const gchar *name, TMParserType lang)
{
	kindDefinition *def = getLanguageKindForName(lang, name);
	return def ? def->letter : '-';
}


guint tm_ctags_get_lang_count(void)
{
	return countParsers();
}
