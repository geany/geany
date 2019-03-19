#!/bin/sh
set -e

# THIS IS OBSOLETE AS WE DO NOT MENTION THE CURRENT YEAR ANY LONGER IN COPYRIGHT NOTICES

# prevent sed from doing stupid things in case the locale encoding doesn't
# match the files'.  Unlikely, but doesn't hurt.
export LANG=C

year=$(grep -Po '(?<="Copyright \(c\) 2005-)20[0-9][0-9](?=\\n)' src/about.c)
echo "new years are: $year"

for f in po/*.po; do
  echo "processing $f..."
  sed -f /dev/stdin -i "$f" <<EOF
/^"Copyright (c) 2005-20[0-9][0-9]\\\\n"\$/{
  s/\\(2005-\\)20[0-9][0-9]/\\1$year/
  n
  :loop
    /^msgstr/{
      n
      # in case the range uses something else than the ASCII dash
      s/\\(2005.*\\)20[0-9][0-9]/\\1$year/
      b done
    }
    n
    b loop
  :done
}
EOF
done
