/*
 *      navqueue.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Dave Moore <wrex006(at)gmail(dot)com>
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

/*
 * Simple code navigation
 */

#include "geany.h"

#include "navqueue.h"
#include "sciwrappers.h"
#include "document.h"
#include "utils.h"
#include "support.h"


// for the navigation history queue
typedef struct
{
	gchar *file;	// this is the tagmanager filename, not document::file_name
	/// TODO maybe it is better to work on positions than on lines to be more accurate when
	/// switching back or forward, sci_get_position_from_line() could be used for tm_tag lines
	gint line;	// line is counted with 1 as the first line, not 0
} filepos;

static GQueue *navigation_queue;
static guint nav_queue_pos;

static GtkWidget *navigation_buttons[2];



void navqueue_init()
{
	navigation_queue = g_queue_new();
	nav_queue_pos = 0;

	navigation_buttons[0] = lookup_widget(app->window, "toolbutton_back");
	navigation_buttons[1] = lookup_widget(app->window, "toolbutton_forward");
}


void navqueue_free()
{
	while (! g_queue_is_empty(navigation_queue))
	{
		g_free(g_queue_pop_tail(navigation_queue));
	}
	g_queue_free(navigation_queue);
}


static void adjust_buttons(void)
{
	if (g_queue_get_length(navigation_queue) < 2)
	{
		gtk_widget_set_sensitive(navigation_buttons[0], FALSE);
		gtk_widget_set_sensitive(navigation_buttons[1], FALSE);
		return;
	}
	if (nav_queue_pos == 0)
	{
		gtk_widget_set_sensitive(navigation_buttons[0], TRUE);
		gtk_widget_set_sensitive(navigation_buttons[1], FALSE);
		return;
	}
	// forward should be sensitive since where not at the start
	gtk_widget_set_sensitive(navigation_buttons[1], TRUE);

	// back should be sensitive if there's a place to go back to
	(nav_queue_pos < g_queue_get_length(navigation_queue) - 1) ?
		gtk_widget_set_sensitive(navigation_buttons[0], TRUE) :
			gtk_widget_set_sensitive(navigation_buttons[0], FALSE);
}


static gboolean
queue_pos_matches(guint queue_pos, const gchar *fname, gint line)
{
	if (queue_pos < g_queue_get_length(navigation_queue))
	{
		filepos *fpos = g_queue_peek_nth(navigation_queue, queue_pos);

		return (utils_str_equal(fpos->file, fname) && fpos->line == line);
	}
	return FALSE;
}


static void add_new_position(gchar *tm_filename, gint line)
{
	filepos *npos;
	guint i;

	if (queue_pos_matches(nav_queue_pos, tm_filename, line))
		return;	// prevent duplicates

	npos = g_new0(filepos, 1);
	npos->file = tm_filename;
	npos->line = line;

	// if we've jumped to a new position from
	// inside the queue rather than going forward
	if (nav_queue_pos > 0)
	{
		for (i = 0; i < nav_queue_pos; i++)
		{
			g_free(g_queue_pop_head(navigation_queue));
		}
		nav_queue_pos = 0;
	}
	g_queue_push_head(navigation_queue, npos);
	adjust_buttons();
}


/* Adds the current document position to the queue before adding the new position.
 * line is counted with 1 as the first line, not 0. */
gboolean navqueue_goto_line(gint old_idx, gint new_idx, gint line)
{
	g_return_val_if_fail(DOC_IDX_VALID(old_idx), FALSE);
	g_return_val_if_fail(DOC_IDX_VALID(new_idx), FALSE);
	g_return_val_if_fail(doc_list[new_idx].tm_file, FALSE);
	g_return_val_if_fail(line >= 1, FALSE);

	// first add old file as old position
	if (doc_list[old_idx].tm_file)
	{
		gint cur_line = sci_get_current_line(doc_list[old_idx].sci);

		add_new_position(doc_list[old_idx].tm_file->file_name, cur_line + 1);
	}

	add_new_position(doc_list[new_idx].tm_file->file_name, line);
	return utils_goto_line(new_idx, line);
}


void navqueue_go_back()
{
	filepos *fprev;

	// return if theres no place to go back to
	if (g_queue_is_empty(navigation_queue) ||
		nav_queue_pos >= g_queue_get_length(navigation_queue) - 1)
		return;

	// jump back
	fprev = g_queue_peek_nth(navigation_queue, nav_queue_pos + 1);
	if (utils_goto_file_line(fprev->file, TRUE, fprev->line))
	{
		nav_queue_pos++;
	}
	else
	{
		/// TODO: add option to re open the file
		g_free(g_queue_pop_nth(navigation_queue, nav_queue_pos + 1));
	}
	adjust_buttons();
}


void navqueue_go_forward()
{
	filepos *fnext;

	if (nav_queue_pos < 1 ||
		nav_queue_pos >= g_queue_get_length(navigation_queue))
		return;

	// jump forward
	fnext = g_queue_peek_nth(navigation_queue, nav_queue_pos - 1);
	if (utils_goto_file_line(fnext->file, TRUE, fnext->line))
	{
		nav_queue_pos--;
	}
	else
	{
		/// TODO: add option to re open the file
		g_free(g_queue_pop_nth(navigation_queue, nav_queue_pos - 1));
	}

	adjust_buttons();
}

