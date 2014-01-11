/*
 *      fileloc.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2013 Arthur Rosenstein <artros(dot)misc(at)gmail(dot)com>
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

/*
 *  @file fileloc.h
 *  @brief File location data-type.
 **/

#ifndef GEANY_FILELOC_H
#define GEANY_FILELOC_H

#include "document.h"

#include <glib.h>

/*
 *  @struct GeanyFileLocation
 *  @brief Opaque data-type representing a location within a file.
 *
 *  @c GeanyFileLocation consolidates the plethora of methods to represent a
 *  location within a file: filename or actual document object, line and column
 *  numbers or Scintilla position, etc. A producer constructs a
 *  @c GeanyFileLocation object using whatever parameters it finds suitable, and
 *  consumers can then extract the location information using a different set of
 *  parameters, while @c GeanyFileLocation handles the conversion internally.
 *
 *  @c GeanyFileLocation has two groups of parameters: file parameters, which
 *  specify a file, and location parameters, which specify a location within the
 *  file. Each of these groups is controlled by a subset of its parameters
 *  called the @e pivot. The pivot is the set of invariant parameters from which
 *  the rest are derived. For example, a document can have different filenames
 *  at different times, and conversely a filename can refer to different
 *  documents at different times. If the file pivot of a @c GeanyFileLocation
 *  object is a document, then the document parameter stays constant while the
 *  filename changes, while if the file pivot is a filename the filename
 *  parameter stays constant while the document changes. The combination of the
 *  file pivot and the localtion pivot is referred to as the pivot of the
 *  @c GeanyFileLocation object.
 *
 *  @c GeanyFileLocation objects can contain incomplete information: not all the
 *  parameters must have an associated value. For example, a
 *  @c GeanyFileLocation object can specify a filename and a line number, but
 *  not a column number. Or, a @c GeanyFileLocation object might specify all of
 *  the above, but not have an associated character number in case no physical
 *  document backs up the filename.
 **/
typedef struct GeanyFileLocation GeanyFileLocation;


/*
 *  @enum GeanyFileLocationFilePivot
 *  @brief @c GeanyFileLocation file pivot.
 **/
typedef enum
{
	/* Unspecified file pivot. */
	GEANY_FLFP_UNSPECIFIED = -1,
	/* The file pivot is the document parameter. */
	GEANY_FLFP_DOCUMENT,
	/* The file pivot is the filename parameter. */
	GEANY_FLFP_FILENAME,

	GEANY_FLFP_MAX
}
GeanyFileLocationFilePivot;

/*
 *  @enum GeanyFileLocationLocationPivot
 *  @brief @c GeanyFileLocation location pivot.
 **/
typedef enum
{
	/* Unspecified location pivot. */
	GEANY_FLLP_UNSPECIFIED = -1,
	/* The location pivot is the Scintilla position parameter. */
	GEANY_FLLP_POS,
	/* The location pivot is the line and character number parameters. */
	GEANY_FLLP_LINE_CHARACTER,
	/* The location pivot is the line and UTF-8 byte number parameters. */
	GEANY_FLLP_LINE_UTF8_BYTE,
	/* The location pivot is the line and encoding byte number parameters. */
	GEANY_FLLP_LINE_ENCODING_BYTE,
	/* The location pivot is the line and locale byte number parameters. */
	GEANY_FLLP_LINE_LOCALE_BYTE,
	/* The location pivot is the line and column number parameters. */
	GEANY_FLLP_LINE_COLUMN,

	GEANY_FLLP_MAX
}
GeanyFileLocationLocationPivot;

/*
 *  @enum GeanyFileLocationOffsetKind
 *  @brief Offset kind.
 **/
typedef enum
{
	/* Character offset. */
	GEANY_FLOK_CHARACTER,
	/* UTF-8 byte offset. */
	GEANY_FLOK_UTF8_BYTE,
	/* Encoding byte offset. */
	GEANY_FLOK_ENCODING_BYTE,
	/* Locale byte offset. */
	GEANY_FLOK_LOCALE_BYTE,
	/* Column offset. */
	GEANY_FLOK_COLUMN,

	GEANY_FLOK_LINE_MAX
}
GeanyFileLocationOffsetKind;


GeanyFileLocation *fileloc_new(void);
GeanyFileLocation *fileloc_copy(const GeanyFileLocation *src_fileloc);
GeanyFileLocation *fileloc_new_from_doc_pos_or_current(GeanyDocument *doc, gint pos);
void fileloc_clear(GeanyFileLocation *fileloc);
void fileloc_assign(GeanyFileLocation *fileloc, const GeanyFileLocation *src_fileloc);
void fileloc_free(GeanyFileLocation *fileloc);

GeanyFileLocationFilePivot fileloc_get_file_pivot(const GeanyFileLocation *fileloc);
GeanyFileLocationLocationPivot fileloc_get_location_pivot(const GeanyFileLocation *fileloc);
gboolean fileloc_convert_pivot(GeanyFileLocation *fileloc,
								GeanyFileLocationFilePivot file_pivot,
								GeanyFileLocationLocationPivot location_pivot,
								gboolean force);

gboolean fileloc_has_file(const GeanyFileLocation *fileloc);

GeanyDocument *fileloc_get_document(const GeanyFileLocation *fileloc);
void fileloc_set_document(GeanyFileLocation *fileloc, GeanyDocument *doc);

gchar *fileloc_get_filename(const GeanyFileLocation *fileloc);
gchar *fileloc_get_locale_filename(const GeanyFileLocation *fileloc);
void fileloc_set_filename(GeanyFileLocation *fileloc, const gchar *filename);
void fileloc_set_locale_filename(GeanyFileLocation *fileloc, const gchar *locale_filename);

gboolean fileloc_has_location(const GeanyFileLocation *fileloc);

gint fileloc_get_pos(const GeanyFileLocation *fileloc);
void fileloc_set_pos(GeanyFileLocation *fileloc, gint pos);

gint fileloc_get_line(const GeanyFileLocation *fileloc);
void fileloc_set_line(GeanyFileLocation *fileloc, gint line);

gint fileloc_get_line_offset(const GeanyFileLocation *fileloc, GeanyFileLocationOffsetKind offset_kind);
void fileloc_set_line_offset(GeanyFileLocation *fileloc, GeanyFileLocationOffsetKind offset_kind, gint offset);

gboolean fileloc_sanitize(GeanyFileLocation *fileloc, gboolean clamp);
gboolean fileloc_fill_missing_params(GeanyFileLocation *fileloc,
										gboolean fill_file,
										gboolean fill_location);

GeanyDocument *fileloc_open_document(const GeanyFileLocation *fileloc,
										gboolean readonly, GeanyFiletype *ft,
										const gchar *forced_enc,
										gboolean fall_back_to_current_doc_dir);
GeanyDocument *fileloc_open_document_and_convert_pivot(
										GeanyFileLocation *fileloc,
										gboolean readonly, GeanyFiletype *ft,
										const gchar *forced_enc,
										gboolean fall_back_to_current_doc_dir);

#endif /* GEANY_FILELOC_H */
