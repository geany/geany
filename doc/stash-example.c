GeanyPrefGroup *group;
gboolean china_enabled;
gchar *potter_name = NULL; /* strings must be initialised */
GKeyFile *keyfile;
const gchar filename[] = "/path/to/data.conf";
gchar *data;

/* setup the group */
group = stash_group_new("cups");
stash_group_add_boolean(group, &china_enabled, "china", TRUE);
stash_group_add_string(group, &potter_name, "potter_name", "Miss Clay");

/* load the settings from a file */
keyfile = g_key_file_new();
g_key_file_load_from_file(keyfile, filename, G_KEY_FILE_NONE, NULL);
stash_group_load_from_key_file(group, keyfile);
g_key_file_free(keyfile);

/* now use settings china_enabled and potter_name */
...

/* save settings to file */
keyfile = g_key_file_new();
stash_group_save_to_key_file(group, keyfile);
data = g_key_file_to_data(keyfile, NULL, NULL);
if (utils_write_file(filename, data) != 0)
	dialogs_show_msgbox(GTK_MESSAGE_ERROR,
		_("Could not save keyfile %s!"), filename);
g_free(data);
g_key_file_free(keyfile);

/* free memory */
stash_group_free(group);
