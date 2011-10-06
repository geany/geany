man_MANS=geany.1
DOCDIR = $(DESTDIR)$(docdir)
IMAGE_FILES = images/*.png
EXTRA_DIST = geany.html geany.css geany.txt geany.1 \
	plugins.dox pluginsymbols.c pluginsignals.c \
	stash-example.c stash-gui-example.c \
	makefile.win32 \
	$(srcdir)/$(IMAGE_FILES)

pdf: geany.txt
	rst2latex -stg --documentoptions="10pt,a4paper" --output-encoding=utf8 $(srcdir)/geany.txt geany.tex
	pdflatex geany.tex
	rm -f geany.tex geany.aux geany.log geany.out
	mv $(srcdir)/geany.pdf geany-$(VERSION).pdf

api-doc: Doxyfile
	doxygen

hacking-doc: ../HACKING
	rst2html -stg --stylesheet=geany.css $^ hacking.html

# when generating documentation, first try rst2html.py as it is the upstream default
doc: geany.txt
	(rst2html.py --version) < /dev/null > /dev/null 2>&1 && \
	rst2html.py -stg --stylesheet=geany.css $(srcdir)/geany.txt geany.html || \
	rst2html -stg --stylesheet=geany.css $(srcdir)/geany.txt geany.html

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
	$(INSTALL_DATA) $(top_srcdir)/THANKS $(DOCDIR)
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt

