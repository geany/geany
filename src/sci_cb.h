/*
 *      sci_cb.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef GEANY_SCI_CB_H
#define GEANY_SCI_CB_H 1


// callback func called by all editors when a signals arises
void on_editor_notification(GtkWidget* editor, gint scn, gpointer lscn, gpointer user_data);

gboolean sci_cb_start_auto_complete(ScintillaObject *sci, gint pos);

void sci_cb_get_indent(ScintillaObject *sci, gint pos, gboolean use_this_line);

void sci_cb_close_block(ScintillaObject *sci, gint pos);

void sci_cb_auto_forif(ScintillaObject *sci, gint idx);

void sci_cb_show_macro_list(ScintillaObject *sci);

gboolean sci_cb_handle_xml(ScintillaObject *sci, gchar ch);

gboolean sci_cb_handle_uri(ScintillaObject *sci, gint pos);

gboolean sci_cb_show_calltip(ScintillaObject *sci, gint pos);

void sci_cb_do_comment(gint idx);

void sci_cb_highlight_braces(ScintillaObject *sci);

void sci_cb_auto_multiline(ScintillaObject *sci, gint pos);

void sci_cb_auto_table(ScintillaObject *sci, gint pos);

void sci_cb_auto_close_bracket(ScintillaObject *sci, gint pos, gchar c);

#define sci_cb_html_entities "&nbsp; &quot; &amp; &lt; &gt;  &Uuml;&trade; &euro; &bull; &infin; &prop; &lowast;\
 &minus; &sum; &prod; &isin; &empty; &pi; &Omega; &omega; &alpha; &yuml; &yacute;\
 &divide; &oslash; &ugrave; &uacute; &ucirc; &uuml; &auml; &aring; &aelig; &ccedil;\
 &egrave; &ecirc; &euml; &igrave; &icirc; &iuml; &ntilde; &ograve; &oacute; &ocirc;\
 &otilde; &ouml; &cent; &pound; &curren; &yen; &sect; &uml; &copy; &ordf; &reg; &deg;\
 &plusmn; &sup2; &sup3; &frac34; &Agrave; &Aacute; &Acirc; &Atilde; &Auml; &Aring;\
 &AElig; &Ccedil; &Egrave; &Eacute; &Ecirc; &frac12; &frac14; &ordm; &sup1; &para;\
 &micro; &Euml; &Igrave; &Iacute; &Icirc; &Iuml; &Ntilde; &Ograve; &Oacute; &Ocirc;\
 &Otilde; &Ouml; &times; &Oslash; &Ugrave; &Uacute; &Ucirc;"

#endif
