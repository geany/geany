#! /bin/sh
#
# This script creates a "configure" script and a Makefile to imitate autotools
# but Waf is actually used to build

WAF="waf"
(waf --version) < /dev/null > /dev/null 2>&1 || {
    WAF="./waf"
}

# Makefile
cat > Makefile << EOF

.PHONY: build configure

all: build

build:
	@$WAF build \$@

install:
	@$WAF install \$@

uninstall:
	@$WAF uninstall

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
	@$WAF configure \$@

EOF

# src/Makefile
cat > src/Makefile << EOF

all: build

build:
	cd .. && $WAF build --targets=geany \$@

EOF

# tagmanager/Makefile
cat > tagmanager/Makefile << EOF

all: build

build:
	cd .. && $WAF build --targets=tagmanager \$@

EOF

# scintilla/Makefile
cat > scintilla/Makefile << EOF

all: build

build:
	cd .. && $WAF build --targets=scintilla \$@

EOF

# configure
cat > configure << EOF
#!/bin/sh

$WAF configure \$@

EOF

chmod 755 configure
