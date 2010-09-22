gboolean want_handle;
StashGroup *group = ...;

/* Add the stash setting first so we can load it from disk if we want.
 * Effectively, stash_group_add_boolean() is called for you.
 * We need to use either a widget pointer or a widget name, and as we
 * haven't created the widget yet we'll use a name - check_handle. */
stash_group_add_toggle_button(group, &want_handle, "handle", TRUE, "check_handle");

/* here we could load the setting from disk */

...
/* Later we create a dialog holding the toggle button widget.
 * (Note: a check button is a subclass of a toggle button). */
GtkWidget *dialog = ...;
GtkWidget *check_button = gtk_check_button_new_with_label(_("Handle"));

/* pack the widget into the dialog */
gtk_container_add(GTK_CONTAINER(dialog->vbox), check_button);

/* Now we set a name to lookup the widget from the dialog.
 * We must remember to pass 'dialog' as an argument to Stash later. */
ui_hookup_widget(dialog, check_button, "check_handle");

...
/* At some point we want to display the dialog.
 * First we apply the want_handle boolean variable to the widget */
stash_group_display(group, dialog);

/* now display the dialog */
gtk_widget_show_all(dialog);

/* let the user manipulate widgets */
...
/* Now synchronize the want_handle variable */
stash_group_update(group, dialog);
