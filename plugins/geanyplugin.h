/*
 *      geanyplugin.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *  @file geanyplugin.h
 *  Single include for plugins.
 **/


#ifndef GEANY_PLUGIN_H
#define GEANY_PLUGIN_H 1

#ifndef HAVE_PLUGINS
# define HAVE_PLUGINS 1
#endif

/* Only include public headers here */
#include "app.h"
#include "build.h"
#include "dialogs.h"
#include "document.h"
#include "editor.h"
#include "encodings.h"
#include "filetypes.h"
#include "geany.h"
#include "geanyfunctions.h"
#include "highlighting.h"
#include "keybindings.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "plugindata.h"
#include "pluginutils.h"
#include "prefs.h"
#include "project.h"
#include "sciwrappers.h"
#include "search.h"
#include "spawn.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"

#include "gtkcompat.h"

#endif
