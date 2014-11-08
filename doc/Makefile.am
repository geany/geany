man_MANS=geany.1

if INSTALL_HTML_DOCS
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
endif

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

all-local: geany.html hacking.html

# clean on 'maintainer-clean' rather than 'clean' in case it was not
# built by Make but rather part of the distribution.  This is fine even
# then, as configure will properly require what is needed to build it
# again if it is missing.
maintainer-clean-local: maintainer-clean-html-local
maintainer-clean-html-local:
	-rm -f geany.html

clean-local: clean-html-local
clean-html-local:
	-rm -f hacking.html

endif

# PDF user manual
if WITH_RST2PDF

geany-$(VERSION).pdf: geany.txt
	$(AM_V_GEN)$(RST2PDF) $(srcdir)/geany.txt -o $@

all-local: geany-$(VERSION).pdf

clean-local: clean-pdf-local
clean-pdf-local:
	-rm -f geany-$(VERSION).pdf

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
	$(top_srcdir)/tagmanager/src/tm_workspace.[ch]

Doxyfile.stamp: Doxyfile $(doxygen_sources)
	$(AM_V_GEN)$(DOXYGEN) Doxyfile && echo "" > $@

all-local: Doxyfile.stamp

clean-local: clean-api-docs-local
clean-api-docs-local:
	-rm -rf reference/ Doxyfile.stamp doxygen_*

endif

uninstall-local:
	rm -f $(DOCDIR)/html/index.html
	rm -f $(DOCDIR)/manual.txt
	rm -f $(DOCDIR)/ScintillaLicense.txt

# manually install some files under another name
install-data-local:
if INSTALL_HTML_DOCS
	$(mkinstalldirs) $(DOCDIR)/html
#	as we don't install with the automated mechanism so we can rename the file,
#	we need to find the source file in the right location (either builddir or srcdir)
	dir=$(builddir); test -f "$$dir/geany.html" || dir=$(srcdir); \
	$(INSTALL_DATA) "$$dir/geany.html" $(DOCDIR)/html/index.html
endif
	$(mkinstalldirs) $(DOCDIR)
	$(INSTALL_DATA) $(srcdir)/geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt
