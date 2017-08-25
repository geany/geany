/*
 *      sciwrappers.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef GEANY_SCI_WRAPPERS_H
#define GEANY_SCI_WRAPPERS_H 1

#include "geany.h" /* for GEANY_DEPRECATED */
#include "gtkcompat.h" /* Needed by ScintillaWidget.h */
#include "Scintilla.h" /* Needed by ScintillaWidget.h */
#include "ScintillaWidget.h" /* for ScintillaObject */


G_BEGIN_DECLS

#ifdef GEANY_PRIVATE
# ifndef NDEBUG
#  define SSM(s, m, w, l) sci_send_message_internal(__FILE__, __LINE__, s, m, w, l)
sptr_t sci_send_message_internal (const gchar *file, guint line, ScintillaObject *sci,
	guint msg, uptr_t wparam, sptr_t lparam);
# else
#  define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)
# endif
#endif

void 				sci_set_text				(ScintillaObject *sci,  const gchar *text);
gboolean			sci_has_selection			(ScintillaObject *sci);
void 				sci_end_undo_action			(ScintillaObject *sci);
void 				sci_start_undo_action		(ScintillaObject *sci);

void				sci_set_marker_at_line		(ScintillaObject *sci, gint line_number, gint marker);
void				sci_delete_marker_at_line	(ScintillaObject *sci, gint line_number, gint marker);
gboolean 			sci_is_marker_set_at_line	(ScintillaObject *sci, gint line, gint marker);

gint 				sci_get_col_from_position	(ScintillaObject *sci, gint position);
gint 				sci_get_line_from_position	(ScintillaObject *sci, gint position);
gint 				sci_get_position_from_line	(ScintillaObject *sci, gint line);
gint 				sci_get_current_position	(ScintillaObject *sci);
void 				sci_set_current_position	(ScintillaObject *sci, gint position, gboolean scroll_to_caret);

gint				sci_get_selection_start		(ScintillaObject *sci);
gint				sci_get_selection_end		(ScintillaObject *sci);
void 				sci_replace_sel				(ScintillaObject *sci, const gchar *text);
gint				sci_get_selection_mode		(ScintillaObject *sci);
void				sci_set_selection_mode		(ScintillaObject *sci, gint mode);
void 				sci_set_selection_start		(ScintillaObject *sci, gint position);
void				sci_set_selection_end		(ScintillaObject *sci, gint position);

gint				sci_get_length				(ScintillaObject *sci);
gchar*				sci_get_contents			(ScintillaObject *sci, gint buffer_len);
gint				sci_get_selected_text_length(ScintillaObject *sci);
gchar*				sci_get_selection_contents	(ScintillaObject *sci);
gchar*				sci_get_line				(ScintillaObject *sci, gint line_num);
gint 				sci_get_line_length			(ScintillaObject *sci, gint line);
gint				sci_get_line_count			(ScintillaObject *sci);

gint				sci_get_line_end_position	(ScintillaObject *sci, gint line);

gboolean			sci_get_line_is_visible		(ScintillaObject *sci, gint line);
void				sci_ensure_line_is_visible	(ScintillaObject *sci, gint line);

gint				sci_get_tab_width			(ScintillaObject *sci);
gchar				sci_get_char_at				(ScintillaObject *sci, gint pos);

void				sci_scroll_caret			(ScintillaObject *sci);
gint				sci_find_text				(ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf);
void				sci_set_font				(ScintillaObject *sci, gint style, const gchar *font, gint size);
void				sci_goto_line				(ScintillaObject *sci, gint line, gboolean unfold);
gint				sci_get_style_at			(ScintillaObject *sci, gint position);
gchar*				sci_get_contents_range		(ScintillaObject *sci, gint start, gint end);
void				sci_insert_text				(ScintillaObject *sci, gint pos, const gchar *text);

void				sci_set_target_start		(ScintillaObject *sci, gint start);
void				sci_set_target_end			(ScintillaObject *sci, gint end);
gint				sci_replace_target			(ScintillaObject *sci, const gchar *text, gboolean regex);

gint				sci_get_lexer				(ScintillaObject *sci);
void				sci_send_command			(ScintillaObject *sci, gint cmd);

gint				sci_get_current_line		(ScintillaObject *sci);

void				sci_indicator_set			(ScintillaObject *sci, gint indic);
void				sci_indicator_clear			(ScintillaObject *sci, gint pos, gint len);

void				sci_set_line_indentation	(ScintillaObject *sci, gint line, gint indent);
gint				sci_get_line_indentation	(ScintillaObject *sci, gint line);
gint				sci_find_matching_brace		(ScintillaObject *sci, gint pos);

#ifndef GEANY_DISABLE_DEPRECATED
void				sci_get_text				(ScintillaObject *sci, gint len, gchar *text) GEANY_DEPRECATED_FOR(sci_get_contents);
void				sci_get_selected_text		(ScintillaObject *sci, gchar *text) GEANY_DEPRECATED_FOR(sci_get_selection_contents);
void				sci_get_text_range			(ScintillaObject *sci, gint start, gint end, gchar *text) GEANY_DEPRECATED_FOR(sci_get_contents_range);
#endif	/* GEANY_DISABLE_DEPRECATED */

#ifdef GEANY_PRIVATE

gchar*				sci_get_string				(ScintillaObject *sci, guint msg, gulong wParam);

void 				sci_set_line_numbers		(ScintillaObject *sci,  gboolean set);
void				sci_set_mark_long_lines		(ScintillaObject *sci,	gint type, gint column, const gchar *color);

void 				sci_add_text				(ScintillaObject *sci,  const gchar *text);
gboolean			sci_can_redo				(ScintillaObject *sci);
gboolean			sci_can_undo				(ScintillaObject *sci);
void 				sci_undo					(ScintillaObject *sci);
void 				sci_redo					(ScintillaObject *sci);
void 				sci_empty_undo_buffer		(ScintillaObject *sci);
gboolean			sci_is_modified				(ScintillaObject *sci);

void				sci_set_visible_eols		(ScintillaObject *sci, gboolean set);
void				sci_set_lines_wrapped		(ScintillaObject *sci, gboolean set);
void				sci_set_visible_white_spaces(ScintillaObject *sci, gboolean set);
void 				sci_convert_eols			(ScintillaObject *sci, gint eolmode);
gint				sci_get_eol_mode			(ScintillaObject *sci);
void 				sci_set_eol_mode			(ScintillaObject *sci, gint eolmode);
void 				sci_zoom_in					(ScintillaObject *sci);
void 				sci_zoom_out				(ScintillaObject *sci);
void 				sci_zoom_off				(ScintillaObject *sci);
void				sci_toggle_marker_at_line	(ScintillaObject *sci, gint line, gint marker);
gint				sci_marker_next				(ScintillaObject *sci, gint line, gint marker_mask, gboolean wrap);
gint				sci_marker_previous			(ScintillaObject *sci, gint line, gint marker_mask, gboolean wrap);

gint 				sci_get_position_from_col (ScintillaObject *sci, gint line, gint col);
void 				sci_set_current_line		(ScintillaObject *sci, gint line);
gint 				sci_get_cursor_virtual_space(ScintillaObject *sci);

void 				sci_cut						(ScintillaObject *sci);
void 				sci_copy					(ScintillaObject *sci);
void 				sci_paste					(ScintillaObject *sci);
void 				sci_clear					(ScintillaObject *sci);

gint				sci_get_pos_at_line_sel_start(ScintillaObject*sci, gint line);
gint				sci_get_pos_at_line_sel_end	(ScintillaObject *sci, gint line);
void				sci_set_selection			(ScintillaObject *sci, gint anchorPos, gint currentPos);

gint				sci_get_position_from_xy	(ScintillaObject *sci, gint x, gint y, gboolean nearby);

void 				sci_set_undo_collection		(ScintillaObject *sci, gboolean set);

void 				sci_toggle_fold				(ScintillaObject *sci, gint line);
gint				sci_get_fold_level			(ScintillaObject *sci, gint line);
gint				sci_get_fold_parent			(ScintillaObject *sci, gint start_line);

void 				sci_set_folding_margin_visible (ScintillaObject *sci, gboolean set);
gboolean			sci_get_fold_expanded		(ScintillaObject *sci, gint line);

void				sci_colourise				(ScintillaObject *sci, gint start, gint end);
void				sci_clear_all				(ScintillaObject *sci);
gint				sci_get_end_styled			(ScintillaObject *sci);
void				sci_set_tab_width			(ScintillaObject *sci, gint width);
void				sci_set_savepoint			(ScintillaObject *sci);
void				sci_set_indentation_guides	(ScintillaObject *sci, gint mode);
void				sci_use_popup				(ScintillaObject *sci, gboolean enable);
void				sci_goto_pos				(ScintillaObject *sci, gint pos, gboolean unfold);
void				sci_set_search_anchor		(ScintillaObject *sci);
void				sci_set_anchor				(ScintillaObject *sci, gint pos);
void				sci_scroll_columns			(ScintillaObject *sci, gint columns);
gint				sci_search_next				(ScintillaObject *sci, gint flags, const gchar *text);
gint				sci_search_prev				(ScintillaObject *sci, gint flags, const gchar *text);
void				sci_marker_delete_all		(ScintillaObject *sci, gint marker);
void				sci_set_symbol_margin		(ScintillaObject *sci, gboolean set);
void				sci_set_codepage			(ScintillaObject *sci, gint cp);
void				sci_clear_cmdkey			(ScintillaObject *sci, gint key);
void				sci_assign_cmdkey			(ScintillaObject *sci, gint key, gint command);
void				sci_selection_duplicate		(ScintillaObject *sci);
void				sci_line_duplicate			(ScintillaObject *sci);

void				sci_set_keywords			(ScintillaObject *sci, guint k, const gchar *text);
void				sci_set_lexer				(ScintillaObject *sci, guint lexer_id);
void				sci_set_readonly			(ScintillaObject *sci, gboolean readonly);

gint				sci_get_lines_selected		(ScintillaObject *sci);
gint				sci_get_first_visible_line	(ScintillaObject *sci);

void				sci_indicator_fill			(ScintillaObject *sci, gint pos, gint len);

void				sci_select_all				(ScintillaObject *sci);
gint				sci_get_line_indent_position(ScintillaObject *sci, gint line);
void				sci_set_autoc_max_height	(ScintillaObject *sci, gint val);

gint				sci_get_overtype			(ScintillaObject *sci);
void				sci_set_tab_indents			(ScintillaObject *sci, gboolean set);
void				sci_set_use_tabs			(ScintillaObject *sci, gboolean set);

void				sci_set_scrollbar_mode		(ScintillaObject *sci, gboolean visible);
void				sci_set_caret_policy_x		(ScintillaObject *sci, gint policy, gint slop);
void				sci_set_caret_policy_y		(ScintillaObject *sci, gint policy, gint slop);

void				sci_set_scroll_stop_at_last_line	(ScintillaObject *sci, gboolean set);

void				sci_cancel					(ScintillaObject *sci);

gint				sci_get_position_after		(ScintillaObject *sci, gint start);
gint				sci_word_start_position		(ScintillaObject *sci, gint position, gboolean onlyWordCharacters);
gint				sci_word_end_position		(ScintillaObject *sci, gint position, gboolean onlyWordCharacters);

void				sci_lines_join				(ScintillaObject *sci);
gint				sci_text_width				(ScintillaObject *sci, gint styleNumber, const gchar *text);

void				sci_move_selected_lines_down    (ScintillaObject *sci);
void				sci_move_selected_lines_up      (ScintillaObject *sci);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_SCI_WRAPPERS_H */
