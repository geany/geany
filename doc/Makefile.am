man_MANS=geany.1
DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@
IMAGE_FILES = images/*.png
EXTRA_DIST = geany.html geany.css geany.txt geany.1 \
			 $(srcdir)/$(IMAGE_FILES)

pdf: geany.txt
	rst2latex -stg --documentoptions="10pt,a4paper" --output-encoding=utf8 $(srcdir)/geany.txt >geany.tex
	pdflatex geany.tex
	rm -f geany.tex geany.aux geany.log geany.out
	mv $(srcdir)/geany.pdf geany-$(VERSION).pdf

doc: geany.txt
	rst2html -stg --stylesheet=geany.css $(srcdir)/geany.txt >geany.html

doc-clean:
	rm -f geany.html
	rm -f geany.1
	rm -f *.pdf

uninstall-local:
	rm -rf $(DOCDIR);

install-data-local:
	$(mkinstalldirs) $(DOCDIR)/html/images
	for file in `ls $(srcdir)/$(IMAGE_FILES)`; do \
	  if [ -f $$file ]; then \
	    basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	    $(INSTALL_DATA) $$file $(DOCDIR)/html/images/$$basefile; \
	  fi \
	done
	$(INSTALL_DATA) $(srcdir)/geany.html $(DOCDIR)/html/index.html
	$(INSTALL_DATA) $(srcdir)/geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) $(top_srcdir)/README $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/ChangeLog $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/NEWS $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/COPYING $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/AUTHORS $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/TODO $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt

