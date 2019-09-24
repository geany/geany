#!/bin/bash
#
# Author:  Enrico Tröger
# License: GPL v2 or later
#
# Updates the `functions` and `variables` entries in data/filetypes.nsis.

set -e

TOKENS_CPP_FILE="/tmp/nsis_tokens.cpp"
TOKENS_CPP_URL="https://raw.githubusercontent.com/kichik/nsis/master/Source/tokens.cpp"
BUILD_CPP_FILE="/tmp/nsis_build.cpp"
BUILD_CPP_URL="https://raw.githubusercontent.com/kichik/nsis/master/Source/build.cpp"
DATA_FILE=data/filedefs/filetypes.nsis

[ -f "${DATA_FILE}" ]

# download tokens.cpp and build.cpp from NSIS
wget --quiet --output-document="${TOKENS_CPP_FILE}" "${TOKENS_CPP_URL}"
wget --quiet --output-document="${BUILD_CPP_FILE}" "${BUILD_CPP_URL}"

normalize() {
   # sort, remove line breaks, convert to lower case and remove leading and trailing whitespace
   sort | tr '[:upper:][:space:]' '[:lower:] ' | sed 's/^[[:blank:]]*//;s/[[:blank:]]*$//'
}

# extract function names (then sort the result, conver to lowercase and replace new lines by spaces)
functions=$(
    sed --silent --regexp-extended 's/^\{TOK_.*,_T\("(.*)"\),[0-9]+,.*$/\1/p' "${TOKENS_CPP_FILE}" | \
    normalize
)

# extract variable names (then sort the result, conver to lowercase and replace new lines by spaces)
variables=$(
    sed --silent --regexp-extended \
        --expression 's/^[ ]*m_ShellConstants.add\(_T\("(.*)"\),.*,.*\);.*$/\1/p' \
        --expression 's/^[ ]*m_UserVarNames.add\(_T\("(.*)"\),.*\);.*$/\1/p' "${BUILD_CPP_FILE}" | \
    normalize
)

# hardcode a few more, as found in the documentation ("4.2.2 Other Writable Variables")
variables_extra='{nsisdir} 0 1 2 3 4 5 6 7 8 9 r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 \\n \\r \\t $'
variables="${variables_extra} ${variables}"
# prefix each element with a dollar sign
variables="$(echo "$variables" | sed 's/[^ ]*/$&/g')"

rm "${TOKENS_CPP_FILE}" "${BUILD_CPP_FILE}"

sed --expression "s/^functions=.*$/functions=$functions/" \
    --expression "s/^variables=.*$/variables=$variables/" \
    --in-place "${DATA_FILE}"
