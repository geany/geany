/*
 *      printing.c - this file is part of Geany, a fast and lightweight IDE
 *
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


/*
 * GTK printing support
 * (basic code layout were adopted from Sylpheed's printing implementation, thanks)
 */

#include <math.h>
#include <time.h>
#include <string.h>

#include "geany.h"
#include "printing.h"
#include "document.h"
#include "sciwrappers.h"
#include "editor.h"
#include "sciwrappers.h"
#include "utils.h"
#include "support.h"
#include "dialogs.h"
#include "utils.h"
#include "ui_utils.h"
#include "msgwindow.h"


PrintingPrefs printing_prefs;


#define ROTATE_RGB(color) \
	(((color) & 0xFF0000) >> 16) + ((color) & 0x00FF00) + (((color) & 0x0000FF) << 16)
#define ADD_ATTR(l, a) \
	pango_attr_list_insert((l), (a)); \
	(a)->start_index = 0; \
	(a)->end_index = G_MAXUINT;


enum
{
	FORE = 0,
	BACK,
	BOLD,
	ITALIC,
	MAX_TYPES
};


/* document-related variables */
typedef struct
{
	GeanyDocument *doc;
	gint font_width;
	gint lines;
	gint n_pages;
	gint lines_per_page;
	gint max_line_number_margin;
	gint cur_line;
	gint cur_pos;
	gint styles[STYLE_MAX + 1][MAX_TYPES];
	gdouble line_height;
	/* whether we have a wrapped line on page end to take care of on next page */
	gboolean long_line;
	/* set in begin_print() to hold the time when printing was started to ensure all printed
	 * pages have the same date and time (in case of slow machines and many pages where rendering
	 * takes more than a second) */
	time_t print_time;
	PangoLayout *layout; /* commonly used layout object */
} DocInfo;

/* widget references for the custom widget in the print dialog */
typedef struct
{
	GtkWidget *check_print_linenumbers;
	GtkWidget *check_print_pagenumbers;
	GtkWidget *check_print_pageheader;
	GtkWidget *check_print_basename;
	GtkWidget *entry_print_dateformat;
} PrintWidgets;


static GtkPrintSettings *settings = NULL;
static GtkPageSetup *page_setup = NULL;



/* returns the "width" (count of needed characters) for the given number */
static gint get_line_numbers_arity(gint x)
{
	gint a = 0;
	while ((x /= 10) != 0)
		a++;
	return a;
}


/* split a RGB colour into the three colour components */
static void get_rgb_values(gint c, gint *r, gint *g, gint *b)
{
	c = ROTATE_RGB(c);
	if (interface_prefs.highlighting_invert_all)
		c = utils_invert_color(c);

	*r = c % 256;
	*g = (c & - 16711936) / 256;
	*b = (c & 0xff0000) / 65536;

	*r *= 257;
	*g *= 257;
	*b *= 257;
}


/* creates a commonly used layout object from the given context for use in get_page_count and
 * draw_page */
static PangoLayout *setup_pango_layout(GtkPrintContext *context, PangoFontDescription *desc)
{
	PangoLayout *layout;

	layout = gtk_print_context_create_pango_layout(context);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_spacing(layout, 0);
	pango_layout_set_attributes(layout, NULL);
	pango_layout_set_font_description(layout, desc);

	return layout;
}


static gboolean utils_font_desc_check_monospace(PangoContext *pc, PangoFontDescription *desc)
{
	PangoFontFamily **families;
	gint n_families, i;
	const gchar *font;
	gboolean ret = TRUE;

	font = pango_font_description_get_family(desc);
	pango_context_list_families(pc, &families, &n_families);
	for (i = 0; i < n_families; i++)
	{
		if (utils_str_equal(font, pango_font_family_get_name(families[i])))
		{
			if (!pango_font_family_is_monospace(families[i]))
			{
				ret = FALSE;
			}
		}
	}
	g_free(families);
	return ret;
}


/* We don't support variable width fonts (yet) */
static gint get_font_width(GtkPrintContext *context, PangoFontDescription *desc)
{
	PangoContext *pc;
	PangoFontMetrics *metrics;
	gint width;

	pc = gtk_print_context_create_pango_context(context);

	if (!utils_font_desc_check_monospace(pc, desc))
		dialogs_show_msgbox_with_secondary(GTK_MESSAGE_WARNING,
			_("The editor font is not a monospaced font!"),
			_("Text will be wrongly spaced."));

	metrics = pango_context_get_metrics(pc, desc, pango_context_get_language(pc));
	/** TODO is this the best result we can get? */
	/* digit and char width are mostly equal for monospace fonts, char width might be
	 * for dual width characters(e.g. Japanese) so use digit width to get sure we get the width
	 * for one character */
	width = pango_font_metrics_get_approximate_digit_width(metrics) / PANGO_SCALE;

	pango_font_metrics_unref(metrics);
	g_object_unref(pc);

	return width;
}


static gint get_page_count(GtkPrintContext *context, DocInfo *dinfo)
{
	gdouble width, height;
	gint layout_h;
	gint i, j, lines_left;
	gchar *line_buf;

	if (dinfo == NULL)
		return -1;

	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);

	if (printing_prefs.print_line_numbers)
		/* remove line number margin space from overall width */
		width -= dinfo->max_line_number_margin * dinfo->font_width;

	pango_layout_set_width(dinfo->layout, width * PANGO_SCALE);

	/* add test text to get line height */
	pango_layout_set_text(dinfo->layout, "Test 1", -1);
	pango_layout_get_size(dinfo->layout, NULL, &layout_h);
	if (layout_h <= 0)
	{
		geany_debug("Invalid layout_h (%d). Falling back to default height (%d)",
			layout_h, 100 * PANGO_SCALE);
		layout_h = 100 * PANGO_SCALE;
	}
	dinfo->line_height = (gdouble)layout_h / PANGO_SCALE;
	dinfo->lines_per_page = ceil((height - dinfo->line_height) / dinfo->line_height);
#ifdef GEANY_PRINT_DEBUG
	geany_debug("max lines_per_page: %d", dinfo->lines_per_page);
#endif
	if (printing_prefs.print_page_numbers)
		dinfo->lines_per_page -= 2;
	if (printing_prefs.print_page_header)
		dinfo->lines_per_page -= 3;

	lines_left = dinfo->lines_per_page;

	i = 0;
	for (j = 0; j < dinfo->lines; j++)
	{
		gint lines = 1;
		gint line_width;

		line_buf = sci_get_line(dinfo->doc->editor->sci, j);
		line_width = (g_utf8_strlen(line_buf, -1) + 1) * dinfo->font_width;
		if (line_width > width)
			lines = ceil(line_width / width);
#ifdef GEANY_PRINT_DEBUG
		if (lines != 1) geany_debug("%d %d", j+1, lines);
#endif

		while (lines_left < lines)
		{
			lines -= lines_left;
			lines_left = dinfo->lines_per_page;
			i++;
		}
		lines_left -= lines;
		g_free(line_buf);
	}

	return i + 1;
}


static void add_page_header(DocInfo *dinfo, cairo_t *cr, gint width, gint page_nr)
{
	gint ph_height = dinfo->line_height * 3;
	gchar *data;
	gchar *datetime;
	gchar *tmp_file_name = (dinfo->doc->file_name != NULL) ?
		dinfo->doc->file_name : GEANY_STRING_UNTITLED;
	gchar *file_name = (printing_prefs.page_header_basename) ?
		g_path_get_basename(tmp_file_name) : g_strdup(tmp_file_name);
	PangoLayout *layout = dinfo->layout;

	/* draw the frame */
	cairo_set_line_width(cr, 0.3);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, 2, 2, width - 4, ph_height - 4);
	cairo_stroke(cr);

	/* width - 8: 2px between doc border and frame border, 2px between frame border and text
	 * and this on left and right side, so (2 + 2) * 2 */
	pango_layout_set_width(layout, (width - 8) * PANGO_SCALE);

	if ((g_utf8_strlen(file_name, -1) * dinfo->font_width) >= ((width - 4) - (dinfo->font_width * 2)))
		/* if the filename is wider than the available space on the line, skip parts of it */
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_MIDDLE);

	data = g_strdup_printf("<b>%s</b>", file_name);
	pango_layout_set_markup(layout, data, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	cairo_move_to(cr, 4, dinfo->line_height * 0.5);
	pango_cairo_show_layout(cr, layout);
	g_free(data);
	g_free(file_name);

	data = g_strdup_printf(_("<b>Page %d of %d</b>"), page_nr + 1, dinfo->n_pages);
	pango_layout_set_markup(layout, data, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	cairo_move_to(cr, 4, dinfo->line_height * 1.5);
	pango_cairo_show_layout(cr, layout);
	g_free(data);

	datetime = utils_get_date_time(printing_prefs.page_header_datefmt, &(dinfo->print_time));
	if (G_LIKELY(NZV(datetime)))
	{
		data = g_strdup_printf("<b>%s</b>", datetime);
		pango_layout_set_markup(layout, data, -1);
		pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
		cairo_move_to(cr, 2, dinfo->line_height * 1.5);
		pango_cairo_show_layout(cr, layout);
		g_free(data);
	}
	g_free(datetime);

	/* reset layout and re-position cairo context */
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	pango_layout_set_ellipsize(layout, FALSE);
	pango_layout_set_justify(layout, FALSE);
	pango_layout_set_width(layout, width * PANGO_SCALE);
	cairo_move_to(cr, 0, dinfo->line_height * 3);
}


static void custom_widget_apply(GtkPrintOperation *operation, GtkWidget *widget, gpointer user_data)
{
	PrintWidgets *w = user_data;

	printing_prefs.print_line_numbers =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->check_print_linenumbers));

	printing_prefs.print_page_numbers =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->check_print_pagenumbers));

	printing_prefs.print_page_header =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->check_print_pageheader));

	printing_prefs.page_header_basename =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->check_print_basename));

	g_free(printing_prefs.page_header_datefmt);
	printing_prefs.page_header_datefmt =
		g_strdup(gtk_entry_get_text(GTK_ENTRY(w->entry_print_dateformat)));
}


static void on_page_header_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);
	PrintWidgets *w = user_data;

	gtk_widget_set_sensitive(w->check_print_basename, sens);
	gtk_widget_set_sensitive(w->entry_print_dateformat, sens);
}


static GtkWidget *create_custom_widget(GtkPrintOperation *operation, gpointer user_data)
{	/* copied from interface.c */
	GtkWidget *page;
	GtkWidget *frame33;
	GtkWidget *alignment36;
	GtkWidget *vbox30;
	GtkWidget *hbox10;
	GtkWidget *label203;
	PrintWidgets *w = user_data;

	gtk_print_operation_set_custom_tab_label(operation, _("Document Setup"));

	page = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(page), 5);

	w->check_print_linenumbers = gtk_check_button_new_with_mnemonic(_("Print line numbers"));
	gtk_box_pack_start(GTK_BOX(page), w->check_print_linenumbers, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(w->check_print_linenumbers, _("Add line numbers to the printed page"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->check_print_linenumbers), printing_prefs.print_line_numbers);

	w->check_print_pagenumbers = gtk_check_button_new_with_mnemonic(_("Print page numbers"));
	gtk_box_pack_start(GTK_BOX(page), w->check_print_pagenumbers, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(w->check_print_pagenumbers, _("Add page numbers at the bottom of each page. It takes 2 lines of the page."));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->check_print_pagenumbers), printing_prefs.print_page_numbers);

	w->check_print_pageheader = gtk_check_button_new_with_mnemonic(_("Print page header"));
	gtk_box_pack_start(GTK_BOX(page), w->check_print_pageheader, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(w->check_print_pageheader, _("Add a little header to every page containing the page number, the filename and the current date (see below). It takes 3 lines of the page."));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->check_print_pageheader), printing_prefs.print_page_header);
	g_signal_connect(w->check_print_pageheader, "toggled", G_CALLBACK(on_page_header_toggled), w);

	frame33 = gtk_frame_new(NULL);
	gtk_box_pack_start(GTK_BOX(page), frame33, FALSE, FALSE, 0);
	gtk_frame_set_label_align(GTK_FRAME(frame33), 0, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame33), GTK_SHADOW_NONE);

	alignment36 = gtk_alignment_new(0, 0.5, 1, 1);
	gtk_container_add(GTK_CONTAINER(frame33), alignment36);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment36), 0, 0, 12, 0);

	vbox30 = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(alignment36), vbox30);

	w->check_print_basename = gtk_check_button_new_with_mnemonic(_("Use the basename of the printed file"));
	gtk_box_pack_start(GTK_BOX(vbox30), w->check_print_basename, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(w->check_print_basename, _("Print only the basename(without the path) of the printed file"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->check_print_basename), printing_prefs.page_header_basename);

	hbox10 = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox30), hbox10, TRUE, TRUE, 0);

	label203 = gtk_label_new(_("Date format:"));
	gtk_box_pack_start(GTK_BOX(hbox10), label203, FALSE, FALSE, 0);

	w->entry_print_dateformat = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(w->entry_print_dateformat));
	gtk_box_pack_start(GTK_BOX(hbox10), w->entry_print_dateformat, TRUE, TRUE, 0);
	gtk_widget_set_tooltip_text(w->entry_print_dateformat, _("Specify a format for the date and time stamp which is added to the page header on each page. You can use any conversion specifiers which can be used with the ANSI C strftime function."));
	gtk_entry_set_text(GTK_ENTRY(w->entry_print_dateformat), printing_prefs.page_header_datefmt);

	on_page_header_toggled(GTK_TOGGLE_BUTTON(w->check_print_pageheader), w);
	gtk_widget_show_all(page);
	return page;
}


static void end_print(GtkPrintOperation *operation, GtkPrintContext *context, gpointer user_data)
{
	DocInfo *dinfo = user_data;

	if (dinfo == NULL)
		return;

	gtk_widget_hide(main_widgets.progressbar);
	g_object_unref(dinfo->layout);
}


static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, gpointer user_data)
{
	DocInfo *dinfo = user_data;
	PangoFontDescription *desc;
	gint i;
	gint style_max;

	if (dinfo == NULL)
		return;

	gtk_widget_show(main_widgets.progressbar);

	desc = pango_font_description_from_string(interface_prefs.editor_font);

	/* init dinfo fields */
	dinfo->lines = sci_get_line_count(dinfo->doc->editor->sci);
	dinfo->lines_per_page = 0;
	dinfo->cur_line = 0;
	dinfo->cur_pos = 0;
	dinfo->long_line = FALSE;
	dinfo->print_time = time(NULL);
	dinfo->max_line_number_margin = get_line_numbers_arity(dinfo->lines) + 1;
	/* increase font width by 1 (looks better) */
	dinfo->font_width = get_font_width(context, desc) + 1;
	/* create a PangoLayout to be commonly used in get_page_count and draw_page */
	dinfo->layout = setup_pango_layout(context, desc);
	/* this is necessary because of possible line breaks on the printed page and then
	 * lines_per_page differs from document line count */
	dinfo->n_pages = get_page_count(context, dinfo);

	/* read all styles from Scintilla */
	style_max = pow(2, scintilla_send_message(dinfo->doc->editor->sci, SCI_GETSTYLEBITS, 0, 0));
	/* if the lexer uses only the first 32 styles(style bits = 5),
	 * we need to add the pre-defined styles */
	if (style_max == 32)
		style_max = STYLE_LASTPREDEFINED;
	for (i = 0; i < style_max; i++)
	{
		dinfo->styles[i][FORE] = ROTATE_RGB(scintilla_send_message(
			dinfo->doc->editor->sci, SCI_STYLEGETFORE, i, 0));
		if (i == STYLE_LINENUMBER)
		{	/* ignore background colour for line number margin to avoid trouble with wrapped lines */
			dinfo->styles[STYLE_LINENUMBER][BACK] = ROTATE_RGB(scintilla_send_message(
				dinfo->doc->editor->sci, SCI_STYLEGETBACK, STYLE_DEFAULT, 0));
		}
		else
		{
			dinfo->styles[i][BACK] = ROTATE_RGB(scintilla_send_message(
				dinfo->doc->editor->sci, SCI_STYLEGETBACK, i, 0));
		}
		/* use white background color unless foreground is white to save ink */
		if (dinfo->styles[i][FORE] != 0xffffff)
			dinfo->styles[i][BACK] = 0xffffff;
		dinfo->styles[i][BOLD] =
			scintilla_send_message(dinfo->doc->editor->sci, SCI_STYLEGETBOLD, i, 0);
		dinfo->styles[i][ITALIC] =
			scintilla_send_message(dinfo->doc->editor->sci, SCI_STYLEGETITALIC, i, 0);
	}

	if (dinfo->n_pages >= 0)
		gtk_print_operation_set_n_pages(operation, dinfo->n_pages);

	pango_font_description_free(desc);
}


static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
					  gint page_nr, gpointer user_data)
{
	DocInfo *dinfo = user_data;
	GeanyEditor *editor;
	cairo_t *cr;
	gdouble width, height;
	gdouble x = 0.0, y = 0.0;
	/*gint layout_h;*/
	gint count;
	GString *str;

	if (dinfo == NULL || page_nr >= dinfo->n_pages)
		return;

	editor = dinfo->doc->editor;

	if (dinfo->n_pages > 0)
	{
		gdouble fraction = (page_nr + 1) / (gdouble) dinfo->n_pages;
		gchar *text = g_strdup_printf(_("Page %d of %d"), page_nr, dinfo->n_pages);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(main_widgets.progressbar), fraction);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main_widgets.progressbar), text);
		g_free(text);
	}

#ifdef GEANY_PRINT_DEBUG
	geany_debug("draw_page = %d, pages = %d, (real) lines_per_page = %d",
		page_nr, dinfo->n_pages, dinfo->lines_per_page);
#endif

	str = g_string_sized_new(256);
	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);

	cairo_set_source_rgb(cr, 0, 0, 0);
#ifdef GEANY_PRINT_DEBUG
	cairo_set_line_width(cr, 0.2);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_stroke(cr);
#endif
	cairo_move_to(cr, 0, 0);

	pango_layout_set_width(dinfo->layout, width * PANGO_SCALE);
	pango_layout_set_alignment(dinfo->layout, PANGO_ALIGN_LEFT);
	pango_layout_set_ellipsize(dinfo->layout, FALSE);
	pango_layout_set_justify(dinfo->layout, FALSE);

	if (printing_prefs.print_page_header)
		add_page_header(dinfo, cr, width, page_nr);

	count = 0;	/* the actual line counter for the current page, might be different from
				 * dinfo->cur_line due to possible line breaks */
	while (count < dinfo->lines_per_page)
	{
		gchar c = 'a';
		gint style = -1;
		PangoAttrList *layout_attr;
		PangoAttribute *attr;
		gint colours[3] = { 0 };
		gboolean add_linenumber = TRUE;
		gboolean at_eol;

		while (count < dinfo->lines_per_page && c != '\0')
		{
			at_eol = FALSE;

			g_string_erase(str, 0, str->len); /* clear the string */

			/* line numbers */
			if (printing_prefs.print_line_numbers && add_linenumber)
			{
				/* if we had a wrapped line on the last page which needs to be continued, don't
				 * add a line number */
				if (dinfo->long_line)
				{
					add_linenumber = FALSE;
				}
				else
				{
					gchar *line_number = NULL;
					gint cur_line_number_margin = get_line_numbers_arity(dinfo->cur_line + 1);
					gchar *fill = g_strnfill(
						dinfo->max_line_number_margin - cur_line_number_margin - 1, ' ');

					line_number = g_strdup_printf("%s%d ", fill, dinfo->cur_line + 1);
					g_string_append(str, line_number);
					dinfo->cur_line++; /* increase document line */
					add_linenumber = FALSE;
					style = STYLE_LINENUMBER;
					c = 'a'; /* dummy value */
					g_free(fill);
					g_free(line_number);
				}
			}
			/* data */
			else
			{
				style = sci_get_style_at(dinfo->doc->editor->sci, dinfo->cur_pos);
				c = sci_get_char_at(dinfo->doc->editor->sci, dinfo->cur_pos);
				if (c == '\0' || style == -1)
				{	/* if c gets 0, we are probably out of document boundaries,
					 * so stop to break out of outer loop */
					count = dinfo->lines_per_page;
					break;
				}
				dinfo->cur_pos++;

				/* convert tabs to spaces which seems to be better than using Pango tabs */
				if (c == '\t')
				{
					gint tab_width = sci_get_tab_width(editor->sci);
					gchar *s = g_strnfill(tab_width, ' ');
					g_string_append(str, s);
					g_free(s);
				}
				/* don't add line breaks, they are handled manually below */
				else if (c == '\r' || c == '\n')
				{
					gchar c_next = sci_get_char_at(dinfo->doc->editor->sci, dinfo->cur_pos);
					at_eol = TRUE;
					if (c == '\r' && c_next == '\n')
						dinfo->cur_pos++; /* skip LF part of CR/LF */
				}
				else
				{
					g_string_append_c(str, c); /* finally add the character */

					/* handle UTF-8: since we add char by char (better: byte by byte), we need to
					 * keep UTF-8 characters together(e.g. two bytes for one character)
					 * the input is always UTF-8 and c is signed, so all non-Ascii
					 * characters are less than 0 and consist of all bytes less than 0.
					 * style doesn't change since it is only one character with multiple bytes. */
					while (c < 0)
					{
						c = sci_get_char_at(dinfo->doc->editor->sci, dinfo->cur_pos);
						if (c < 0)
						{	/* only add the byte when it is part of the UTF-8 character
							 * otherwise we could add e.g. a '\n' and it won't be visible in the
							 * printed document */
							g_string_append_c(str, c);
							dinfo->cur_pos++;
						}
					}
				}
			}

			if (! at_eol)
			{
				/* set text */
				pango_layout_set_text(dinfo->layout, str->str, -1);
				/* attributes */
				layout_attr = pango_attr_list_new();
				/* foreground colour */
				get_rgb_values(dinfo->styles[style][FORE], &colours[0], &colours[1], &colours[2]);
				attr = pango_attr_foreground_new(colours[0], colours[1], colours[2]);
				ADD_ATTR(layout_attr, attr);
				/* background colour */
				get_rgb_values(dinfo->styles[style][BACK], &colours[0], &colours[1], &colours[2]);
				attr = pango_attr_background_new(colours[0], colours[1], colours[2]);
				ADD_ATTR(layout_attr, attr);
				/* bold text */
				if (dinfo->styles[style][BOLD])
				{
					attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
					ADD_ATTR(layout_attr, attr);
				}
				/* italic text */
				if (dinfo->styles[style][ITALIC])
				{
					attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
					ADD_ATTR(layout_attr, attr);
				}
				pango_layout_set_attributes(dinfo->layout, layout_attr);
				pango_layout_context_changed(dinfo->layout);
				pango_attr_list_unref(layout_attr);
			}

			cairo_get_current_point(cr, &x, &y);


			/* normal line break at eol character in document */
			if (at_eol)
			{
				/*pango_layout_get_size(dinfo->layout, NULL, &layout_h);*/
				/*cairo_move_to(cr, 0, y + (gdouble)layout_h / PANGO_SCALE);*/
				cairo_move_to(cr, 0, y + dinfo->line_height);

				count++;
				/* we added a new document line so request a new line number */
				add_linenumber = TRUE;
			}
			else
			{
				gint x_offset = 0;
				/* maybe we need to force a line break because of too long line */
				if (x >= (width - dinfo->font_width))
				{
					/* don't start the line at horizontal origin because we need to skip the
					 * line number margin */
					if (printing_prefs.print_line_numbers)
					{
						x_offset = (dinfo->max_line_number_margin + 1) * dinfo->font_width;
					}

					/*pango_layout_get_size(dinfo->layout, NULL, &layout_h);*/
					/*cairo_move_to(cr, x_offset, y + (gdouble)layout_h / PANGO_SCALE);*/
					/* this is faster but not exactly the same as above */
					cairo_move_to(cr, x_offset, y + dinfo->line_height);
					cairo_get_current_point(cr, &x, &y);
					count++;
				}
				if (count < dinfo->lines_per_page)
				{
					/* str->len is counted in bytes not characters, so use g_utf8_strlen() */
					x_offset = (g_utf8_strlen(str->str, -1) * dinfo->font_width);

					if (dinfo->long_line && count == 0)
					{
						x_offset = (dinfo->max_line_number_margin + 1) * dinfo->font_width;
						dinfo->long_line = FALSE;
					}

					pango_cairo_show_layout(cr, dinfo->layout);
					cairo_move_to(cr, x + x_offset, y);
				}
				else
				/* we are on a wrapped line but we are out of lines on this page, so continue
				 * the current line on the next page and remember to continue in current line */
					dinfo->long_line = TRUE;
			}
		}
	}

	if (printing_prefs.print_line_numbers)
	{	/* print a thin line between the line number margin and the data */
		gint y_start = 0;

		if (printing_prefs.print_page_header)
			y_start = (dinfo->line_height * 3) - 2;	/* "- 2": to connect the line number line to
													 * the page header frame */

		cairo_set_line_width(cr, 0.3);
		cairo_move_to(cr, (dinfo->max_line_number_margin * dinfo->font_width) + 1, y_start);
		cairo_line_to(cr, (dinfo->max_line_number_margin * dinfo->font_width) + 1,
			y + dinfo->line_height); /* y is last added line, we reuse it */
		cairo_stroke(cr);
	}

	if (printing_prefs.print_page_numbers)
	{
		gchar *line = g_strdup_printf("<small>- %d -</small>", page_nr + 1);
		pango_layout_set_markup(dinfo->layout, line, -1);
		pango_layout_set_alignment(dinfo->layout, PANGO_ALIGN_CENTER);
		cairo_move_to(cr, 0, height - dinfo->line_height);
		pango_cairo_show_layout(cr, dinfo->layout);
		g_free(line);

#ifdef GEANY_PRINT_DEBUG
		cairo_set_line_width(cr, 0.3);
		cairo_move_to(cr, 0, height - (1.25 * dinfo->line_height));
		cairo_line_to(cr, width - 1, height - (1.25 * dinfo->line_height));
		cairo_stroke(cr);
#endif
	}
	g_string_free(str, TRUE);
}


static void status_changed(GtkPrintOperation *op, gpointer data)
{
	gchar *filename = (data != NULL) ? data : GEANY_STRING_UNTITLED;
	if (gtk_print_operation_get_status(op) == GTK_PRINT_STATUS_FINISHED_ABORTED)
		msgwin_status_add(_("Did not send document %s to the printing subsystem."), filename);
	else if (gtk_print_operation_get_status(op) == GTK_PRINT_STATUS_FINISHED)
		msgwin_status_add(_("Document %s was sent to the printing subsystem."), filename);
}


static void printing_print_gtk(GeanyDocument *doc)
{
	GtkPrintOperation *op;
	GtkPrintOperationResult res = GTK_PRINT_OPERATION_RESULT_ERROR;
	GError *error = NULL;
	DocInfo *dinfo;
	PrintWidgets *widgets;

	/** TODO check for monospace font, detect the widest character in the font and
	  * use it at font_width */

	widgets = g_new0(PrintWidgets, 1);
	dinfo = g_new0(DocInfo, 1);
	/* all other fields are initialised in begin_print() */
	dinfo->doc = doc;

	op = gtk_print_operation_new();

	gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
	gtk_print_operation_set_show_progress(op, TRUE);
#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_print_operation_set_embed_page_setup(op, TRUE);
#endif

	g_signal_connect(op, "begin-print", G_CALLBACK(begin_print), dinfo);
	g_signal_connect(op, "end-print", G_CALLBACK(end_print), dinfo);
	g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), dinfo);
	g_signal_connect(op, "status-changed", G_CALLBACK(status_changed), doc->file_name);
	g_signal_connect(op, "create-custom-widget", G_CALLBACK(create_custom_widget), widgets);
	g_signal_connect(op, "custom-widget-apply", G_CALLBACK(custom_widget_apply), widgets);

	if (settings != NULL)
		gtk_print_operation_set_print_settings(op, settings);
	if (page_setup != NULL)
		gtk_print_operation_set_default_page_setup(op, page_setup);

	res = gtk_print_operation_run(
		op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(main_widgets.window), &error);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
	{
		if (settings != NULL)
			g_object_unref(settings);
		settings = g_object_ref(gtk_print_operation_get_print_settings(op));
		/* status message is printed in the status-changed handler */
	}
	else if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Printing of %s failed (%s)."),
							doc->file_name, error->message);
		g_error_free(error);
	}

	g_object_unref(op);
	g_free(dinfo);
	g_free(widgets);
}


void printing_page_setup_gtk(void)
{
	GtkPageSetup *new_page_setup;

	if (settings == NULL)
		settings = gtk_print_settings_new();

	new_page_setup = gtk_print_run_page_setup_dialog(
		GTK_WINDOW(main_widgets.window), page_setup, settings);

	if (page_setup != NULL)
		g_object_unref(page_setup);

	page_setup = new_page_setup;
}


/* simple file print using an external tool */
static void print_external(GeanyDocument *doc)
{
	gchar *cmdline;

	if (doc->file_name == NULL)
		return;

	if (! NZV(printing_prefs.external_print_cmd))
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Please set a print command in the preferences dialog first."));
		return;
	}

	cmdline = g_strdup(printing_prefs.external_print_cmd);
	utils_str_replace_all(&cmdline, "%f", doc->file_name);

	if (dialogs_show_question(
			_("The file \"%s\" will be printed with the following command:\n\n%s"),
			doc->file_name, cmdline))
	{
		GError *error = NULL;

#ifdef G_OS_WIN32
		gchar *tmp_cmdline = g_strdup(cmdline);
#else
		/* /bin/sh -c emulates the system() call and makes complex commands possible
		 * but only needed on non-win32 systems due to the lack of win32's shell capabilities */
		gchar *tmp_cmdline = g_strconcat("/bin/sh -c \"", cmdline, "\"", NULL);
#endif

		if (! g_spawn_command_line_async(tmp_cmdline, &error))
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Printing of \"%s\" failed (return code: %s)."),
				doc->file_name, error->message);
			g_error_free(error);
		}
		else
		{
			msgwin_status_add(_("File %s printed."), doc->file_name);
		}
		g_free(tmp_cmdline);
	}
	g_free(cmdline);
}


void printing_print_doc(GeanyDocument *doc)
{
	if (doc == NULL)
		return;

	if (printing_prefs.use_gtk_printing)
		printing_print_gtk(doc);
	else
		print_external(doc);
}
