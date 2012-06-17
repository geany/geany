/*
 *      classbuilder.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Alexander Rodin <rodin(dot)alexander(at)gmail(dot)com>
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

/* Class Builder - creates source files containing a new class interface and definition. */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "geanyplugin.h"

GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("Class Builder"), _("Creates source files for new class types."), VERSION,
	"Alexander Rodin, Ondrej Donek, the Geany developer team")


static GtkWidget *main_menu_item = NULL;


enum
{
	GEANY_CLASS_TYPE_CPP,
	GEANY_CLASS_TYPE_GTK,
	GEANY_CLASS_TYPE_PHP
};

typedef struct _ClassInfo	ClassInfo;

struct _ClassInfo
{
	gint type;
	gchar *namespace;
	gchar *namespace_up;
	gchar *namespace_low;
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
	/* These are needed only for PHP classes */
	gchar *namespace_decl;
	gchar *implements_decl;
	gchar *abstract_decl;
	gchar *singleton_impl;
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
	/* These are needed only for PHP classes */
	GtkWidget *class_namespace_entry;
	GtkWidget *class_implements_entry;
	GtkWidget *create_isabstract_box;
	GtkWidget *create_issingleton_box;
} CreateClassDialog;


/* TODO make these templates configurable */
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
		/* add your private declarations */\n\
};\n\
\n\
#endif /* {header_guard} */ \n\
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
\n\n\
#define {namespace_up}TYPE_{class_name_up}             ({namespace_low}{class_name_low}_get_type ())\n\
#define {namespace_up}{class_name_up}(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), {namespace_up}TYPE_{class_name_up}, {namespace}{class_name}))\n\
#define {namespace_up}{class_name_up}_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), {namespace_up}TYPE_{class_name_up}, {namespace}{class_name}Class))\n\
#define {namespace_up}IS_{class_name_up}(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), {namespace_up}TYPE_{class_name_up}))\n\
#define {namespace_up}IS_{class_name_up}_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), {namespace_up}TYPE_{class_name_up}))\n\
#define {namespace_up}{class_name_up}_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), {namespace_up}TYPE_{class_name_up}, {namespace}{class_name}Class))\n\
\n\
typedef struct _{namespace}{class_name}         {namespace}{class_name};\n\
typedef struct _{namespace}{class_name}Class    {namespace}{class_name}Class;\n\
typedef struct _{namespace}{class_name}Private  {namespace}{class_name}Private;\n\
\n\
struct _{namespace}{class_name}\n\
{\n\
  {base_name} parent;\n\
  /* add your public declarations here */\n\
  {namespace}{class_name}Private *priv;\n\
};\n\
\n\
struct _{namespace}{class_name}Class\n\
{\n\
  {base_name}Class parent_class;\n\
};\n\
\n\n\
GType {namespace_low}{class_name_low}_get_type (void);\n\n\
{constructor_decl}\
\n\n\
G_END_DECLS\n\
\n\
#endif /* __{header_guard}__ */\n\
";

static const gchar templates_gtk_class_source[] = "{fileheader}\n\
#include \"{header}\"\n\
\n\
struct _{namespace}{class_name}Private\n\
{\n\
  /* add your private declarations here */\n\
  gpointer delete_me;\n\
};\n\
\n\
{destructor_decl}\
\n\
G_DEFINE_TYPE ({namespace}{class_name}, {namespace_low}{class_name_low}, {base_gtype})\n\
\n\n\
static void\n\
{namespace_low}{class_name_low}_class_init ({namespace}{class_name}Class *klass)\n\
{\n\
  {gtk_destructor_registration}\n\
  g_type_class_add_private ((gpointer)klass, sizeof ({namespace}{class_name}Private));\n\
}\n\
\n\
{destructor_impl}\n\
\n\
static void\n\
{namespace_low}{class_name_low}_init ({namespace}{class_name} *self)\n\
{\n\
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, {namespace_up}TYPE_{class_name_up}, {namespace}{class_name}Private);\n\
}\n\
\n\
{constructor_impl}\n\
";

static const gchar templates_php_class_source[] = "<?php\n\
{fileheader}\n\
{namespace_decl}\n\
{base_include}\n\
{abstract_decl}class {class_name}{base_decl}{implements_decl}\n{\n\
{singleton_impl}\
{constructor_impl}\
{destructor_impl}\n\
	// ...\n\n\
}\n\
";


static void cc_dlg_on_set_sensitive_toggled(GtkWidget *toggle_button, GtkWidget *target_widget);
static void cc_dlg_on_class_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg);
static void cc_dlg_on_class_namespace_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg);
static void cc_dlg_on_base_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg);
static gboolean create_class(CreateClassDialog *cc_dlg);


/* The list must be ended with NULL as an extra check that arg_count is correct. */
static void
free_pointers(gsize arg_count, ...)
{
	va_list a;
	gsize i;
	gpointer ptr;

	va_start(a, arg_count);
	for (i = 0; i < arg_count; i++)
	{
		ptr = va_arg(a, gpointer);
		g_free(ptr);
	}
	ptr = va_arg(a, gpointer);
	if (ptr)
		g_warning("Wrong arg_count!");
	va_end(a);
}


static gchar*
get_template_class_header(ClassInfo *class_info)
{
	gchar *fileheader = NULL;
	GString *template = NULL;

	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
			fileheader = templates_get_template_fileheader(GEANY_FILETYPES_CPP, class_info->header);
			template = g_string_new(templates_cpp_class_header);
			utils_string_replace_all(template, "{fileheader}", fileheader);
			utils_string_replace_all(template, "{header_guard}", class_info->header_guard);
			utils_string_replace_all(template, "{base_include}", class_info->base_include);
			utils_string_replace_all(template, "{class_name}", class_info->class_name);
			utils_string_replace_all(template, "{base_decl}", class_info->base_decl);
			utils_string_replace_all(template, "{constructor_decl}",
					class_info->constructor_decl);
			utils_string_replace_all(template, "{destructor_decl}",
					class_info->destructor_decl);
			break;

		case GEANY_CLASS_TYPE_GTK:
			fileheader = templates_get_template_fileheader(GEANY_FILETYPES_C, class_info->header);
			template = g_string_new(templates_gtk_class_header);
			utils_string_replace_all(template, "{fileheader}", fileheader);
			utils_string_replace_all(template, "{header_guard}", class_info->header_guard);
			utils_string_replace_all(template, "{base_include}", class_info->base_include);
			utils_string_replace_all(template, "{namespace}", class_info->namespace);
			utils_string_replace_all(template, "{namespace_up}", class_info->namespace_up);
			utils_string_replace_all(template, "{namespace_low}", class_info->namespace_low);
			utils_string_replace_all(template, "{class_name}", class_info->class_name);
			utils_string_replace_all(template, "{class_name_up}", class_info->class_name_up);
			utils_string_replace_all(template, "{class_name_low}", class_info->class_name_low);
			utils_string_replace_all(template, "{base_name}", class_info->base_name);
			utils_string_replace_all(template, "{constructor_decl}",
					class_info->constructor_decl);
			break;
	}

	g_free(fileheader);

	if (template)
		return g_string_free(template, FALSE);
	else
		return NULL;
}


static gchar*
get_template_class_source(ClassInfo *class_info)
{
	gchar *fileheader = NULL;
	GString *template = NULL;

	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
			fileheader = templates_get_template_fileheader(GEANY_FILETYPES_CPP, class_info->source);
			template = g_string_new(templates_cpp_class_source);
			utils_string_replace_all(template, "{fileheader}", fileheader);
			utils_string_replace_all(template, "{header}", class_info->header);
			utils_string_replace_all(template, "{class_name}", class_info->class_name);
			utils_string_replace_all(template, "{base_include}", class_info->base_include);
			utils_string_replace_all(template, "{base_name}", class_info->base_name);
			utils_string_replace_all(template, "{constructor_impl}",
					class_info->constructor_impl);
			utils_string_replace_all(template, "{destructor_impl}",
					class_info->destructor_impl);
			break;

		case GEANY_CLASS_TYPE_GTK:
			fileheader = templates_get_template_fileheader(GEANY_FILETYPES_C, class_info->source);
			template = g_string_new(templates_gtk_class_source);
			utils_string_replace_all(template, "{fileheader}", fileheader);
			utils_string_replace_all(template, "{header}", class_info->header);
			utils_string_replace_all(template, "{namespace}", class_info->namespace);
			utils_string_replace_all(template, "{namespace_up}", class_info->namespace_up);
			utils_string_replace_all(template, "{namespace_low}", class_info->namespace_low);
			utils_string_replace_all(template, "{class_name}", class_info->class_name);
			utils_string_replace_all(template, "{class_name_up}", class_info->class_name_up);
			utils_string_replace_all(template, "{class_name_low}", class_info->class_name_low);
			utils_string_replace_all(template, "{base_name}", class_info->base_name);
			utils_string_replace_all(template, "{base_gtype}", class_info->base_gtype);
			utils_string_replace_all(template, "{destructor_decl}", class_info->destructor_decl);
			utils_string_replace_all(template, "{constructor_impl}",
					class_info->constructor_impl);
			utils_string_replace_all(template, "{destructor_impl}",
					class_info->destructor_impl);
			utils_string_replace_all(template, "{gtk_destructor_registration}",
					class_info->gtk_destructor_registration);
			break;

		case GEANY_CLASS_TYPE_PHP:
			fileheader = templates_get_template_fileheader(GEANY_FILETYPES_PHP, class_info->source);
			template = g_string_new(templates_php_class_source);
			utils_string_replace_all(template, "{fileheader}", fileheader);
			utils_string_replace_all(template, "{namespace_decl}", class_info->namespace_decl);
			utils_string_replace_all(template, "{base_include}", class_info->base_include);
			utils_string_replace_all(template, "{abstract_decl}", class_info->abstract_decl);
			utils_string_replace_all(template, "{class_name}", class_info->class_name);
			utils_string_replace_all(template, "{base_decl}", class_info->base_decl);
			utils_string_replace_all(template, "{implements_decl}", class_info->implements_decl);
			utils_string_replace_all(template, "{constructor_impl}", class_info->constructor_impl);
			utils_string_replace_all(template, "{destructor_impl}", class_info->destructor_impl);
			utils_string_replace_all(template, "{singleton_impl}", class_info->singleton_impl);
			break;
	}

	g_free(fileheader);

	if (template)
		return g_string_free(template, FALSE);
	else
		return NULL;
}

/* Creates a new option label, indented on the left */
static GtkWidget *cc_option_label_new(const gchar *text)
{
	GtkWidget *align;
	GtkWidget *label;

	align = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

	label = gtk_label_new(text);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_container_add(GTK_CONTAINER(align), label);

	return align;
}

/* Attaches a new section label at the specified table row, optionally
 * padded at the top, and returns the new label. */
static GtkWidget *cc_table_attach_section_label(GtkWidget *table, const gchar *text,
		guint row, gboolean top_padding)
{
	gchar *markup;
	GtkWidget *label, *align;

	label = gtk_label_new(NULL);
	markup = g_markup_printf_escaped("<b>%s</b>", text);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	align = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
	if (top_padding)
		gtk_alignment_set_padding(GTK_ALIGNMENT(align), 6, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(align), label);

	gtk_table_attach(GTK_TABLE(table), align,
					 0, 2, row, row+1,
					 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	return label;
}

/* Attach a new option label at the specified table row and returns
 * the label */
static GtkWidget *cc_table_attach_option_label(GtkWidget *table, const gchar *text, guint row)
{
	GtkWidget *opt_label = cc_option_label_new(text);
	gtk_table_attach(GTK_TABLE(table), opt_label,
					 0, 1, row, row+1,
					 GTK_FILL|GTK_SHRINK, GTK_FILL|GTK_SHRINK, 0, 0);
	return opt_label;
}

/* Attach an option label and entry to the table at the specified row.
 * The label associated with the widget is set as data on the entry
 * with the "label" key, if access to it is needed later.  The entry
 * widget is returned. */
static GtkWidget *cc_table_attach_option_entry(GtkWidget *table, const gchar *text, guint row)
{
	GtkWidget *label;
	GtkWidget *entry;
	label = cc_table_attach_option_label(table, text, row);
	entry = gtk_entry_new();
	g_object_set_data(G_OBJECT(entry), "label", label);
	gtk_table_attach(GTK_TABLE(table), entry,
					 1, 2, row, row+1,
					 GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	return entry;
}

static void show_dialog_create_class(gint type)
{
	CreateClassDialog *cc_dlg;
	GtkWidget *main_box, *table, *label, *hdr_hbox;
	GtkWidget *opt_table, *align;
	guint row;

	cc_dlg = g_new0(CreateClassDialog, 1);
	cc_dlg->class_type = type;

	cc_dlg->dialog = gtk_dialog_new_with_buttons(_("Create Class"),
			GTK_WINDOW(geany->main_widgets->window),
			GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_has_separator(GTK_DIALOG(cc_dlg->dialog), TRUE);

	switch (type)
	{
		case GEANY_CLASS_TYPE_CPP:
			gtk_window_set_title(GTK_WINDOW(cc_dlg->dialog), _("Create C++ Class"));
			break;
		case GEANY_CLASS_TYPE_GTK:
			gtk_window_set_title(GTK_WINDOW(cc_dlg->dialog), _("Create GTK+ Class"));
			break;
		case GEANY_CLASS_TYPE_PHP:
			gtk_window_set_title(GTK_WINDOW(cc_dlg->dialog), _("Create PHP Class"));
			break;
	}

	g_signal_connect_swapped(cc_dlg->dialog, "destroy", G_CALLBACK(g_free), (gpointer)cc_dlg);

	table = gtk_table_new(13, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	main_box = ui_dialog_vbox_new(GTK_DIALOG(cc_dlg->dialog));
	gtk_box_pack_start(GTK_BOX(main_box), table, TRUE, TRUE, 0);

	row = 0;

	if (type == GEANY_CLASS_TYPE_PHP || type == GEANY_CLASS_TYPE_GTK)
	{
		cc_table_attach_section_label(table, _("Namespace"), row++, FALSE);
		cc_dlg->class_namespace_entry = cc_table_attach_option_entry(table, _("Name:"), row++);
		g_signal_connect(cc_dlg->class_namespace_entry, "changed",
				G_CALLBACK(cc_dlg_on_class_namespace_entry_changed), cc_dlg);
	}

	if (type == GEANY_CLASS_TYPE_PHP || type == GEANY_CLASS_TYPE_GTK)
		cc_table_attach_section_label(table, _("Class"), row++, TRUE);
	else
		cc_table_attach_section_label(table, _("Class"), row++, FALSE);

	cc_dlg->class_name_entry = cc_table_attach_option_entry(table, _("Name:"), row++);
	g_signal_connect(cc_dlg->class_name_entry, "changed",
			G_CALLBACK(cc_dlg_on_class_name_entry_changed), cc_dlg);

	if (type != GEANY_CLASS_TYPE_PHP)
		cc_dlg->header_entry = cc_table_attach_option_entry(table, _("Header file:"), row++);

	cc_dlg->source_entry = cc_table_attach_option_entry(table, _("Source file:"), row++);

	cc_table_attach_section_label(table, _("Inheritance"), row++, TRUE);

	cc_dlg->base_name_entry = cc_table_attach_option_entry(table, _("Base class:"), row++);

	if (type == GEANY_CLASS_TYPE_GTK)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_name_entry), "GObject");
	g_signal_connect(cc_dlg->base_name_entry, "changed",
			G_CALLBACK(cc_dlg_on_base_name_entry_changed), (gpointer)cc_dlg);

	if (type == GEANY_CLASS_TYPE_PHP)
		cc_dlg->base_header_entry = cc_table_attach_option_entry(table, _("Base source:"), row++);
	else
	{
		hdr_hbox = gtk_hbox_new(FALSE, 6);

		label = cc_table_attach_option_label(table, _("Base header:"), row);

		cc_dlg->base_header_entry = gtk_entry_new();
		g_object_set_data(G_OBJECT(cc_dlg->base_header_entry), "label", label);
		gtk_box_pack_start(GTK_BOX(hdr_hbox),
						   cc_dlg->base_header_entry,
						   TRUE, TRUE, 0);

		cc_dlg->base_header_global_box = gtk_check_button_new_with_label(_("Global"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box), TRUE);
		gtk_box_pack_start(GTK_BOX(hdr_hbox),
						   cc_dlg->base_header_global_box,
						   FALSE, TRUE, 0);

		gtk_table_attach(GTK_TABLE(table), hdr_hbox,
						 1, 2, row, row+1,
						 GTK_FILL | GTK_EXPAND,
						 GTK_FILL | GTK_EXPAND,
						 0, 0);
		row++;
	}

	if (type == GEANY_CLASS_TYPE_GTK)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_header_entry), "glib-object.h");

	if (type == GEANY_CLASS_TYPE_GTK)
	{
		cc_dlg->base_gtype_entry = cc_table_attach_option_entry(table, _("Base GType:"), row++);
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->base_gtype_entry), "G_TYPE_OBJECT");
	}

	if (type == GEANY_CLASS_TYPE_PHP)
		cc_dlg->class_implements_entry = cc_table_attach_option_entry(table, _("Implements:"), row++);

	cc_table_attach_section_label(table, _("Options"), row++, TRUE);

	align = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

	opt_table = gtk_table_new(1, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(opt_table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(opt_table), 6);
	gtk_container_add(GTK_CONTAINER(align), opt_table);

	gtk_table_attach(GTK_TABLE(table), align,
					 0, 2, row, row+1,
					 GTK_FILL|GTK_EXPAND,
					 GTK_FILL|GTK_EXPAND,
					 0, 0);
	row++;

	cc_dlg->create_constructor_box = gtk_check_button_new_with_label(_("Create constructor"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box), TRUE);
	gtk_table_attach(GTK_TABLE(opt_table), cc_dlg->create_constructor_box,
					 0, 1, 0, 1, GTK_FILL|GTK_SHRINK, GTK_FILL|GTK_SHRINK, 0, 0);

	cc_dlg->create_destructor_box = gtk_check_button_new_with_label(_("Create destructor"));
	gtk_table_attach(GTK_TABLE(opt_table), cc_dlg->create_destructor_box,
					 1, 2, 0, 1, GTK_FILL|GTK_SHRINK, GTK_FILL|GTK_SHRINK, 0, 0);

	if (type == GEANY_CLASS_TYPE_PHP)
	{
		gtk_table_resize(GTK_TABLE(opt_table), 2, 2);
		cc_dlg->create_isabstract_box = gtk_check_button_new_with_label(_("Is abstract"));
		gtk_table_attach(GTK_TABLE(opt_table), cc_dlg->create_isabstract_box,
						 0, 1, 1, 2, GTK_FILL|GTK_SHRINK, GTK_FILL|GTK_SHRINK, 0, 0);
		cc_dlg->create_issingleton_box = gtk_check_button_new_with_label(_("Is singleton"));
		gtk_table_attach(GTK_TABLE(opt_table), cc_dlg->create_issingleton_box,
						 1, 2, 1, 2, GTK_FILL|GTK_SHRINK, GTK_FILL|GTK_SHRINK, 0, 0);
	}

	gtk_widget_show_all(align);

	if (type == GEANY_CLASS_TYPE_GTK)
	{
		cc_dlg->gtk_constructor_type_entry = cc_table_attach_option_entry(table,
			_("Constructor type:"), row++);
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry), "GObject");
		g_signal_connect(cc_dlg->create_constructor_box, "toggled",
						 G_CALLBACK(cc_dlg_on_set_sensitive_toggled),
						 cc_dlg->gtk_constructor_type_entry);
	}
	else if (type == GEANY_CLASS_TYPE_PHP)
		gtk_table_resize(GTK_TABLE(table), row, 2);
	else if (type == GEANY_CLASS_TYPE_CPP)
		gtk_table_resize(GTK_TABLE(table), row, 2);

	gtk_widget_show_all(cc_dlg->dialog);
	while (gtk_dialog_run(GTK_DIALOG(cc_dlg->dialog)) == GTK_RESPONSE_OK)
	{
		if (create_class(cc_dlg))
			break;
		else
			gdk_beep();
	}
	gtk_widget_destroy(cc_dlg->dialog);
}


static void cc_dlg_on_set_sensitive_toggled(GtkWidget *toggle_button, GtkWidget *target_widget)
{
	GtkWidget *label;

	g_return_if_fail(toggle_button != NULL);
	g_return_if_fail(GTK_IS_TOGGLE_BUTTON(toggle_button));
	g_return_if_fail(target_widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(target_widget));

	label = g_object_get_data(G_OBJECT(target_widget), "label");

	gtk_widget_set_sensitive(target_widget,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
	gtk_widget_set_sensitive(label,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}


static void cc_dlg_update_file_names(CreateClassDialog *cc_dlg)
{
	gchar *class_name;
	gchar *class_name_down;
	gchar *class_header = NULL;
	gchar *class_source = NULL;

	g_return_if_fail(cc_dlg != NULL);

	class_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_name_entry)));
	class_name_down = g_ascii_strdown(class_name, -1);
	switch (cc_dlg->class_type)
	{
		case GEANY_CLASS_TYPE_CPP:
		{
			class_header = g_strconcat(class_name_down, ".h", NULL);
			class_source = g_strconcat(class_name_down, ".cpp", NULL);
			break;
		}
		case GEANY_CLASS_TYPE_GTK:
		{
			const gchar *namespace;
			gchar *namespace_down;

			namespace = gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_namespace_entry));
			namespace_down = g_ascii_strdown(namespace, -1);
			class_header = g_strconcat(namespace_down, class_name_down, ".h", NULL);
			class_source = g_strconcat(namespace_down, class_name_down, ".c", NULL);
			g_free(namespace_down);
			break;
		}
		case GEANY_CLASS_TYPE_PHP:
		{
			class_header = NULL;
			class_source = g_strconcat(class_name, ".php", NULL);
			break;
		}
	}

	if (cc_dlg->header_entry != NULL && class_header != NULL)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->header_entry), class_header);
	if (cc_dlg->source_entry != NULL && class_source != NULL)
		gtk_entry_set_text(GTK_ENTRY(cc_dlg->source_entry), class_source);

	g_free(class_name);
	g_free(class_name_down);
	g_free(class_header);
	g_free(class_source);
}


static void cc_dlg_on_class_name_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg)
{
	cc_dlg_update_file_names(cc_dlg);
}


static void cc_dlg_on_class_namespace_entry_changed(GtkWidget *entry, CreateClassDialog *cc_dlg)
{

	if (cc_dlg->class_type == GEANY_CLASS_TYPE_GTK)
		cc_dlg_update_file_names(cc_dlg);
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
		/*tmp = g_strconcat("gtk/", gtk_entry_get_text(GTK_ENTRY(entry)), ".h", NULL);*/
		/* With GTK 2.14 (and later GTK 3), single header includes are encouraged */
		tmp = g_strdup("gtk/gtk.h");
	else if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), "GObject"))
		tmp = g_strdup("glib-object.h");
	else if (cc_dlg->class_type == GEANY_CLASS_TYPE_PHP)
		tmp = g_strconcat(gtk_entry_get_text(GTK_ENTRY(entry)), ".php", NULL);
	else
		tmp = g_strconcat(gtk_entry_get_text(GTK_ENTRY(entry)), ".h", NULL);

	if (cc_dlg->class_type == GEANY_CLASS_TYPE_PHP)
		base_header = g_strdup(tmp);
	else
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
		else if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), "GObject"))
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


static gboolean create_class(CreateClassDialog *cc_dlg)
{
	ClassInfo *class_info;
	GeanyDocument *doc;
	gchar *text;
	gchar *tmp;

	g_return_val_if_fail(cc_dlg != NULL, FALSE);

	if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_name_entry)), ""))
		return FALSE;

	class_info = g_new0(ClassInfo, 1);
	class_info->type = cc_dlg->class_type;
	class_info->class_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_name_entry)));
	tmp = str_case_split(class_info->class_name, '_');
	class_info->class_name_up = g_ascii_strup(tmp, -1);
	class_info->class_name_low = g_ascii_strdown(class_info->class_name_up, -1);
	if (! utils_str_equal(gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_name_entry)), ""))
	{
		class_info->base_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_name_entry)));
		if (class_info->type != GEANY_CLASS_TYPE_PHP)
		{
			class_info->base_include = g_strdup_printf("\n#include %c%s%c\n",
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box)) ?
				'<' : '\"',
				gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_header_entry)),
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->base_header_global_box)) ?
				'>' : '\"');
		}
		else
		{
			class_info->base_include = g_strdup_printf("\nrequire_once \"%s\";\n",
				gtk_entry_get_text(GTK_ENTRY(cc_dlg->base_header_entry)));
				class_info->base_decl = g_strdup_printf(" extends %s", class_info->base_name);
		}
	}
	else
	{
		class_info->base_name = g_strdup("");
		class_info->base_include = g_strdup("");
	}
	if (cc_dlg->header_entry != NULL)
	{
		class_info->header = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->header_entry)));
		class_info->header_guard = g_ascii_strup(class_info->header, -1);
		g_strdelimit(class_info->header_guard, ".-", '_');
	}
	switch (class_info->type)
	{
		case GEANY_CLASS_TYPE_CPP:
		{
			class_info->source = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->source_entry)));
			if (! utils_str_equal(class_info->base_name, ""))
				class_info->base_decl = g_strdup_printf(": public %s", class_info->base_name);
			else
				class_info->base_decl = g_strdup("");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)))
			{
				gchar *base_constructor;

				if (utils_str_equal(class_info->base_name, ""))
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
			class_info->namespace = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_namespace_entry)));
			if (!NZV(class_info->namespace))
			{
				class_info->namespace_up = g_strdup("");
				class_info->namespace_low = g_strdup("");
			}
			else
			{
				gchar *tmp_namespace;
				gchar *tmp_namespace_split;

				tmp_namespace_split = str_case_split(class_info->namespace, '_');
				tmp_namespace = g_strconcat(tmp_namespace_split, "_", NULL);
				class_info->namespace_up = g_ascii_strup(tmp_namespace, -1);
				class_info->namespace_low = g_ascii_strdown(class_info->namespace_up, -1);
				g_free(tmp_namespace);
				g_free(tmp_namespace_split);
			}
			class_info->base_gtype = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(cc_dlg->base_gtype_entry)));
			class_info->source = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->source_entry)));
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)))
			{
				class_info->constructor_decl = g_strdup_printf("%s *%s%s_new (void);\n",
						gtk_entry_get_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry)),
						class_info->namespace_low, class_info->class_name_low);
				class_info->constructor_impl = g_strdup_printf("\n"
						"%s *\n"
						"%s%s_new (void)\n"
						"{\n"
						"  return g_object_new (%sTYPE_%s, NULL);\n"
						"}",
						gtk_entry_get_text(GTK_ENTRY(cc_dlg->gtk_constructor_type_entry)),
						class_info->namespace_low, class_info->class_name_low,
						class_info->namespace_up, class_info->class_name_up);
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
						"  g_object_class = G_OBJECT_CLASS (klass);\n\n"
						"  g_object_class->finalize = %s%s_finalize;\n",
						class_info->namespace_low, class_info->class_name_low);
				class_info->destructor_decl =
						g_strdup_printf("static void %s%s_finalize (GObject *object);\n",
						class_info->namespace_low, class_info->class_name_low);
				class_info->destructor_impl = g_strdup_printf("\n"
						"static void\n"
						"%s%s_finalize (GObject *object)\n"
						"{\n"
						"  %s%s *self;\n\n"
						"  g_return_if_fail (%sIS_%s (object));\n\n"
						"  self = %s%s (object);\n\n"
						"  G_OBJECT_CLASS (%s%s_parent_class)->finalize (object);\n"
						"}\n",
						class_info->namespace_low,	class_info->class_name_low,
						class_info->namespace,		class_info->class_name,
						class_info->namespace_up,	class_info->class_name_up,
						class_info->namespace_up,	class_info->class_name_up,
						class_info->namespace_low,	class_info->class_name_low);
			}
			else
			{
				class_info->gtk_destructor_registration = g_strdup("");
				class_info->destructor_decl = g_strdup("");
				class_info->destructor_impl = g_strdup("");
			}
			break;
		}
		case GEANY_CLASS_TYPE_PHP:
		{
			const gchar *tmp_str;

			class_info->source = g_strdup(gtk_entry_get_text(GTK_ENTRY(cc_dlg->source_entry)));

			tmp_str = gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_namespace_entry));
			if (! utils_str_equal(tmp_str, ""))
				class_info->namespace_decl = g_strdup_printf("namespace %s;", tmp_str);
			else
				class_info->namespace_decl = g_strdup("");

			tmp_str = gtk_entry_get_text(GTK_ENTRY(cc_dlg->class_implements_entry));
			if (! utils_str_equal(tmp_str, ""))
				class_info->implements_decl = g_strdup_printf(" implements %s", tmp_str);
			else
				class_info->implements_decl = g_strdup("");

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)) &&
			    ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_isabstract_box)))
			{
				class_info->constructor_impl = g_strdup_printf("\n"
					"\t/**\n"
					"\t * Constructor of class %s.\n"
					"\t *\n"
					"\t * @return void\n"
					"\t */\n"
					"\tpublic function __construct()\n"
					"\t{\n"
					"\t\t// ...\n"
					"\t}\n",
					class_info->class_name);
			}
			else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_constructor_box)) &&
			         gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_isabstract_box)))
			{
				class_info->constructor_impl = g_strdup_printf("\n"
					"\t/**\n"
					"\t * Constructor of class %s.\n"
					"\t *\n"
					"\t * @return void\n"
					"\t */\n"
					"\tprotected function __construct()\n"
					"\t{\n"
					"\t\t// ...\n"
					"\t}\n",
					class_info->class_name);
			}
			else
				class_info->constructor_impl = g_strdup("");

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_destructor_box)))
			{
				class_info->destructor_impl = g_strdup_printf("\n"
					"\t/**\n"
					"\t * Destructor of class %s.\n"
					"\t *\n"
					"\t * @return void\n"
					"\t */\n"
					"\tpublic function __destruct()\n"
					"\t{\n"
					"\t\t// ...\n"
					"\t}\n",
					class_info->class_name);
			}
			else
				class_info->destructor_impl = g_strdup("");

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_isabstract_box)))
				class_info->abstract_decl = g_strdup("abstract ");
			else
				class_info->abstract_decl = g_strdup("");

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cc_dlg->create_issingleton_box)))
			{
				class_info->singleton_impl = g_strdup_printf("\n"
					"\t/**\n"
					"\t * Holds instance of self.\n"
					"\t * \n"
					"\t * @var %s\n"
					"\t */\n"
					"\tprotected static $kInstance = null;\n\n"
					"\t/**\n"
					"\t * Returns instance of self.\n"
					"\t * \n"
					"\t * @return %s\n"
					"\t */\n"
					"\tpublic static function getInstance() {\n"
					"\t\tif(!(self::$kInstance instanceof %s)) {\n"
					"\t\t\tself::$kInstance = new self();\n"
					"\t\t}\n"
					"\t\treturn self::$kInstance;\n"
					"\t}\n",
					class_info->class_name,
					class_info->class_name,
					class_info->class_name);
			}
			else
				class_info->singleton_impl = g_strdup("");
			break;
		}
	}

	/* only create the files if the filename is not empty */
	if (! utils_str_equal(class_info->source, ""))
	{
		doc = document_new_file(class_info->source, NULL, NULL);
		text = get_template_class_source(class_info);
		editor_insert_text_block(doc->editor, text, 0, -1, 0, TRUE);
		g_free(text);
		sci_set_current_position(doc->editor->sci, 0, TRUE);
	}

	if (! utils_str_equal(class_info->header, "") && class_info->type != GEANY_CLASS_TYPE_PHP)
	{
		doc = document_new_file(class_info->header, NULL, NULL);
		text = get_template_class_header(class_info);
		editor_insert_text_block(doc->editor, text, 0, -1, 0, TRUE);
		g_free(text);
		sci_set_current_position(doc->editor->sci, 0, TRUE);
	}

	free_pointers(24, tmp, class_info->namespace, class_info->namespace_up,
		class_info->namespace_low, class_info->class_name, class_info->class_name_up,
		class_info->base_name, class_info->class_name_low, class_info->base_include,
		class_info->header, class_info->header_guard, class_info->source, class_info->base_decl,
		class_info->constructor_decl, class_info->constructor_impl,
		class_info->gtk_destructor_registration, class_info->destructor_decl,
		class_info->destructor_impl, class_info->base_gtype,
		class_info->namespace_decl, class_info->implements_decl,
		class_info->abstract_decl, class_info->singleton_impl, class_info, NULL);
	return TRUE;
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


static void
on_menu_create_php_class_activate	(GtkMenuItem	*menuitem,
					 gpointer	user_data)
{
	show_dialog_create_class(GEANY_CLASS_TYPE_PHP);
}


void plugin_init(GeanyData *data)
{
	GtkWidget *menu_create_class1;
	GtkWidget *menu_create_class1_menu;
	GtkWidget *menu_create_cpp_class;
	GtkWidget *menu_create_gtk_class;
	GtkWidget *menu_create_php_class;

	menu_create_class1 = ui_image_menu_item_new (GTK_STOCK_ADD, _("Create Cla_ss"));
	gtk_container_add (GTK_CONTAINER (geany->main_widgets->tools_menu), menu_create_class1);

	menu_create_class1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_create_class1), menu_create_class1_menu);

	menu_create_cpp_class = gtk_menu_item_new_with_mnemonic (_("_C++ Class"));
	gtk_container_add (GTK_CONTAINER (menu_create_class1_menu), menu_create_cpp_class);

	menu_create_gtk_class = gtk_menu_item_new_with_mnemonic (_("_GTK+ Class"));
	gtk_container_add (GTK_CONTAINER (menu_create_class1_menu), menu_create_gtk_class);

	menu_create_php_class = gtk_menu_item_new_with_mnemonic (_("_PHP Class"));
	gtk_container_add (GTK_CONTAINER (menu_create_class1_menu), menu_create_php_class);

	g_signal_connect(menu_create_cpp_class, "activate",
		G_CALLBACK (on_menu_create_cpp_class_activate),
		NULL);
	g_signal_connect(menu_create_gtk_class, "activate",
		G_CALLBACK (on_menu_create_gtk_class_activate),
		NULL);
	g_signal_connect(menu_create_php_class, "activate",
		G_CALLBACK (on_menu_create_php_class_activate),
		NULL);

	gtk_widget_show_all(menu_create_class1);

	main_menu_item = menu_create_class1;
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
}
