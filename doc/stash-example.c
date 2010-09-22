StashGroup *group;
gboolean china_enabled;
gchar *potter_name;
const gchar filename[] = "/path/data.conf";

/* setup the group */
group = stash_group_new("cup");
stash_group_add_boolean(group, &china_enabled, "china", TRUE);
stash_group_add_string(group, &potter_name, "potter_name", "Miss Clay");

/* load the settings from a file */
if (!stash_group_load_from_file(group, filename))
	g_warning(_("Could not load keyfile %s!"), filename);

/* now use settings china_enabled and potter_name */
...

/* save settings to file */
if (stash_group_save_to_file(group, filename, G_KEY_FILE_NONE) != 0)
	g_error(_("Could not save keyfile %s!"), filename);

/* free memory */
stash_group_free(group);
