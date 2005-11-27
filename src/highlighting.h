/*
 *      highlighting.h - this file is part of Geany, a fast and lightweight IDE
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


#include "geany.h"


typedef struct
{
	gint styling[55][3];
	gchar **keywords;
} style_set;


void styleset_free_styles(void);

void styleset_common(ScintillaObject *sci, gint style_bits);

void styleset_c(ScintillaObject *sci);

void styleset_makefile(ScintillaObject *sci);

void styleset_tex(ScintillaObject *sci);

void styleset_php(ScintillaObject *sci);

void styleset_java(ScintillaObject *sci);

void styleset_pascal(ScintillaObject *sci);

void styleset_perl(ScintillaObject *sci);

void styleset_python(ScintillaObject *sci);

void styleset_sh(ScintillaObject *sci);

void styleset_xml(ScintillaObject *sci);

void styleset_markup(ScintillaObject *sci);

void styleset_docbook(ScintillaObject *sci);

void styleset_none(ScintillaObject *sci);

void styleset_css(ScintillaObject *sci);

void styleset_conf(ScintillaObject *sci);
