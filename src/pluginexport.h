/*
 * pluginexport.h - this file is part of Geany, a fast and lightweight IDE
 *
 * Copyright 2014 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_PLUGINEXPORT_H
#define GEANY_PLUGINEXPORT_H 1

#if defined(_WIN32) || defined(__CYGWIN__)
# define GEANY_EXPORT_SYMBOL __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define GEANY_EXPORT_SYMBOL __attribute__((visibility ("default")))
#else
# define GEANY_API_SYMBOL
#endif

#define GEANY_API_SYMBOL GEANY_EXPORT_SYMBOL

#endif /* GEANY_PLUGINEXPORT_H */
