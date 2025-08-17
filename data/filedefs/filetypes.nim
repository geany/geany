# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment
commentdoc=comment_doc
commentline=comment_line
commentlinedoc=comment_doc
number=number_1
string=string_1
character=character
word=keyword_1
word2=keyword_2
triple=string_2
tripledouble=string_2
backticks=identifier_1
funcname=identifier_1
stringeol=string_eol
numerror=error
operator=operator
identifier=identifier_1

[keywords]
# all items must be in one line
# entries taken from https://nim-lang.org/docs/manual.html
keywords=addr and as asm bind block break case cast concept const continue converter defer discard distinct div do elif else end enum except export finally for from func if import in include interface is isnot iterator let macro method mixin mod nil not notin object of or out proc ptr raise ref return shl shr static template try tuple type using var when while xor yield

# built-in identifiers, will be highlighted with style "word2"
builtin_identifiers=array assert auto bool byte char doAssert echo false float float32 float64 int int16 int32 int64 int8 lent Natural openArray Ordinal parallel Positive result seq sink spawn string true uint uint16 uint32 uint64 uint8

[lexer_properties]

[settings]
# default extension used when saving files
extension=nim

# MIME type
mime_type=text/x-nim

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
comment_open=#[
comment_close=]#

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
# 		#command_example();
# setting to false would generate this
# #		command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build-menu]
FT_00_LB=nim c
FT_00_CM=nim c "%f"
FT_00_WD=%d
FT_02_LB=_Lint
FT_02_CM=nimpretty --maxLineLen:80 "%f"
EX_01_WD=
error_regex=(.+)\(([0-9]+),\s+([0-9]+)\)\s+Error
FT_01_LB=nim r
FT_01_CM=nim r "%f"
FT_01_WD=%d
