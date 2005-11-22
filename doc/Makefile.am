
DOCDIR = $(DESTDIR)/$(datadir)/doc/@PACKAGE@
HTML_FILES = html/*
IMAGE_FILES = images/*
EXTRA_DIST = geany.css make_webable.sh geany.txt geany_gpl.docbook geany.1 geany.docbook $(HTML_FILES) $(IMAGE_FILES)

geany.1:
	docbook-to-man ../debian/geany.sgml >geany.1
#	docbook2man -o geany.1 ../debian/geany.sgml

geany.pdf:
	jw -b pdf geany.docbook

geany.txt:
	jw -b txt geany.docbook

html/*:
#	xmlto -o html/ xhtml geany.docbook
	$(mkinstalldirs) html
	test -f html/geany.css || cp geany.css html/
	xsltproc --stringparam html.stylesheet geany.css -o html/ \
	/usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl geany.docbook

#all: geany.1 html/*

doc: geany.1 geany.txt html/*

install-data-local: geany.1 geany.txt html/*
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
	$(INSTALL_DATA) geany.txt $(DOCDIR)

