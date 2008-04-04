## Process this file with automake to produce Makefile.in

SUBDIRS = tagmanager scintilla src plugins icons po doc

WIN32_BUILD_FILES = \
	geany_private.rc \
    win32-config.h \
	makefile.win32 \
    scintilla/makefile.win32 \
    tagmanager/makefile.win32 \
    src/makefile.win32

SYS_DATA_FILES = \
	data/global.tags \
	data/php.tags \
	data/latex.tags \
	data/pascal.tags \
	data/html_entities.tags \
	$(srcdir)/data/filetypes.* \
	data/filetype_extensions.conf \
	data/snippets.conf

EXTRA_DIST = \
	autogen.sh \
	geany.desktop.in \
	geany.pc.in \
	geany.spec \
	geany.glade \
	geany.gladep \
	HACKING \
	README.I18N \
	README.Packagers \
	scripts/intl_stats.sh \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(SYS_DATA_FILES) \
	$(WIN32_BUILD_FILES)

DISTCLEANFILES = \
	geany.desktop \
	intltool-extract \
	intltool-merge \
	intltool-update

uninstall-local:
	rm -rf $(DESTDIR)$(pkgdatadir);
	rm -f $(DESTDIR)$(datadir)/pixmaps/geany.png;
	rm -f $(DESTDIR)$(datadir)/pixmaps/geany.ico;

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps; \
	  $(INSTALL_DATA) $(srcdir)/pixmaps/geany.png $(DESTDIR)$(datadir)/pixmaps; \
	fi
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/global.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/php.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/pascal.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/latex.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/html_entities.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/COPYING $(DESTDIR)$(pkgdatadir)/GPL-2; \
	for file in $(srcdir)/data/*; do \
	  if test -f $$file; then \
	    $(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir)/; \
	  fi \
	done

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

BZIP2_ENV =--best

dist-bzip2: distdir
	BZIP2=$(BZIP2_ENV) $(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

deb:
	dpkg-buildpackage -rfakeroot


rpm: dist
	rpmbuild -ta $(distdir).tar.gz


logodir = $(datadir)/pixmaps

pkgconfig_DATA = geany.pc
pkgconfigdir = $(libdir)/pkgconfig

desktopdir = $(datadir)/applications
desktop_in_files = geany.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
