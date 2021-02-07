/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Encapsulates ctags so it is isolated from the rest of Geany.
*/
#ifndef TM_CTAGS_H
#define TM_CTAGS_H

#include <glib.h>

#include "tm_source_file.h"

G_BEGIN_DECLS

#ifdef GEANY_PRIVATE

void tm_ctags_init(void);
void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType language, TMSourceFile *source_file);
const gchar *tm_ctags_get_lang_name(TMParserType lang);
TMParserType tm_ctags_get_named_lang(const gchar *name);
const gchar *tm_ctags_get_lang_kinds(TMParserType lang);
const gchar *tm_ctags_get_kind_name(gchar kind, TMParserType lang);
gchar tm_ctags_get_kind_from_name(const gchar *name, TMParserType lang);
guint tm_ctags_get_lang_count(void);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* TM_CTAGS_H */
