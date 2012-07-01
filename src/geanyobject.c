/*
 *      geanyobject.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/* A GObject used for connecting and emitting signals when certain events happen,
 * e.g. opening a document.
 * Mainly used for plugins - see the API docs.
 *
 * Core-only signals:
 *
 * signal void save_settings(GObject *obj, GKeyFile *keyfile, gpointer user_data);
 * Emitted just before saving main keyfile settings.

 * signal void load_settings(GObject *obj, GKeyFile *keyfile, gpointer user_data);
 * Emitted just after loading main keyfile settings.
 */

#include "geany.h"
#include "geanyobject.h"

/* extern in geany.h */
GObject	*geany_object;

static guint geany_object_signals[GCB_MAX] = { 0 };


typedef struct _GeanyObjectPrivate GeanyObjectPrivate;

struct _GeanyObjectPrivate
{
	/* to avoid warnings (g_type_class_add_private: assertion `private_size > 0' failed) */
	gchar dummy;
};


G_DEFINE_TYPE(GeanyObject, geany_object, G_TYPE_OBJECT)



static void geany_cclosure_marshal_VOID__STRING_INT_POINTER(GClosure *closure, GValue *ret_val,
				guint n_param_vals, const GValue *param_values, gpointer hint, gpointer mdata)
{
	typedef gboolean (*GeanyMarshalFunc_VOID__STRING_INT_POINTER)
		(gpointer data1, gconstpointer arg_1, gint arg_2, gpointer arg_3, gpointer data2);

	register GeanyMarshalFunc_VOID__STRING_INT_POINTER callback;
	register GCClosure* cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail(n_param_vals == 4);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}
	callback = (GeanyMarshalFunc_VOID__STRING_INT_POINTER) (mdata ? mdata : cc->callback);
	callback(data1,
			  g_value_get_string(param_values + 1),
			  g_value_get_int(param_values + 2),
			  g_value_get_pointer(param_values + 3),
			  data2);
}


static void geany_cclosure_marshal_VOID__POINTER_POINTER(GClosure *closure, GValue *ret_val,
				guint n_param_vals, const GValue *param_values, gpointer hint, gpointer mdata)
{
	typedef gboolean (*GeanyMarshalFunc_VOID__POINTER_POINTER)
		(gpointer data1, gconstpointer arg_1, gconstpointer arg_2, gpointer data2);

	register GeanyMarshalFunc_VOID__POINTER_POINTER callback;
	register GCClosure* cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail(n_param_vals == 3);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}
	callback = (GeanyMarshalFunc_VOID__POINTER_POINTER) (mdata ? mdata : cc->callback);
	callback(data1,
			  g_value_get_pointer(param_values + 1),
			  g_value_get_pointer(param_values + 2),
			  data2);
}


static gboolean boolean_handled_accumulator(GSignalInvocationHint *ihint, GValue *return_accu,
											const GValue *handler_return, gpointer dummy)
{
	gboolean continue_emission, signal_handled;

	signal_handled = g_value_get_boolean(handler_return);
	g_value_set_boolean(return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}


static void geany_cclosure_marshal_BOOL__POINTER_POINTER(GClosure *closure, GValue *return_value,
								guint n_param_values, const GValue *param_values,
								gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
	typedef gboolean (*GeanyMarshalFunc_BOOLEAN__POINTER_POINTER)
		(gpointer data1, gpointer arg_1, gpointer arg_2, gpointer data2);

	register GeanyMarshalFunc_BOOLEAN__POINTER_POINTER callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;
	gboolean v_return;

	g_return_if_fail(return_value != NULL);
	g_return_if_fail(n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}
	callback = (GeanyMarshalFunc_BOOLEAN__POINTER_POINTER)
		(marshal_data ? marshal_data : cc->callback);

	v_return = callback(data1,
					   g_value_get_pointer(param_values + 1),
					   g_value_get_pointer(param_values + 2),
					   data2);

	g_value_set_boolean(return_value, v_return);
}


static void create_signals(GObjectClass *g_object_class)
{
	/* Document signals */
	geany_object_signals[GCB_DOCUMENT_NEW] = g_signal_new (
		"document-new",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_new),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_OPEN] = g_signal_new (
		"document-open",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_open),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_RELOAD] = g_signal_new (
		"document-reload",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_reload),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_BEFORE_SAVE] = g_signal_new (
		"document-before-save",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_before_save),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_SAVE] = g_signal_new (
		"document-save",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_save),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_FILETYPE_SET] = g_signal_new (
		"document-filetype-set",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_filetype_set),
		NULL, NULL,
		geany_cclosure_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER, G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_ACTIVATE] = g_signal_new (
		"document-activate",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_activate),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_DOCUMENT_CLOSE] = g_signal_new (
		"document-close",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_close),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	/* Project signals */
	geany_object_signals[GCB_PROJECT_OPEN] = g_signal_new (
		"project-open",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_open),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_PROJECT_SAVE] = g_signal_new (
		"project-save",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_save),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_PROJECT_CLOSE] = g_signal_new (
		"project-close",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_close),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	geany_object_signals[GCB_PROJECT_DIALOG_OPEN] = g_signal_new (
		"project-dialog-open",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_dialog_open),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_PROJECT_DIALOG_CONFIRMED] = g_signal_new (
		"project-dialog-confirmed",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_dialog_confirmed),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_PROJECT_DIALOG_CLOSE] = g_signal_new (
		"project-dialog-close",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, project_dialog_close),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	/* Editor signals */
	geany_object_signals[GCB_UPDATE_EDITOR_MENU] = g_signal_new (
		"update-editor-menu",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, update_editor_menu),
		NULL, NULL,
		geany_cclosure_marshal_VOID__STRING_INT_POINTER,
		G_TYPE_NONE, 3,
		G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
	geany_object_signals[GCB_EDITOR_NOTIFY] = g_signal_new (
		"editor-notify",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GeanyObjectClass, update_editor_menu),
		boolean_handled_accumulator, NULL,
		geany_cclosure_marshal_BOOL__POINTER_POINTER,
		G_TYPE_BOOLEAN, 2,
		G_TYPE_POINTER, G_TYPE_POINTER);

	/* General signals */
	geany_object_signals[GCB_GEANY_STARTUP_COMPLETE] = g_signal_new (
		"geany-startup-complete",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, geany_startup_complete),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	geany_object_signals[GCB_BUILD_START] = g_signal_new (
		"build-start",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, build_start),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* Core-only signals */
	geany_object_signals[GCB_SAVE_SETTINGS] = g_signal_new (
		"save-settings",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, save_settings),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
	geany_object_signals[GCB_LOAD_SETTINGS] = g_signal_new (
		"load-settings",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, load_settings),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}


static void geany_object_class_init(GeanyObjectClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(GeanyObjectPrivate));

	create_signals(g_object_class);
}


static void geany_object_init(GeanyObject *self)
{
	/* nothing to do */
}


GObject *geany_object_new(void)
{
	return g_object_new(GEANY_OBJECT_TYPE, NULL);
}
