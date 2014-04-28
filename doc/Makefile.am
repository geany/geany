man_MANS=geany.1

htmldocimagesdir = $(docdir)/html/images
dist_htmldocimages_DATA = \
	images/build_menu_commands_dialog.png \
	images/find_dialog.png \
	images/find_in_files_dialog.png \
	images/main_window.png \
	images/pref_dialog_edit_completions.png \
	images/pref_dialog_edit_display.png \
	images/pref_dialog_edit_features.png \
	images/pref_dialog_edit_indentation.png \
	images/pref_dialog_files.png \
	images/pref_dialog_gen_misc.png \
	images/pref_dialog_gen_startup.png \
	images/pref_dialog_interface_interface.png \
	images/pref_dialog_interface_notebook.png \
	images/pref_dialog_interface_toolbar.png \
	images/pref_dialog_keys.png \
	images/pref_dialog_printing.png \
	images/pref_dialog_templ.png \
	images/pref_dialog_tools.png \
	images/pref_dialog_various.png \
	images/pref_dialog_vte.png \
	images/replace_dialog.png

doc_DATA = \
	$(top_srcdir)/AUTHORS \
	$(top_srcdir)/ChangeLog \
	$(top_srcdir)/COPYING \
	$(top_srcdir)/NEWS \
	$(top_srcdir)/README \
	$(top_srcdir)/THANKS \
	$(top_srcdir)/TODO

DOCDIR = $(DESTDIR)$(docdir)

EXTRA_DIST = \
	geany.html \
	geany.css \
	geany.txt \
	geany.1 \
	plugins.dox \
	pluginsymbols.c \
	pluginsignals.c \
	stash-example.c \
	stash-gui-example.c \
	makefile.win32

# HTML user manual and hacking file
if WITH_RST2HTML

geany.html: $(srcdir)/geany.css $(srcdir)/geany.txt
	$(AM_V_GEN)$(RST2HTML) -stg --stylesheet=$(srcdir)/geany.css $(srcdir)/geany.txt $@

hacking.html: $(srcdir)/geany.css $(top_srcdir)/HACKING
	$(AM_V_GEN)$(RST2HTML) -stg --stylesheet=$(srcdir)/geany.css $(top_srcdir)/HACKING $@

all-html-local: geany.html hacking.html

clean-html-local:
	-rm -f hacking.html
# FIXME: why is the generated HTML manual checked-in to VCS?
#	-rm -f geany.html

else
all-html-local:;
clean-html-local:;
endif

# PDF user manual
if WITH_LATEXPDF

# FIXME: - grep hack because pdflatex barfs all kinds of crazy info besides warnings/errors
#        - images in output PDF file are way too big, need something equivalent to:
#            \includegraphics[width=\textwidth,height=\textheight,keepaspectratio]{...}
geany-$(VERSION).pdf: geany-$(VERSION).tex
	$(AM_V_GEN)\
	(absbuilddir="$$(pwd)/$(builddir)"; \
	 cd "$(srcdir)" && \
	 $(PDFLATEX) $(PDFLATEX_FLAGS) -interaction=nonstopmode -halt-on-error -file-line-error \
	 	-output-directory="$$absbuilddir" "$$absbuilddir/geany-$(VERSION).tex" | \
	 { r=$?; grep -iA1 "^!\|.*:[0-9]*:.*\|warning" >&2; exit $r; })

geany-$(VERSION).tex: $(srcdir)/geany.txt
	$(AM_V_GEN)$(RST2LATEX) -stg --documentoptions="10pt,a4paper" --output-encoding=utf8 $(srcdir)/geany.txt $@

all-pdf-local: geany-$(VERSION).pdf

clean-pdf-local:
	-rm -f geany-$(VERSION).tex geany-$(VERSION).aux geany-$(VERSION).log \
		geany-$(VERSION).out geany-$(VERSION).toc geany-$(VERSION).pdf

else
all-pdf-local:;
clean-pdf-local:;
endif

# API Documentation
if WITH_DOXYGEN

doxygen_sources = \
	$(srcdir)/*.[ch] \
	$(srcdir)/*.dox \
	$(top_srcdir)/src/*.[ch] \
	$(top_srcdir)/plugins/geanyplugin.h \
	$(top_srcdir)/plugins/geanyfunctions.h \
	$(top_srcdir)/tagmanager/src/tm_source_file.[ch] \
	$(top_srcdir)/tagmanager/src/tm_work_object.[ch] \
	$(top_srcdir)/tagmanager/src/tm_workspace.[ch]

Doxyfile.stamp: Doxyfile $(doxygen_sources)
	$(AM_V_GEN)$(DOXYGEN) Doxyfile && echo "" > $@

all-api-docs-local: Doxyfile.stamp

clean-api-docs-local:
	-rm -rf reference/ Doxyfile.stamp

else
all-api-docs-local:;
clean-api-docs-local:;
endif

all-local: all-html-local all-pdf-local all-api-docs-local
clean-local: clean-html-local clean-pdf-local clean-api-docs-local

uninstall-local:
	rm -rf $(DOCDIR);

# manually install some files under another name
install-data-local:
	$(mkinstalldirs) $(DOCDIR)/html
	$(INSTALL_DATA) $(srcdir)/geany.html $(DOCDIR)/html/index.html
	$(INSTALL_DATA) $(srcdir)/geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt
