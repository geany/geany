/*
 *      gb.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2014 Colomban Wendling <ban(at)herbesfolles(dot)org>
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
 * A small Pong-like.
 */

#include "utils.h"

#include "gtkcompat.h"


#define AREA_SIZE 300
#define BALL_SIZE 4
#define BORDER_THIKNESS 4
#define HANDLE_THIKNESS 4
#define HANDLE_SHRINK 3


#define GEANY_TYPE_PONG		(geany_pong_get_type())
#define GEANY_PONG(o)		(G_TYPE_CHECK_INSTANCE_CAST((o), GEANY_TYPE_PONG, GeanyPong))
#define GEANY_IS_PONG(o)	(G_TYPE_CHECK_INSTANCE_TYPE((o), GEANY_TYPE_PONG))


typedef struct _GeanyPong GeanyPong;
typedef struct _GeanyPongClass GeanyPongClass;

struct _GeanyPong
{
	GtkDialog parent;
	/* no need for private data as the whole thing is private */

	GtkWidget *score_label;
	GtkWidget *area;

	gint area_height;
	gint area_width;

	guint ball_speed;
	gdouble ball_pos[2];
	gdouble ball_vec[2];
	gint handle_width;
	gint handle_pos;

	guint score;

	guint source_id;
};

struct _GeanyPongClass
{
	GtkDialogClass parent_class;
};


static void geany_pong_finalize(GObject *obj);
static void geany_pong_response(GtkDialog *self, gint response);
static GType geany_pong_get_type(void) G_GNUC_CONST;


G_DEFINE_TYPE(GeanyPong, geany_pong, GTK_TYPE_DIALOG)


#if GTK_CHECK_VERSION(3, 0, 0)
static void geany_pong_set_cairo_source_color(cairo_t *cr, GdkRGBA *c, gdouble a)
{
	cairo_set_source_rgba(cr, c->red, c->green, c->blue, MIN(c->alpha, a));
}
#else
static void geany_pong_set_cairo_source_color(cairo_t *cr, GdkColor *c, gdouble a)
{
	cairo_set_source_rgba(cr, c->red/65535.0, c->green/65535.0, c->blue/65535.0, a);
}
#endif


static gboolean geany_pong_area_draw(GtkWidget *area, cairo_t *cr, GeanyPong *self)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	/* we use the window style context because the area one has a transparent
	 * background and we want something to paint for the overlay */
	GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(self));
	GtkStateFlags state = gtk_style_context_get_state(ctx);
	GdkRGBA fg, bg;

	gtk_style_context_get_color(ctx, state, &fg);
	gtk_style_context_get_background_color(ctx, state, &bg);
#else
	GtkStyle *style = gtk_widget_get_style(area);
	GdkColor fg = style->fg[GTK_STATE_NORMAL];
	GdkColor bg = style->bg[GTK_STATE_NORMAL];
#endif

	self->area_width = gtk_widget_get_allocated_width(area);
	self->area_height = gtk_widget_get_allocated_height(area);

	cairo_set_line_width(cr, BORDER_THIKNESS);

	/* draw the border */
	cairo_rectangle(cr, BORDER_THIKNESS/2, BORDER_THIKNESS/2,
			self->area_width - BORDER_THIKNESS, self->area_height /* we don't wanna see the bottom */);
	geany_pong_set_cairo_source_color(cr, &fg, 1.0);
	cairo_stroke(cr);

	/* draw the handle */
	cairo_rectangle(cr, self->handle_pos - self->handle_width/2, self->area_height - HANDLE_THIKNESS,
						self->handle_width, HANDLE_THIKNESS);
	cairo_fill(cr);

	/* draw the ball */
	cairo_arc(cr, self->ball_pos[0], self->ball_pos[1], BALL_SIZE, 0, 2*G_PI);
	cairo_fill(cr);

	/* if not running, add an info */
	if (! self->source_id || self->handle_width < 1)
	{
		PangoLayout *layout;
		gint pw, ph;
		gdouble scale;

		geany_pong_set_cairo_source_color(cr, &bg, 0.8);
		cairo_rectangle(cr, 0, 0, self->area_width, self->area_height);
		cairo_paint(cr);

		geany_pong_set_cairo_source_color(cr, &fg, 1.0);
		layout = pango_cairo_create_layout(cr);
#if GTK_CHECK_VERSION(3, 0, 0)
		PangoFontDescription *font = NULL;
		gtk_style_context_get(ctx, state, GTK_STYLE_PROPERTY_FONT, &font, NULL);
		if (font)
		{
			pango_layout_set_font_description(layout, font);
			pango_font_description_free(font);
		}
#else
		pango_layout_set_font_description(layout, style->font_desc);
#endif
		if (! self->handle_width)
			pango_layout_set_markup(layout, "<b>You won!</b>\n<small>OK, go back to work now.</small>", -1);
		else
			pango_layout_set_text(layout, "Click to Play", -1);
		pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
		pango_layout_get_pixel_size(layout, &pw, &ph);

		scale = MIN(0.9 * self->area_width / pw, 0.9 * self->area_height / ph);
		cairo_move_to(cr, (self->area_width - pw * scale) / 2, (self->area_height - ph * scale) / 2);
		cairo_scale(cr, scale, scale);
		pango_cairo_show_layout(cr, layout);

		g_object_unref(layout);
	}

	return TRUE;
}


#if ! GTK_CHECK_VERSION(3, 0, 0)
static gboolean geany_pong_area_expose(GtkWidget *area, GdkEventExpose *event, GeanyPong *self)
{
	cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(area));
	gboolean ret;

	ret = geany_pong_area_draw(area, cr, self);
	cairo_destroy(cr);

	return ret;
}
#endif


static void geany_pong_reset_ball(GeanyPong *self)
{
	self->ball_speed = 5;
	self->ball_pos[0] = self->area_width / 2;
	self->ball_pos[1] = self->area_height / 2;
	self->ball_vec[0] = g_random_double_range(.2, .8);
	self->ball_vec[1] = 1.0 - self->ball_vec[0];
	if (g_random_boolean())
		self->ball_vec[0] *= -1;
}


static void geany_pong_update_score(GeanyPong *self)
{
	gchar buf[16];

	g_snprintf(buf, sizeof buf, "%u", self->score);
	gtk_label_set_text(GTK_LABEL(self->score_label), buf);
}


static gboolean geany_pong_area_timeout(gpointer data)
{
	GeanyPong *self = data;
	const gdouble x = BORDER_THIKNESS + BALL_SIZE/2;
	const gdouble y = BORDER_THIKNESS + BALL_SIZE/2;
	const gdouble w = self->area_width - BORDER_THIKNESS - BALL_SIZE/2;
	const gdouble h = self->area_height - HANDLE_THIKNESS - BALL_SIZE/2;
	const gdouble old_ball_pos[2] = { self->ball_pos[0], self->ball_pos[1] };
	const gdouble step[2] = { self->ball_speed * self->ball_vec[0],
							  self->ball_speed * self->ball_vec[1] };

	/* left & right */
	if (self->ball_pos[0] + step[0] >= w ||
		self->ball_pos[0] + step[0] <= x)
		self->ball_vec[0] = -self->ball_vec[0];
	/* top */
	if (self->ball_pos[1] + step[1] <= y)
		self->ball_vec[1] = -self->ball_vec[1];
	/* bottom */
	if (self->ball_pos[1] + step[1] >= h)
	{
		if (self->ball_pos[0] + step[0] >= self->handle_pos - self->handle_width/2 &&
			self->ball_pos[0] + step[0] <= self->handle_pos + self->handle_width/2 &&
			/* only bounce *above* the handle, not below */
			self->ball_pos[1] <= h)
		{
			self->score += self->ball_speed * 2;
			geany_pong_update_score(self);

			self->ball_vec[1] = -self->ball_vec[1];
			self->ball_speed++;
			self->handle_width -= HANDLE_SHRINK;
			/* we don't allow a handle smaller than a shrink step */
			if (self->handle_width < HANDLE_SHRINK)
			{
				self->handle_width = 0;
				self->source_id = 0;
			}
		}
		/* let the ball fall completely off before losing */
		else if (self->ball_pos[1] + step[1] >= self->area_height + BALL_SIZE)
		{	/* lost! */
			self->source_id = 0;
			geany_pong_reset_ball(self);
		}
	}

	if (self->source_id)
	{
		self->ball_pos[0] += self->ball_speed * self->ball_vec[0];
		self->ball_pos[1] += self->ball_speed * self->ball_vec[1];
	}

	if (! self->source_id)
	{
		/* we will draw a text all over, just invalidate everything */
		gtk_widget_queue_draw(self->area);
	}
	else
	{
		/* compute the rough bounding box to redraw the ball */
		const gint bb[4] = {
			(gint) MIN(self->ball_pos[0], old_ball_pos[0]) - BALL_SIZE - 1,
			(gint) MIN(self->ball_pos[1], old_ball_pos[1]) - BALL_SIZE - 1,
			(gint) MAX(self->ball_pos[0], old_ball_pos[0]) + BALL_SIZE + 1,
			(gint) MAX(self->ball_pos[1], old_ball_pos[1]) + BALL_SIZE + 1
		};

		gtk_widget_queue_draw_area(self->area, bb[0], bb[1], bb[2] - bb[0], bb[3] - bb[1]);
		/* always redraw the handle in case it has moved */
		gtk_widget_queue_draw_area(self->area,
				BORDER_THIKNESS, self->area_height - HANDLE_THIKNESS,
				self->area_width - BORDER_THIKNESS*2, HANDLE_THIKNESS);
	}

	return self->source_id != 0;
}


static gboolean geany_pong_area_button_press(GtkWidget *area, GdkEventButton *event, GeanyPong *self)
{
	if (event->type == GDK_BUTTON_PRESS &&
		self->handle_width > 0)
	{
		if (! self->source_id)
			self->source_id = g_timeout_add(1000/60, geany_pong_area_timeout, self);
		else
		{
			g_source_remove(self->source_id);
			self->source_id = 0;
		}
		gtk_widget_queue_draw(area);
		return TRUE;
	}

	return FALSE;
}


static gboolean geany_pong_area_motion_notify(GtkWidget *area, GdkEventMotion *event, GeanyPong *self)
{
	self->handle_pos = (gint) event->x;
	/* clamp so the handle is always fully in */
	if (self->handle_pos < self->handle_width/2 + BORDER_THIKNESS)
		self->handle_pos = self->handle_width/2 + BORDER_THIKNESS;
	else if (self->handle_pos > self->area_width - self->handle_width/2 - BORDER_THIKNESS)
		self->handle_pos = self->area_width - self->handle_width/2 - BORDER_THIKNESS;

	return TRUE;
}


static void geany_pong_class_init(GeanyPongClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS(klass);

	object_class->finalize = geany_pong_finalize;
	dialog_class->response = geany_pong_response;
}


static void geany_pong_init(GeanyPong *self)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;

	self->score = 0;
	self->source_id = 0;
	self->area_height = AREA_SIZE;
	self->area_width = AREA_SIZE;
	self->handle_width = self->area_width / 2;
	self->handle_pos = self->area_width / 2;
	geany_pong_reset_ball(self);

	gtk_window_set_title(GTK_WINDOW(self), "Happy Easter!");
	gtk_window_set_position(GTK_WINDOW(self), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(self), TRUE);
	gtk_window_set_modal(GTK_WINDOW(self), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(self), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(self), FALSE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(self))), vbox, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Score:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	self->score_label = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(hbox), self->score_label, FALSE, FALSE, 0);

	self->area = gtk_drawing_area_new();
	gtk_widget_add_events(self->area, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_signal_connect(self->area, "draw", G_CALLBACK(geany_pong_area_draw), self);
#else
	g_signal_connect(self->area, "expose-event", G_CALLBACK(geany_pong_area_expose), self);
#endif
	g_signal_connect(self->area, "button-press-event", G_CALLBACK(geany_pong_area_button_press), self);
	g_signal_connect(self->area, "motion-notify-event", G_CALLBACK(geany_pong_area_motion_notify), self);
	gtk_widget_set_size_request(self->area, AREA_SIZE, AREA_SIZE);
	gtk_box_pack_start(GTK_BOX(vbox), self->area, TRUE, TRUE, 0);

	gtk_dialog_add_buttons(GTK_DIALOG(self),
		GTK_STOCK_HELP, GTK_RESPONSE_HELP,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(self), GTK_RESPONSE_HELP);
	gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(self), GTK_RESPONSE_HELP));

	gtk_widget_show_all(vbox);
}


static void geany_pong_finalize(GObject *obj)
{
	GeanyPong *self = GEANY_PONG(obj);

	if (self->source_id)
		g_source_remove(self->source_id);

	G_OBJECT_CLASS(geany_pong_parent_class)->finalize(obj);
}


static void geany_pong_help(GeanyPong *self)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *scrolledwindow;
	GtkWidget *textview;
	GtkTextBuffer *buffer;

	dialog = gtk_dialog_new_with_buttons("Help", GTK_WINDOW(self),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 1);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_IN);

	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scrolledwindow), textview);
	gtk_widget_set_size_request(textview, 450, -1);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 2);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(buffer,
		"A small Pong-like\n"
		"\n"
		"Click to start, and then bounce the ball off the walls without it "
		"falling down the bottom edge. You control the bottom handle with "
		"the mouse, but beware! the ball goes faster and faster and the "
		"handle grows smaller and smaller!", -1);

	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


static void geany_pong_response(GtkDialog *self, gint response)
{
	g_return_if_fail(GEANY_IS_PONG(self));

	switch (response)
	{
		case GTK_RESPONSE_HELP:
			geany_pong_help(GEANY_PONG(self));
			break;

		default:
			gtk_widget_destroy(GTK_WIDGET(self));
	}
}


static GtkWidget *geany_pong_new(GtkWindow *parent)
{
	return g_object_new(GEANY_TYPE_PONG, "transient-for", parent, NULL);
}


static gboolean gb_on_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	static gchar text[] = "geany";

	if (event->keyval < 0x80)
	{
		memmove (text, &text[1], G_N_ELEMENTS(text) - 1);
		text[G_N_ELEMENTS(text) - 2] = (gchar) event->keyval;

		if (utils_str_equal(text, "geany"))
		{
			GtkWidget *pong = geany_pong_new(GTK_WINDOW(widget));
			gtk_widget_show(pong);
			return TRUE;
		}
	}

	return FALSE;
}
