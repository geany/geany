#!/bin/sh
FILES=$*
if [ -n "$FILES" ]; then
	svn add $FILES
	svn propset svn:keywords 'Author Date Id Revision' $FILES
	svn propset svn:eol-style native $FILES
fi
echo '>>> Remember to update Makefile.am, makefile.win32, wscript, po/POTFILES.in, geany.nsi (if necessary) <<<'
