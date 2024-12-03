/*
 *      demopluginext.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2024 The Geany contributors
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

/**
 * Demo plugin extension - example of a plugin using the PluginExtension API
 * to provide simple autocompletion of Python keywords. This is the plugin
 * example used in the documentation with full implementation of
 * @c autocomplete_perform().
 *
 * Note: This is not installed by default, but (on *nix) you can build it as follows:
 * cd plugins
 * make demopluginext.so
 *
 * Then copy or symlink the plugins/demopluginext.so file to ~/.config/geany/plugins
 * - it will be loaded at next startup.
 */

#include <geanyplugin.h>

static gboolean autocomplete_provided(GeanyDocument *doc, gpointer data)
{
    return doc->file_type->id == GEANY_FILETYPES_PYTHON;
}


static void autocomplete_perform(GeanyDocument *doc, gboolean force, gpointer data)
{
    const gchar *kwd_str = "False None True and as assert async await break case class continue def del elif else except finally for from global if import in is lambda match nonlocal not or pass raise return try while with yield";
    gint pos = sci_get_current_position(doc->editor->sci);
    gchar *word = editor_get_word_at_pos(doc->editor, pos, NULL);
    gchar **kwd;

    if (word && *word)
    {
        GString *words = g_string_sized_new(100);
        gchar **kwds = g_strsplit(kwd_str, " ", -1);

        foreach_strv(kwd, kwds)
        {
            if (g_str_has_prefix(*kwd, word))
            {
                if (words->len > 0)
                    g_string_append(words, "\n");

                g_string_append(words, *kwd);
            }
        }

        scintilla_send_message(doc->editor->sci, SCI_AUTOCSHOW, strlen(word), (sptr_t) words->str);
        g_string_free(words, TRUE);
        g_strfreev(kwds);
    }

    g_free(word);
}


static PluginExtension extension = {
    .autocomplete_provided = autocomplete_provided,
    .autocomplete_perform = autocomplete_perform
};


static gboolean on_editor_notify(G_GNUC_UNUSED GObject *obj, GeanyEditor *editor, SCNotification *nt,
    G_GNUC_UNUSED gpointer user_data)
{
    if (nt->nmhdr.code == SCN_AUTOCSELECTION)
    {
        if (plugin_extension_autocomplete_provided(editor->document, &extension))
        {
            /* we can be sure it was us who performed the autocompletion and
             * not Geany or some other plugin extension */
            msgwin_status_add("PluginExtensionDemo autocompleted '%s'", nt->text);
        }
    }

    return FALSE;
}


static PluginCallback plugin_callbacks[] = {
    {"editor-notify", (GCallback) &on_editor_notify, FALSE, NULL},
    {NULL, NULL, FALSE, NULL}
};


static gboolean init_func(GeanyPlugin *plugin, gpointer pdata)
{
    plugin_extension_register(&extension, "Python keyword autocompletion", 450, NULL);
    return TRUE;
}


static void cleanup_func(GeanyPlugin *plugin, gpointer pdata)
{
    plugin_extension_unregister(&extension);
}


G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name = "PluginExtensionDemo";
    plugin->info->description = "Demo performing simple Python keyword autocompletion";
    plugin->info->version = "1.0";
    plugin->info->author = "John Doe <john.doe@example.org>";

    plugin->funcs->init = init_func;
    plugin->funcs->cleanup = cleanup_func;
    plugin->funcs->callbacks = plugin_callbacks;

    GEANY_PLUGIN_REGISTER(plugin, 248);
}
