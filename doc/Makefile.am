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
EXTRA_DIST = geany.html geany.css geany.txt geany.1 \
	plugins.dox pluginsymbols.c pluginsignals.c \
	stash-example.c stash-gui-example.c \
	makefile.win32

geany-$(VERSION).pdf: geany.txt
	rst2latex -stg --documentoptions="10pt,a4paper" --output-encoding=utf8 $(srcdir)/geany.txt geany.tex
	(absbuilddir="$$(pwd)/$(builddir)"; \
	 cd "$(srcdir)" && \
	 pdflatex -output-directory="$$absbuilddir" "$$absbuilddir/geany.tex")
	rm -f geany.tex geany.aux geany.log geany.out
	mv geany.pdf $@

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

all-local: Doxyfile.stamp

clean-local:
	-rm -rf reference/ Doxyfile.stamp

endif

hacking.html: ../HACKING
	rst2html -stg --stylesheet=$(srcdir)/geany.css $(srcdir)/../HACKING $@

# when generating documentation, first try rst2html.py as it is the upstream default
geany.html: geany.txt
	(rst2html.py --version) < /dev/null > /dev/null 2>&1 && \
	rst2html.py -stg --stylesheet=$(srcdir)/geany.css $(srcdir)/geany.txt $@ || \
	rst2html -stg --stylesheet=$(srcdir)/geany.css $(srcdir)/geany.txt $@

hacking-doc: hacking.html
doc: geany.html
pdf: geany-$(VERSION).pdf

doc-clean:
	rm -f geany.html
	rm -f geany.1
	rm -f *.pdf
	rm -f hacking.html

uninstall-local:
	rm -rf $(DOCDIR);

# manually install some files under another name
install-data-local:
	$(mkinstalldirs) $(DOCDIR)/html
	$(INSTALL_DATA) $(srcdir)/geany.html $(DOCDIR)/html/index.html
	$(INSTALL_DATA) $(srcdir)/geany.txt $(DOCDIR)/manual.txt
	$(INSTALL_DATA) $(top_srcdir)/scintilla/License.txt $(DOCDIR)/ScintillaLicense.txt

