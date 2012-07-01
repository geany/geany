/*
 *      pluginsignals.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* Note: this file is for Doxygen only. */

/**
 *  @file pluginsignals.c
 *  Plugin Signals
 *
 *
 *  @section Usage
 *
 *  To use plugin signals in Geany, you have two options:
 *
 *  -# Create a PluginCallback array with the @ref plugin_callbacks symbol. List the signals
 *     you want to listen to and create the appropiate signal callbacks for each signal.
 *     The callback array is read @a after plugin_init() has been called.
 *  -# Use plugin_signal_connect(), which can be called at any time and can also connect
 *     to non-Geany signals (such as GTK widget signals).
 *
 *  This page lists the signal prototypes, but you connect to them using the
 *  string name (which by convention uses @c - hyphens instead of @c _ underscores).
 *
 *  E.g. @c "document-open" for @ref document_open.
 *
 *  The following code demonstrates how to use signals in Geany plugins. The code can be inserted
 *  in your plugin code at any desired position.
 *
 *  @code
static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	printf("Example: %s was opened\n", DOC_FILENAME(doc));
}

PluginCallback plugin_callbacks[] =
{
	{ "document-open", (GCallback) &on_document_open, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};
 *  @endcode
 *  @note The PluginCallback array has to be ended with a final @c NULL entry.
 */

/** Sent when a new document is created.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the new document.
 *  @param user_data user data.
 */
signal void (*document_new)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent when a new document is opened.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the opened document.
 *  @param user_data user data.
 */
signal void (*document_open)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent when an existing document is reloaded.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the re-opened document.
 *  @param user_data user data.
 *
 *  @since 0.21
 */
signal void (*document_reload)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent before a document is saved.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the document to be saved.
 *  @param user_data user data.
 */
signal void (*document_before_save)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent when a new document is saved.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the saved document.
 *  @param user_data user data.
 */
signal void (*document_save)(GObject *obj, GeanyDocument *doc, gpointer user_data);


/** Sent after the filetype of a document has been changed.
 *  The previous filetype object is passed but it can be NULL (e.g. at startup).
 *  The new filetype can be read with: @code
 *     GeanyFiletype *ft = doc->file_type;
 *  @endcode
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the saved document.
 *  @param filetype_old the previous filetype of the document.
 *  @param user_data user data.
 */
signal void (*document_filetype_set)(GObject *obj, GeanyDocument *doc, GeanyFiletype *filetype_old, gpointer user_data);

/** Sent when switching notebook pages.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the current document.
 *  @param user_data user data.
 */
signal void (*document_activate)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent before closing a document.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param doc the document about to be closed.
 *  @param user_data user data.
 */
signal void (*document_close)(GObject *obj, GeanyDocument *doc, gpointer user_data);

/** Sent after a project is opened but before session files are loaded.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param config an exising GKeyFile object which can be used to read and write data.
 *    It must not be closed or freed.
 *  @param user_data user data.
 */
signal void (*project_open)(GObject *obj, GKeyFile *config, gpointer user_data);

/** Sent when a project is saved(happens when the project is created, the properties
 *  dialog is closed or Geany is exited). This signal is emitted shortly before Geany
 *  will write the contents of the GKeyFile to the disc.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param config an exising GKeyFile object which can be used to read and write data.
 *    It must not be closed or freed.
 *  @param user_data user data.
 */
signal void (*project_save)(GObject *obj, GKeyFile *config, gpointer user_data);

/** Sent after a project is closed.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param user_data user data.
 */
signal void (*project_close)(GObject *obj, gpointer user_data);

/** Sent after a project dialog is opened but before it is displayed. Plugins
 *  can append their own project settings tabs by using this signal.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param notebook a GtkNotebook instance that can be used by plugins to append their
 *  settings tabs.
 *  @param user_data user data.
 */
signal void (*project_dialog_open)(GObject *obj, GtkWidget *notebook, gpointer user_data);

/** Sent when the settings dialog is confirmed by the user. Plugins can use
 *  this signal to read the settings widgets previously added by using the
 *  @c project-dialog-open signal.
 *  @warning The dialog will still be running afterwards if the user chose 'Apply'.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param notebook a GtkNotebook instance that can be used by plugins to read their
 *  settings widgets.
 *  @param user_data user data.
 */
signal void (*project_dialog_confirmed)(GObject *obj, GtkWidget *notebook, gpointer user_data);

/** Sent before project dialog is closed. By using this signal, plugins can remove 
 *  tabs previously added in project-dialog-open signal handler.
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param notebook a GtkNotebook instance that can be used by plugins to remove
 *  settings tabs previously added in the project-dialog-open signal handler.
 *  @param user_data user data.
 */
signal void (*project_dialog_close)(GObject *obj, GtkWidget *notebook, gpointer user_data);

/** Sent once Geany has finished all initialization and startup tasks and the GUI has been
 *  realized. This signal is the very last step in the startup process and is sent once
 *  the GTK main event loop has been entered.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param user_data user data.
 */
signal void (*geany_startup_complete)(GObject *obj, gpointer user_data);

/** Sent before build is started. A plugin could use this signal e.g. to save all unsaved documents
 *  before the build starts.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param user_data user data.
 */
signal void (*build_start)(GObject *obj, gpointer user_data);

/** Sent before the popup menu of the editing widget is shown. This can be used to modify or extend
 *  the popup menu.
 *
 *  @note You can add menu items from @c plugin_init() using @c geany->main_widgets->editor_menu,
 *  remembering to destroy them in @c plugin_cleanup().
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param word the current word (in UTF-8 encoding) below the cursor position
		   where the popup menu will be opened.
 *  @param click_pos the cursor position where the popup will be opened.
 *  @param doc the current document.
 *  @param user_data user data.
 */
signal void (*update_editor_menu)(GObject *obj, const gchar *word, gint pos, GeanyDocument *doc,
		gpointer user_data);

/** Sent whenever something in the editor widget changes.
 *  E.g. Character added, fold level changes, clicks to the line number margin.
 *  A detailed description of possible notifications and the SCNotification can be found at
 *  http://www.scintilla.org/ScintillaDoc.html#Notifications.
 *
 *  If you connect to this signal, you must check @c nt->nmhdr.code for the notification type
 *  to prevent handling unwanted notifications. This is important because for instance SCN_UPDATEUI
 *  is sent very often whereas you probably don't want to handle this notification.
 *
 *  By default, the signal is sent before Geany's default handler is processing the event.
 *  Your callback function should return FALSE to allow Geany processing the event as well. If you
 *  want to prevent this for some reason, return TRUE.
 *  Please use this with care as it can break basic functionality of Geany.
 *
 *  The signal can be sent after Geany's default handler has been run when you set
 *  PluginCallback::after field to TRUE.
 *
 *  An example callback implemention of this signal can be found in the Demo plugin.
 *
 *  @warning This signal has much power and should be used carefully. You should especially
 *           care about the return value; make sure to return TRUE only if it is necessary
 *           and in the correct situations.
 *
 *  @param obj a GeanyObject instance, should be ignored.
 *  @param editor The current GeanyEditor.
 *  @param nt A pointer to the SCNotification struct which holds additional information for
 *            the event.
 *  @param user_data user data.
 *  @return @c TRUE to stop other handlers from being invoked for the event.
 * 	    @c FALSE to propagate the event further.
 *
 *  @since 0.16
 */
signal gboolean (*editor_notify)(GObject *obj, GeanyEditor *editor, SCNotification *nt,
		gpointer user_data);
