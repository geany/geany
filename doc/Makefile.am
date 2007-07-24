man_MANS=geany.1
DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@
HTML_FILES = html/*.html
IMAGE_FILES = images/*.png
EXTRA_DIST = geany.css geany.xsl make_webable.sh geany.txt geany_gpl.docbook geany.1 \
			 scikeybinding.docbook geany.docbook $(srcdir)/$(HTML_FILES) $(srcdir)/$(IMAGE_FILES)

pdf:
	SP_ENCODING=XML SP_CHARSET_FIXED=YES jw -b pdf $(srcdir)/geany.docbook
	mv doc/geany.pdf geany-$(VERSION).pdf

geany.txt:
	echo $(srcdir)
	jw -b txt $(srcdir)/geany.docbook

htmldoc: geany.docbook
	if [ geany.docbook -nt "./html/index.html" ]; then \
		$(mkinstalldirs) html; \
		xmlto -m geany.xsl -o html/ xhtml $(srcdir)/geany.docbook; \
		test -f html/geany.css || cp $(srcdir)/geany.css html/; \
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

install-data-local:
	$(mkinstalldirs) $(DOCDIR)/html/images
	for file in `ls $(srcdir)/$(HTML_FILES)`; do \
	  if [ -f $$file ]; then \
	    basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	    $(INSTALL_DATA) $$file $(DOCDIR)/html/$$basefile; \
	  fi \
	done
	for file in `ls $(srcdir)/$(IMAGE_FILES)`; do \
	  if [ -f $$file ]; then \
	    basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	    $(INSTALL_DATA) $$file $(DOCDIR)/html/images/$$basefile; \
	  fi \
	done
	$(INSTALL_DATA) $(srcdir)/geany.css $(DOCDIR)/html/geany.css
	$(INSTALL_DATA) $(srcdir)/geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) $(top_srcdir)/README $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/ChangeLog $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/NEWS $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/COPYING $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/AUTHORS $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/TODO $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt

