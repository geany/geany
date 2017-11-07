/*
 *      highlighting.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_HIGHLIGHTING_H
#define GEANY_HIGHLIGHTING_H 1

#include "filetypes.h"

#include "gtkcompat.h" /* Needed by ScintillaWidget.h */
#include "Scintilla.h" /* Needed by ScintillaWidget.h */
#include "ScintillaWidget.h" /* for ScintillaObject */


#include <glib.h>

G_BEGIN_DECLS

/** Fields representing the different attributes of a Scintilla lexer style.
 * @see Scintilla messages @c SCI_STYLEGETFORE, etc, for use with scintilla_send_message(). */
typedef struct GeanyLexerStyle
{
	gint	foreground;	/**< Foreground text colour, in @c 0xBBGGRR format. */
	gint	background;	/**< Background text colour, in @c 0xBBGGRR format. */
	gboolean bold;		/**< Bold. */
	gboolean italic;	/**< Italic. */
}
GeanyLexerStyle;


const GeanyLexerStyle *highlighting_get_style(gint ft_id, gint style_id);

void highlighting_set_styles(ScintillaObject *sci, GeanyFiletype *ft);

gboolean highlighting_is_string_style(gint lexer, gint style);
gboolean highlighting_is_comment_style(gint lexer, gint style);
gboolean highlighting_is_code_style(gint lexer, gint style);


#ifdef GEANY_PRIVATE

void highlighting_init_styles(guint filetype_idx, GKeyFile *config, GKeyFile *configh);

void highlighting_free_styles(void);

void highlighting_show_color_scheme_dialog(void);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_HIGHLIGHTING_H */
