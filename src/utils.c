/*
 *      utils.c - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */


#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include "geany.h"

#ifndef GEANY_WIN32
# include <sys/utsname.h>
#endif

#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "document.h"
#include "msgwindow.h"
#include "encodings.h"
#include "templates.h"
#include "treeviews.h"

#include "utils.h"

#include "images.c"


void utils_start_browser(gchar *uri)
{
#ifdef GEANY_WIN32
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
#else
	gchar *argv[] = { app->build_browser_cmd, uri, NULL };

	if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL))
	{
		argv[0] = "firefox";
		if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL))
		{
			argv[0] = "mozilla";
			if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL))
			{
				argv[0] = "opera";
				if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL))
				{
					argv[0] = "konqueror";
					if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL))
					{
						argv[0] = "netscape";
						g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
					}
				}
			}
		}
	}
#endif
}


/* updates the status bar */
void utils_update_statusbar(gint idx)
{
	gchar *text = (gchar*) g_malloc0(150);
	gchar *cur_tag;
	guint line, col;
	gint pos;

	if (idx == -1) idx = document_get_cur_idx();

	if (idx > -1 && IS_SCINTILLA(doc_list[idx].sci))
	{
		if (doc_list[idx].file_type->id == GEANY_FILETYPES_JAVA ||
			doc_list[idx].file_type->id == GEANY_FILETYPES_ALL)
		{
			cur_tag = g_strdup(_("unknown"));
		}
		else
		{
			utils_get_current_tag(idx, &cur_tag);
		}

		pos = sci_get_current_position(doc_list[idx].sci);
		line = sci_get_line_from_position(doc_list[idx].sci, pos);
		col = sci_get_col_from_position(doc_list[idx].sci, pos);

		g_snprintf(text, 150,
			_("%c  line: % 4d column: % 3d  selection: % 4d   %s     mode: %s     cur. function: %s"),
			(doc_list[idx].changed) ? 42 : 32,
			(line + 1), (col + 1),
			sci_get_selected_text_length(doc_list[idx].sci) - 1,
			doc_list[idx].do_overwrite ? _("OVR") : _("INS"),
			document_get_eol_mode(idx),
			cur_tag);
		gtk_statusbar_pop(GTK_STATUSBAR (app->statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR (app->statusbar), 1, text);
		g_free(cur_tag);
	}
	else
	{
		gtk_statusbar_pop(GTK_STATUSBAR (app->statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR (app->statusbar), 1, text);
	}

	g_free(text);
}



void utils_update_popup_reundo_items(gint index)
{
	gboolean enable_undo;
	gboolean enable_redo;

	if (index == -1)
	{
		enable_undo = FALSE;
		enable_redo = FALSE;
	}
	else
	{
		enable_undo = sci_can_undo(doc_list[index].sci);
		enable_redo = sci_can_redo(doc_list[index].sci);
	}

	// index 0 is the popup menu, 1 is the menubar
	gtk_widget_set_sensitive(app->undo_items[0], enable_undo);
	gtk_widget_set_sensitive(app->undo_items[1], enable_undo);

	gtk_widget_set_sensitive(app->redo_items[0], enable_redo);
	gtk_widget_set_sensitive(app->redo_items[1], enable_redo);
}


void utils_update_popup_copy_items(gint index)
{
	gboolean enable;
	guint i;

	if (index == -1) enable = FALSE;
	else enable = sci_can_copy(doc_list[index].sci);

	for(i = 0; i < (sizeof(app->popup_items)/sizeof(GtkWidget*)); i++)
		gtk_widget_set_sensitive(app->popup_items[i], enable);
}


void utils_update_menu_copy_items(gint idx)
{
	gboolean enable;
	guint i;

	if (idx == -1) enable = FALSE;
	else enable = sci_can_copy(doc_list[idx].sci);

	for(i = 0; i < (sizeof(app->menu_copy_items)/sizeof(GtkWidget*)); i++)
		gtk_widget_set_sensitive(app->menu_copy_items[i], enable);
}


void utils_update_insert_include_item(gint idx, gint item)
{
	gboolean enable = FALSE;

	if (idx == -1) enable = FALSE;
	else if (doc_list[idx].file_type->id == GEANY_FILETYPES_C ||
			 doc_list[idx].file_type->id == GEANY_FILETYPES_CPP)
	{
		enable = TRUE;
	}
	gtk_widget_set_sensitive(app->menu_insert_include_item[item], enable);
}


void utils_update_popup_goto_items(gboolean enable)
{
	gtk_widget_set_sensitive(app->popup_goto_items[0], enable);
	gtk_widget_set_sensitive(app->popup_goto_items[1], enable);
	gtk_widget_set_sensitive(app->popup_goto_items[2], enable);
}


void utils_save_buttons_toggle(gboolean enable)
{
	gtk_widget_set_sensitive(app->save_buttons[0], enable);
	gtk_widget_set_sensitive(app->save_buttons[1], enable);
}


void utils_close_buttons_toggle(void)
{
	guint i;
	gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) ? TRUE : FALSE;

	for(i = 0; i < (sizeof(app->sensitive_buttons)/sizeof(GtkWidget*)); i++)
			gtk_widget_set_sensitive(app->sensitive_buttons[i], enable);
}


GtkFileFilter *utils_create_file_filter(filetype *ft)
{
	GtkFileFilter *new_filter;
	gint i;

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, ft->title);

	// (GEANY_FILETYPES_MAX_PATTERNS - 1) because the last field in pattern is NULL
	for (i = 0; i < (GEANY_MAX_PATTERNS - 1) && ft->pattern[i]; i++)
	{
		gtk_file_filter_add_pattern(new_filter, ft->pattern[i]);
	}

	return new_filter;
}


int utils_get_new_sci_number(void)
{
	static guint count = 0;
	return ++count;	// id 0 created on startup
}


/* taken from anjuta, to determine the EOL mode of the file */
gint utils_get_line_endings(gchar* buffer, glong size)
{
	gint i;
	guint cr, lf, crlf, max_mode;
	gint mode;

	cr = lf = crlf = 0;

	for ( i = 0; i < size ; i++ )
	{
		if ( buffer[i] == 0x0a ){
			// LF
			lf++;
		}
		else if ( buffer[i] == 0x0d )
		{
			if (i >= (size-1))
			{
				// Last char
				// CR
				cr++;
			} else {
				if (buffer[i+1] != 0x0a)
				{
					// CR
					cr++;
				}
				else
				{
					// CRLF
					crlf++;
				}
				i++;
			}
		}
	}

	/* Vote for the maximum */
	mode = SC_EOL_LF;
	max_mode = lf;
	if (crlf > max_mode) {
		mode = SC_EOL_CRLF;
		max_mode = crlf;
	}
	if (cr > max_mode) {
		mode = SC_EOL_CR;
		max_mode = cr;
	}
	//geany_debug("EOL chars: LF = %d, CR = %d, CRLF = %d", lf, cr, crlf);


	return mode;
}


gboolean utils_isbrace(gchar c)
{
	switch (c)
	{
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':	return TRUE;
		//case '<':
		//case '>':	return TRUE;
		default:	return FALSE;
	}
}



void utils_set_font(void)
{
	gint i, size;
	gchar *fname;
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_from_string(app->editor_font);

	fname = g_strdup_printf("!%s", pango_font_description_get_family(font_desc));
	size = pango_font_description_get_size(font_desc) / PANGO_SCALE;

	/* We copy the current style, and update the font in all open tabs. */
	for(i = 0; i < GEANY_MAX_OPEN_FILES; i++)
	{
		if (doc_list[i].sci)
		{
			document_set_font(doc_list[i].sci, fname, size);
		}
	}
	pango_font_description_free(font_desc);

	msgwin_status_add(_("Font updated (%s)."), app->editor_font);
	g_free(fname);
}


/* This sets the window title according to the current filename. */
void utils_set_window_title(gint index)
{
	gchar *title;

	if (index >= 0)
	{
		title = g_strdup_printf ("%s: %s %s",
				PACKAGE,
				(doc_list[index].file_name != NULL) ? g_filename_to_utf8(doc_list[index].file_name, -1, NULL, NULL, NULL) : _("untitled"),
				doc_list[index].changed ? _("(Unsaved)") : "");
		gtk_window_set_title(GTK_WINDOW(app->window), title);
		g_free(title);
	}
	else
		gtk_window_set_title(GTK_WINDOW(app->window), PACKAGE);
}


const GList *utils_get_tag_list(gint idx, guint tag_types)
{
	static GList *tag_names = NULL;

	if (doc_list[idx].sci && doc_list[idx].tm_file && doc_list[idx].tm_file->tags_array)
	{
		TMTag *tag;
		guint i;
		GeanySymbol *symbol;

		if (tag_names)
		{
			GList *tmp;
			for (tmp = tag_names; tmp; tmp = g_list_next(tmp))
			{
				g_free(((GeanySymbol*)tmp->data)->str);
				g_free(tmp->data);
			}
			g_list_free(tag_names);
			tag_names = NULL;
		}

		for (i = 0; i < (doc_list[idx].tm_file)->tags_array->len; ++i)
		{
			tag = TM_TAG((doc_list[idx].tm_file)->tags_array->pdata[i]);
			if (tag == NULL)
				return NULL;
			//geany_debug("%s: %d", doc_list[idx].file_name, tag->type);
			if (tag->type & tag_types)
			{
				if ((tag->atts.entry.scope != NULL) && isalpha(tag->atts.entry.scope[0]))
				{
					symbol = g_new(GeanySymbol, 1);
					symbol->str = g_strdup_printf("%s::%s [%ld]", tag->atts.entry.scope, tag->name, tag->atts.entry.line);
					symbol->type = tag->type;
					tag_names = g_list_prepend(tag_names, symbol);
				}
				else
				{
					symbol = g_new(GeanySymbol, 1);
					symbol->str = g_strdup_printf("%s [%ld]", tag->name, tag->atts.entry.line);
					symbol->type = tag->type;
					tag_names = g_list_prepend(tag_names, symbol);
				}
			}
		}
		tag_names = g_list_sort(tag_names, (GCompareFunc) utils_compare_symbol);
		return tag_names;
	}
	else
		return NULL;
}

/* returns the line of the given tag */
gint utils_get_local_tag(gint idx, const gchar *qual_name)
{
	guint line;
	gchar *spos;
	gchar *epos;

	g_return_val_if_fail((doc_list[idx].sci && qual_name), -1);

	spos = strchr(qual_name, '[');
	if (spos)
	{
		epos = strchr(spos+1, ']');
		if (epos)
		{
			*epos = '\0';
			line = atol(spos + 1);
			*epos = ']';
			if (line > 0)
			{
				return line;
			}
		}
	}
	return -1;
}


gboolean utils_goto_workspace_tag(const gchar *file, gint line)
{
	gint page_num;
	gint file_idx = document_find_by_filename(file);
	gboolean ret;

	page_num = gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), GTK_WIDGET(doc_list[file_idx].sci));

	ret = utils_goto_line(file_idx, line);

	// finally switch to the page
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page_num);

	return ret;
}


gboolean utils_goto_line(gint idx, gint line)
{
	line--;	// the User counts lines from 1, we begin at 0 so bring the User line to our one

	if (idx == -1 || line < 0)
	{
		return FALSE;
	}

	// mark the tag and ensure that we have arround 5 lines visible around the mark
	sci_goto_line(doc_list[idx].sci, line - 5);
	sci_goto_line(doc_list[idx].sci, line + 5);
	sci_goto_line(doc_list[idx].sci, line);
	sci_marker_delete_all(doc_list[idx].sci, 0);
	sci_set_marker_at_line(doc_list[idx].sci, line, TRUE, 0);

	return TRUE;
}


GdkPixbuf *utils_new_pixbuf_from_inline(gint img, gboolean small_img)
{
	switch(img)
	{
		case GEANY_IMAGE_SMALL_CROSS: return gdk_pixbuf_new_from_inline(-1, close_small_inline, FALSE, NULL); break;
		case GEANY_IMAGE_LOGO: return gdk_pixbuf_new_from_inline(-1, aladin_inline, FALSE, NULL); break;
		case GEANY_IMAGE_SAVE_ALL:
		{
			if ((app->toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
			{
				return gdk_pixbuf_scale_simple(gdk_pixbuf_new_from_inline(-1, save_all_inline, FALSE, NULL),
                                             16, 16, GDK_INTERP_HYPER);
			}
			else
			{
				return gdk_pixbuf_new_from_inline(-1, save_all_inline, FALSE, NULL);
			}
			break;
		}
		case GEANY_IMAGE_NEW_ARROW:
		{
			if ((app->toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
			{
				return gdk_pixbuf_scale_simple(gdk_pixbuf_new_from_inline(-1, newfile_inline, FALSE, NULL),
                                             16, 16, GDK_INTERP_HYPER);
			}
			else
			{
				return gdk_pixbuf_new_from_inline(-1, newfile_inline, FALSE, NULL);
			}
			break;
		}
		default: return NULL;
	}

	//return gtk_image_new_from_pixbuf(pixbuf);
}


void utils_update_toolbar_icons(GtkIconSize size)
{
	GtkWidget *button_image, *widget, *oldwidget;

	// destroy old widget
	widget = lookup_widget(app->window, "toolbutton22");
	oldwidget = gtk_tool_button_get_icon_widget(GTK_TOOL_BUTTON(widget));
	if (GTK_IS_WIDGET(oldwidget)) gtk_widget_destroy(oldwidget);
	// create new widget
	button_image = utils_new_image_from_inline(GEANY_IMAGE_SAVE_ALL, FALSE);
	gtk_widget_show(button_image);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(widget), button_image);

	// destroy old widget
	oldwidget = NULL;
	widget = lookup_widget(app->window, "toolbutton_new");
	g_object_get(G_OBJECT(widget), "image", &oldwidget, NULL);
	if (GTK_IS_WIDGET(oldwidget)) gtk_widget_destroy(oldwidget);
	// create new widget
	button_image = utils_new_image_from_inline(GEANY_IMAGE_NEW_ARROW, FALSE);
	gtk_widget_show(button_image);
	g_object_set(G_OBJECT(widget), "image", button_image, NULL);

	gtk_toolbar_set_icon_size(GTK_TOOLBAR(app->toolbar), size);
}


GtkWidget *utils_new_image_from_inline(gint img, gboolean small_img)
{
	return gtk_image_new_from_pixbuf(utils_new_pixbuf_from_inline(img, small_img));
}


gint utils_write_file(const gchar *filename, const gchar *text)
{
	FILE *fp;
	gint bytes_written, len;

	if (filename == NULL)
	{
		return ENOENT;
	}

	len = strlen(text);

	fp = fopen(filename, "w");
	if (fp != NULL)
	{
		bytes_written = fwrite(text, sizeof (gchar), len, fp);
		fclose(fp);

		if (len != bytes_written)
		{
			return EIO;
		}
	}
	else
	{
		return errno;
	}
	return 0;
}


void utils_show_indention_guides(void)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_indentionguides(doc_list[idx].sci, app->show_indent_guide);
	}
}


void utils_show_white_space(void)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_visible_white_spaces(doc_list[idx].sci, app->show_white_space);
	}
}


void utils_line_breaking(void)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		//sci_set_lines_wrapped(doc_list[idx].sci, app->line_breaking);
	}
}


void utils_show_line_endings(void)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_visible_eols(doc_list[idx].sci, app->show_line_endings);
	}
}


void utils_show_markers_margin(void)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_symbol_margin(doc_list[idx].sci, app->show_markers_margin);
	}
}


void utils_set_fullscreen(void)
{
	if (app->fullscreen)
	{
		gtk_window_fullscreen(GTK_WINDOW(app->window));
	}
	else
	{
		gtk_window_unfullscreen(GTK_WINDOW(app->window));
	}
}


void utils_update_tag_list(gint idx, gboolean update)
{
	GList *tmp;
	const GList *tags;

	if (gtk_bin_get_child(GTK_BIN(app->tagbar)))
		gtk_container_remove(GTK_CONTAINER(app->tagbar), gtk_bin_get_child(GTK_BIN(app->tagbar)));

	if (app->default_tag_tree == NULL)
	{
		GtkTreeIter iter;
		GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);
		app->default_tag_tree = gtk_tree_view_new();
		treeviews_prepare_taglist(app->default_tag_tree, store);
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, 0, _("No tags found"), -1);
		gtk_widget_show(app->default_tag_tree);
		g_object_ref((gpointer)app->default_tag_tree);	// to hold it after removing
	}

	// make all inactive, because there is no more tab left, or something strange occured
	if (idx == -1 || ! app->treeview_symbol_visible || ! doc_list[idx].file_type->has_tags)
	{
		gtk_widget_set_sensitive(app->tagbar, FALSE);
		gtk_container_add(GTK_CONTAINER(app->tagbar), app->default_tag_tree);
		return;
	}
	//geany_debug("before %d", update);

	if (update)
	{	// updating the tag list in the left tag window
		if (doc_list[idx].tag_tree == NULL)
		{
			doc_list[idx].tag_store = gtk_tree_store_new(1, G_TYPE_STRING);
			doc_list[idx].tag_tree = gtk_tree_view_new();
			treeviews_prepare_taglist(doc_list[idx].tag_tree, doc_list[idx].tag_store);
			gtk_widget_show(doc_list[idx].tag_tree);
			g_object_ref((gpointer)doc_list[idx].tag_tree);	// to hold it after removing
		}

		tags = utils_get_tag_list(idx, tm_tag_max_t);
		if (doc_list[idx].tm_file != NULL && tags != NULL)
		{
			GtkTreeIter iter;
			GtkTreeModel *model;

			gtk_tree_store_clear(doc_list[idx].tag_store);
			// unref the store to speed up the filling(from TreeView Tutorial)
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(doc_list[idx].tag_tree));
			g_object_ref(model); // Make sure the model stays with us after the tree view unrefs it
			gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), NULL); // Detach model from view

			treeviews_init_tag_list(idx);
			for (tmp = (GList*)tags; tmp; tmp = g_list_next(tmp))
			{
				switch (((GeanySymbol*)tmp->data)->type)
				{
					case tm_tag_prototype_t:
					case tm_tag_function_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_function));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_macro_t:
					case tm_tag_macro_with_arg_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_macro));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_member_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_member));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_typedef_t:
					case tm_tag_struct_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_struct));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_variable_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_variable));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_namespace_t:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_namespace));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					default:
					{
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_other));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
					}
				}
			}
			gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), model); // Re-attach model to view
			g_object_unref(model);
			gtk_tree_view_expand_all(GTK_TREE_VIEW(doc_list[idx].tag_tree));

			gtk_widget_set_sensitive(app->tagbar, TRUE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), doc_list[idx].tag_tree);
			/// TODO why I have to do this here?
			g_object_ref((gpointer)doc_list[idx].tag_tree);
		}
		else
		{	// tags == NULL
			gtk_widget_set_sensitive(app->tagbar, FALSE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), app->default_tag_tree);
		}
	}
	else
	{	// update == FALSE
		gtk_widget_set_sensitive(app->tagbar, TRUE);
		gtk_container_add(GTK_CONTAINER(app->tagbar), doc_list[idx].tag_tree);
	}

}


gchar *utils_convert_to_utf8_from_charset(const gchar *buffer, gsize size, const gchar *charset)
{
	gchar *utf8_content = NULL;
	GError *conv_error = NULL;
	gchar* converted_contents = NULL;
	gsize bytes_written;

	g_return_val_if_fail(buffer != NULL, NULL);

	geany_debug("Trying to convert from %s to UTF-8", charset);
	converted_contents = g_convert(buffer, size, "UTF-8", charset, NULL,
									&bytes_written, &conv_error);

	if ((conv_error != NULL) ||
	    !g_utf8_validate(converted_contents, bytes_written, NULL))
	{
		geany_debug("Couldn't convert from %s to UTF-8.", charset);
		if (converted_contents != NULL) g_free(converted_contents);

		if (conv_error != NULL)
		{
			g_error_free(conv_error);
			conv_error = NULL;
		}

		utf8_content = NULL;
	}
	else
	{
		geany_debug("Converted from %s to UTF-8.", charset);
		utf8_content = converted_contents;
	}
	return utf8_content;
}


gchar *utils_convert_to_utf8(const gchar *buffer, gsize size, gchar **used_encoding)
{
	GList *encodings = NULL;
	GList *start;
	gchar *locale_charset;
	GList *encoding_strings;

	g_return_val_if_fail(!g_utf8_validate(buffer, size, NULL),
			g_strndup(buffer, size < 0 ? strlen(buffer) : size));

	encoding_strings = utils_glist_from_string("UTF-8 ISO-8859-1 ISO-8859-15");
	encodings = encoding_get_encodings(encoding_strings);
	utils_glist_strings_free(encoding_strings);

	if (g_get_charset((const gchar**)&locale_charset) == FALSE)
	{
		const GeanyEncoding *locale_encoding;

		// not using a UTF-8 locale, so try converting from that first
		if (locale_charset != NULL)
		{
			if (strcmp(locale_charset, "ANSI_X3.4-1968") == 0)
			{
				/// TODO very dirty quick hack, if LC=C then all oes wrong
				locale_charset = g_strdup("ISO-8859-15");
				locale_encoding = encoding_get_from_charset(locale_charset);
				g_free(locale_charset);
			}
			else
				locale_encoding = encoding_get_from_charset(locale_charset);

			encodings = g_list_prepend(encodings,
						(gpointer) locale_encoding);
			geany_debug("Current charset = %s", locale_charset);
			//geany_debug("Current encoding = %s", locale_encoding->name);
		}
	}

	start = encodings;

	while (encodings != NULL)
	{
		GeanyEncoding *enc;
		const gchar *charset;
		gchar *utf8_content;

		enc = (GeanyEncoding *)encodings->data;

		charset = encoding_get_charset(enc);
		geany_debug("Trying to convert %d bytes of data into UTF-8.", size);
		fflush(stdout);
		utf8_content = utils_convert_to_utf8_from_charset(buffer, size, charset);

		if (utf8_content != NULL)
		{
			if (used_encoding != NULL)
			{
				if (*used_encoding != NULL)
				{
					g_free(*used_encoding);
					geany_debug("%s:%d", __FILE__, __LINE__);
				}
				*used_encoding = g_strdup(charset);
			}
			return utf8_content;
		}
		encodings = encodings->next;
	}

	g_list_free(start);

	return NULL;
}


gchar *utils_convert_to_utf8_simple(const gchar *str)
{
	GError *error = NULL;
	gchar *utf8_msg_string = NULL;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(strlen(str) > 0, NULL);

	if (g_utf8_validate(str, -1, NULL))
	{
		utf8_msg_string = g_strdup(str);
	}
	else
	{
		gsize rbytes, wbytes;
		utf8_msg_string = g_locale_to_utf8 (str, -1, &rbytes, &wbytes, &error);
		geany_debug("%d\t%d", rbytes, wbytes);
		if (error != NULL) {
			g_warning ("g_locale_to_utf8 failed: %s\n", error->message);
			g_error_free(error);
			g_free(utf8_msg_string);
			return NULL;
		}
	}
	return utf8_msg_string;
}


/**
 * (stolen from anjuta and modified)
 * Search backward through size bytes looking for a '<', then return the tag if any
 * @return The tag name
 */
gchar *utils_find_open_xml_tag(const gchar sel[], gint size, gboolean check_tag)
{
	// 40 chars per tag should be enough, or not?
	gint i = 0, max_tag_size = 40;
	gchar *result = g_malloc(max_tag_size);
	const gchar *begin, *cur;

	if (size < 3) {
		// Smallest tag is "<p>" which is 3 characters
		return result;
	}
	begin = &sel[0];
	if (check_tag)
		cur = &sel[size - 3];
	else
		cur = &sel[size - 1];

	cur--; // Skip past the >
	while (cur > begin)
	{
		if (*cur == '<') break;
		else if (! check_tag && *cur == '>') break;
		--cur;
	}

	if (*cur == '<')
	{
		cur++;
		while((strchr(":_-.", *cur) || isalnum(*cur)) && i < (max_tag_size - 1))
		{
			result[i++] = *cur;
			cur++;
		}
	}

	result[i] = '\0';
	// Return the tag name or ""
	return result;
}


gboolean utils_check_disk_status(gint idx, const gboolean force)
{
#ifndef GEANY_WIN32
	struct stat st;
	time_t t;
	gchar *buff;

	if (idx == -1) return FALSE;

	t = time(NULL);

	if (doc_list[idx].last_check > (t - 30)) return TRUE;

	if (stat(doc_list[idx].file_name, &st) != 0) return TRUE;

	if (doc_list[idx].mtime > t || st.st_mtime > t)
	{
		/* Something is wrong with the time stamps. They are refering to the
		 * future or the system clock is wrong. */
		g_warning("Something is wrong with the time stamps.");
		return TRUE;
	}
/*	geany_debug("check_disk: %d\t%d\t%d (%s)",
		(int)doc_list[idx].mtime, (int)st.st_mtime, (int)st.st_mtime - (int)doc_list[idx].mtime, doc_list[idx].file_name);
*/
	if (doc_list[idx].mtime < st.st_mtime)
	{
		if (force)
		{
			document_open_file(idx, NULL, 0, doc_list[idx].readonly);
		}
		else
		{
			buff = g_strdup_printf(_
						 ("The file '%s' on the disk is more recent than\n"
						  "the current buffer.\nDo you want to reload it?"), g_path_get_basename(doc_list[idx].file_name));


			if (dialogs_show_reload_warning(buff))
			{
				document_open_file(idx, NULL, 0, doc_list[idx].readonly);
				doc_list[idx].last_check = t;
			}
			else
				doc_list[idx].mtime = t;

			g_free(buff);
			return FALSE;
		}
	}
#endif
	return TRUE;
}


gint utils_get_current_tag(gint idx, gchar **tagname)
{
	gint tag_line;
	gint pos;
	gint line;
	gint fold_level;
	gint start, end;
	gint tmp;
	const GList *tags;

	pos = sci_get_current_position(doc_list[idx].sci);
	line = sci_get_line_from_position(doc_list[idx].sci, pos);

	tmp = line + 1;

	fold_level = sci_get_fold_level(doc_list[idx].sci, line);
	if ((fold_level & 0xFF) != 0)
	{
		while((fold_level & 0x10FF) != 0x1000 && line >= 0)
			fold_level = sci_get_fold_level(doc_list[idx].sci, --line);

		// look first in the tag list
		tags = utils_get_tag_list(idx, tm_tag_max_t);
		for (; tags; tags = g_list_next(tags))
		{
			tag_line = utils_get_local_tag(idx, tags->data);
			if (line == (tag_line - 2))
			{
				*tagname = g_strdup(strtok(tags->data, " "));
				return tag_line;
			}
		}

		start = sci_get_position_from_line(doc_list[idx].sci, line + 1);
		while (sci_get_style_at(doc_list[idx].sci, start) != SCE_C_IDENTIFIER) start++;
		end = start;
		while (sci_get_style_at(doc_list[idx].sci, end) == SCE_C_IDENTIFIER ||
					sci_get_char_at(doc_list[idx].sci, end) == ':')	end++;

		*tagname = g_malloc(end - start + 1);
		sci_get_text_range(doc_list[idx].sci, start, end, *tagname);

		// stimmt die 2 IMMER?
		return line + 2;
	}

	*tagname = g_strdup(_("unknown"));
	return -1;
}


void utils_find_current_word(ScintillaObject *sci, gint pos, gchar *word)
{
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint startword = pos - line_start;
	gint endword = pos - line_start;
	gchar *chunk = g_malloc(sci_get_line_length(sci, line) + 1);

	word[0] = '\0';
	sci_get_line(sci, line, chunk);
	chunk[sci_get_line_length(sci, line)] = '\0';

	while (startword > 0 && strchr(GEANY_WORDCHARS, chunk[startword - 1]))
		startword--;
	while (chunk[endword] && strchr(GEANY_WORDCHARS, chunk[endword]))
		endword++;
	if(startword == endword)
		return;

	chunk[endword] = '\0';

	strncpy(word, chunk + startword, MAX(endword - startword + 1, sizeof word));
	g_free(chunk);
}


/* returns the end-of-line character(s) length of the specified editor */
gint utils_get_eol_char_len(gint idx)
{
	if (idx == -1) return 0;

	switch (sci_get_eol_mode(doc_list[idx].sci))
	{
		case SC_EOL_CRLF: return 2; break;
		default: return 1; break;
	}
}


/* returns the end-of-line character(s) of the specified editor */
gchar *utils_get_eol_char(gint idx)
{
	if (idx == -1) return '\0';

	switch (sci_get_eol_mode(doc_list[idx].sci))
	{
		case SC_EOL_CRLF: return "\r\n"; break;
		case SC_EOL_CR: return "\r"; break;
		case SC_EOL_LF:
		default: return "\n"; break;
	}
}


/* mainly debug function, to get TRUE or FALSE as ascii from a gboolean */
gchar *utils_btoa(gboolean sbool)
{
	return (sbool) ? "TRUE" : "FALSE";
}


gboolean utils_atob(const gchar *str)
{
	if (str == NULL) return FALSE;
	else if (strcasecmp(str, "TRUE")) return FALSE;
	else return TRUE;
}


/* replaces all occurrences of c in source with d,
 * source must be null-terminated */
void utils_replace_c(gchar *source, gchar c, gchar d)
{
	gint i, len = strlen(source);
	for (i = 0; i < len; i++)
	{
		if (source[i] == c)
		{
			source[i] = d;
		}
	}
}


/* (stolen from bluefish, thanks)
 * Returns number of characters, lines and words in the supplied gchar*.
 * Handles UTF-8 correctly. Input must be properly encoded UTF-8.
 * Words are defined as any characters grouped, separated with spaces.
 */
void utils_wordcount(gchar *text, guint *chars, guint *lines, guint *words)
{
	guint in_word = 0;
	gunichar utext;

	if (!text) return; // politely refuse to operate on NULL

	*chars = *words = *lines = 0;
	while (*text != '\0')
	{
		(*chars)++;

		switch (*text)
		{
			case '\n':
				(*lines)++;
			case '\r':
			case '\f':
			case '\t':
			case ' ':
			case '\v':
				mb_word_separator:
				if (in_word)
				{
					in_word = 0;
					(*words)++;
				}
				break;
			default:
				utext = g_utf8_get_char_validated(text, 2); // This might be an utf-8 char
				if (g_unichar_isspace(utext)) // Unicode encoded space?
					goto mb_word_separator;
				if (g_unichar_isgraph(utext)) // Is this something printable?
					in_word = 1;
				break;
		}
		text = g_utf8_next_char(text); // Even if the current char is 2 bytes, this will iterate correctly.
	}

	// Capture last word, if there's no whitespace at the end of the file.
	if (in_word) (*words)++;
	// We start counting line numbers from 1
	if (*chars > 0) (*lines)++;
}


/* currently unused */
gboolean utils_is_absolute_path(const gchar *path)
{
	if (! path || *path == '\0')
		return FALSE;
#ifdef GEANY_WIN32
	if (path[0] == '\\' || path[1] == ':')
		return TRUE;
#else
	if (path[0] == '/')
		return TRUE;
#endif

	return FALSE;
}


void utils_ensure_final_newline(gint idx)
{
	gint max_lines = sci_get_line_count(doc_list[idx].sci);
	gboolean append_newline = (max_lines == 1);
	gint end_document = sci_get_position_from_line(doc_list[idx].sci, max_lines);

	if (max_lines > 1)
	{
		append_newline = end_document > sci_get_position_from_line(doc_list[idx].sci, max_lines - 1);
	}
	if (append_newline)
	{
		const gchar *eol = "\n";
		switch (sci_get_eol_mode(doc_list[idx].sci))
		{
			case SC_EOL_CRLF:
				eol = "\r\n";
				break;
			case SC_EOL_CR:
				eol = "\r";
				break;
		}
		sci_insert_text(doc_list[idx].sci, end_document, eol);
	}
}


void utils_strip_trailing_spaces(gint idx)
{
	gint max_lines = sci_get_line_count(doc_list[idx].sci);
	gint line;

	for (line = 0; line < max_lines; line++)
	{
		gint line_start = sci_get_position_from_line(doc_list[idx].sci, line);
		gint line_end = sci_get_line_end_from_position(doc_list[idx].sci, line);
		gint i = line_end - 1;
		gchar ch = sci_get_char_at(doc_list[idx].sci, i);

		while ((i >= line_start) && ((ch == ' ') || (ch == '\t')))
		{
			i--;
			ch = sci_get_char_at(doc_list[idx].sci, i);
		}
		if (i < (line_end-1))
		{
			sci_target_start(doc_list[idx].sci, i + 1);
			sci_target_end(doc_list[idx].sci, line_end);
			sci_target_replace(doc_list[idx].sci, "");
		}
	}
}


gdouble utils_scale_round (gdouble val, gdouble factor)
{
	//val = floor(val * factor + 0.5);
	val = floor(val);
	val = MAX(val, 0);
	val = MIN(val, factor);

	return val;
}


void utils_widget_show_hide(GtkWidget *widget, gboolean show)
{
	if (show)
	{
		gtk_widget_show(widget);
	}
	else
	{
		gtk_widget_hide(widget);
	}
}


void utils_build_show_hide(gint idx)
{
	gboolean is_header = FALSE;
	gchar *ext = NULL;
	static gboolean button_is_html = FALSE;
	static GtkTooltips *tooltips = NULL;

	if (doc_list[idx].file_name)
	{
		ext = strrchr(doc_list[idx].file_name, '.');
	}

	if (! ext || utils_strcmp(ext + 1, "h") || utils_strcmp(ext + 1, "hpp"))
	{
		is_header = TRUE;
	}

	if (tooltips == NULL)
	{
		tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
	}

	if (button_is_html)
	{
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(app->compile_button), GTK_STOCK_CONVERT);
		gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(app->compile_button), tooltips,
										_("Compile the current file"), NULL);
		button_is_html = FALSE;
	}

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_C:	// intended fallthrough, C and C++ behave equal
		case GEANY_FILETYPES_CPP:
		{
			gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), TRUE);
			gtk_widget_set_sensitive(app->compile_button, TRUE);
			gtk_widget_set_sensitive(app->build_menu_item_link, TRUE);
			if (! is_header) break;
		}
		case GEANY_FILETYPES_JAVA:
		{
			gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), TRUE);
			gtk_widget_set_sensitive(app->compile_button, TRUE);
			gtk_widget_set_sensitive(app->build_menu_item_link, FALSE);
			if (! is_header) break;
		}
		case GEANY_FILETYPES_XML:	// intended fallthrough, HTML and XML behave equal
		case GEANY_FILETYPES_PHP:
		{
			gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), FALSE);
			gtk_widget_set_sensitive(app->compile_button, TRUE);
			//gtk_widget_set_sensitive(app->build_menu_item_link, FALSE);

			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(app->compile_button), GTK_STOCK_FIND);
			gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(app->compile_button), tooltips,
									_("View the current file in a browser"), NULL);

			button_is_html = TRUE;

			if (! is_header) break;
		}
		case GEANY_FILETYPES_PASCAL:
		{
			gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), TRUE);
			gtk_widget_set_sensitive(app->compile_button, TRUE);
			gtk_widget_set_sensitive(app->build_menu_item_link, FALSE);
			if (! is_header) break;
		}
		default:
		{
			gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), FALSE);
			gtk_widget_set_sensitive(app->compile_button, FALSE);
			gtk_widget_set_sensitive(app->build_menu_item_link, FALSE);
			break;
		}
	}
}


/* (taken from libexo from os-cillation)
 * NULL-safe string comparison. Returns TRUE if both a and b are
 * NULL or if a and b refer to valid strings which are equal.
 */
gboolean utils_strcmp(const gchar *a, const gchar *b)
{
	if (a == NULL && b == NULL) return TRUE;
	else if (a == NULL || b == NULL) return FALSE;

	while (*a == *b++)
		if (*a++ == '\0')
			return TRUE;

	return FALSE;
}


/* removes the extension from filename and return the result in
 * a newly allocated string */
gchar *utils_remove_ext_from_filename(const gchar *filename)
{
	gchar *result = g_malloc0(strlen(filename));
	gchar *last_dot = strrchr(filename, '.');
	gint i = 0;

	if (! last_dot) return g_strdup(filename);

	while ((filename + i) != last_dot)
	{
		result[i] = filename[i];
		i++;
	}

	return result;
}


/* Finds a corresponding matching brace to the given pos
 * (this is taken from Scintilla Editor.cxx,
 * fit to work with sci_cb_close_block) */
gint utils_brace_match(ScintillaObject *sci, gint pos)
{
	gchar chBrace = sci_get_char_at(sci, pos);
	gchar chSeek = utils_brace_opposite(chBrace);
	gint direction, styBrace, depth = 1;

	if (chSeek == '\0') return -1;
	styBrace = sci_get_style_at(sci, pos);
	direction = -1;

	if (chBrace == '(' || chBrace == '[' || chBrace == '{' || chBrace == '<')
		direction = 1;

	pos = pos + direction;
	while ((pos >= 0) && (pos < sci_get_length(sci)))
	{
		gchar chAtPos = sci_get_char_at(sci, pos - 1);
		gint styAtPos = sci_get_style_at(sci, pos);

		if ((pos > sci_get_end_styled(sci)) || (styAtPos == styBrace))
		{
			if (chAtPos == chBrace)
				depth++;
			if (chAtPos == chSeek)
				depth--;
			if (depth == 0)
				return pos;
		}
		pos = pos + direction;
	}
	return - 1;
}


gchar utils_brace_opposite(gchar ch)
{
	switch (ch)
	{
		case '(': return ')';
		case ')': return '(';
		case '[': return ']';
		case ']': return '[';
		case '{': return '}';
		case '}': return '{';
		case '<': return '>';
		case '>': return '<';
		default: return '\0';
	}
}


void utils_replace_tabs(gint idx)
{
	gint i, len, j = 0, tabs_amount = 0;
	gint tab_w = sci_get_tab_width(doc_list[idx].sci);
	gchar *data, replacement[tab_w + 1];
	gchar *text;

	// get the text
	len = sci_get_length(doc_list[idx].sci) + 1;
	data = g_malloc(len);
	sci_get_text(doc_list[idx].sci, len, data);

	// prepare the spaces with the width of a tab
	for (i = 0; i < tab_w; i++) replacement[i] = 32;
	replacement[tab_w] = 0;

	for (i = 0; i < len; i++) if (data[i] == 9) tabs_amount++;

	text = g_malloc(len + (tabs_amount * (tab_w - 1)));

	for (i = 0; i < len; i++)
	{
		if (data[i] == 9)
		{
			text[j++] = 32;
			text[j++] = 32;
			text[j++] = 32;
			text[j++] = 32;
		}
		else
		{
			text[j++] = data[i];
		}
	}
	geany_debug("tabs_amount: %d, len: %d, %d == %d", tabs_amount, len, len + (tabs_amount * (tab_w - 1)), j);
	sci_set_text(doc_list[idx].sci, text);

	g_free(data);
	g_free(text);
}

/* GList of strings operations */
GList *utils_glist_from_string(const gchar *string)
{
	gchar *str, *temp, buff[256];
	GList *list;
	gchar *word_start, *word_end;
	gboolean the_end;

	list = NULL;
	the_end = FALSE;
	temp = g_strdup(string);
	str = temp;
	if (!str)
		return NULL;

	while (1)
	{
		gint i;
		gchar *ptr;

		/* Remove leading spaces */
		while (isspace (*str) && *str != '\0')
			str++;
		if (*str == '\0')
			break;

		/* Find start and end of word */
		word_start = str;
		while (!isspace (*str) && *str != '\0')
			str++;
		word_end = str;

		/* Copy the word into the buffer */
		for (ptr = word_start, i = 0; ptr < word_end; ptr++, i++)
			buff[i] = *ptr;
		buff[i] = '\0';
		if (strlen (buff))
			list = g_list_append(list, g_strdup (buff));
		if (*str == '\0')
			break;
	}
	if (temp)
		g_free(temp);
	return list;
}


/* Free the strings and GList */
void utils_glist_strings_free(GList *list)
{
	GList *node;
	node = list;
	while (node)
	{
		if (node->data)
			g_free(node->data);
		node = g_list_next(node);
	}
	g_list_free(list);
}


gchar *utils_get_hostname(void)
{
#if defined(GEANY_WIN32) || ! defined(HAVE_GETHOSTNAME)
	return g_strdup("localhost");
#else
	gchar *host = g_malloc(25);
	if (gethostname(host, 24) == 0)
	{
		return host;
	}
	else
	{
		g_free(host);
		return g_strdup("localhost");
	}
#endif
}


gint utils_make_settings_dir(void)
{
	gint error_nr = 0;
	gchar *fileytpes_readme = g_strconcat(
					app->configdir, G_DIR_SEPARATOR_S, "template.README", NULL);

	if (! g_file_test(app->configdir, G_FILE_TEST_EXISTS))
	{
		geany_debug("creating config directory %s", app->configdir);
#ifdef GEANY_WIN32
		if (mkdir(app->configdir) != 0) error_nr = errno;
#else
		if (mkdir(app->configdir, 0700) != 0) error_nr = errno;
#endif
	}

	if (error_nr == 0 && ! g_file_test(fileytpes_readme, G_FILE_TEST_EXISTS))
	{	// try to write template.README
		error_nr = utils_write_file(fileytpes_readme,
"There are several template files in this directory. For these templates you can use wildcards.\n\
For more information read the documentation (in " DOCDIR " or visit " GEANY_HOMEPAGE ").");
		{ // check whether write test was successful, otherwise directory is not writeable

		}
	}
	g_free(fileytpes_readme);

	return error_nr;
}


gchar *utils_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement)
{
	gint i;
	gchar *start;
	gint lt_pos;
	gchar *result;
	GString *str;

	if (haystack == NULL) return NULL;

	start = strstr(haystack, needle);
	lt_pos = utils_strpos(haystack, needle);

	if (start == NULL) return haystack;

	// substitute by copying
	str = g_string_sized_new(strlen(haystack));
	for (i = 0; i < lt_pos; i++)
	{
		g_string_append_c(str, haystack[i]);
	}
	g_string_append(str, replacement);
	g_string_append(str, haystack + lt_pos + strlen(needle));

	result = str->str;
	g_free(haystack);
	g_string_free(str, FALSE);
	return utils_str_replace(result, needle, replacement);
}


gint utils_strpos(const gchar* haystack, const gchar * needle)
{
	gint haystack_length = strlen(haystack);
	gint needle_length = strlen(needle);
	gint i, j, pos = -1;

	if (needle_length > haystack_length)
	{
		return -1;
	}
	else
	{
		for (i = 0; (i < haystack_length) && pos == -1; i++)
		{
			if (haystack[i] == needle[0])
			{
				//printf("aussen: i: %d, test: %c == %c pos: %d\n",i,haystack[i],needle[0], pos);
				for (j = 1; (j < needle_length); j++)
				{
					if (haystack[i+j] == needle[j])
					{
						if (pos == -1) pos = i;
						//printf("innen: i: %d, j: %d test: %c == %c pos: %d\n",i,j,haystack[i+j],needle[j], pos);
					}
					else
					{
						pos = -1;
						break;
					}
				}
			}
		}
		return pos;
	}
}


gchar *utils_get_date_time(void)
{
	time_t tp = time(NULL);
	const struct tm *tm = localtime(&tp);
	gchar *date = g_malloc0(25);

	strftime(date, 25, "%d.%m.%Y %H:%M:%S %Z", tm);
	return date;
}

gchar *utils_get_date(void)
{
	time_t tp = time(NULL);
	const struct tm *tm = localtime(&tp);
	gchar *date = g_malloc0(11);

	strftime(date, 11, "%Y-%m-%d", tm);
	return date;
}


void utils_create_insert_menu_items(void)
{
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(app->window, "insert_include2_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(app->popup_menu, "insert_include1_menu"));
	gint i, include_files_len = 29;
	const gchar *c_include_files[] = {
		NULL,
		"<span weight=\"ultrabold\">Std.Lib</span>",
		"(blank)",
		"assert.h",
		"ctype.h",
		"errno.h",
		"float.h",
		"limits.h",
		"locale.h",
		"math.h",
		"setjmp.h",
		"signal.h",
		"stdarg.h",
		"stddef.h",
		"stdio.h",
		"stdlib.h",
		"string.h",
		"time.h",
		NULL,
		"<span weight=\"ultrabold\">C99</span>",
		"complex.h",
		"fenv.h",
		"inttypes.h",
		"iso646.h",
		"stdbool.h",
		"stdint.h",
		"tgmath.h",
		"wchar.h",
		"wctype.h"
	};

	for (i = 0; i < include_files_len; i++)
	{
		if (c_include_files[i] == NULL)
		{
			GtkWidget *tmp_menu = gtk_menu_item_new_with_label("");
			GtkWidget *tmp_popup = gtk_menu_item_new_with_label("");
			i++;
			gtk_widget_set_sensitive(tmp_menu, FALSE);
			gtk_widget_set_sensitive(tmp_popup, FALSE);
			gtk_label_set_markup (GTK_LABEL(gtk_bin_get_child(GTK_BIN(tmp_menu))), c_include_files[i]);
			gtk_label_set_markup (GTK_LABEL(gtk_bin_get_child(GTK_BIN(tmp_popup))), c_include_files[i]);
			gtk_widget_show(tmp_menu);
			gtk_widget_show(tmp_popup);
			gtk_container_add(GTK_CONTAINER(menu_edit), tmp_menu);
			gtk_container_add(GTK_CONTAINER(menu_popup), tmp_popup);
			tmp_menu = gtk_separator_menu_item_new();
			tmp_popup = gtk_separator_menu_item_new();
			gtk_widget_show(tmp_menu);
			gtk_widget_show(tmp_popup);
			gtk_container_add(GTK_CONTAINER(menu_edit), tmp_menu);
			gtk_container_add(GTK_CONTAINER(menu_popup), tmp_popup);
		}
		else
		{
			GtkWidget *tmp_menu = gtk_menu_item_new_with_label(c_include_files[i]);
			GtkWidget *tmp_popup = gtk_menu_item_new_with_label(c_include_files[i]);
			gtk_widget_show(tmp_menu);
			gtk_widget_show(tmp_popup);
			gtk_container_add(GTK_CONTAINER(menu_edit), tmp_menu);
			gtk_container_add(GTK_CONTAINER(menu_popup), tmp_popup);
			g_signal_connect((gpointer) tmp_menu, "activate", G_CALLBACK(on_insert_include_activate), (gpointer) c_include_files[i]);
			g_signal_connect((gpointer) tmp_popup, "activate", G_CALLBACK(on_insert_include_activate), (gpointer) c_include_files[i]);
		}

	}

}


gchar *utils_get_initials(gchar *name)
{
	gint i = 1, j = 1;
	gchar *initials = g_malloc0(5);

	initials[0] = name[0];
	while (name[i] != '\0' && j < 4)
	{
		if (name[i] == ' ' && name[i + 1] != ' ')
		{
			initials[j++] = name[i + 1];
		}
		i++;
	}
	return initials;
}


void utils_free_ptr_array(gchar *array[], gint len)
{
	gint i;

	for (i = 0; i < len; i++)
	{
		g_free(array[i]);
	}
}


void utils_update_recent_menu(void)
{
	GtkWidget *recent_menu = lookup_widget(app->window, "recent_files1_menu");
	GtkWidget *tmp;
	gchar *filename;
	GList *children = gtk_container_get_children(GTK_CONTAINER(recent_menu));

	// clean the MRU list
	if (g_list_length(children) > app->mru_length)
	{
		children = g_list_nth(children, app->mru_length - 1);
		while (children)
		{
			gtk_widget_destroy(GTK_WIDGET(children->data));
			children = g_list_next(children);
		}
	}

	filename = g_queue_peek_head(app->recent_queue);
	tmp = gtk_menu_item_new_with_label(filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_insert(GTK_MENU_SHELL(recent_menu), tmp, 0);
	g_signal_connect((gpointer) tmp, "activate",
				G_CALLBACK(on_recent_file_activate), (gpointer) filename);
}


/* Wrapper functions for Key-File-Parser from GLib in keyfile.c to reduce code size */
gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key, const gint default_value)
{
	gint tmp;
	GError *error = NULL;

	if (config == NULL) return default_value;

	tmp = g_key_file_get_integer(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return default_value;
	}
	return tmp;
}


gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key, const gboolean default_value)
{
	gboolean tmp;
	GError *error = NULL;

	if (config == NULL) return default_value;

	tmp = g_key_file_get_boolean(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return default_value;
	}
	return tmp;
}


gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key, const gchar *default_value)
{
	gchar *tmp;
	GError *error = NULL;

	if (config == NULL) return g_strdup(default_value);

	tmp = g_key_file_get_string(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return (gchar*) g_strdup(default_value);
	}
	return tmp;
}


void utils_switch_document(gint direction)
{
	gint page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));

	if (direction == LEFT && cur_page > 0)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), cur_page - 1);
	}
	else if (direction == RIGHT && cur_page < page_count)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), cur_page + 1);
	}
}


void utils_replace_filename(gint idx)
{
	gint pos = sci_get_current_position(doc_list[idx].sci);
	gchar *filebase = g_strconcat(GEANY_STRING_UNTITLED, ".", (doc_list[idx].file_type)->extension, NULL);
	gchar *filename = g_path_get_basename(doc_list[idx].file_name);

	sci_set_current_position(doc_list[idx].sci, 0);
	sci_set_search_anchor(doc_list[idx].sci);
	// stop if filebase was not found
	if (sci_search_next(doc_list[idx].sci, SCFIND_MATCHCASE, filebase) == -1)
	{
		g_free(filebase);
		g_free(filename);
		return;
	}

	sci_replace_sel(doc_list[idx].sci, filename);
	g_free(filebase);
	g_free(filename);

	sci_set_current_position(doc_list[idx].sci, pos);
}


/*
GdkColor *utils_get_color_from_bint(gint icolor)
{
	GdkColor *gcolor = g_new(GdkColor, 1);
	guint16 r, g, b;

	r = icolor;
	g = r >> 8;
	b = g >> 8;

	//gcolor->red = r & 255;
	//gcolor->green = g & 255;
	//gcolor->blue = b & 255;
	//geany_debug("%d %d %d", gcolor->red, gcolor->green, gcolor->blue);


	geany_debug("%d %d %d", r, g, b);

	gcolor->red   = (r << 8) + r;
	gcolor->green = (g << 8) + g;
	gcolor->blue  = (b << 8) + b;

	//gcolor->red = (r & 255) * 257;
	//gcolor->green = (g & 255) * 257;
	//gcolor->blue = (b & 255) * 257;
	//geany_debug("%d %d %d %d", icolor, gcolor->red, gcolor->green, gcolor->blue);

	return gcolor;
}
*/

/* wrapper function to let strcmp work with GeanySymbol struct */
gint utils_compare_symbol(const GeanySymbol *a, const GeanySymbol *b)
{
	if (a == NULL || b == NULL) return 0;

	return strcmp(a->str, b->str);
}


gchar *utils_get_hex_from_color(GdkColor *color)
{
	gchar *hex = g_malloc0(9);

	g_snprintf(hex, 8, "#%02X%02X%02X",
	      (guint) (utils_scale_round(color->red / 256, 255)),
	      (guint) (utils_scale_round(color->green / 256, 255)),
	      (guint) (utils_scale_round(color->blue / 256, 255)));

	return hex;
}


/* utils_is_hex() and utils_get_int_from_hexcolor() taken from pango-color.c to get red, green and
 * blue values from a hex string like #C0C0C0 */
gboolean utils_is_hex(const gchar *spec, gint len, guint *c)
{
	const gchar *end;
	*c = 0;
	for (end = spec + len; spec != end; spec++)
	{
		if (g_ascii_isxdigit(*spec)) *c = (*c << 4) | g_ascii_xdigit_value(*spec);
		else return FALSE;
	}
	return TRUE;
}


gint utils_get_int_from_hexcolor(const gchar *hex)
{
#define DEFAULT_COLOR 12774338
	if (hex[0] == '#')
	{
		size_t len;
		guint r, g, b;

		hex++;
		len = strlen(hex);
		if (len % 3 || len < 3 || len > 12) return DEFAULT_COLOR;

		len /= 3;

		if (! utils_is_hex(hex, len, &r) ||
			! utils_is_hex(hex + len, len, &g) ||
			! utils_is_hex(hex + len * 2, len, &b))
			return FALSE;

		gint bits = len * 4;
		r <<= 8 - bits;
		g <<= 8 - bits;
		b <<= 8 - bits;
		while (bits < 8)
		{
			r |= (r >> bits);
			g |= (g >> bits);
			b |= (b >> bits);
			bits *= 2;
		}
		//geany_debug("%d %d %d", r, g, b);
		return r | (g << 8) | (b << 16);
	}
	return DEFAULT_COLOR;
}
