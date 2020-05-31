#!/bin/bash

set -e

# must be run from within scintilla subdirectory
THIS_DIR=$(basename $(realpath .))

if [ "$THIS_DIR" != "scintilla" ]; then
    echo "WARNING: Should run from scintilla dir"
fi

if [ -z "$1" ]; then
    echo "ERROR: Version, like 4.4.5, must be passed as parameter, or path to tarball"
    exit 1
fi

if [ -f "$1" ]; then
    FILE="$1"
else
    VER=$(echo $1 | tr -d '.')
    URL="https://www.scintilla.org/scintilla${VER}.tgz"
    wget "$URL"
    TEMP_FILE="scintilla${VER}.tgz"
    FILE="$TEMP_FILE"
fi

md5sum_=$(md5sum "$FILE")
shasum_=$(sha256sum "$FILE")

# git rm would delete the scintilla directory if we don't add a temporary
# untracked file
touch __DONT_DELETE_DIR__
git ls-files -z | xargs -0 -r git rm -qf
tar --strip-components=1 -xz -f "$FILE"
# use file date for commit. Conviniently, the tarball downloaded
# from scintilla.org has the release date as file timestamp.
CI_DATE=$(date --reference "$FILE")
rm -f "$TEMP_FILE" __DONT_DELETE_DIR__

SCIVER=$(cat version.txt)
LOG_MSG="Import upstream scintilla from tarball, Version ${SCIVER}\n"
LOG_MSG="${LOG_MSG}\n"
LOG_MSG="${LOG_MSG}md5: $md5sum_\n"
LOG_MSG="${LOG_MSG}sha256: $shasum_\n"

git add .
git commit -m "$(echo -e $LOG_MSG)" --date="$CI_DATE"
