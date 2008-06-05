#! /bin/sh
#
# This script creates a "configure" script and a Makefile to imitate autotools
# but Waf is actually used to build

WAF=`which waf`
if [ "x$WAF" = "x" ]
then
    WAF="./waf"
fi


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

cat > configure << EOF
#!/bin/sh

$WAF configure \$@

EOF

chmod 755 configure
