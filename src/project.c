/*
 *      project.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

#include "geany.h"
#include "project.h"
#include "dialogs.h"
#include "support.h"

static gboolean close_open_project();


void project_new()
{
	if (! close_open_project()) return;

	// simply create an empty project and show the properties dialog
	app->project = g_new0(GeanyProject, 1);
	project_properties();
}


void project_open()
{
	if (! close_open_project()) return;


}


void project_close()
{
	/// TODO should we handle open files in any way here?

	g_return_if_fail(app->project != NULL);

	g_free(app->project->name);
	g_free(app->project->description);
	g_free(app->project->file_name);
	g_free(app->project->base_path);
	g_free(app->project->executable);

	g_free(app->project);
	app->project = NULL;
}


void project_properties()
{
	g_return_if_fail(app->project != NULL);
}


/* checks whether there is an already open project and asks the user if he want to close it or
 * abort the current action. Returns FALSE when the current action(the caller) should be cancelled
 * and TRUE if we can go ahead */
static gboolean close_open_project()
{
	if (app->project != NULL)
	{
		gchar *msg =
			_("There is already an open project \"%s\". Do you want to close it before proceed?");

		if (dialogs_show_question(msg, app->project->name))
		{
			project_close();
			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return TRUE;
}
