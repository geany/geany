man_MANS=geany.1
DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@
HTML_FILES = html/*.html geany.css
IMAGE_FILES = images/*.jpg
EXTRA_DIST = geany.css geany.xsl make_webable.sh geany.txt geany_gpl.docbook geany.1 \
			 geany.docbook $(HTML_FILES) $(IMAGE_FILES)

pdf:
	jw -b pdf geany.docbook
	mv geany.pdf geany-$(VERSION).pdf

geany.txt:
	jw -b txt geany.docbook

htmldoc: geany.docbook
	if [ geany.docbook -nt "./html/index.html" ]; then \
		$(mkinstalldirs) html; \
		# hack for my local system to generate documentation in UTF-8 charset \
		if [ `hostname` = "ukio" ]; then \
			LC_CTYPE=de_DE.utf8@euro xmlto -x geany.xsl -o html/ xhtml geany.docbook; \
		else \
			xmlto -x geany.xsl -o html/ xhtml geany.docbook; \
		fi; \
		test -f html/geany.css || cp geany.css html/; \
	fi

doc: geany.txt htmldoc

doc-clean:
	rm -rf html/*.html
	rm -f html/geany.css
	rm -f geany.1
	rm -f geany.txt
	rm -f *.pdf

uninstall-local:
	rm -rf $(DOCDIR);

install-data-local: geany.txt htmldoc
	$(mkinstalldirs) $(DOCDIR)/html/images
	for file in `ls $(HTML_FILES)`; do \
	  if [ -f $$file ]; then \
	    basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	    $(INSTALL_DATA) $(srcdir)/$$file $(DOCDIR)/html/$$basefile; \
	  fi \
	done
	for file in `ls $(IMAGE_FILES)`; do \
	  if [ -f $$file ]; then \
	    basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	    $(INSTALL_DATA) $(srcdir)/$$file $(DOCDIR)/html/images/$$basefile; \
	  fi \
	done
	$(INSTALL_DATA) geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) ../README $(DOCDIR)
	$(INSTALL_DATA) ../ChangeLog $(DOCDIR)
	$(INSTALL_DATA) ../NEWS $(DOCDIR)
	$(INSTALL_DATA) ../COPYING $(DOCDIR)
	$(INSTALL_DATA) ../AUTHORS $(DOCDIR)
	$(INSTALL_DATA) ../TODO $(DOCDIR)
	$(INSTALL_DATA) ../scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt

