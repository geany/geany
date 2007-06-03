/*
 *      navqueue.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Dave Moore <wrex006@gmail.com>
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


// for the navigation history queue
typedef struct
{
	gchar *file;
	/// TODO maybe it is better to work on positions than on lines to be more accurate when
	/// switching back or forward, sci_get_position_from_line() could be used for tm_tag lines
	gint line;
} filepos;

GQueue *navigation_queue;
guint nav_queue_pos;


void navqueue_init()
{
	navigation_queue = g_queue_new();
	nav_queue_pos = 0;
}


void navqueue_free()
{
	while (! g_queue_is_empty(navigation_queue))
	{
		g_free(g_queue_pop_tail(navigation_queue));
	}
	g_queue_free(navigation_queue);
}


static void adjust_buttons()
{
	if (g_queue_get_length(navigation_queue) < 2)
	{
		gtk_widget_set_sensitive(app->navigation_buttons[0], FALSE);
		gtk_widget_set_sensitive(app->navigation_buttons[1], FALSE);
		return;
	}
	if (nav_queue_pos == 0)
	{
		gtk_widget_set_sensitive(app->navigation_buttons[0], TRUE);
		gtk_widget_set_sensitive(app->navigation_buttons[1], FALSE);
		return;
	}
	// forward should be sensitive since where not at the start
	gtk_widget_set_sensitive(app->navigation_buttons[1], TRUE);

	// back should be sensitive if there's a place to go back to
	(nav_queue_pos < g_queue_get_length(navigation_queue) - 1) ?
		gtk_widget_set_sensitive(app->navigation_buttons[0], TRUE) :
			gtk_widget_set_sensitive(app->navigation_buttons[0], FALSE);
}


void navqueue_new_position(gchar *file, gint line)
{
	filepos *npos;
	guint i;

	npos = g_new0(filepos, 1);
	npos->file = file;
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
	/// TODO add check to not add an entry if "Go To Defintion" on a definition is used
	g_queue_push_head(navigation_queue, npos);
	adjust_buttons();
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
		g_queue_pop_nth(navigation_queue, nav_queue_pos + 1);
	}
	adjust_buttons();
}


void navqueue_go_forward()
{
	filepos *fnext;

	if (nav_queue_pos < 1 || g_queue_get_length(navigation_queue) < 2)
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
		g_queue_pop_nth(navigation_queue, nav_queue_pos - 1);
	}

	adjust_buttons();
}

