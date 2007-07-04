/*
 *      classbuilder.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Alexander Rodin <rodin(dot)alexander(at)gmail(dot)com>
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 *
 * $Id$
 */

/* Class Builder - creates source files containing a new class interface and definition. */

#include "geany.h"
#include "plugindata.h"
#include "support.h"
#include "filetypes.h"
#include "document.h"


enum
{
	GEANY_CLASS_TYPE_CPP,
	GEANY_CLASS_TYPE_GTK
};

typedef struct _ClassInfo	ClassInfo;

struct _ClassInfo
{
	gint type;
	gchar *class_name;
	gchar *class_name_up;
	gchar *class_name_low;
	gchar *base_name;
	gchar *base_gtype;
	gchar *header;
	gchar *header_guard;
	gchar *base_include;
	gchar *base_decl;
	gchar *constructor_decl;
	gchar *destructor_decl;
	gchar *source;
	gchar *constructor_impl;
	gchar *destructor_impl;
	gchar *gtk_destructor_registration;
};

typedef struct _CreateClassDialog
{
	gint class_type;
	GtkWidget *dialog;
	GtkWidget *class_name_entry;
	GtkWidget *header_entry;
	GtkWidget *source_entry;
	GtkWidget *base_name_entry;
	GtkWidget *base_header_entry;
	GtkWidget *base_header_global_box;
	GtkWidget *base_gtype_entry;
	GtkWidget *create_constructor_box;
	GtkWidget *create_destructor_box;
	GtkWidget *gtk_constructor_type_entry;
} CreateClassDialog;


static PluginData *plugin_data;

#define doc_array	plugin_data->doc_array
// can't use document as a macro because it's currently a typename
#define documents	plugin_data->document
#define scintilla	plugin_data->sci
#define templates	plugin_data->templates
#define utils		plugin_data->utils


static struct
{
	GtkWidget *menu_item;
}
local_data;


static const gchar templates_cpp_class_header[] = "{fileheader}\n\n\
#ifndef {header_guard}\n\
#define {header_guard}\n\
{base_include}\n\
class {class_name}{base_decl}\n\
{\n\
	public:\n\
		{constructor_decl}\
		{destructor_decl}\
	\n\
	private:\n\
		// add your private declarations\n\
};\n\
\n\
#endif // {header_guard}\n\
";

static const gchar templates_cpp_class_source[] = "{fileheader}\n\n\
#include \"{header}\"\n\
\n\
{constructor_impl}\n\
{destructor_impl}\n\
";

static const gchar templates_gtk_class_header[] = "{fileheader}\n\n\
#ifndef __{header_guard}__\n\
#define __{header_guard}__\n\
{base_include}\n\
G_BEGIN_DECLS\n\
\n\
#define {class_name_up}_TYPE				({class_name_low}_get_type())\n\
#define {class_name_up}(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\\\n\
		{class_name_up}_TYPE, {class_name}))\n\
#define {class_name_up}_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\\\n\
		{class_name_up}_TYPE, {class_name}Class))\n\
#define IS_{class_name_up}(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\\\n\
		{class_name_up}_TYPE))\n\
#define IS_{class_name_up}_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\\\n\
		{class_name_up}_TYPE))\n\
\n\
typedef struct _{class_name}				{class_name};\n\
typedef struct _{class_name}Class			{class_name}Class;\n\
\n\
struct _{class_name}\n\
{\n\
	{base_name} parent;\n\
	/* add your public declarations here */\n\
};\n\
\n\
struct _{class_name}Class\n\
{\n\
	{base_name}Class parent_class;\n\
};\n\
\n\
GType		{class_name_low}_get_type	(void);\n\
{constructor_decl}\
\n\
G_END_DECLS\n\
\n\
#endif /* __{header_guard}__ */\n\
";

static const gchar templates_gtk_class_source[] = "{fileheader}\n\
#include \"{header}\"\n\
\n\
typedef struct _{class_name}Private				{class_name}Private;\n\
\n\
#define {class_name_up}_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\\\n\
		{class_name_up}_TYPE, {class_name}Private))\n\
\n\
struct _{class_name}Private\n\
{\n\
	/* add your private declarations here */\n\
};\n\
\n\
static void {class_name_low}_class_init			({class_name}Class *klass);\n\
static void {class_name_low}_init				({class_name} *self);\n\
{destructor_decl}\
\n\
/* Local data */\n\
static {base_name}Class *parent_class = NULL;\n\
\n\
GType {class_name_low}_get_type(void)\n\
{\n\
	static GType self_type = 0;\n\
	if (! self_type)\n\
	{\n\
		static const GTypeInfo self_info = \n\
		{\n\
			sizeof({class_name}Class),\n\
			NULL, /* base_init */\n\
			NULL, /* base_finalize */\n\
			(GClassInitFunc){class_name_low}_class_init,\n\
			NULL, /* class_finalize */\n\
			NULL, /* class_data */\n\
			sizeof({class_name}),\n\
			0,\n\
			(GInstanceInitFunc){class_name_low}_init,\n\
		};\n\
		\n\
		self_type = g_type_register_static({base_gtype}, \"{class_name}\", &self_info, 0);\
	}\n\
	\n\
	return self_type;\n\
}\n\
\n\
static void {class_name_low}_class_init({class_name}Class *klass)\n\
{\n\
	{gtk_destructor_registration}\n\
	parent_class = ({base_name}Class*)g_type_class_peek({base_gtype});\n\
	g_type_class_add_private((gpointer)klass, sizeof({class_name}Private));\n\
}\n\
\n\
static void {class_name_low}_init({class_name} *self)\n\
{\n\
	\n\
}\n\
\n\
{constructor_impl}\n\
{destructor_impl}\n\
";


static void cc_dlg_on_set_sensitive_toggled(GtkWidget *toggle_button, GtkWidget *target_widget);
static void cc_dlg_on_class_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg);
static void cc_dlg_on_base_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg);
static void cc_dlg_on_create_class(CreateClassDialog *cc_dlg);


VERSION_CHECK(3)

PLUGIN_INFO(_("Class Builder"), _("Creates source files for new class types."))


/* I don't want this to be in the plugin API because it can cause leaks if any pointers
 * are NULL -ntrel. */
/* Frees all passed pointers if they are *ALL* non-NULL.
 * Do not use if any pointers may be NULL.
 * The first argument is nothing special, it will also be freed.
 * The list must be ended with NULL. */
static void
utils_free_pointers(gpointer first, ...)
{
	va_list a;
	gpointer sa;

    for (va_start(a, first);  (sa = va_arg(a, gpointer), sa!=NULL);)
    {
    	if (sa != NULL)
    		g_free(sa);
	}
	va_end(a);

    if (first != NULL)
    	g_free(first);
}


static gchar*
get_template_class_header(ClassInfo *class_info)
{
	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
		{
			gchar *fileheader = NULL;
			gchar *template;

			fileheader = templates->get_template_fileheader(GEANY_FILETYPES_C, class_info->header);
			template = g_strdup(templates_cpp_class_header);
			template = utils->str_replace(template, "{fileheader}", fileheader);
			template = utils->str_replace(template, "{header_guard}", class_info->header_guard);
			template = utils->str_replace(template, "{base_include}", class_info->base_include);
			template = utils->str_replace(template, "{class_name}", class_info->class_name);
			template = utils->str_replace(template, "{base_decl}", class_info->base_decl);
			template = utils->str_replace(template, "{constructor_decl}",
					class_info->constructor_decl);
			template = utils->str_replace(template, "{destructor_decl}",
					class_info->destructor_decl);

			return template;
		}
		case GEANY_CLASS_TYPE_GTK:
		{
			gchar *fileheader = NULL;
			gchar *template;

			fileheader = templates->get_template_fileheader(GEANY_FILETYPES_C, class_info->header);
			template = g_strdup(templates_gtk_class_header);
			template = utils->str_replace(template, "{fileheader}", fileheader);
			template = utils->str_replace(template, "{header_guard}", class_info->header_guard);
			template = utils->str_replace(template, "{base_include}", class_info->base_include);
			template = utils->str_replace(template, "{class_name}", class_info->class_name);
			template = utils->str_replace(template, "{class_name_up}", class_info->class_name_up);
			template = utils->str_replace(template, "{class_name_low}", class_info->class_name_low);
			template = utils->str_replace(template, "{base_name}", class_info->base_name);
			template = utils->str_replace(template, "{constructor_decl}",
					class_info->constructor_decl);

			return template;
		}
	}

	return NULL;
}

static gchar*
get_template_class_source(ClassInfo *class_info)
{
	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
		{
			gchar *fileheader = NULL;
			gchar *template;

			fileheader = templates->get_template_fileheader(GEANY_FILETYPES_C, class_info->source);
			template = g_strdup(templates_cpp_class_source);
			template = utils->str_replace(template, "{fileheader}", fileheader);
			template = utils->str_replace(template, "{header}", class_info->header);
			template = utils->str_replace(template, "{class_name}", class_info->class_name);
			template = utils->str_replace(template, "{base_include}", class_info->base_include);
			template = utils->str_replace(template, "{base_name}", class_info->base_name);
			template = utils->str_replace(template, "{constructor_impl}",
					class_info->constructor_impl);
			template = utils->str_replace(template, "{destructor_impl}",
					class_info->destructor_impl);

			return template;
		}
		case GEANY_CLASS_TYPE_GTK:
		{
			gchar *fileheader = NULL;
			gchar *template;

			fileheader = templates->get_template_fileheader(GEANY_FILETYPES_C, class_info->source);
			template = g_strdup(templates_gtk_class_source);
			template = utils->str_replace(template, "{fileheader}", fileheader);
			template = utils->str_replace(template, "{header}", class_info->header);
			template = utils->str_replace(template, "{class_name}", class_info->class_name);
			template = utils->str_replace(template, "{class_name_up}", class_info->class_name_up);
			template = utils->str_replace(template, "{class_name_low}", class_info->class_name_low);
			template = utils->str_replace(template, "{base_name}", class_info->base_name);
			template = utils->str_replace(template, "{base_gtype}", class_info->base_gtype);
			template = utils->str_replace(template, "{destructor_decl}", class_info->destructor_decl);
			template = utils->str_replace(template, "{constructor_impl}",
					class_info->constructor_impl);
			template = utils->str_replace(template, "{destructor_impl}",
					class_info->destructor_impl);
			template = utils->str_replace(template, "{gtk_destructor_registration}",
					class_info->gtk_destructor_registration);

			return template;
		}
	}

	return NULL;
}


void show_dialog_create_class(gint type)
{
	CreateClassDialog *cc_dlg;
	GtkWidget *main_box;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;

	cc_dlg = g_new0(CreateClassDialog, 1);
	cc_dlg->class_type = type;

	cc_dlg->dialog = gtk_dialog_new_with_buttons(_("Create class"),
			GTK_WINDOW(plugin_data->app->window),
			GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_container_set_border_width(GTK_CONTAINER(cc_dlg->dialog), 5);
	g_signal_connect_swapped(G_OBJECT(cc_dlg->dialog), "destroy",
			G_CALLBACK(g_free), (gpointer)cc_dlg);

	main_box = GTK_DIALOG(cc_dlg->dialog)->vbox;
	gtk_box_set_spacing(GTK_BOX(main_box), 10);

	frame = gtk_frame_new(_("Class"));
	gtk_container_add(GTK_CONTAINER(main_box), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Class name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cc_dlg->class_name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), cc_dlg->class_name_entry, TRUE, TRUE, 0);
	gtk_widget_show(cc_dlg->class_name_entry);
	g_signal_connect(G_OBJECT(cc_dlg->class_name_entry), "changed",
			G_CALLBACK(cc_dlg_on_class_name_entry_changed), cc_dlg);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Header file:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cc_dlg->header_entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->header_entry);
	gtk_widget_show(cc_dlg->header_entry);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Source file:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cc_dlg->source_entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->source_entry);
	gtk_widget_show(cc_dlg->source_entry);

	frame = gtk_frame_new(_("Inheritance"));
	gtk_container_add(GTK_CONTAINER(main_box), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Base class:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cc_dlg->base_name_entry = gtk_entry_new();
	if (type == GEANY_CLASS_TYPE_GTK)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_name_entry), "GObject");
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->base_name_entry);
	gtk_widget_show(cc_dlg->base_name_entry);
	g_signal_connect(G_OBJECT(cc_dlg->base_name_entry), "changed",
			G_CALLBACK(cc_dlg_on_base_name_entry_changed), (gpointer)cc_dlg);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Base header:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cc_dlg->base_header_entry = gtk_entry_new();
	if (type == GEANY_CLASS_TYPE_GTK)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_header_entry), "glib-object.h");
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->base_header_entry);
	gtk_widget_show(cc_dlg->base_header_entry);

	cc_dlg->base_header_global_box = gtk_check_button_new_with_label(_("Global"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box), TRUE);
	gtk_box_pack_end(GTK_BOX(hbox), cc_dlg->base_header_global_box, FALSE, FALSE, 0);
	gtk_widget_show(cc_dlg->base_header_global_box);

	if (type == GEANY_CLASS_TYPE_GTK)
	{
		hbox = gtk_hbox_new(FALSE, 10);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_widget_show(hbox);

		label = gtk_label_new(_("Base GType:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		cc_dlg->base_gtype_entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_gtype_entry), "GTK_TYPE_OBJECT");
		gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->base_gtype_entry);
		gtk_widget_show(cc_dlg->base_gtype_entry);
	}

	frame = gtk_frame_new(_("Options"));
	gtk_container_add(GTK_CONTAINER(main_box), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);

	cc_dlg->create_constructor_box = gtk_check_button_new_with_label(_("Create constructor"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box), TRUE);
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->create_constructor_box);
	gtk_widget_show(cc_dlg->create_constructor_box);

	cc_dlg->create_destructor_box = gtk_check_button_new_with_label(_("Create destructor"));
	gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->create_destructor_box);
	gtk_widget_show(cc_dlg->create_destructor_box);

	if (type == GEANY_CLASS_TYPE_GTK)
	{
		hbox = gtk_hbox_new(FALSE, 10);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_widget_show(hbox);
		g_signal_connect(G_OBJECT(cc_dlg->create_constructor_box), "toggled",
				G_CALLBACK(cc_dlg_on_set_sensitive_toggled), (gpointer)hbox);

		label = gtk_label_new(_("GTK+ constructor type"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		cc_dlg->gtk_constructor_type_entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry), "GObject");
		gtk_container_add(GTK_CONTAINER(hbox), cc_dlg->gtk_constructor_type_entry);
		gtk_widget_show(cc_dlg->gtk_constructor_type_entry);
	}

	if (gtk_dialog_run(GTK_DIALOG(cc_dlg->dialog)) == GTK_RESPONSE_OK)
		cc_dlg_on_create_class(cc_dlg);

	gtk_widget_destroy(cc_dlg->dialog);
//	g_object_unref(G_OBJECT(cc_dlg->dialog));
}


static void cc_dlg_on_set_sensitive_toggled(GtkWidget *toggle_button, GtkWidget *target_widget)
{
	g_return_if_fail(toggle_button != NULL);
	g_return_if_fail(GTK_IS_TOGGLE_BUTTON(toggle_button));
	g_return_if_fail(target_widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(target_widget));

	gtk_widget_set_sensitive(target_widget,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}


static void cc_dlg_on_class_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg)
{
	gchar *class_name_down;
	gchar *class_header;
	gchar *class_source;

	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));
	g_return_if_fail(cc_dlg != NULL);

	class_name_down = g_ascii_strdown(gtk_entry_get_text(GTK_ENTRY(entry)), -1);
	class_header = g_strconcat(class_name_down, ".h", NULL);
	if (cc_dlg->class_type == GEANY_CLASS_TYPE_CPP)
		class_source = g_strconcat(class_name_down, ".cpp", NULL);
	else
		class_source = g_strconcat(class_name_down, ".c", NULL);

	gtk_entry_set_text(GTK_ENTRY(cc_dlg->header_entry), class_header);
	gtk_entry_set_text(GTK_ENTRY(cc_dlg->source_entry), class_source);

	g_free(class_name_down);
	g_free(class_header);
	g_free(class_source);
}


static gchar* str_case_split(const gchar *str, gchar splitter)
{
	GString *result;

	g_return_val_if_fail(str != NULL, NULL);
	if (*str == '\0')
		return g_strdup("");

	result = g_string_new(NULL);
	g_string_append_c(result, *str);
	while (*(++str) != '\0')
	{
		if (g_ascii_isupper(*str) && g_ascii_islower(result->str[result->len - 1]))
			g_string_append_c(result, splitter);
		g_string_append_c(result, *str);
	}
	return g_string_free(result, FALSE);
}


static void cc_dlg_on_base_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg)
{
	gchar *base_name_splitted;
	gchar *base_header;
	gchar *tmp;

	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));
	g_return_if_fail(cc_dlg != NULL);

	base_name_splitted = str_case_split(gtk_entry_get_text(GTK_ENTRY(entry)), '_');
	if (! g_ascii_strncasecmp(gtk_entry_get_text(GTK_ENTRY(entry)), "gtk", 3))
		tmp = g_strconcat("gtk/", gtk_entry_get_text(GTK_ENTRY(entry)), ".h", NULL);
	else if (utils->str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), "GObject"))
		tmp = g_strdup("glib-object.h");
	else
		tmp = g_strconcat(gtk_entry_get_text(GTK_ENTRY(entry)), ".h", NULL);
	base_header = g_ascii_strdown(tmp, -1);
	g_free(tmp);

	gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_header_entry), base_header);

	if (cc_dlg->class_type == GEANY_CLASS_TYPE_GTK)
	{
		gchar *base_gtype;
		if (! g_ascii_strncasecmp(gtk_entry_get_text(GTK_ENTRY(entry)), "gtk", 3))
			tmp = g_strdup_printf("%.3s_TYPE%s",
					base_name_splitted,
					base_name_splitted + 3);
		else if (utils->str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), "GObject"))
			tmp = g_strdup("G_TYPE_OBJECT");
		else
			tmp = g_strconcat(base_name_splitted, "_TYPE", NULL);
		base_gtype = g_ascii_strup(tmp, -1);
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_gtype_entry), base_gtype);

		g_free(base_gtype);
		g_free(tmp);
	}

	g_free(base_name_splitted);
	g_free(base_header);
}


static void cc_dlg_on_create_class(CreateClassDialog *cc_dlg)
{
	ClassInfo *class_info;
	gint idx;
	gchar *text;
	gchar *tmp;

	g_return_if_fail(cc_dlg != NULL);

	if (utils->str_equal(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_name_entry)), ""))
		return;

	class_info = g_new0(ClassInfo, 1);
	class_info->type = cc_dlg->class_type;
	class_info->class_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_name_entry)));
	tmp = str_case_split(class_info->class_name, '_');
	class_info->class_name_up = g_ascii_strup(tmp, -1);
	class_info->class_name_low = g_ascii_strdown(class_info->class_name_up, -1);
	if (! utils->str_equal(gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_name_entry)), ""))
	{
		class_info->base_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_name_entry)));
		class_info->base_include = g_strdup_printf("\n#include %c%s%c\n",
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box)) ?
				'<' : '\"',
			gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_header_entry)),
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box)) ?
				'>' : '\"');
	}
	else
	{
		class_info->base_name = g_strdup("");
		class_info->base_include = g_strdup("");
	}
	class_info->header = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->header_entry)));
	class_info->header_guard = g_ascii_strup(class_info->header, -1);
	g_strdelimit(class_info->header_guard, ".", '_');
	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
		{
			class_info->source = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->source_entry)));
			if (! utils->str_equal(class_info->base_name, ""))
				class_info->base_decl = g_strdup_printf(": public %s", class_info->base_name);
			else
				class_info->base_decl = g_strdup("");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)))
			{
				gchar *base_constructor;

				if (utils->str_equal(class_info->base_name, ""))
					base_constructor = g_strdup("");
				else
					base_constructor = g_strdup_printf("\t: %s()\n", class_info->base_name);
				class_info->constructor_decl = g_strdup_printf("%s();\n", class_info->class_name);
				class_info->constructor_impl = g_strdup_printf("\n%s::%s()\n%s{\n\t\n}\n",
					class_info->class_name, class_info->class_name, base_constructor);
				g_free(base_constructor);
			}
			else
			{
				class_info->constructor_decl = g_strdup("");
				class_info->constructor_impl = g_strdup("");
			}
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_destructor_box)))
			{
				class_info->destructor_decl =
						g_strdup_printf("virtual ~%s();\n", class_info->class_name);
				class_info->destructor_impl = g_strdup_printf("\n%s::~%s()\n{\n\t\n}\n",
					class_info->class_name, class_info->class_name);
			}
			else
			{
				class_info->destructor_decl = g_strdup("");
				class_info->destructor_impl = g_strdup("");
			}
			break;
		}
		case GEANY_CLASS_TYPE_GTK:
		{
			class_info->base_gtype = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(cc_dlg->base_gtype_entry)));
			class_info->source = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->source_entry)));
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)))
			{
				class_info->constructor_decl = g_strdup_printf("%s*\t%s_new\t\t\t(void);\n",
						gtk_entry_get_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry)),
						class_info->class_name_low);
				class_info->constructor_impl = g_strdup_printf("\n"
						"%s* %s_new(void)\n"
						"{\n"
						"\treturn (%s*)g_object_new(%s_TYPE, NULL);\n"
						"}\n",
						gtk_entry_get_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry)),
						class_info->class_name_low,
						gtk_entry_get_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry)),
						class_info->class_name_up);
			}
			else
			{
				class_info->constructor_decl = g_strdup("");
				class_info->constructor_impl = g_strdup("");
			}
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_destructor_box)))
			{
				class_info->gtk_destructor_registration =
						g_strdup_printf("GObjectClass *g_object_class;\n\n"
						"\tg_object_class = G_OBJECT_CLASS(klass);\n\n"
						"\tg_object_class->finalize = %s_finalize;\n",
						class_info->class_name_low);
				class_info->destructor_decl =
						g_strdup_printf("static void %s_finalize\t\t\t(GObject *object);\n",
						class_info->class_name_low);
				class_info->destructor_impl = g_strdup_printf("\n"
						"void %s_finalize(GObject *object)\n"
						"{\n"
						"\t%s *self;\n\n"
						"\tg_return_if_fail(object != NULL);\n"
						"\tg_return_if_fail(IS_%s(object));\n\n"
						"\tself = %s(object);\n\n"
						"\tif (G_OBJECT_CLASS(parent_class)->finalize)\n"
						"\t\t(* G_OBJECT_CLASS(parent_class)->finalize)(object);\n"
						"}\n",
						class_info->class_name_low,
						class_info->class_name,
						class_info->class_name_up,
						class_info->class_name_up);
			}
			else
			{
				class_info->gtk_destructor_registration = g_strdup("");
				class_info->destructor_decl = g_strdup("");
				class_info->destructor_impl = g_strdup("");
			}
			break;
		}
	}

	// only create the files if the filename is not empty
	if (! utils->str_equal(class_info->source, ""))
	{
		text = get_template_class_source(class_info);
		idx = documents->new_file(class_info->source, NULL);
		scintilla->set_text(doc_list[idx].sci, text);
		g_free(text);
	}

	if (! utils->str_equal(class_info->header, ""))
	{
		text = get_template_class_header(class_info);
		idx = documents->new_file(class_info->header, NULL);
		scintilla->set_text(doc_list[idx].sci, text);
		g_free(text);
	}

	utils_free_pointers(tmp, class_info->class_name, class_info->class_name_up,
		class_info->base_name, class_info->class_name_low, class_info->base_include,
		class_info->header, class_info->header_guard, class_info->source, class_info->base_decl,
		class_info->constructor_decl, class_info->constructor_impl,
		class_info->gtk_destructor_registration, class_info->destructor_decl,
		class_info->destructor_impl, class_info->base_gtype, class_info, NULL);
}


static void
on_menu_create_cpp_class_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	show_dialog_create_class(GEANY_CLASS_TYPE_CPP);
}


static void
on_menu_create_gtk_class_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	show_dialog_create_class(GEANY_CLASS_TYPE_GTK);
}


void init(PluginData *data)
{
	GtkWidget *menu_create_class1;
	GtkWidget *image1861;
	GtkWidget *menu_create_class1_menu;
	GtkWidget *menu_create_cpp_class;
	GtkWidget *menu_create_gtk_class;

	plugin_data = data;

	menu_create_class1 = gtk_image_menu_item_new_with_mnemonic (_("Create Cla_ss"));
	gtk_container_add (GTK_CONTAINER (data->tools_menu), menu_create_class1);

	image1861 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_create_class1), image1861);

	menu_create_class1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_create_class1), menu_create_class1_menu);

	menu_create_cpp_class = gtk_menu_item_new_with_mnemonic (_("C++ class"));
	gtk_container_add (GTK_CONTAINER (menu_create_class1_menu), menu_create_cpp_class);

	menu_create_gtk_class = gtk_menu_item_new_with_mnemonic (_("GTK+ class"));
	gtk_container_add (GTK_CONTAINER (menu_create_class1_menu), menu_create_gtk_class);

	g_signal_connect ((gpointer) menu_create_cpp_class, "activate",
		G_CALLBACK (on_menu_create_cpp_class_activate),
		NULL);
	g_signal_connect ((gpointer) menu_create_gtk_class, "activate",
		G_CALLBACK (on_menu_create_gtk_class_activate),
		NULL);

	gtk_widget_show_all(menu_create_class1);

	local_data.menu_item = menu_create_class1;
}


void cleanup()
{
	gtk_widget_destroy(local_data.menu_item);
}
