/*
 *      printing.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2012 Colomban Wendling <ban(at)herbesfolles(dot)org>
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
 * GTK printing support
 * (basic code layout were adopted from Sylpheed's printing implementation, thanks)
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "printing.h"

#include "app.h"
#include "dialogs.h"
#include "document.h"
#include "geany.h"
#include "highlighting.h"
#include "msgwindow.h"
#include "sciwrappers.h"
#include "settings.h"
#include "spawn.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"

#include <math.h>
#include <time.h>
#include <string.h>


PrintingPrefs printing_prefs;


/* document-related variables */
typedef struct
{
	GeanyDocument *doc;
	ScintillaObject *sci;
	gdouble margin_width;
	gdouble line_height;
	/* set in begin_print() to hold the time when printing was started to ensure all printed
	 * pages have the same date and time (in case of slow machines and many pages where rendering
	 * takes more than a second) */
	time_t print_time;
	PangoLayout *layout; /* commonly used layout object */
	gdouble sci_scale;

	struct Sci_RangeToFormat fr;
	GArray *pages;
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


static void get_text_dimensions(PangoLayout *layout, const gchar *text, gdouble *width, gdouble *height)
{
	gint layout_w, layout_h;

	pango_layout_set_text(layout, text, -1);
	pango_layout_get_size(layout, &layout_w, &layout_h);
	if (layout_w <= 0)
	{
		gint default_w = 50 * strlen(text) * PANGO_SCALE;

		geany_debug("Invalid layout_w (%d). Falling back to default width (%d)",
			layout_w, default_w);
		layout_w = default_w;
	}
	if (layout_h <= 0)
	{
		gint default_h = 100 * PANGO_SCALE;

		geany_debug("Invalid layout_h (%d). Falling back to default height (%d)",
			layout_h, default_h);
		layout_h = default_h;
	}

	if (width)
		*width = (gdouble)layout_w / PANGO_SCALE;
	if (height)
		*height = (gdouble)layout_h / PANGO_SCALE;
}


static void add_page_header(DocInfo *dinfo, cairo_t *cr, gint width, gint page_nr)
{
	gint ph_height = dinfo->line_height * 3;
	gchar *data;
	gchar *datetime;
	const gchar *tmp_file_name = DOC_FILENAME(dinfo->doc);
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
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_MIDDLE);

	data = g_strdup_printf("<b>%s</b>", file_name);
	pango_layout_set_markup(layout, data, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	cairo_move_to(cr, 4, dinfo->line_height * 0.5);
	pango_cairo_show_layout(cr, layout);
	g_free(data);
	g_free(file_name);

	data = g_strdup_printf(_("<b>Page %d of %d</b>"), page_nr + 1, dinfo->pages->len);
	pango_layout_set_markup(layout, data, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	cairo_move_to(cr, 4, dinfo->line_height * 1.5);
	pango_cairo_show_layout(cr, layout);
	g_free(data);

	datetime = utils_get_date_time(printing_prefs.page_header_datefmt, &(dinfo->print_time));
	if (G_LIKELY(!EMPTY(datetime)))
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
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
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
	g_object_unref(dinfo->sci);
	g_object_unref(dinfo->layout);
	g_array_free(dinfo->pages, TRUE);
}


static void setup_range(DocInfo *dinfo, GtkPrintContext *ctx)
{
	dinfo->fr.hdc = dinfo->fr.hdcTarget = gtk_print_context_get_cairo_context(ctx);

	dinfo->fr.rcPage.left   = 0;
	dinfo->fr.rcPage.top    = 0;
	dinfo->fr.rcPage.right  = gtk_print_context_get_width(ctx);
	dinfo->fr.rcPage.bottom = gtk_print_context_get_height(ctx);

	dinfo->fr.rc.left   = dinfo->fr.rcPage.left;
	dinfo->fr.rc.top    = dinfo->fr.rcPage.top;
	dinfo->fr.rc.right  = dinfo->fr.rcPage.right;
	dinfo->fr.rc.bottom = dinfo->fr.rcPage.bottom;

	if (printing_prefs.print_page_header)
		dinfo->fr.rc.top += dinfo->line_height * 3; /* header height */
	if (printing_prefs.print_page_numbers)
		dinfo->fr.rc.bottom -= dinfo->line_height * 1; /* footer height */

	dinfo->fr.rcPage.left   /= dinfo->sci_scale;
	dinfo->fr.rcPage.top    /= dinfo->sci_scale;
	dinfo->fr.rcPage.right  /= dinfo->sci_scale;
	dinfo->fr.rcPage.bottom /= dinfo->sci_scale;
	dinfo->fr.rc.left   /= dinfo->sci_scale;
	dinfo->fr.rc.top    /= dinfo->sci_scale;
	dinfo->fr.rc.right  /= dinfo->sci_scale;
	dinfo->fr.rc.bottom /= dinfo->sci_scale;

	dinfo->fr.chrg.cpMin = 0;
	dinfo->fr.chrg.cpMax = sci_get_length(dinfo->sci);
}


static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, gpointer user_data)
{
	DocInfo *dinfo = user_data;
	PangoContext *pango_ctx, *widget_pango_ctx;
	PangoFontDescription *desc;
	gdouble pango_res, widget_res;
	gchar *font_name;

	if (dinfo == NULL)
		return;

	gtk_widget_show(main_widgets.progressbar);

	/* init dinfo fields */

	/* setup printing scintilla object */
	dinfo->sci = editor_create_widget(dinfo->doc->editor);
	/* since we won't add the widget to any container, assume it's ownership */
	g_object_ref_sink(dinfo->sci);
	scintilla_send_message(dinfo->sci, SCI_SETDOCPOINTER, 0,
			scintilla_send_message(dinfo->doc->editor->sci, SCI_GETDOCPOINTER, 0, 0));
	highlighting_set_styles(dinfo->sci, dinfo->doc->file_type);
	sci_set_line_numbers(dinfo->sci, printing_prefs.print_line_numbers);
	scintilla_send_message(dinfo->sci, SCI_SETVIEWWS, SCWS_INVISIBLE, 0);
	scintilla_send_message(dinfo->sci, SCI_SETVIEWEOL, FALSE, 0);
	scintilla_send_message(dinfo->sci, SCI_SETEDGEMODE, EDGE_NONE, 0);
	scintilla_send_message(dinfo->sci, SCI_SETPRINTCOLOURMODE, SC_PRINT_COLOURONWHITE, 0);

	/* Scintilla doesn't respect the context resolution, so we'll scale ourselves.
	 * Actually Scintilla simply doesn't know about the resolution since it creates its own
	 * Pango context out of the Cairo target, and the resolution is in the GtkPrintOperation's
	 * Pango context */
	pango_ctx = gtk_print_context_create_pango_context(context);
	pango_res = pango_cairo_context_get_resolution(pango_ctx);
	g_object_unref(pango_ctx);
	widget_pango_ctx = gtk_widget_get_pango_context(GTK_WIDGET(dinfo->sci));
	widget_res = pango_cairo_context_get_resolution(widget_pango_ctx);
	/* On Windows, for some reason the widget's resolution is -1, so follow
	 * Pango docs and peek the font map's one. */
	if (widget_res < 0)
	{
		widget_res = pango_cairo_font_map_get_resolution(
			(PangoCairoFontMap*) pango_context_get_font_map(widget_pango_ctx));
	}
	dinfo->sci_scale = pango_res / widget_res;

	dinfo->pages = g_array_new(FALSE, FALSE, sizeof(gint));

	dinfo->print_time = time(NULL);
	/* create a PangoLayout to be commonly used in add_page_header() and draw_page() */
	font_name = settings_get_string("editor-font");
	desc = pango_font_description_from_string(font_name);
	g_free(font_name);
	dinfo->layout = setup_pango_layout(context, desc);
	pango_font_description_free(desc);
	get_text_dimensions(dinfo->layout, "|XMfjgq_" /* reasonably representative character set */,
		NULL, &dinfo->line_height);
	get_text_dimensions(dinfo->layout, "99999 " /* Scintilla resets the margin to the width of "99999" when printing */,
		&dinfo->margin_width, NULL);
	/* setup dinfo->fr */
	setup_range(dinfo, context);
}


static gint format_range(DocInfo *dinfo, gboolean draw)
{
	gint pos;

	cairo_save(dinfo->fr.hdc);
	cairo_scale(dinfo->fr.hdc, dinfo->sci_scale, dinfo->sci_scale);
	pos = (gint) scintilla_send_message(dinfo->sci, SCI_FORMATRANGE, draw, (sptr_t) &dinfo->fr);
	cairo_restore(dinfo->fr.hdc);

	return pos;
}


static gboolean paginate(GtkPrintOperation *operation, GtkPrintContext *context, gpointer user_data)
{
	DocInfo *dinfo = user_data;

	/* for whatever reason we get called one more time after we returned TRUE, so avoid adding
	 * an empty page at the end */
	if (dinfo->fr.chrg.cpMin >= dinfo->fr.chrg.cpMax)
		return TRUE;

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(main_widgets.progressbar));
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main_widgets.progressbar), _("Paginating"));

	g_array_append_val(dinfo->pages, dinfo->fr.chrg.cpMin);
	dinfo->fr.chrg.cpMin = format_range(dinfo, FALSE);

	gtk_print_operation_set_n_pages(operation, dinfo->pages->len);

	return dinfo->fr.chrg.cpMin >= dinfo->fr.chrg.cpMax;
}


static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
					  gint page_nr, gpointer user_data)
{
	DocInfo *dinfo = user_data;
	cairo_t *cr;
	gdouble width, height;

	g_return_if_fail(dinfo != NULL);
	g_return_if_fail((guint)page_nr < dinfo->pages->len);

	if (dinfo->pages->len > 0)
	{
		gdouble fraction = (page_nr + 1) / (gdouble) dinfo->pages->len;
		gchar *text = g_strdup_printf(_("Page %d of %d"), page_nr + 1, dinfo->pages->len);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(main_widgets.progressbar), fraction);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main_widgets.progressbar), text);
		g_free(text);
	}

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);

	if (printing_prefs.print_page_header)
		add_page_header(dinfo, cr, width, page_nr);

	dinfo->fr.chrg.cpMin = g_array_index(dinfo->pages, gint, page_nr);
	if ((guint)page_nr + 1 < dinfo->pages->len)
		dinfo->fr.chrg.cpMax = g_array_index(dinfo->pages, gint, page_nr + 1) - 1;
	else /* it's the last page, print 'til the end */
		dinfo->fr.chrg.cpMax = sci_get_length(dinfo->sci);

	format_range(dinfo, TRUE);

	/* reset color */
	cairo_set_source_rgb(cr, 0, 0, 0);

	if (printing_prefs.print_line_numbers)
	{	/* print a thin line between the line number margin and the data */
		gdouble y1 = dinfo->fr.rc.top * dinfo->sci_scale;
		gdouble y2 = dinfo->fr.rc.bottom * dinfo->sci_scale;
		gdouble x = dinfo->fr.rc.left * dinfo->sci_scale + dinfo->margin_width;

		if (printing_prefs.print_page_header)
			y1 -= 2 - 0.3;	/* to connect the line number line to the page header frame,
							 * 2 is the border, and 0.3 the line width */

		cairo_set_line_width(cr, 0.3);
		cairo_move_to(cr, x, y1);
		cairo_line_to(cr, x, y2);
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
	}
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
	static const DocInfo dinfo0;
	DocInfo dinfo = dinfo0;
	PrintWidgets *widgets;

	/** TODO check for monospace font, detect the widest character in the font and
	  * use it at font_width */

	widgets = g_new0(PrintWidgets, 1);
	/* all other fields are initialised in begin_print() */
	dinfo.doc = doc;

	op = gtk_print_operation_new();

	gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
	gtk_print_operation_set_show_progress(op, TRUE);
	gtk_print_operation_set_embed_page_setup(op, TRUE);

	g_signal_connect(op, "begin-print", G_CALLBACK(begin_print), &dinfo);
	g_signal_connect(op, "end-print", G_CALLBACK(end_print), &dinfo);
	g_signal_connect(op, "paginate", G_CALLBACK(paginate), &dinfo);
	g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), &dinfo);
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

	if (EMPTY(printing_prefs.external_print_cmd))
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
		/* /bin/sh -c emulates the system() call and makes complex commands possible
		 * but only on non-win32 systems due to the lack of win32's shell capabilities */
	#ifdef G_OS_UNIX
		gchar *argv[] = { "/bin/sh", "-c", cmdline, NULL };

		if (!spawn_async(NULL, NULL, argv, NULL, NULL, &error))
	#else
		if (!spawn_async(NULL, cmdline, NULL, NULL, NULL, &error))
	#endif
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Cannot execute print command \"%s\": %s. "
				"Check the path setting in Preferences."),
				printing_prefs.external_print_cmd, error->message);
			g_error_free(error);
		}
		else
		{
			msgwin_status_add(_("File %s printed."), doc->file_name);
		}
	}
	g_free(cmdline);
}


void printing_print_doc(GeanyDocument *doc)
{
	g_return_if_fail(DOC_VALID(doc));

	if (printing_prefs.use_gtk_printing)
		printing_print_gtk(doc);
	else
		print_external(doc);
}
