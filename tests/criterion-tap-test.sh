#!/bin/sh

# See https://github.com/Snaipe/Criterion/blob/bleeding/dev/autotools/README.md
$1 -Otap:- --always-succeed 2>&1 >/dev/null
