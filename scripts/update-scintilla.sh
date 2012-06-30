#!/bin/sh
#
# Copyright:	2012, Colomban Wendling
# License:		GNU GPL v2 or later

# Update Geany's bundled Scintilla from a given Scintilla source directory
# (e.g. an upstream Mercurial clone or tarball)


SCI_SRC=

# parse arguments
if [ $# -eq 1 ]; then
	SCI_SRC="$1"
else
	echo "USAGE: $0 SCINTILLA_SOURCE_DIRECTORY" >&2
	exit 1
fi

# check source directory
if ! [ -f "$SCI_SRC"/version.txt ]; then
	echo "'$SCI_SRC' is not a valid Scintilla source directory!" >&2
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

# copy everything from scintilla but lexers
cp -v "$SCI_SRC"/src/*.cxx        scintilla/src     || exit 1
cp -v "$SCI_SRC"/src/*.h          scintilla/src     || exit 1
cp -v "$SCI_SRC"/include/*.h      scintilla/include || exit 1
cp -v "$SCI_SRC"/include/*.iface  scintilla/include || exit 1
cp -v "$SCI_SRC"/gtk/*.c          scintilla/gtk     || exit 1
cp -v "$SCI_SRC"/gtk/*.cxx        scintilla/gtk     || exit 1
cp -v "$SCI_SRC"/gtk/*.h          scintilla/gtk     || exit 1
cp -v "$SCI_SRC"/gtk/*.list       scintilla/gtk     || exit 1
cp -v "$SCI_SRC"/lexlib/*.cxx     scintilla/lexlib  || exit 1
cp -v "$SCI_SRC"/lexlib/*.h       scintilla/lexlib  || exit 1
cp -v "$SCI_SRC"/License.txt      scintilla         || exit 1
cp -v "$SCI_SRC"/version.txt      scintilla         || exit 1
# now copy the lexers we use
git ls-files scintilla/lexers/*.cxx | sed 's%^scintilla/%./%' | while read f; do
  cp -v "$SCI_SRC/$f" "scintilla/$f" || exit 1
done

# apply our patch
git apply -p0 scintilla/scintilla_changes.patch || {
	echo "scintilla_changes.patch doesn't apply, please update it and retry."
	echo "Changes for the catalogue are:"
	git diff -p -R scintilla/src/Catalogue.cxx | tee
	echo "Make sure to strip the leading a/ and b/!"
	exit 1
}

#check whether there are new files
if git status -unormal -s scintilla | grep '^??'; then
	echo <<EOF

Untracked files above have been introduced by the new Scintilla version and
should be added to version control if appropriate, or removed.
EOF
fi

# summary
cat << EOF

Scintilla update successful!

Please check the diff and upgrade the style mappings in
src/highlightingmappins.h.

Check the diff of scintilla/include/SciLexer.h to see whether and which
mapping to add or update (use git diff scintilla/include/SciLexer.h).
Don't forget to also update the comment and string styles in
src/highlighting.c.

Finally, add or update the appropriate line in NEWS.
EOF
