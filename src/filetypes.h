/*
 *      filetypes.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef GEANY_FILETYPES_H
#define GEANY_FILETYPES_H 1

#include "geany.h" /* for GEANY() macro */
#include "tm_parser.h" /* for TMParserType */

#include "gtkcompat.h" /* Needed by ScintillaWidget.h */
#include "Scintilla.h" /* Needed by ScintillaWidget.h */
#include "ScintillaWidget.h" /* for ScintillaObject */

#include <glib.h>


G_BEGIN_DECLS

/* Forward-declared to avoid including document.h since it includes this header */
struct GeanyDocument;

/** IDs of known filetypes
 *
 * @ref filetypes will contain an item for each. Use GeanyData::filetypes_array to
 * determine the known filetypes at runtime */
typedef enum
{
	GEANY_FILETYPES_NONE = 0,	/* first filetype is always None & must be 0 */

	GEANY_FILETYPES_PHP,
	GEANY_FILETYPES_BASIC,	/* FreeBasic */
	GEANY_FILETYPES_MATLAB,
	GEANY_FILETYPES_RUBY,
	GEANY_FILETYPES_LUA,
	GEANY_FILETYPES_FERITE,
	GEANY_FILETYPES_YAML,
	GEANY_FILETYPES_C,
	GEANY_FILETYPES_NSIS,
	GEANY_FILETYPES_GLSL,
	GEANY_FILETYPES_PO,
	GEANY_FILETYPES_MAKE,
	GEANY_FILETYPES_TCL,
	GEANY_FILETYPES_XML,
	GEANY_FILETYPES_CSS,
	GEANY_FILETYPES_REST,
	GEANY_FILETYPES_HASKELL,
	GEANY_FILETYPES_JAVA,
	GEANY_FILETYPES_CAML,
	GEANY_FILETYPES_AS,
	GEANY_FILETYPES_R,
	GEANY_FILETYPES_DIFF,
	GEANY_FILETYPES_HTML,
	GEANY_FILETYPES_PYTHON,
	GEANY_FILETYPES_CS,
	GEANY_FILETYPES_PERL,
	GEANY_FILETYPES_VALA,
	GEANY_FILETYPES_PASCAL,
	GEANY_FILETYPES_LATEX,
	GEANY_FILETYPES_ASM,
	GEANY_FILETYPES_CONF,
	GEANY_FILETYPES_HAXE,
	GEANY_FILETYPES_CPP,
	GEANY_FILETYPES_SH,
	GEANY_FILETYPES_FORTRAN,
	GEANY_FILETYPES_SQL,
	GEANY_FILETYPES_F77,
	GEANY_FILETYPES_DOCBOOK,
	GEANY_FILETYPES_D,
	GEANY_FILETYPES_JS,
	GEANY_FILETYPES_VHDL,
	GEANY_FILETYPES_ADA,
	GEANY_FILETYPES_CMAKE,
	GEANY_FILETYPES_MARKDOWN,
	GEANY_FILETYPES_TXT2TAGS,
	GEANY_FILETYPES_ABC,
	GEANY_FILETYPES_VERILOG,
	GEANY_FILETYPES_FORTH,
	GEANY_FILETYPES_LISP,
	GEANY_FILETYPES_ERLANG,
	GEANY_FILETYPES_COBOL,
	GEANY_FILETYPES_OBJECTIVEC,
	GEANY_FILETYPES_ASCIIDOC,
	GEANY_FILETYPES_ABAQUS,
	GEANY_FILETYPES_BATCH,
	GEANY_FILETYPES_POWERSHELL,
	GEANY_FILETYPES_RUST,
	GEANY_FILETYPES_COFFEESCRIPT,
	GEANY_FILETYPES_GO,
	GEANY_FILETYPES_ZEPHIR,
	/* ^ append items here */
	GEANY_MAX_BUILT_IN_FILETYPES	/* Don't use this, use filetypes_array->len instead */
}
GeanyFiletypeID;

#ifndef GEANY_DISABLE_DEPRECATED
/* compat define - should be removed in the future */
typedef GeanyFiletypeID filetype_id GEANY_DEPRECATED_FOR(GeanyFiletypeID);
#endif /* GEANY_DISABLE_DEPRECATED */

/** @gironly
 * Filetype categories
 *
 * These are used to provide submenus for each category in the GUI */
typedef enum
{
	GEANY_FILETYPE_GROUP_NONE,
	GEANY_FILETYPE_GROUP_COMPILED,
	GEANY_FILETYPE_GROUP_SCRIPT,
	GEANY_FILETYPE_GROUP_MARKUP,
	GEANY_FILETYPE_GROUP_MISC,
	GEANY_FILETYPE_GROUP_COUNT
}
GeanyFiletypeGroupID;


/* Safe wrapper to get the id field of a possibly NULL filetype pointer.
 * This shouldn't be necessary since GeanyDocument::file_type is always non-NULL. */
#define FILETYPE_ID(filetype_ptr) \
	(((filetype_ptr) != NULL) ? (filetype_ptr)->id : GEANY_FILETYPES_NONE)

/** Represents a filetype. */
typedef struct GeanyFiletype
{
	GeanyFiletypeID	  id;				/**< Index in @ref filetypes. */
	/* Represents the TMParserType of tagmanager (see the table
	 * in src/tagmanager/tm_parser.h). */
	TMParserType	  lang;
	/** Untranslated short name, such as "C", "None".
	 * Must not be translated as it's used for hash table lookups - use
	 * filetypes_get_display_name() instead. */
	gchar			 *name;
	/** Shown in the file open dialog, such as "C source file". */
	gchar			 *title;
	gchar			 *extension;		/**< Default file extension for new files, or @c NULL. */
	gchar			**pattern;			/**< Array of filename-matching wildcard strings. */
	gchar			 *context_action_cmd;
	gchar			 *comment_open;
	gchar			 *comment_close;
	gboolean		  comment_use_indent;
	GeanyFiletypeGroupID group;
	gchar			 *error_regex_string;
	struct GeanyFiletype	 *lexer_filetype;
	gchar			 *mime_type;
	GIcon			 *icon;
	gchar			 *comment_single; /* single-line comment */
	/* filetype indent settings, -1 if not set */
	gint			  indent_type;
	gint			  indent_width;

	struct GeanyFiletypePrivate	*priv;	/* must be last, append fields before this item */
}
GeanyFiletype;

/** Wraps @ref GeanyData::filetypes_array so it can be used with C array syntax.
 * Example: filetypes[GEANY_FILETYPES_C]->name = ...;
 * @see filetypes_index(). */
#define filetypes	((GeanyFiletype **)GEANY(filetypes_array)->pdata)


GeanyFiletype *filetypes_detect_from_file(const gchar *utf8_filename);

GeanyFiletype *filetypes_lookup_by_name(const gchar *name);

GeanyFiletype *filetypes_index(gint idx);

const gchar *filetypes_get_display_name(GeanyFiletype *ft);

const GSList *filetypes_get_sorted_by_name(void);

#define GEANY_TYPE_FILETYPE (filetype_get_type())

GType filetype_get_type (void);

#ifdef GEANY_PRIVATE

extern GPtrArray *filetypes_array;

extern GSList *filetypes_by_title;


GeanyFiletype *filetypes_find(GCompareFunc predicate, gpointer user_data);


void filetypes_init(void);

void filetypes_init_types(void);

void filetypes_reload_extensions(void);

void filetypes_reload(void);


GeanyFiletype *filetypes_detect_from_document(struct GeanyDocument *doc);

GeanyFiletype *filetypes_detect_from_extension(const gchar *utf8_filename);

void filetypes_free_types(void);

void filetypes_load_config(guint ft_id, gboolean reload);

void filetypes_save_commands(GeanyFiletype *ft);

void filetypes_select_radio_item(const GeanyFiletype *ft);

GtkFileFilter *filetypes_create_file_filter(const GeanyFiletype *ft);

GtkFileFilter *filetypes_create_file_filter_all_source(void);

gboolean filetype_has_tags(GeanyFiletype *ft);

gboolean filetypes_parse_error_message(GeanyFiletype *ft, const gchar *message,
		gchar **filename, gint *line);

gboolean filetype_get_comment_open_close(const GeanyFiletype *ft, gboolean single_first,
		const gchar **co, const gchar **cc);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_FILETYPES_H */
