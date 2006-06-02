
DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@
HTML_FILES = html/*.html geany.css
IMAGE_FILES = images/*.jpg
EXTRA_DIST = geany.css make_webable.sh geany.txt geany_gpl.docbook geany.1 geany.docbook $(HTML_FILES) $(IMAGE_FILES)

geany.1:
	docbook-to-man ../debian/geany.sgml >geany.1
#	docbook2man -o geany.1 ../debian/geany.sgml

pdf:
	jw -b pdf geany.docbook
	mv geany.pdf geany-$(VERSION).pdf

geany.txt:
	jw -b txt geany.docbook

html/index.html:
#	xmlto -o html/ xhtml geany.docbook
	$(mkinstalldirs) html
	xsltproc --stringparam html.stylesheet geany.css -o html/ \
	/usr/share/sgml/docbook/xsl-stylesheets/xhtml/chunk.xsl geany.docbook
#	/usr/share/sgml/docbook/xsl-stylesheets-1.65.1-2/xhtml/chunk.xsl
#	/usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl geany.docbook
	test -f html/geany.css || cp geany.css html/

doc: geany.1 geany.txt html/index.html

doc-clean:
	rm -rf html/*.html
	rm -f html/geany.css
	rm -f geany.1
	rm -f geany.txt
	rm -f *.pdf

uninstall-local:
	rm -rf $(DOCDIR);

install-data-local: geany.1 geany.txt html/index.html
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

