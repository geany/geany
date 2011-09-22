/*
 * Compatibility wrapper for old Glade2 code generation file.
 *
 * DO NOT ADD NEW CODE HERE!
 */

GtkWidget* create_window1 (void);
GtkWidget* create_toolbar_popup_menu1 (void);
GtkWidget* create_edit_menu1 (void);
GtkWidget* create_prefs_dialog (void);
GtkWidget* create_project_dialog (void);

GObject *interface_get_object(const gchar *name);
void interface_set_object(GObject *obj, const gchar *name);
