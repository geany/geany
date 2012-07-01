/*
 *      htmlchars.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/* HTML Characters plugin (Inserts HTML character entities like '&amp;') */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "geanyplugin.h"
#include <string.h>
#include "SciLexer.h"


GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("HTML Characters"), _("Inserts HTML character entities like '&amp;'."), VERSION,
	_("The Geany developer team"))


/* Keybinding(s) */
enum
{
	KB_INSERT_HTML_CHARS,
	KB_REPLACE_HTML_ENTITIES,
	KB_HTMLTOGGLE_ACTIVE,
	KB_COUNT
};

PLUGIN_KEY_GROUP(html_chars, KB_COUNT)


enum
{
	COLUMN_CHARACTER,
	COLUMN_HTML_NAME,
	N_COLUMNS
};

static GtkWidget *main_menu_item = NULL;
static GtkWidget *main_menu = NULL;
static GtkWidget *main_menu_submenu = NULL;
static GtkWidget *menu_bulk_replace = NULL;
static GtkWidget *sc_dialog = NULL;
static GtkTreeStore *sc_store = NULL;
static GtkTreeView *sc_tree = NULL;
static GtkWidget *menu_htmltoggle = NULL;
static gboolean plugin_active = FALSE;

/* Configuration file */
static gchar *config_file = NULL;

const gchar *chars[][2] ={
	{ N_("HTML characters"), NULL },
	{ "\"", "&quot;" },
	{ "&", "&amp;" },
	{ "<", "&lt;" },
	{ ">", "&gt;" },

	{ N_("ISO 8859-1 characters"), NULL },
	{ " ", "&nbsp;" },
	{ "¡", "&iexcl;" },
	{ "¢", "&cent;" },
	{ "£", "&pound;" },
	{ "¤", "&curren;" },
	{ "¥", "&yen;" },
	{ "¦", "&brvbar;" },
	{ "§", "&sect;" },
	{ "¨", "&uml;" },
	{ "©", "&copy;" },
	{ "®", "&reg;" },
	{ "«", "&laquo;" },
	{ "»", "&raquo;" },
	{ "¬", "&not;" },
	{ " ", "&shy;" },
	{ "¯", "&macr;" },
	{ "°", "&deg;" },
	{ "±", "&plusmn;" },
	{ "¹", "&sup1;" },
	{ "²", "&sup2;" },
	{ "³", "&sup3;" },
	{ "¼", "&frac14;" },
	{ "½", "&frac12;" },
	{ "¾", "&frac34;" },
	{ "×", "&times;" },
	{ "÷", "&divide;" },
	{ "´", "&acute;" },
	{ "µ", "&micro;" },
	{ "¶", "&para;" },
	{ "·", "&middot;" },
	{ "¸", "&cedil;" },
	{ "ª", "&ordf;" },
	{ "º", "&ordm;" },
	{ "¿", "&iquest;" },
	{ "À", "&Agrave;" },
	{ "Á", "&Aacute;" },
	{ "Â", "&Acirc;" },
	{ "Ã", "&Atilde;" },
	{ "Ä", "&Auml;" },
	{ "Å", "&Aring;" },
	{ "Æ", "&AElig;" },
	{ "Ç", "&Ccedil;" },
	{ "È", "&Egrave;" },
	{ "É", "&Eacute;" },
	{ "Ê", "&Ecirc;" },
	{ "Ë", "&Euml;" },
	{ "Ì", "&Igrave;" },
	{ "Í", "&Iacute;" },
	{ "Î", "&Icirc;" },
	{ "Ï", "&Iuml;" },
	{ "Ð", "&ETH;" },
	{ "Ñ", "&Ntilde;" },
	{ "Ò", "&Ograve;" },
	{ "Ó", "&Oacute;" },
	{ "Ô", "&Ocirc;" },
	{ "Õ", "&Otilde;" },
	{ "Ö", "&Ouml;" },
	{ "Ø", "&Oslash;" },
	{ "Ù", "&Ugrave;" },
	{ "Ú", "&Uacute;" },
	{ "Û", "&Ucirc;" },
	{ "Ü", "&Uuml;" },
	{ "Ý", "&Yacute;" },
	{ "Þ", "&THORN;" },
	{ "ß", "&szlig;" },
	{ "à", "&agrave;" },
	{ "á", "&aacute;" },
	{ "â", "&acirc;" },
	{ "ã", "&atilde;" },
	{ "ä", "&auml;" },
	{ "å", "&aring;" },
	{ "æ", "&aelig;" },
	{ "ç", "&ccedil;" },
	{ "è", "&egrave;" },
	{ "é", "&eacute;" },
	{ "ê", "&ecirc;" },
	{ "ë", "&euml;" },
	{ "ì", "&igrave;" },
	{ "í", "&iacute;" },
	{ "î", "&icirc;" },
	{ "ï", "&iuml;" },
	{ "ð", "&eth;" },
	{ "ñ", "&ntilde;" },
	{ "ò", "&ograve;" },
	{ "ó", "&oacute;" },
	{ "ô", "&ocirc;" },
	{ "õ", "&otilde;" },
	{ "ö", "&ouml;" },
	{ "ø", "&oslash;" },
	{ "ù", "&ugrave;" },
	{ "ú", "&uacute;" },
	{ "û", "&ucirc;" },
	{ "ü", "&uuml;" },
	{ "ý", "&yacute;" },
	{ "þ", "&thorn;" },
	{ "ÿ", "&yuml;" },

	{ N_("Greek characters"), NULL },
	{ "Α", "&Alpha;" },
	{ "α", "&alpha;" },
	{ "Β", "&Beta;" },
	{ "β", "&beta;" },
	{ "Γ", "&Gamma;" },
	{ "γ", "&gamma;" },
	{ "Δ", "&Delta;" },
	{ "δ", "&Delta;" },
	{ "δ", "&delta;" },
	{ "Ε", "&Epsilon;" },
	{ "ε", "&epsilon;" },
	{ "Ζ", "&Zeta;" },
	{ "ζ", "&zeta;" },
	{ "Η", "&Eta;" },
	{ "η", "&eta;" },
	{ "Θ", "&Theta;" },
	{ "θ", "&theta;" },
	{ "Ι", "&Iota;" },
	{ "ι", "&iota;" },
	{ "Κ", "&Kappa;" },
	{ "κ", "&kappa;" },
	{ "Λ", "&Lambda;" },
	{ "λ", "&lambda;" },
	{ "Μ", "&Mu;" },
	{ "μ", "&mu;" },
	{ "Ν", "&Nu;" },
	{ "ν", "&nu;" },
	{ "Ξ", "&Xi;" },
	{ "ξ", "&xi;" },
	{ "Ο", "&Omicron;" },
	{ "ο", "&omicron;" },
	{ "Π", "&Pi;" },
	{ "π", "&pi;" },
	{ "Ρ", "&Rho;" },
	{ "ρ", "&rho;" },
	{ "Σ", "&Sigma;" },
	{ "ς", "&sigmaf;" },
	{ "σ", "&sigma;" },
	{ "Τ", "&Tau;" },
	{ "τ", "&tau;" },
	{ "Υ", "&Upsilon;" },
	{ "υ", "&upsilon;" },
	{ "Φ", "&Phi;" },
	{ "φ", "&phi;" },
	{ "Χ", "&Chi;" },
	{ "χ", "&chi;" },
	{ "Ψ", "&Psi;" },
	{ "ψ", "&psi;" },
	{ "Ω", "&Omega;" },
	{ "ω", "&omega;" },
	{ "ϑ", "&thetasym;" },
	{ "ϒ", "&upsih;" },
	{ "ϖ", "&piv;" },

	{ N_("Mathematical characters"), NULL },
	{ "∀", "&forall;" },
	{ "∂", "&part;" },
	{ "∃", "&exist;" },
	{ "∅", "&empty;" },
	{ "∇", "&nabla;" },
	{ "∈", "&isin;" },
	{ "∉", "&notin;" },
	{ "∋", "&ni;" },
	{ "∏", "&prod;" },
	{ "∑", "&sum;" },
	{ "−", "&minus;" },
	{ "∗", "&lowast;" },
	{ "√", "&radic;" },
	{ "∝", "&prop;" },
	{ "∞", "&infin;" },
	{ "∠", "&ang;" },
	{ "∧", "&and;" },
	{ "∨", "&or;" },
	{ "∩", "&cap;" },
	{ "∪", "&cup;" },
	{ "∫", "&int;" },
	{ "∴", "&there4;" },
	{ "∼", "&sim;" },
	{ "≅", "&cong;" },
	{ "≈", "&asymp;" },
	{ "≠", "&ne;" },
	{ "≡", "&equiv;" },
	{ "≤", "&le;" },
	{ "≥", "&ge;" },
	{ "⊂", "&sub;" },
	{ "⊃", "&sup;" },
	{ "⊄", "&nsub;" },
	{ "⊆", "&sube;" },
	{ "⊇", "&supe;" },
	{ "⊕", "&oplus;" },
	{ "⊗", "&otimes;" },
	{ "⊥", "&perp;" },
	{ "⋅", "&sdot;" },
	{ "◊", "&loz;" },

	{ N_("Technical characters"), NULL },
	{ "⌈", "&lceil;" },
	{ "⌉", "&rceil;" },
	{ "⌊", "&lfloor;" },
	{ "⌋", "&rfloor;" },
	{ "〈", "&lang;" },
	{ "〉", "&rang;" },

	{ N_("Arrow characters"), NULL },
	{ "←", "&larr;" },
	{ "↑", "&uarr;" },
	{ "→", "&rarr;" },
	{ "↓", "&darr;" },
	{ "↔", "&harr;" },
	{ "↵", "&crarr;" },
	{ "⇐", "&lArr;" },
	{ "⇑", "&uArr;" },
	{ "⇒", "&rArr;" },
	{ "⇓", "&dArr;" },
	{ "⇔", "&hArr;" },

	{ N_("Punctuation characters"), NULL },
	{ "–", "&ndash;" },
	{ "—", "&mdash;" },
	{ "‘", "&lsquo;" },
	{ "’", "&rsquo;" },
	{ "‚", "&sbquo;" },
	{ "“", "&ldquo;" },
	{ "”", "&rdquo;" },
	{ "„", "&bdquo;" },
	{ "†", "&dagger;" },
	{ "‡", "&Dagger;" },
	{ "…", "&hellip;" },
	{ "‰", "&permil;" },
	{ "‹", "&lsaquo;" },
	{ "›", "&rsaquo;" },

	{ N_("Miscellaneous characters"), NULL },
	{ "•", "&bull;" },
	{ "′", "&prime;" },
	{ "″", "&Prime;" },
	{ "‾", "&oline;" },
	{ "⁄", "&frasl;" },
	{ "℘", "&weierp;" },
	{ "ℑ", "&image;" },
	{ "ℜ", "&real;" },
	{ "™", "&trade;" },
	{ "€", "&euro;" },
	{ "ℵ", "&alefsym;" },
	{ "♠", "&spades;" },
	{ "♣", "&clubs;" },
	{ "♥", "&hearts;" },
	{ "♦", "&diams;" },
	{ "Œ", "&OElig;" },
	{ "œ", "&oelig;" },
	{ "Š", "&Scaron;" },
	{ "š", "&scaron;" },
	{ "Ÿ", "&Yuml;" },
	{ "ƒ", "&fnof;" },
};

static gboolean ht_editor_notify_cb(GObject *object, GeanyEditor *editor,
									SCNotification *nt, gpointer data);


PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &ht_editor_notify_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* Functions to toggle the status of plugin */
static void set_status(gboolean new_status)
{
	if (plugin_active != new_status)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(config_file);

		plugin_active = new_status;

		/* Now we save the new status into configuration file to
		 * remember next time */
		g_key_file_set_boolean(config, "general", "replacement_on_typing_active",
			plugin_active);

		if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
			&& utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			data = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(config_file, data);
			g_free(data);
		}
		g_free(config_dir);
		g_key_file_free(config);
	}
}


static void toggle_status(G_GNUC_UNUSED GtkMenuItem * menuitem)
{
	if (plugin_active == TRUE)
		set_status(FALSE);
	else
		set_status(TRUE);
}


static void sc_on_tools_show_dialog_insert_special_chars_response
		(GtkDialog *dialog, gint response, gpointer user_data);
static void sc_on_tree_row_activated
		(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data);
static void sc_fill_store(GtkTreeStore *store);
static gboolean sc_insert(GtkTreeModel *model, GtkTreeIter *iter);


/* Function takes over value of key which was pressed and returns
 * HTML/SGML entity if any */
static const gchar *get_entity(gchar *letter)
{
	guint i, len;

	len = G_N_ELEMENTS(chars);

	/* Ignore tags marking caracters as well as spaces*/
	for (i = 7; i < len; i++)
	{
		if (utils_str_equal(chars[i][0], letter) &&
			!utils_str_equal(" ", letter))
		{
			return chars[i][1];
		}
	}

	/* if the char is not in the list */
	return NULL;
}


static gboolean ht_editor_notify_cb(GObject *object, GeanyEditor *editor,
									SCNotification *nt, gpointer data)
{
	gint lexer;

	g_return_val_if_fail(editor != NULL, FALSE);

	if (!plugin_active)
		return FALSE;

	lexer = sci_get_lexer(editor->sci);
	if (lexer != SCLEX_HTML && lexer != SCLEX_XML)
			return FALSE;

	if (nt->nmhdr.code == SCN_CHARADDED)
	{
		gchar buf[7];
		gint len;

		len = g_unichar_to_utf8(nt->ch, buf);
		if (len > 0)
		{
			const gchar *entity;
			buf[len] = '\0';
			entity = get_entity(buf);

			if (entity != NULL)
			{
				gint pos = sci_get_current_position(editor->sci);

				sci_set_selection_start(editor->sci, pos - len);
				sci_set_selection_end(editor->sci, pos);

				sci_replace_sel(editor->sci, entity);
			}
		}
	}

	return FALSE;
}


/* Called when keys were pressed */
static void kbhtmltoggle_toggle(G_GNUC_UNUSED guint key_id)
{
	if (plugin_active == TRUE)
	{
		set_status(FALSE);
	}
	else
	{
		set_status(TRUE);
	}
}


static void tools_show_dialog_insert_special_chars(void)
{
	if (sc_dialog == NULL)
	{
		gint height;
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		GtkWidget *swin, *vbox, *label;

		sc_dialog = gtk_dialog_new_with_buttons(
					_("Special Characters"), GTK_WINDOW(geany->main_widgets->window),
					GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					_("_Insert"), GTK_RESPONSE_OK, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(sc_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 6);
		gtk_widget_set_name(sc_dialog, "GeanyDialog");

		height = GEANY_DEFAULT_DIALOG_HEIGHT;
		gtk_window_set_default_size(GTK_WINDOW(sc_dialog), height * 8 / 10, height);
		gtk_dialog_set_default_response(GTK_DIALOG(sc_dialog), GTK_RESPONSE_CANCEL);

		label = gtk_label_new(_("Choose a special character from the list below and double click on it or use the button to insert it at the current cursor position."));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		sc_tree = GTK_TREE_VIEW(gtk_tree_view_new());

		sc_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(sc_tree),
								GTK_TREE_MODEL(sc_store));
		g_object_unref(sc_store);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("Character"), renderer, "text", COLUMN_CHARACTER, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(sc_tree), column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("HTML (name)"), renderer, "text", COLUMN_HTML_NAME, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(sc_tree), column);

		swin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport(
					GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(sc_tree));

		gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

		g_signal_connect(sc_tree, "row-activated", G_CALLBACK(sc_on_tree_row_activated), NULL);

		g_signal_connect(sc_dialog, "response",
					G_CALLBACK(sc_on_tools_show_dialog_insert_special_chars_response), NULL);

		g_signal_connect(sc_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		sc_fill_store(sc_store);

		/*gtk_tree_view_expand_all(special_characters_tree);*/
		gtk_tree_view_set_search_column(sc_tree, COLUMN_HTML_NAME);
	}
	gtk_widget_show_all(sc_dialog);
}


/* fill the tree model with data
 ** TODO move this in a file and make it extendable for more data types */
static void sc_fill_store(GtkTreeStore *store)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	guint i, len;

	len = G_N_ELEMENTS(chars);
	for (i = 0; i < len; i++)
	{
		if (chars[i][1] == NULL)
		{	/* add a category */
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter, COLUMN_CHARACTER, chars[i][0], -1);
			if (parent_iter != NULL) gtk_tree_iter_free(parent_iter);
			parent_iter = gtk_tree_iter_copy(&iter);
		}
		else
		{	/* add child to parent_iter */
			gtk_tree_store_append(store, &iter, parent_iter);
			gtk_tree_store_set(store, &iter, COLUMN_CHARACTER, chars[i][0],
											 COLUMN_HTML_NAME, chars[i][1], -1);
		}
	}
}


/* just inserts the HTML_NAME coloumn of the selected row at current position
 * returns only TRUE if a valid selection(i.e. no category) could be found */
static gboolean sc_insert(GtkTreeModel *model, GtkTreeIter *iter)
{
	GeanyDocument *doc = document_get_current();
	gboolean result = FALSE;

	if (doc != NULL)
	{
		gchar *str;
		gint pos = sci_get_current_position(doc->editor->sci);

		gtk_tree_model_get(model, iter, COLUMN_HTML_NAME, &str, -1);
		if (NZV(str))
		{
			sci_insert_text(doc->editor->sci, pos, str);
			g_free(str);
			result = TRUE;
		}
	}
	return result;
}


static void sc_on_tools_show_dialog_insert_special_chars_response(GtkDialog *dialog, gint response,
														gpointer user_data)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(sc_tree);

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			/* only hide dialog if selection was not a category */
			if (sc_insert(model, &iter))
				gtk_widget_hide(GTK_WIDGET(dialog));
		}
	}
	else
		gtk_widget_hide(GTK_WIDGET(dialog));
}


static void sc_on_tree_row_activated(GtkTreeView *treeview, GtkTreePath *path,
											  GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(sc_store);

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		/* only hide dialog if selection was not a category */
		if (sc_insert(model, &iter))
			gtk_widget_hide(sc_dialog);
		else
		{	/* double click on a category to toggle the expand or collapse it */
			if (gtk_tree_view_row_expanded(sc_tree, path))
				gtk_tree_view_collapse_row(sc_tree, path);
			else
				gtk_tree_view_expand_row(sc_tree, path, FALSE);
		}
	}
}


static void replace_special_character(void)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if (doc != NULL && sci_has_selection(doc->editor->sci))
	{
		gsize selection_len;
		gchar *selection;
		GString *replacement = g_string_new(NULL);
		gsize i;
		gchar *new;
		const gchar *entity = NULL;
		gchar buf[7];
		gint len;

		selection = sci_get_selection_contents(doc->editor->sci);

		selection_len = strlen(selection);
		for (i = 0; i < selection_len; i++)
		{
			len = g_unichar_to_utf8(g_utf8_get_char(selection + i), buf);
			i = (guint)len - 1 + i;

			buf[len] = '\0';
			entity = get_entity(buf);

			if (entity != NULL)
			{
				replacement = g_string_append(replacement, entity);
			}
			else
			{
				replacement = g_string_append(replacement, buf);
			}
		}
		new = g_string_free(replacement, FALSE);
		sci_replace_sel(doc->editor->sci, new);
		g_free(selection);
		g_free(new);
	}
}


/* Callback for special chars menu */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	tools_show_dialog_insert_special_chars();
}


static void kb_activate(G_GNUC_UNUSED guint key_id)
{
	item_activate(NULL, NULL);
}


/* Callback for bulk replacement of selected text */
static void
replace_special_character_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	replace_special_character();
}


static void kb_special_chars_replacement(G_GNUC_UNUSED guint key_id)
{
	replace_special_character();
}


static void init_configuration(void)
{
	GKeyFile *config = g_key_file_new();

	/* loading configurations from file ...*/
	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
	"plugins", G_DIR_SEPARATOR_S,
	"htmchars", G_DIR_SEPARATOR_S, "general.conf", NULL);

	/* ... and initialising options from config file */
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	plugin_active = utils_get_setting_boolean(config, "general",
		"replacement_on_typing_active", FALSE);
}


/* Called by Geany to initialise the plugin */
void plugin_init(GeanyData *data)
{
	GtkWidget *menu_item;
	const gchar *menu_text = _("_Insert Special HTML Characters");

	/* First we catch the configuration and initialize them */
	init_configuration();

	/* Add an item to the Tools menu for html chars dialog*/
	menu_item = gtk_menu_item_new_with_mnemonic(menu_text);
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(item_activate), NULL);

	/* disable menu_item when there are no documents open */
	ui_add_document_sensitive(menu_item);

	/* Add menuitem for html replacement functions*/
	main_menu = gtk_menu_item_new_with_mnemonic(_("_HTML Replacement"));
	gtk_widget_show_all(main_menu);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu);

	main_menu_submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu), main_menu_submenu);

	menu_htmltoggle = gtk_check_menu_item_new_with_mnemonic(_("_Auto-replace Special Characters"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_htmltoggle),
		plugin_active);
	gtk_container_add(GTK_CONTAINER(main_menu_submenu), menu_htmltoggle);

	g_signal_connect((gpointer) menu_htmltoggle, "activate",
			 G_CALLBACK(toggle_status), NULL);

	menu_bulk_replace = gtk_menu_item_new_with_mnemonic(
						_("_Replace Characters in Selection"));
	g_signal_connect((gpointer) menu_bulk_replace, "activate",
			 G_CALLBACK(replace_special_character_activated), NULL);

	gtk_container_add(GTK_CONTAINER(main_menu_submenu), menu_bulk_replace);

	ui_add_document_sensitive(main_menu);
	gtk_widget_show(menu_bulk_replace);
	gtk_widget_show(menu_htmltoggle);

	main_menu_item = menu_item;

	/* setup keybindings */
	keybindings_set_item(plugin_key_group, KB_INSERT_HTML_CHARS,
		kb_activate, 0, 0, "insert_html_chars",
		_("Insert Special HTML Characters"), menu_item);
	keybindings_set_item(plugin_key_group, KB_REPLACE_HTML_ENTITIES,
		kb_special_chars_replacement, 0, 0, "replace_special_characters",
		_("Replace special characters"), NULL);
	keybindings_set_item(plugin_key_group, KB_HTMLTOGGLE_ACTIVE,
		kbhtmltoggle_toggle, 0, 0, "htmltoogle_toggle_plugin_status",
		_("Toggle plugin status"), menu_htmltoggle);
}


/* Destroy widgets */
void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
	gtk_widget_destroy(main_menu);

	if (sc_dialog != NULL)
		gtk_widget_destroy(sc_dialog);

	if (config_file != NULL)
		g_free(config_file);
}
