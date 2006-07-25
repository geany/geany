## Process this file with automake to produce Makefile.in

SUBDIRS = tagmanager scintilla src po doc

EXTRA_DIST = \
	autogen.sh \
	geany.desktop.in \
	geany.spec \
	geany.glade \
	geany_windres.rc \
	geany_windres.h \
	geany.gladep \
	data/global.tags \
	data/php.tags \
	data/latex.tags \
	data/html_entities.tags \
	data/filetypes.*

uninstall-local:
	rm -rf $(DESTDIR)$(pkgdatadir);
	rm $(DESTDIR)$(datadir)/pixmaps/geany.png;

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(datadir)/pixmaps; \
	    fi \
	  done \
	fi
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) data/global.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) data/php.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) data/latex.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) data/html_entities.tags $(DESTDIR)$(pkgdatadir); \
	$(INSTALL_DATA) COPYING $(DESTDIR)$(pkgdatadir)/GPL-2; \
	for file in $(srcdir)/data/*; do \
	  if test -f $$file; then \
	    $(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir)/; \
	  fi \
	done

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

if MINGW
install-win32:
	GEANY_INSTALL_DIR=geany; \
	mkdir $(GEANY_INSTALL_DIR); \
	cp ChangeLog $(GEANY_INSTALL_DIR)/ChangeLog.txt; \
	cp TODO $(GEANY_INSTALL_DIR)/ToDo.txt; \
	cp COPYING $(GEANY_INSTALL_DIR)/ChangeLog.txt; \
	cp AUTHORS $(GEANY_INSTALL_DIR)/Authors.txt; \
	cp README $(GEANY_INSTALL_DIR)/ReadMe.txt; \
	cp THANKS $(GEANY_INSTALL_DIR)/Thanks.txt; \
	@# to be finished
endif

BZIP2_ENV =--best

dist-bzip2: distdir
	BZIP2=$(BZIP2_ENV) $(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

deb:
	dpkg-buildpackage -rfakeroot


rpm: dist
	rpmbuild -ta $(distdir).tar.gz


Applicationsdir = $(datadir)/applications
Applications_DATA = geany.desktop
logodir = $(datadir)/pixmaps
