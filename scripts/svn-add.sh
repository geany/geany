#!/bin/sh
FILE=$1
if [ -n "$FILE" ]; then
	svn add $FILE
	svn propset svn:keywords 'Author Date Id Revision' $FILE
	svn propset svn:eol-style native $FILE
fi
echo '>>> Remember to update po/POTFILES.in (if necessary) <<<'
