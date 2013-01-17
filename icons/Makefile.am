SUBDIRS =  16x16 24x24 32x32 48x48 scalable tango

# only install index.theme on Windows
if MINGW
iconsdir = $(datadir)/icons/hicolor
dist_icons_DATA = index.theme
endif

dist_noinst_DATA = geany.ico

gtk_update_icon_cache = gtk-update-icon-cache -f -t

install-data-hook:
	@-if test -z "$(DESTDIR)"; then					\
		echo "Updating Gtk icon cache.";			\
		$(gtk_update_icon_cache) "$(datadir)/icons/hicolor";	\
		$(gtk_update_icon_cache) "$(datadir)/icons/Tango"; \
	else								\
		echo "*** Icon cache not updated. Remember to run:";	\
		echo "***";						\
		echo "***   $(gtk_update_icon_cache) '$(datadir)/icons/hicolor'";\
		echo "***   $(gtk_update_icon_cache) '$(datadir)/icons/Tango'";\
		echo "***";						\
	fi

uninstall-local:
	rm -f $(DESTDIR)$(datadir)/icons/hicolor/icon-theme.cache
	rm -f $(DESTDIR)$(datadir)/icons/Tango/icon-theme.cache

