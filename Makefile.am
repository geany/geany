## Process this file with automake to produce Makefile.in

SUBDIRS = tagmanager scintilla src plugins icons po doc

AUTOMAKE_OPTIONS =	1.7

WIN32_BUILD_FILES = \
	geany_private.rc \
    win32-config.h \
	makefile.win32 \
    scintilla/makefile.win32 \
    tagmanager/makefile.win32 \
    src/makefile.win32

SYS_DATA_FILES = \
	data/c99.tags \
	data/php.tags \
	data/python.tags \
	data/latex.tags \
	data/pascal.tags \
	data/html_entities.tags \
	$(srcdir)/data/filetypes.* \
	data/filetype_extensions.conf \
	data/snippets.conf \
	data/ui_toolbar.xml

EXTRA_DIST = \
	autogen.sh \
	wscript \
	waf \
	geany.desktop.in \
	geany.pc.in \
	geany.spec \
	geany.glade \
	geany.gladep \
	ChangeLog.pre-0-17 \
	HACKING \
	README.I18N \
	README.Packagers \
	po/intl_stats.sh \
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

install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/c99.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/php.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/python.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/pascal.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/latex.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/data/html_entities.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) $(srcdir)/COPYING $(DESTDIR)$(pkgdatadir)/GPL-2; \
	for file in $(srcdir)/data/*; do \
	  if test -f $$file; then \
	    $(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir)/; \
	  fi \
	done


BZIP2_ENV =--best

dist-bzip2: distdir
	BZIP2=$(BZIP2_ENV) $(AMTAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

sign:
	if test -f $(PACKAGE)-$(VERSION).tar.gz; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.gz; fi
	if test -f $(PACKAGE)-$(VERSION).tar.bz2; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.bz2; fi

deb:
	dpkg-buildpackage -rfakeroot


rpm: dist
	rpmbuild -ta $(distdir).tar.gz


pkgconfig_DATA = geany.pc
pkgconfigdir = $(libdir)/pkgconfig

desktopdir = $(datadir)/applications
desktop_in_files = geany.desktop
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
