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
#include "param_p.h"

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


static void enable_roles(const TMParserType lang, guint kind)
{
	unsigned int c = countLanguageRoles(lang, kind);
	kindDefinition *def = getLanguageKind(lang, kind);
	gchar kind_letter = def->letter;

	for (unsigned int i = 0; i < c; i++)
	{
		roleDefinition* rdef = getLanguageRole(lang, kind, (int)i);
		gboolean should_enable = tm_parser_enable_role(lang, kind_letter);
		enableRole(rdef, should_enable);
	}
}


static void enable_kinds_and_roles()
{
	TMParserType lang;

	for (lang = 0; lang < (gint)countParsers(); lang++)
	{
		guint kind_num = countLanguageKinds(lang);
		guint kind;

		for (kind = 0; kind < kind_num; kind++)
		{
			kindDefinition *def = getLanguageKind(lang, kind);
			gboolean should_enable = tm_parser_enable_kind(lang, def->letter);

			enableKind(def, should_enable);
			if (should_enable)
				enable_roles(lang, kind);
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
	tag->flags = tm_tag_flag_none_t;
	if (isTagExtraBitMarked(tag_entry, XTAG_ANONYMOUS))
		tag->flags |= tm_tag_flag_anon_t;
	tag->kind_letter = kind_letter;
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
	if (tag->scope)
	{
		gchar *new_scope = tm_parser_update_scope(tag->lang, tag->scope);
		if (new_scope != tag->scope)
		{
			g_free(tag->scope);
			tag->scope = new_scope;
		}
	}
	return TRUE;
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
	initRegexOptscript();

	/* make sure all parsers are initialized */
	initializeParser(LANG_AUTO);

	/* change default values which are false */
	enableXtag(XTAG_TAGS_GENERATED_BY_GUEST_PARSERS, true);
	enableXtag(XTAG_REFERENCE_TAGS, true);

	/* some kinds we are interested in are disabled by default */
	enable_kinds_and_roles();
}


void tm_ctags_add_ignore_symbol(const char *value)
{
	langType lang = getNamedLanguage ("CPreProcessor", 0);
	gchar *val = g_strdup(value);

	/* make sure we don't enter empty string - passing NULL or "" clears
	 * the ignore list in ctags */
	val = g_strstrip(val);
	if (*val)
		applyParameter (lang, "ignore", val);
	g_free(val);
}


void tm_ctags_clear_ignore_symbols(void)
{
	langType lang = getNamedLanguage ("CPreProcessor", 0);
	applyParameter (lang, "ignore", NULL);
}


/* call after all tags have been collected so we don't have to handle reparses
 * with the counter (which gets complicated when also subparsers are involved) */
static void rename_anon_tags(TMSourceFile *source_file)
{
	gboolean is_c = source_file->lang == TM_PARSER_C || source_file->lang == TM_PARSER_CPP;
	gint *anon_counter_table = NULL;
	GPtrArray *removed_typedefs = NULL;
	guint i;

	for (i = 0; i < source_file->tags_array->len; i++)
	{
		TMTag *tag = TM_TAG(source_file->tags_array->pdata[i]);
		if (tm_tag_is_anon(tag))
		{
			gchar *orig_name, *new_name = NULL;
			guint j;
			guint new_name_len, orig_name_len;
			gboolean inside_nesting = FALSE;
			guint scope_len = tag->scope ? strlen(tag->scope) : 0;
			gchar kind = tag->kind_letter;

			orig_name = tag->name;
			orig_name_len = strlen(orig_name);

			if (is_c)
			{
				/* First check if there's a typedef behind the scope nesting
				 * such as typedef struct {} Foo; - in this case we can replace
				 * the anon tag with Foo */
				for (j = i + 1; j < source_file->tags_array->len; j++)
				{
					TMTag *nested_tag = TM_TAG(source_file->tags_array->pdata[j]);
					guint nested_scope_len = nested_tag->scope ? strlen(nested_tag->scope) : 0;

					/* Tags can be interleaved with scopeless macros - skip those */
					if (nested_tag->type & (tm_tag_macro_t | tm_tag_macro_with_arg_t))
						continue;

					/* Nested tags have longer scope than the parent - once the scope
					 * is equal or lower than the parent scope, we are outside the tag's
					 * scope. */
					if (nested_scope_len <= scope_len)
						break;
				}

				/* We are out of the nesting - the next tag could be a typedef */
				if (j < source_file->tags_array->len)
				{
					TMTag *typedef_tag = TM_TAG(source_file->tags_array->pdata[j]);
					guint typedef_scope_len = typedef_tag->scope ? strlen(typedef_tag->scope) : 0;

					/* Should be at the same scope level as the anon tag */
					if (typedef_tag->type == tm_tag_typedef_t &&
						typedef_scope_len == scope_len &&
						g_strcmp0(typedef_tag->var_type, tag->name) == 0)
					{
						/* set the name of the original anon tag and pretend
						 * it wasn't a anon tag */
						tag->name = g_strdup(typedef_tag->name);
						tag->flags &= ~tm_tag_flag_anon_t;
						new_name = tag->name;
						/* the typedef tag will be removed */
						if (!removed_typedefs)
							removed_typedefs = g_ptr_array_new();
						g_ptr_array_add(removed_typedefs, GUINT_TO_POINTER(j));
					}
				}
			}

			/* there's no typedef name for the anon tag so let's generate one  */
			if (!new_name)
			{
				gchar buf[50];
				guint anon_counter;
				const gchar *kind_name = tm_ctags_get_kind_name(kind, tag->lang);

				if (!anon_counter_table)
					anon_counter_table = g_new0(gint, 256);

				anon_counter = ++anon_counter_table[kind];

				sprintf(buf, "anon_%s_%u", kind_name, anon_counter);
				tag->name = g_strdup(buf);
				new_name = tag->name;
			}

			new_name_len = strlen(new_name);

			/* Check if this tag is parent of some other tag - if so, we have to
			 * update the scope. It can only be parent of the following tags
			 * so start with the next tag. */
			for (j = i + 1; j < source_file->tags_array->len; j++)
			{
				TMTag *nested_tag = TM_TAG(source_file->tags_array->pdata[j]);
				guint nested_scope_len = nested_tag->scope ? strlen(nested_tag->scope) : 0;
				gchar *pos;

				/* Tags can be interleaved with scopeless macros - skip those */
				if (is_c && nested_tag->type & (tm_tag_macro_t | tm_tag_macro_with_arg_t))
					continue;

				/* In Fortran, we can create variables of anonymous structures:
				 *     structure var1, var2
				 *         integer a
				 *     end structure
				 * and the parser first generates tags for var1 and var2 which
				 * are on the same scope as the structure itself. So first
				 * we need to skip past the tags on the same scope and only
				 * afterwards we get the nested tags.
				 * */
				if (source_file->lang == TM_PARSER_FORTRAN &&
					!inside_nesting && nested_scope_len == scope_len)
					continue;

				inside_nesting = TRUE;

				/* Terminate if outside of tag scope, see above */
				if (nested_scope_len <= scope_len)
					break;

				pos = strstr(nested_tag->scope, orig_name);
				/* We found the parent name in the nested tag scope - replace it
				 * with the new name. Note: anonymous tag names generated by
				 * ctags are unique enough that we don't have to check for
				 * scope separators here. */
				if (pos)
				{
					gchar *str = g_malloc(nested_scope_len + 50);
					guint prefix_len = pos - nested_tag->scope;

					strncpy(str, nested_tag->scope, prefix_len);
					strcpy(str + prefix_len, new_name);
					strcpy(str + prefix_len + new_name_len, pos + orig_name_len);
					g_free(nested_tag->scope);
					nested_tag->scope = str;
				}
			}

			/* We are out of the nesting - the next tags could be variables
			 * of an anonymous struct such as "struct {} a[2], *b, c;" */
			while (j < source_file->tags_array->len)
			{
				TMTag *var_tag = TM_TAG(source_file->tags_array->pdata[j]);
				guint var_scope_len = var_tag->scope ? strlen(var_tag->scope) : 0;
				gchar *pos;

				/* Should be at the same scope level as the anon tag */
				if (var_scope_len == scope_len &&
					var_tag->var_type && (pos = strstr(var_tag->var_type, orig_name)))
				{
					GString *str = g_string_new(var_tag->var_type);
					gssize p = pos - var_tag->var_type;
					g_string_erase(str, p, strlen(orig_name));
					g_string_insert(str, p, new_name);
					g_free(var_tag->var_type);
					var_tag->var_type = str->str;
					g_string_free(str, FALSE);
				}
				else
					break;

				j++;
			}

			g_free(orig_name);
		}
	}

	if (removed_typedefs)
	{
		for (i = 0; i < removed_typedefs->len; i++)
		{
			guint j = GPOINTER_TO_UINT(removed_typedefs->pdata[i]);
			TMTag *tag = TM_TAG(source_file->tags_array->pdata[j]);
			tm_tag_unref(tag);
			source_file->tags_array->pdata[j] = NULL;
		}

		/* remove NULL entries from the array */
		tm_tags_prune(source_file->tags_array);

		g_ptr_array_free(removed_typedefs, TRUE);
	}

	if (anon_counter_table)
		g_free(anon_counter_table);
}


void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType language, TMSourceFile *source_file)
{
	g_return_if_fail(buffer != NULL || file_name != NULL);

	parseRawBuffer(file_name, buffer, buffer_size, language, source_file);

	rename_anon_tags(source_file);
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
