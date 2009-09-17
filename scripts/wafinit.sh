#! /bin/sh
#
# This script creates a "configure" script and a Makefile to imitate autotools
# but Waf is actually used to build

WAF="./waf"

# Makefile
cat > Makefile << EOF

.PHONY: build configure

all: build

build:
	@$WAF build

install:
	@if test -n "\$(DESTDIR)"; then \\
		./waf install --destdir="\$(DESTDIR)"; \\
	else \\
		./waf install; \\
	fi;

uninstall:
	@if test -n "\$(DESTDIR)"; then \\
		$WAF uninstall --destdir="\$(DESTDIR)"; \\
	else \\
		$WAF uninstall; \\
	fi;

clean:
	@$WAF clean

distclean:
	@$WAF distclean
	@-rm -f Makefile

htmldoc:
	@$WAF --htmldoc

apidoc:
	@$WAF --apidoc

configure:
	@$WAF configure

EOF

template="
all: build

build:
	cd .. && $WAF build

"

echo "$template" > src/Makefile
echo "$template" > tagmanager/Makefile
echo "$template" > scintilla/Makefile
echo "$template" > plugins/Makefile


# configure
cat > configure << EOF
#!/bin/sh

$WAF configure \$@

EOF

chmod 755 configure
