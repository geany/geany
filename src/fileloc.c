/*
 *      fileloc.c - this file is part of Geany, a fast and lightweight IDE
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
 *  File location data-type.
 **/

#include "geany.h"
#include "support.h"

#include "fileloc.h"

#include "document.h"
#include "encodings.h"
#include "sciwrappers.h"
#include "utils.h"
#include "ui_utils.h"

#include <stddef.h>
#include <string.h>

#include <glib.h>


struct GeanyFileLocation
{
	/* The file pivot. This field should be big enough to hold the different
	 * members of GeanyFileLocationFilePivot, with the exception of
	 * GEANY_FLFP_UNSPECIFIED and GEANY_FLFP_MAX.*/
	unsigned file_pivot : 1;
	/* The location pivot. This field should be big enough to hold the different
	 * members of GeanyFileLocationLocationPivot, with the exception of
	 * GEANY_FLLP_UNSPECIFIED and GEANY_FLLP_MAX.*/
	unsigned location_pivot : 3;

	union
	{
		/* The associated document, or NULL if no document is associated.
		 * Only used when file_pivot == GEANY_FLFP_DOCUMENT. */
		GeanyDocument *doc;
		/* The associated UTF8-encoded filename, or NULL if no filename is
		 * associated. The memory is owned by the object. Only used when
		 * file_pivot == GEANY_FLFP_FILENAME. */
		gchar *filename;
	};

	union
	{
		/* The associated Scintilla position, or a negative value if no position
		 * is associated. Only used when location_pivot == GEANY_FLLP_POS. */
		gint pos;
		/* The associated line number and offset. Only used when
		 * location_pivot == GEANY_FLLP_LINE_{COLUMN,CHARACTER,UTF8_BYTE,
		 * ENCODING_BYTE,LOCALE_BYTE}. */
		struct
		{
			/* The associated zero-based line number, or a negative number if no
			 * line number is associated. */
			gint line;
			/* The associated zero-based line offset, or a negative number if no
			 * line offset is associated. The interpretation of the offset
			 * depends on the value of location_pivot. */
			gint line_offset;
		};
	};
};


static gboolean is_valid_file_pivot(GeanyFileLocationFilePivot file_pivot,
									gboolean allow_unspecified)
{
	return (file_pivot >= 0 && file_pivot < GEANY_FLFP_MAX) ||
			(allow_unspecified && file_pivot == GEANY_FLFP_UNSPECIFIED);
}

static gboolean is_valid_location_pivot(GeanyFileLocationLocationPivot location_pivot,
										gboolean allow_unspecified)
{
	return (location_pivot >= 0 && location_pivot < GEANY_FLLP_MAX) ||
			(allow_unspecified && location_pivot == GEANY_FLLP_UNSPECIFIED);
}

static gboolean is_valid_line_offset_kind(GeanyFileLocationOffsetKind offset_kind)
{
	return offset_kind >= 0 && offset_kind < GEANY_FLOK_LINE_MAX;
}

static gboolean is_line_offset_location_pivot(GeanyFileLocationLocationPivot location_pivot)
{
	g_assert(is_valid_location_pivot(location_pivot, FALSE));

	return location_pivot > GEANY_FLLP_POS;
}

static GeanyFileLocationLocationPivot line_offset_kind_to_location_pivot(GeanyFileLocationOffsetKind offset_kind)
{
	g_assert(is_valid_line_offset_kind(offset_kind));

	return (GeanyFileLocationLocationPivot) (offset_kind + 1);
}

static GeanyFileLocationOffsetKind location_pivot_to_line_offset_kind(GeanyFileLocationLocationPivot location_pivot)
{
	g_assert(is_line_offset_location_pivot(location_pivot));

	return (GeanyFileLocationOffsetKind) (location_pivot - 1);
}

/* Reset a GeanyFileLocation to its initial state without cleaning up first. */
static void init_fileloc(GeanyFileLocation *fileloc)
{
	g_assert(fileloc != NULL);

	fileloc->file_pivot = GEANY_FLFP_DOCUMENT;
	fileloc->location_pivot = GEANY_FLLP_POS;
	fileloc->doc = NULL;
	fileloc->pos = -1;
}

/* Copy a GeanyFileLocation over without cleaning up first. */
static void copy_fileloc(GeanyFileLocation *fileloc, const GeanyFileLocation *src_fileloc)
{
	g_assert(fileloc != NULL);
	g_assert(src_fileloc != NULL);

	memcpy(fileloc, src_fileloc, sizeof(GeanyFileLocation));
	if (src_fileloc->file_pivot == GEANY_FLFP_FILENAME)
		fileloc->filename = g_strdup(src_fileloc->filename);
}

/* Returns the Scintilla position assocaited with a GeanyFileLocation object
 * using the provided document to perform the necessary conversions. If no
 * position is associated with the object or if the conversion fails, returns a
 * negative value. Note that the function does not generally check if the
 * location parameters of the object are within range. */
static gint get_pos_from_fileloc_and_doc(const GeanyFileLocation *fileloc, GeanyDocument *doc)
{
	ScintillaObject *sci;

	g_assert(fileloc != NULL);

	/* If the location pivot is a Scintilla position, we simply return it. */
	if (fileloc->location_pivot == GEANY_FLLP_POS)
		return fileloc->pos;

	/* Otherwise, we must have a valid document to perform the conversion. */
	if (doc == NULL || ! doc->is_valid)
		return -1;
	g_assert(doc->editor != NULL && doc->editor->sci != NULL);
	sci = doc->editor->sci;

	/* At the moment, the only position pivots other than GEANY_FLLP_POS are
	 * line+offset pivots. If this ever changes this code has to be adjusted. */
	g_assert(is_line_offset_location_pivot(fileloc->location_pivot));

	 /* If we don't have a line number, we can ignore the offset and just fail. */
	if (fileloc->line < 0)
		return -1;
	/* If we have a line number but no line offset or a zero line offset, return
	 * the position at the beginning of the line. */
	else if (fileloc->line_offset <= 0)
		return sci_get_position_from_line(sci, fileloc->line);

	/* We have a line number and a line offset. We determine the interpretation
	 * of the line offset according to location_pivot. */
	switch (fileloc->location_pivot)
	{
		case GEANY_FLLP_LINE_COLUMN:
			return sci_get_position_from_col(sci, fileloc->line, fileloc->line_offset);

		case GEANY_FLLP_LINE_CHARACTER:
		{
			gint pos = sci_get_position_from_line(sci, fileloc->line);

			gchar *text = sci_get_line(sci, fileloc->line);

			/* We have to clamp the character number ourselves, since
			 * g_utf8_offset_to_pointer doesn't. */
			gint len = g_utf8_strlen(text, -1);
			gint char_offset = MIN(fileloc->line_offset, len);

			pos += g_utf8_offset_to_pointer(text, char_offset) - text;

			g_free(text);

			return pos;
		}

		case GEANY_FLLP_LINE_UTF8_BYTE:
			return sci_get_position_from_line(sci, fileloc->line) + fileloc->line_offset;

		case GEANY_FLLP_LINE_ENCODING_BYTE:
		{
			gint pos = sci_get_position_from_line(sci, fileloc->line);

			if (doc->encoding == NULL || utils_str_equal(doc->encoding, "UTF-8") ||
				utils_str_equal(doc->encoding, encodings[GEANY_ENCODING_NONE].charset))
			{
				pos += fileloc->line_offset;
			}
			else
			{
				gchar *text_utf8;
				gchar *text_enc;
				gsize len;
				GError *error = NULL;

				/* Convert from UTF-8 to the document's encoding. */
				text_utf8 = sci_get_line(sci, fileloc->line);
				text_enc = g_convert(text_utf8, -1, doc->encoding, "UTF-8",
										NULL, &len, &error);
				g_free(text_utf8);

				/* If a conversion error occurs, return the position at the
				 * beginning of the line. */
				if (error != NULL)
				{
					g_error_free(error);
					return pos;
				}

				/* Clamp the byte offset to the actual length. */
				len = MIN(fileloc->line_offset, len);
				/* Convert the substring up to the byte offset back to UTF-8. */
				text_utf8 = g_convert(text_enc, len, "UTF-8", doc->encoding,
										NULL, &len, &error);
				g_free(text_enc);
				g_free(text_utf8);

				/* If a conversion error occurs, return the position at the
				 * beginning of the line. */
				if (error != NULL)
				{
					g_error_free(error);
					return pos;
				}

				pos += len;
			}

			return pos;
		}

		case GEANY_FLLP_LINE_LOCALE_BYTE:
		{
			gint pos = sci_get_position_from_line(sci, fileloc->line);

			gchar *text_utf8;
			gchar *text_loc;
			gsize len;
			GError *error = NULL;

			/* Convert from UTF-8 to the current locale. */
			text_utf8 = sci_get_line(sci, fileloc->line);
			text_loc = g_locale_from_utf8(text_utf8, -1, NULL, &len, &error);
			g_free(text_utf8);

			/* If a conversion error occurs, return the position at the
			 * beginning of the line. */
			if (error != NULL)
			{
				g_error_free(error);
				return pos;
			}

			/* Clamp the byte offset to the actual length. */
			len = MIN(fileloc->line_offset, len);
			/* Convert the substring up to the byte offset back to UTF-8. */
			text_utf8 = g_locale_to_utf8(text_loc, len, NULL, &len, &error);
			g_free(text_loc);
			g_free(text_utf8);

			/* If a conversion error occurs, return the position at the
			 * beginning of the line. */
			if (error != NULL)
			{
				g_error_free(error);
				return pos;
			}

			pos += len;

			return pos;
		}
	}

	g_assert_not_reached();
	return -1;
}


/*
 *  Allocates a new @c GeanyFileLocation object.
 *
 *  @return The new @c GeanyFileLocation object. The pivot of the returned
 *          object is unspecified. None of the parameters has an associated
 *          value. The object should be eventually freed with a call to
 *          @c fileloc_free .
 **/
GeanyFileLocation *fileloc_new(void)
{
	GeanyFileLocation *fileloc = g_new(GeanyFileLocation, 1);

	init_fileloc(fileloc);

	return fileloc;
}

/*
 *  Allocates a copy of a @c GeanyFileLocation object.
 *
 *  @param src_fileloc The @c GeanyFileLocation object to copy. Can be NULL.
 *
 *  @return The new @c GeanyFileLocation object. The returned object has the
 *          same pivot and parameter values as @a src_fileloc . The object
 *          should be eventually freed with a call to @c fileloc_free .
 *          If @a src_fileloc is NULL, the function returns NULL.
 **/
GeanyFileLocation *fileloc_copy(const GeanyFileLocation *src_fileloc)
{
	GeanyFileLocation *fileloc;

	if (src_fileloc == NULL)
		return NULL;

	fileloc = g_new(GeanyFileLocation, 1);
	copy_fileloc(fileloc, src_fileloc);

	return fileloc;
}

/*
 *  Allocates a new @c GeanyFileLocation object and associates a document and
 *  a Scintilla position with it.
 *
 *  @param doc Document object. If @a doc is @c NULL, the current document and
 *             position are sued.
 *  @param pos Scintilla position. If @a pos is negative and @a doc is not
 *             @c NULL, the current position of @a doc is used.
 *
 *  @return The new @c GeanyFileLocation object. The pivot of the returned
 *          object is the document and Scintilla position parameters. The object
 *          should be eventually freed with a call to @c fileloc_free .
 **/
GeanyFileLocation *fileloc_new_from_doc_pos_or_current(GeanyDocument *doc, gint pos)
{
	GeanyFileLocation *fileloc;

	if (doc == NULL)
	{
		doc = document_get_current();
		pos = -1;
	}

	if (pos < 0 && doc != NULL && doc->is_valid)
	{
		g_assert(doc->editor != NULL && doc->editor->sci != NULL);

		pos = sci_get_current_position(doc->editor->sci);
	}

	fileloc = fileloc_new();
	fileloc_set_document(fileloc, doc);
	fileloc_set_pos(fileloc, pos);

	return fileloc;
}

/*
 *  Clears all the parameters of a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object to clear. On output, none of
 *                 the parameters of @a fileloc has an associated value and its
 *                 pivot is unspecifed.
 **/
void fileloc_clear(GeanyFileLocation *fileloc)
{
	g_return_if_fail(fileloc != NULL);

	if (fileloc->file_pivot == GEANY_FLFP_FILENAME)
		g_free(fileloc->filename);

	init_fileloc(fileloc);
}

/*
 *  Assigns a copy of a @c GeanyFileLocation object to an existing object.
 *
 *  @param fileloc The @c GeanyFileLocation object to assign to. On return,
 *                 @a fileloc has the same pivot and parameter values as
 *                 @a src_fileloc. If @a src_fileloc is NULL, none of the
 *                 parameters of @a fileloc has an associated value and its
 *                 pivot is unspecifed.
 *  @param src_fileloc The @c GeanyFileLocation object to copy. Can be NULL.
 **/
void fileloc_assign(GeanyFileLocation *fileloc, const GeanyFileLocation *src_fileloc)
{
	g_return_if_fail(fileloc != NULL);

	if (fileloc == src_fileloc)
		return;

	fileloc_clear(fileloc);

	if (src_fileloc == NULL)
		return;

	copy_fileloc(fileloc, src_fileloc);
}

/*
 *  Frees a @c GeanyFilePos object.
 *
 *  @param fileloc The @c GeanyFileLocation object to free. Can be NULL, in
 *                 which case the function has no effect.
 **/
void fileloc_free(GeanyFileLocation *fileloc)
{
	if (fileloc)
	{
		fileloc_clear(fileloc);
		g_free(fileloc);
	}
}


/*
 *  Returns the file pivot of a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return The file pivot of @a fileloc .
 **/
GeanyFileLocationFilePivot fileloc_get_file_pivot(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, GEANY_FLFP_UNSPECIFIED);

	return (GeanyFileLocationFilePivot) fileloc->file_pivot;
}

/*
 *  Returns the location pivot of a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return The location pivot of @a fileloc .
 **/
GeanyFileLocationLocationPivot fileloc_get_location_pivot(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, GEANY_FLLP_UNSPECIFIED);

	return (GeanyFileLocationLocationPivot) fileloc->location_pivot;
}

/*
 *  Converts the pivot of a @c GeanyFileLocation object to a different set of
 *  parameters.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param file_pivot The new file pivot, or GEANY_FLFP_UNSPECIFIED to keep the
 *                    current file pivot.
 *  @param location_pivot The new location pivot, or GEANY_FLLP_UNSPECIFIED to
 *                        keep the current location pivot.
 *  @param force Whether to convert the pivot even if information is lost in the
 *               process.
 *
 *  @return If the conversion succeeds, returns TRUE. If the conversion fails,
 *          since @a force is FALSE and information would have been lost or
 *          otherwise, returns FALSE.
 *
 *  @remark If the conversion fails, @a fileloc is left unchanged.
 **/
gboolean fileloc_convert_pivot(GeanyFileLocation *fileloc,
								GeanyFileLocationFilePivot file_pivot,
								GeanyFileLocationLocationPivot location_pivot,
								gboolean force)
{
	GeanyDocument *doc = NULL;
	gchar *filename = NULL;

	g_return_val_if_fail(fileloc != NULL, FALSE);
	g_return_val_if_fail(is_valid_file_pivot(file_pivot, TRUE), FALSE);
	g_return_val_if_fail(is_valid_location_pivot(location_pivot, TRUE), FALSE);

	if (file_pivot == GEANY_FLFP_DOCUMENT)
	{
		/* We don't set the document parameter right away because the conversion
		 * might still fail. We'll set it at the end of the function. */
		doc = fileloc_get_document(fileloc);

		/* Make sure we don't lose information when no document is associated
		 * and force == FALSE. */
		if (! force && doc == NULL && fileloc_has_file(fileloc))
			return FALSE;
	}
	else if (file_pivot == GEANY_FLFP_FILENAME)
	{
		/* We don't set the filename parameter right away because the conversion
		 * might still fail. We'll set it at the end of the function. */
		filename = fileloc_get_filename(fileloc);

		/* Make sure we don't lose information when no filename is associated
		 * and force == FALSE. */
		if (! force && filename == NULL && fileloc_has_file(fileloc))
			return FALSE;
	}
	else
		g_assert(file_pivot == GEANY_FLFP_UNSPECIFIED);

	/* We only need to convert the location parameters if the location pivot
	 * changes. */
	if (location_pivot != GEANY_FLLP_UNSPECIFIED &&
		location_pivot != fileloc->location_pivot)
	{
		if (location_pivot == GEANY_FLLP_POS)
		{
			gint pos = fileloc_get_pos(fileloc);

			/* Make sure we don't lose information when no position is
			 * associated and force == FALSE. */
			if (! force && pos < 0 && fileloc_has_location(fileloc))
				return FALSE;

			fileloc_set_pos(fileloc, pos);
		}
		else
		{
			gint line;
			gint line_offset;

			/* Currently the only possible location pivots are either a
			 * Scintilla position or a line+offset pair. We already covered the
			 * former; now we cover the latter. If this ever changes, this code
			 * has to be adjusted. */
			g_assert(is_line_offset_location_pivot(location_pivot));

			line = fileloc_get_line(fileloc);
			line_offset = fileloc_get_line_offset(
				fileloc,
				location_pivot_to_line_offset_kind(location_pivot)
			);

			/* Make sure no information is lost when force == FALSE. */
			if (! force)
			{
				if (fileloc->location_pivot == GEANY_FLLP_POS)
				{
					if (line < 0 || line_offset < 0)
						return FALSE;
				}
				else
				{
					g_assert(is_line_offset_location_pivot(fileloc->location_pivot));

					if ((line < 0 && fileloc->line >= 0) ||
						(line_offset < 0 && fileloc->line_offset >= 0))
						return FALSE;
				}
			}

			fileloc_set_line(fileloc, line);
			fileloc_set_line_offset(
				fileloc,
				location_pivot_to_line_offset_kind(location_pivot),
				line_offset
			);
		}
	}

	/* Now we can set the file parameter, if necessary. */
	if (file_pivot == GEANY_FLFP_DOCUMENT)
		fileloc_set_document(fileloc, doc);
	else if (file_pivot == GEANY_FLFP_FILENAME)
	{
		fileloc_set_filename(fileloc, filename);
		g_free(filename);
	}
	else
		g_assert(file_pivot == GEANY_FLFP_UNSPECIFIED);

	return TRUE;
}


/*
 *  Checks if a @c GeanyFileLocation object has an associated file.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return If @a fileloc has at least one file parameter with an associated
 *          value, returns TRUE. Otherwise, returns FALSE.
 **/
gboolean fileloc_has_file(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, FALSE);

	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT)
		return fileloc->doc != NULL;
	else
	{
		g_assert(fileloc->file_pivot == GEANY_FLFP_FILENAME);

		return fileloc->filename != NULL;
	}
}


/*
 *  Returns the document object associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return Returns the document assocaited with @a fileloc, or NULL if no
 *          document is associated.
 **/
GeanyDocument *fileloc_get_document(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, NULL);

	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT)
		return fileloc->doc;
	else
	{
		g_assert(fileloc->file_pivot == GEANY_FLFP_FILENAME);

		if (fileloc->filename == NULL)
			return NULL;
		else
			return document_find_by_filename(fileloc->filename);
	}
}

/*
 *  Sets the document object associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param doc The document to associate with @a fileloc, or NULL to associate
 *             no document.
 *
 *  @remark On output, the file pivot of @a fileloc is GEANY_FLFP_DOCUMENT.
 **/
void fileloc_set_document(GeanyFileLocation *fileloc, GeanyDocument *doc)
{
	g_return_if_fail(fileloc != NULL);

	if (fileloc->file_pivot == GEANY_FLFP_FILENAME)
		g_free(fileloc->filename);

	fileloc->file_pivot = GEANY_FLFP_DOCUMENT;
	fileloc->doc = doc;
}


/*
 *  Returns the filename associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return Returns the filename assocaited with @a fileloc in UTF-8 encoding,
 *          or NULL if no filename is associated. The returned string should
 *          be freed by the caller.
 **/
gchar *fileloc_get_filename(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, NULL);

	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT)
	{
		if (fileloc->doc == NULL)
			return NULL;
		else
			return g_strdup(fileloc->doc->file_name);
	}
	else
	{
		g_assert(fileloc->file_pivot == GEANY_FLFP_FILENAME);

		return g_strdup(fileloc->filename);
	}
}

/*
 *  Returns the filename associated with a @c GeanyFileLocation object using
 *  locale encoding.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return Returns the filename assocaited with @a fileloc in locale encoding,
 *          or NULL if no filename is associated. The returned string should
 *          be freed by the caller.
 **/
gchar *fileloc_get_locale_filename(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, NULL);

	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT)
	{
		if (fileloc->doc == NULL)
			return NULL;
		else
			return utils_get_locale_from_utf8(fileloc->doc->file_name);
	}
	else
	{
		g_assert(fileloc->file_pivot == GEANY_FLFP_FILENAME);

		return utils_get_locale_from_utf8(fileloc->filename);
	}
}

/*
 *  Sets the filename associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param filename The filename to associate with @a fileloc in UTF-8 encoding,
 *                  or NULL to associate no filename.
 *
 *  @remark On output, the file pivot of @a fileloc is GEANY_FLFP_FILENAME.
 **/
void fileloc_set_filename(GeanyFileLocation *fileloc, const gchar *filename)
{
	g_return_if_fail(fileloc != NULL);

	if (fileloc->file_pivot == GEANY_FLFP_FILENAME)
	{
		if (filename == fileloc->filename)
			return;

		g_free(fileloc->filename);
	}

	fileloc->file_pivot = GEANY_FLFP_FILENAME;
	fileloc->filename = g_strdup(filename);
}

/*
 *  Sets the filename associated with a @c GeanyFileLocation object using locale
 *  encoding.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param filename The filename to associate with @a fileloc in locale
 *                  encoding, or NULL to associate no filename.
 *
 *  @remark On output, the file pivot of @a fileloc is GEANY_FLFP_FILENAME.
 **/
void fileloc_set_locale_filename(GeanyFileLocation *fileloc, const gchar *locale_filename)
{
	char *filename;

	g_return_if_fail(fileloc != NULL);

	filename = utils_get_utf8_from_locale(locale_filename);

	if (fileloc->file_pivot == GEANY_FLFP_FILENAME)
		g_free(fileloc->filename);

	fileloc->file_pivot = GEANY_FLFP_FILENAME;
	fileloc->filename = filename;
}


/*
 *  Checks if a @c GeanyFileLocation object has an associated location.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return If @a fileloc has at least one location parameter with an associated
 *          value, returns TRUE. Otherwise, returns FALSE.
 **/
gboolean fileloc_has_location(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, FALSE);

	if (fileloc->location_pivot == GEANY_FLLP_POS)
		return fileloc->pos >= 0;
	else
	{
		g_assert(is_line_offset_location_pivot(fileloc->location_pivot));

		return fileloc->line >= 0 || fileloc->line_offset >= 0;
	}
}


/*
 *  Returns the Scintilla position associated with a @c GeanyFileLocation
 *  object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return Returns the Scintilla position associated with @a fileloc, or a
 *          negative value if no Scintilla position is associated.
 **/
gint fileloc_get_pos(const GeanyFileLocation *fileloc)
{
	g_return_val_if_fail(fileloc != NULL, -1);

	/* If the location pivot is a Scintilla position, we simply return it. */
	if (fileloc->location_pivot == GEANY_FLLP_POS)
		return fileloc->pos;

	/* Otherwise, try to convert the location parameters to a Scintilla
	 * position. */
	return get_pos_from_fileloc_and_doc(fileloc, fileloc_get_document(fileloc));
}

/*
 *  Sets the Scintilla position associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param pos The Scintilla position to associate with @a fileloc, or a
 *             negative value to associate no Scintilla position.
 *
 *  @remark On output, the location pivot of @a fileloc is GEANY_FLLP_POS.
 **/
void fileloc_set_pos(GeanyFileLocation *fileloc, gint pos)
{
	g_return_if_fail(fileloc != NULL);

	fileloc->location_pivot = GEANY_FLLP_POS;
	fileloc->pos = pos;
}


/*
 *  Returns the line number associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *
 *  @return Returns the zero-based line number associated with @a fileloc, or a
 *          negative value if no line number is associated.
 **/
gint fileloc_get_line(const GeanyFileLocation *fileloc)
{
	GeanyDocument *doc;
	gint pos;

	g_return_val_if_fail(fileloc != NULL, -1);

	/* If we have a line number, return it. */
	if (is_line_offset_location_pivot(fileloc->location_pivot))
		return fileloc->line;

	/* Otherwise we need a valid document. */
	doc = fileloc_get_document(fileloc);
	if (doc == NULL || ! doc->is_valid)
		return -1;
	g_assert(doc->editor != NULL && doc->editor->sci != NULL);

	/* We convert the location parameters into a Scintilla position and then
	 * convert the position back into a line number. */
	pos = get_pos_from_fileloc_and_doc(fileloc, doc);
	if (pos < 0)
		return -1;

	return sci_get_line_from_position(doc->editor->sci, pos);
}

/*
 *  Sets the line number associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param line The zero-based line number to associate with @a fileloc, or a
 *              negative value to associate no line number.
 *
 *  @remark If the location pivot of @a fileloc on input is one of
 *          GEANY_FLLP_LINE_*, the pivot and any associated line offset
 *          parameter remain unchnaged on output. Otherwise, the location pivot
 *          is one of GEANY_FLLP_LINE_* (which one is unspecified) and no line
 *          offset parameter is associated on output.
 **/
void fileloc_set_line(GeanyFileLocation *fileloc, gint line)
{
	g_return_if_fail(fileloc != NULL);

	if (! is_line_offset_location_pivot(fileloc->location_pivot))
	{
		fileloc->location_pivot = GEANY_FLLP_LINE_COLUMN;
		fileloc->line_offset = -1;
	}

	fileloc->line = line;
}


/*
 *  Returns the a line offset associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param offset_kind The kind of line offset to return.
 *
 *  @return Returns the zero-based line offset associated with @a fileloc, or a
 *          negative value if no line offset of the requested kind is
 *          associated.
 **/
gint fileloc_get_line_offset(const GeanyFileLocation *fileloc, GeanyFileLocationOffsetKind offset_kind)
{
	GeanyDocument *doc;
	ScintillaObject *sci;
	gint pos;
	gint line;

	g_return_val_if_fail(fileloc != NULL, -1);
	g_return_val_if_fail(is_valid_line_offset_kind(offset_kind), -1);

	/* If we have a line offset of the required kind, return it. */
	if (fileloc->location_pivot == line_offset_kind_to_location_pivot(offset_kind))
		return fileloc->line_offset;
	/* Otherwise, if we have a line+offset location pivot... */
	else if (is_line_offset_location_pivot(fileloc->location_pivot))
	{
		/* If we have a zero line offset, any kind of line offset would be zero,
		 * so we can simply return that. */
		if (fileloc->line_offset == 0)
			return 0;
		/* Otherwose, if we don't have an associated line number or line offset,
		 * fail early. */
		else if (fileloc->line < 0 || fileloc->line_offset < 0)
			return -1;
	}

	/* We need a valid document to perform the conversion. */
	doc = fileloc_get_document(fileloc);
	if (doc == NULL || ! doc->is_valid)
		return -1;
	g_assert(doc->editor != NULL && doc->editor->sci != NULL);
	sci = doc->editor->sci;

	/* We convert the location parameters into a Scintilla position and then
	 * convert the position back into a line offset of the requested kind. */
	pos = get_pos_from_fileloc_and_doc(fileloc, doc);
	if (pos < 0)
		return -1;
	else
	{
		/* Supposedly, Scintilla has some bugs when querying with an
		 * out-of-bounds position, so clamp it. */
		gint max_pos = sci_get_length(sci);

		pos = MIN(pos, max_pos);
	}

	/* Get the line number; we need it for most conversions. */
	if (is_line_offset_location_pivot(fileloc->location_pivot))
		line = fileloc->line;
	else
		line = sci_get_line_from_position(sci, pos);

	switch (offset_kind)
	{
		case GEANY_FLOK_COLUMN:
			return sci_get_col_from_position(sci, pos);

		case GEANY_FLOK_CHARACTER:
		{
			gchar *text;
			gint char_offset;

			text = sci_get_contents_range(
				sci,
				sci_get_position_from_line(sci, line),
				pos
			);
			char_offset = g_utf8_strlen(text, -1);
			g_free(text);

			return char_offset;
		}

		case GEANY_FLOK_UTF8_BYTE:
			return pos - sci_get_position_from_line(sci, line);

		case GEANY_FLOK_ENCODING_BYTE:
		{
			if (doc->encoding == NULL || utils_str_equal(doc->encoding, "UTF-8") ||
				utils_str_equal(doc->encoding, encodings[GEANY_ENCODING_NONE].charset))
				return pos - sci_get_position_from_line(sci, line);
			else
			{
				gchar *text_utf8;
				gchar *text_enc;
				gsize len;
				GError *error = NULL;

				/* Convert from UTF-8 to the document's encoding. */
				text_utf8 = sci_get_contents_range(
					sci,
					sci_get_position_from_line(sci, line),
					pos);
				text_enc = g_convert(text_utf8, -1, doc->encoding, "UTF-8",
										NULL, &len, &error);
				g_free(text_utf8);
				g_free(text_enc);

				if (error != NULL)
				{
					g_error_free(error);
					return -1;
				}

				return len;
			}
		}

		case GEANY_FLOK_LOCALE_BYTE:
		{
			gchar *text_utf8;
			gchar *text_loc;
			gsize len;
			GError *error = NULL;

			/* Convert from UTF-8 to the current locale. */
			text_utf8 = sci_get_contents_range(
				sci,
				sci_get_position_from_line(sci, line),
				pos);
			text_loc = g_locale_from_utf8(text_utf8, -1, NULL, &len, &error);
			g_free(text_utf8);
			g_free(text_loc);

			if (error != NULL)
			{
				g_error_free(error);
				return -1;
			}

			return len;
		}

		default:
			g_assert_not_reached();
			return -1;
	}
}

/*
 *  Sets a line offset associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param offset_kind The kind of line offset to set.
 *  @param offset The zero-based line offset to associate with @a fileloc, or a
 *                negative value to associate no line offset.
 *
 *  @remark If the location pivot of @a fileloc on input is one of
 *          GEANY_FLLP_LINE_*, line number remains unchanged on output.
 *          Otherwise, no line number is associated on output. The location
 *          pivot on output depends on the value of @a offset_kind according to
 *          the following table:
 *
 *          @a offset_kind           | Location pivot
 *          ------------------------ | -----------------------------
 *          GEANY_FLOK_COLUMN        | GEANY_FLLP_LINE_COLUMN
 *          GEANY_FLOK_CHARACTER     | GEANY_FLLP_LINE_CHARACTER
 *          GEANY_FLOK_UTF8_BYTE     | GEANY_FLLP_LINE_UTF8_BYTE
 *          GEANY_FLOK_ENCODING_BYTE | GEANY_FLLP_LINE_ENCODING_BYTE
 *          GEANY_FLOK_LOCALE_BYTE   | GEANY_FLLP_LINE_LOCALE_BYTE
 **/
void fileloc_set_line_offset(GeanyFileLocation *fileloc, GeanyFileLocationOffsetKind offset_kind, gint offset)
{
	g_return_if_fail(fileloc != NULL);
	g_return_if_fail(is_valid_line_offset_kind(offset_kind));

	if (! is_line_offset_location_pivot(fileloc->location_pivot))
		fileloc->line = -1;

	fileloc->location_pivot = line_offset_kind_to_location_pivot(offset_kind);
	fileloc->line_offset = offset;
}


/*
 *  Sanitizes the parameters of a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param clamp Whether to clamp out-of-range numeric parameters to the
 *               nearest legal value or to disassociate them.
 *
 *  @return Returns TRUE if all the associated parameters contain valid,
 *          within-range, values, or FALSE if some of the associated parameters
 *          might contain invalid values.
 *
 *  @remark The function verifies that the associated document is valid. The
 *          function clears the line offset parameters if no line number is
 *          associated. The function attempts to adjust the associated location
 *          parameters to their valid range (e.g. so that the line number is not
 *          greater than the number of lines in the file). The function might
 *          fail to do that if there is no associated document to draw the
 *          necessary data from. The function does not verify that the
 *          associated filename exists.
 **/
gboolean fileloc_sanitize(GeanyFileLocation *fileloc, gboolean clamp)
{
	GeanyDocument *doc;
	ScintillaObject *sci;

	g_return_val_if_fail(fileloc != NULL, FALSE);

	/* If we have a document pivot and the document is invalid, disassociate
	 * it. */
	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT &&
		fileloc->doc != NULL && ! fileloc->doc->is_valid)
	{
		fileloc->doc = NULL;
	}

	/* If we don't have any location parameters, there's nothing more to do. */
	if (! fileloc_has_location(fileloc))
		return TRUE;
	/* Otherwise, if we have a line+offset location pivot but no line number,
	 * clear the line offset and return. */
	else if (is_line_offset_location_pivot(fileloc->location_pivot) && fileloc->line < 0)
	{
		fileloc->line_offset = -1;

		return TRUE;
	}

	/* Otherwise, we must have a valid document to verify the location
	 * parameters. */
	doc = fileloc_get_document(fileloc);
	if (doc == NULL || ! doc->is_valid)
		return FALSE;
	g_assert(doc->editor != NULL && doc->editor->sci != NULL);
	sci = doc->editor->sci;

	if (fileloc->location_pivot == GEANY_FLLP_POS)
	{
		/* If we have a Scintilla position location pivot, make sure it's within
		 * range. */
		gint max_pos = sci_get_length(sci);

		if (fileloc->pos > max_pos)
			fileloc->pos = clamp ? max_pos : -1;
	}
	else
	{
		gint max_line;

		/* Otherwise, we have a line+offset location pivot. */
		g_assert(is_line_offset_location_pivot(fileloc->location_pivot));
		/* We already handled the case where fileloc->line is negative. */
		g_assert(fileloc->line >= 0);

		/* Make sure the line number is within bounds. */
		max_line = sci_get_line_count(sci);
		if (fileloc->line > max_line)
		{
			if (clamp)
				fileloc->line = max_line;
			else
			{
				fileloc->line = -1;
				fileloc->line_offset = -1;

				return TRUE;
			}
		}

		/* If there is an associated positive line offset, make sure it's within
		 * bounds. */
		if (fileloc->line_offset > 0)
		{
			gint line_offset_max = -1;
			gint line_pos = sci_get_position_from_line(sci, fileloc->line);
			gint line_end_pos = sci_get_line_end_position(sci, fileloc->line);

			switch (fileloc->location_pivot)
			{
				case GEANY_FLLP_LINE_COLUMN:
					line_offset_max = sci_get_col_from_position(sci, line_end_pos);
					break;

				case GEANY_FLLP_LINE_CHARACTER:
				{
					gchar *line_text = sci_get_contents_range(sci, line_pos, line_end_pos);

					line_offset_max = g_utf8_strlen(line_text, -1);

					g_free(line_text);

					break;
				}

				case GEANY_FLLP_LINE_UTF8_BYTE:
					line_offset_max = line_end_pos - line_pos;
					break;

				case GEANY_FLLP_LINE_ENCODING_BYTE:
				{
					if (doc->encoding == NULL || utils_str_equal(doc->encoding, "UTF-8") ||
						utils_str_equal(doc->encoding, encodings[GEANY_ENCODING_NONE].charset))
					{
						line_offset_max = line_end_pos - line_pos;
					}
					else
					{
						gchar *text_utf8;
						gchar *text_enc;
						gsize len;
						GError *error = NULL;

						/* Convert from UTF-8 to the document's encoding. */
						text_utf8 = sci_get_contents_range(
							sci,
							line_pos,
							line_end_pos);
						text_enc = g_convert(text_utf8, -1, doc->encoding, "UTF-8",
												NULL, &len, &error);
						g_free(text_utf8);
						g_free(text_enc);

						if (error != NULL)
						{
							g_error_free(error);
							return FALSE;
						}

						line_offset_max = (gint) len;
					}

					break;
				}

				case GEANY_FLLP_LINE_LOCALE_BYTE:
				{
					gchar *text_utf8;
					gchar *text_loc;
					gsize len;
					GError *error = NULL;

					/* Convert from UTF-8 to the current locale. */
					text_utf8 = sci_get_contents_range(
						sci,
						line_pos,
						line_end_pos);
					text_loc = g_locale_from_utf8(text_utf8, -1, NULL, &len, &error);
					g_free(text_utf8);
					g_free(text_loc);

					if (error != NULL)
					{
						g_error_free(error);
						return FALSE;
					}

					line_offset_max = (gint) len;

					break;
				}

				default: g_assert_not_reached();
			}

			if (fileloc->line_offset > line_offset_max)
				fileloc->line_offset = clamp ? line_offset_max : -1;
		}
	}

	return TRUE;
}

/*
 *  Fills the missing parameter categories of a @c GeanyFileLocation object with
 *  the current document and position.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param fill_file Whether to associate the current document with @a fileloc
 *                   in case it doesn't have an associated file parameter.
 *  @param fill_location Whether to associate the current position of the
 *                       document associated with @a fileloc , if any, with
 *                       @a fileloc in case it doesn't have an associated
 *                       location parameter.
 *
 *  @return Returns TRUE if all the requested parameter categories have
 *          associated values on output. Otherwise, returns FALSE.
 */
gboolean fileloc_fill_missing_params(GeanyFileLocation *fileloc,
										gboolean fill_file,
										gboolean fill_location)
{
	g_return_val_if_fail(fileloc != NULL, FALSE);

	if (fill_file && ! fileloc_has_file(fileloc))
		fileloc_set_document(fileloc, document_get_current());

	if (fill_location && ! fileloc_has_location(fileloc))
	{
		GeanyDocument *doc = fileloc_get_document(fileloc);

		if (doc == NULL || ! doc->is_valid)
			return FALSE;
		g_assert(doc->editor && doc->editor->sci);

		fileloc_set_pos(fileloc, sci_get_current_position(doc->editor->sci));
	}

	return ((fill_file && fileloc_has_file(fileloc)) == fill_file) &&
			((fill_location && fileloc_has_location(fileloc)) == fill_location);
}


/*
 *  Attempts to open the file associated with a @c GeanyFileLocation object.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param readonly Whether to open the document in read-only mode.
 *  @param ft The filetype for the document or @c NULL to auto-detect the
 *            filetype.
 *  @param forced_enc The file encoding to use or @c NULL to auto-detect the
 *                    file encoding.
 *  @param fall_back_to_current_doc_dir Whether to try to open the same file in
 *                                      the current document's directory in case
 *                                      the file associated with @a fileloc
 *                                      doesn't exist.
 *
 *  @return If a document was opened, or if there's already a document
 *          associated with @a fileloc, returns the document. Otherwise, returns
 *          NULL.
 **/
GeanyDocument *fileloc_open_document(const GeanyFileLocation *fileloc,
										gboolean readonly, GeanyFiletype *ft,
										const gchar *forced_enc,
										gboolean fall_back_to_current_doc_dir)
{
	GeanyDocument *doc;
	gchar *locale_filename;

	g_return_val_if_fail(fileloc != NULL, NULL);

	/* If the pivot is a document, return it. */
	if (fileloc->file_pivot == GEANY_FLFP_DOCUMENT)
		return fileloc->doc;

	/* Otherwise, if there's an assocaited document, return it. */
	doc = fileloc_get_document(fileloc);
	if (doc != NULL)
		return doc;

	/* Otherwise, we try to open the file. */
	locale_filename = fileloc_get_locale_filename(fileloc);
	if (locale_filename == NULL)
		return NULL;

	if (fall_back_to_current_doc_dir)
	{
		/* If the file doesn't exist, try looking for it in the current
		 * document's directory. */
		if (!g_file_test(locale_filename, G_FILE_TEST_EXISTS))
		{
			gchar *cur_dir = utils_get_current_file_dir_utf8();
			gchar *name;

			if (cur_dir)
			{
				SETPTR(cur_dir, utils_get_locale_from_utf8(cur_dir));
				name = g_path_get_basename(locale_filename);
				SETPTR(name, g_build_path(G_DIR_SEPARATOR_S, cur_dir, name, NULL));
				g_free(cur_dir);

				if (g_file_test(name, G_FILE_TEST_EXISTS))
				{
					/* Use UTF-8 filename for the statusbar message. */
					char *filename = fileloc_get_filename(fileloc);
					g_assert(filename != NULL);

					ui_set_statusbar(
						FALSE,
						_("Could not find file '%s' - trying the current document path."),
						filename
					);
					g_free(filename);

					SETPTR(locale_filename, name);
				}
				else
					g_free(name);
			}
		}
	}

	doc = document_open_file(locale_filename, readonly, ft, forced_enc);

	g_free(locale_filename);

	return doc;
}

/*
 *  Attempts to open the file associated with a @c GeanyFileLocation object and
 *  uses the opened document as the file pivot.
 *
 *  @param fileloc The @c GeanyFileLocation object.
 *  @param readonly Whether to open the document in read-only mode.
 *  @param ft The filetype for the document or @c NULL to auto-detect the
 *            filetype.
 *  @param forced_enc The file encoding to use or @c NULL to auto-detect the
 *                    file encoding.
 *  @param fall_back_to_current_doc_dir Whether to try to open the same file in
 *                                      the current document's directory in case
 *                                      the file associated with @a fileloc
 *                                      doesn't exist.
 *
 *  @return If a document was opened, or if there's already a document
 *          associated with @a fileloc, returns the document. Otherwise, returns
 *          NULL.
 *
 *  @remark On output, if the function doesn't return NULL, the returned
 *          document becomes the associated document of @a fileloc and is used
 *          as its file pivot.
 **/
GeanyDocument *fileloc_open_document_and_convert_pivot(
										GeanyFileLocation *fileloc,
										gboolean readonly, GeanyFiletype *ft,
										const gchar *forced_enc,
										gboolean fall_back_to_current_doc_dir)
{
	GeanyDocument *doc;

	g_return_val_if_fail(fileloc != NULL, NULL);

	doc = fileloc_open_document(fileloc, readonly, ft, forced_enc,
								fall_back_to_current_doc_dir);

	if (doc != NULL)
		fileloc_set_document(fileloc, doc);

	return doc;
}
