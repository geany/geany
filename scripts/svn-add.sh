#!/bin/sh
FILES=$*
if [ -n "$FILES" ]; then
	svn add $FILES
	svn propset svn:keywords 'Author Date Id Revision' $FILES
	svn propset svn:eol-style native $FILES
fi
echo '>>> Remember to update po/POTFILES.in (if necessary) <<<'
