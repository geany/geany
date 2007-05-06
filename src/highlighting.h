/*
 *      highlighting.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

#ifndef PLAT_GTK
#   define PLAT_GTK 1	// needed for ScintillaWidget.h
#endif

#include "Scintilla.h"
#include "ScintillaWidget.h"


void styleset_free_styles(void);

void styleset_common(ScintillaObject *sci, gint style_bits);

void styleset_c(ScintillaObject *sci);

void styleset_cpp(ScintillaObject *sci);

void styleset_makefile(ScintillaObject *sci);

void styleset_latex(ScintillaObject *sci);

void styleset_php(ScintillaObject *sci);

void styleset_html(ScintillaObject *sci);

void styleset_java(ScintillaObject *sci);

void styleset_pascal(ScintillaObject *sci);

void styleset_perl(ScintillaObject *sci);

void styleset_python(ScintillaObject *sci);

void styleset_ruby(ScintillaObject *sci);

void styleset_sh(ScintillaObject *sci);

void styleset_xml(ScintillaObject *sci);

void styleset_docbook(ScintillaObject *sci);

void styleset_none(ScintillaObject *sci);

void styleset_css(ScintillaObject *sci);

void styleset_conf(ScintillaObject *sci);

void styleset_asm(ScintillaObject *sci);

void styleset_sql(ScintillaObject *sci);

void styleset_caml(ScintillaObject *sci);

void styleset_haskell(ScintillaObject *sci);

void styleset_oms(ScintillaObject *sci);

void styleset_tcl(ScintillaObject *sci);

void styleset_d(ScintillaObject *sci);

void styleset_fortran(ScintillaObject *sci);

void styleset_diff(ScintillaObject *sci);

void styleset_ferite(ScintillaObject *sci);

void styleset_vhdl(ScintillaObject *sci);

void styleset_js(ScintillaObject *sci);

void styleset_lua(ScintillaObject *sci);

#endif
