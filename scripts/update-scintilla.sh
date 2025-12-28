#!/bin/sh
#
# Copyright:	2012, The Geany contributors
# License:		GNU GPL v2 or later

# Update Geany's bundled Scintilla from a given Scintilla source directory
# (e.g. an upstream Mercurial clone or tarball)


SCI_SRC=

# parse arguments
if [ $# -eq 2 ]; then
	SCI_SRC="$1"
	LEX_SRC="$2"
else
	echo "USAGE: $0 SCINTILLA_SOURCE_DIRECTORY LEXILLA_SOURCE_DIRECTORY" >&2
	exit 1
fi

# check source directory
if ! [ -f "$SCI_SRC/src/ScintillaBase.cxx" ]; then
	echo "'$SCI_SRC' is not a valid Scintilla source directory!" >&2
	exit 1
fi
if ! [ -f "$LEX_SRC/src/Lexilla.cxx" ]; then
	echo "'$LEX_SRC' is not a valid Lexilla source directory!" >&2
	exit 1
fi
# check destination directory
if ! [ -d .git ] || ! [ -f scintilla/version.txt ]; then
	echo "Please run this script from Geany's root source directory." >&2
	exit 1
fi

# make sure destination directory is clean
if git status -unormal -s scintilla | grep '^??'; then
	echo "Please clean scintilla directory from untracked files before." >&2
	exit 1
fi

copy_to()
{
	dest="$1"
	shift

	if ! [ -e "$dest" ]; then
		echo "$dest does not exist." >&2;
		exit 1
	fi
	for f in $@; do
		dos2unix -k -q -n $f $dest/$(basename $f) || (echo "dos2unix $f failed" >&2 ; exit 1)
	done || exit 1
}

# purge executbale bits
umask 111
# copy everything from scintilla but lexers
copy_to scintilla/README          "$SCI_SRC"/README
copy_to scintilla/src             "$SCI_SRC"/src/*.cxx
copy_to scintilla/src             "$SCI_SRC"/src/*.h
copy_to scintilla/include         "$SCI_SRC"/include/*.h
copy_to scintilla/include         "$SCI_SRC"/include/*.iface
copy_to scintilla/gtk             "$SCI_SRC"/gtk/*.c
copy_to scintilla/gtk             "$SCI_SRC"/gtk/*.cxx
copy_to scintilla/gtk             "$SCI_SRC"/gtk/*.h
copy_to scintilla/gtk             "$SCI_SRC"/gtk/*.list
copy_to scintilla                 "$SCI_SRC"/License.txt
copy_to scintilla                 "$SCI_SRC"/version.txt
copy_to scintilla/lexilla/src     "$LEX_SRC"/src/*.cxx
copy_to scintilla/lexilla/include "$LEX_SRC"/include/*.h
copy_to scintilla/lexilla/include "$LEX_SRC"/include/*.iface
copy_to scintilla/lexilla/lexlib  "$LEX_SRC"/lexlib/*.cxx
copy_to scintilla/lexilla/lexlib  "$LEX_SRC"/lexlib/*.h
copy_to scintilla/lexilla/        "$LEX_SRC"/License.txt
copy_to scintilla/lexilla/        "$LEX_SRC"/version.txt
# now copy the lexers we use
git -C scintilla/lexilla/lexers/ ls-files '*.cxx' | while read f; do
  copy_to "scintilla/lexilla/lexers" "$LEX_SRC/lexers/$f"
done

# apply our patch
git apply -p0 scintilla/scintilla_changes.patch || {
	echo "scintilla_changes.patch doesn't apply, please update it and retry."
	exit 1
}

echo "Upstream lexer catalogue changes:"
git diff -p --src-prefix= --dst-prefix= scintilla/lexilla/src/Lexilla.cxx

# show a nice success banner
echo "Scintilla update successful!" | sed 'h;s/./=/g;p;x;p;x'

#check whether there are new files
if git status -unormal -s scintilla | grep '^??'; then
	cat <<EOF

Untracked files above have been introduced by the new Scintilla version and
should be added to version control if appropriate, or removed.

You can add them to Git with the command:
$ git add $(git status -unormal -s scintilla | grep '^??' | cut -b4- | sed '$!{s/$/ \\/};1!{s/^/          /}')

Don't forget to add new files to the build system (scintilla/Makefile.am
and meson.build).
EOF
fi

# check for possible changes to styles
if ! git diff --quiet scintilla/lexilla/include/SciLexer.h; then
	cat << EOF

Check the diff of scintilla/lexilla/include/SciLexer.h to see whether and which
mapping to add or update in src/highlightingmappings.h
(use git diff scintilla/lexilla/include/SciLexer.h).
Don't forget to also update the comment and string styles in
src/highlighting.c.
EOF
fi

# summary
cat << EOF

Don't forget to add or update the appropriate line in NEWS.
EOF
