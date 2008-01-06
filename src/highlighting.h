/*
 *      highlighting.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifndef GEANY_HIGHLIGHTING_H
#define GEANY_HIGHLIGHTING_H 1

#include "Scintilla.h"
#include "ScintillaWidget.h"


typedef struct HighlightingStyle
{
	gint	foreground;
	gint	background;
	gboolean bold:1;
	gboolean italic:1;
} HighlightingStyle;


void highlighting_init_styles(gint filetype_idx, GKeyFile *config, GKeyFile *configh);

void highlighting_set_styles(ScintillaObject *sci, gint filetype_idx);

const HighlightingStyle *highlighting_get_style(gint ft_id, gint style_id);

void highlighting_free_styles(void);

#endif
