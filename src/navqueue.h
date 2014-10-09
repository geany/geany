/*
 *      navqueue.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Dave Moore <wrex006(at)gmail(dot)com>
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/**
 *  @file navqueue.h
 * Simple code navigation
 **/


#ifndef GEANY_NAVQUEUE_H
#define GEANY_NAVQUEUE_H 1

#include "document.h"

#include <glib.h>

G_BEGIN_DECLS

gboolean navqueue_goto_line(GeanyDocument *old_doc, GeanyDocument *new_doc, gint line);


#ifdef GEANY_PRIVATE

void navqueue_init(void);

void navqueue_free(void);

void navqueue_remove_file(const gchar *filename);

void navqueue_go_back(void);

void navqueue_go_forward(void);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_NAVQUEUE_H */
