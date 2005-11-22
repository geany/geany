## Process this file with automake to produce Makefile.in

SUBDIRS = tagmanager scintilla src po doc

EXTRA_DIST = \
	autogen.sh \
	geany.desktop \
	geany.spec \
	geany.glade \
	geany.gladep \
	global.tags \
	data/

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
	$(INSTALL_DATA) global.tags $(DESTDIR)$(pkgdatadir); \
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
