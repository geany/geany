# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment
commentline=comment_line
commentdoc=comment_doc
commentnested=comment
number=number_1
word=keyword_1
word2=keyword_2
word3=keyword_3
typedef=type
string=string_1
stringeol=string_eol
character=character
operator=operator
identifier=identifier_1
commentlinedoc=comment_line_doc
commentdockeyword=comment_doc_keyword
commentdockeyworderror=comment_doc_keyword_error

[keywords]
# all items must be in one line
primary=__FILE__ __MODULE__ __LINE__ __FUNCTION__ __PRETTY_FUNCTION__ __gshared __traits __vector __parameters __DATE__ __EOF__ __TIME__ __TIMESTAMP__ __VENDOR__ __VERSION__ abstract alias align asm assert auto body bool break byte case cast catch cdouble cent cfloat char class const continue creal dchar debug default delegate delete deprecated do double else enum export extern false final finally float for foreach foreach_reverse function goto idouble if ifloat immutable import in inout int interface invariant ireal is lazy long macro mixin module new nothrow null out override package pragma private protected public pure real ref return scope shared short static struct super switch synchronized template this throw true try typedef typeid typeof ubyte ucent uint ulong union unittest ushort version void volatile wchar while with
secondary=
# documentation keywords for D, currently not working
docComment=Authors Bugs Copyright Date Deprecated Examples History License Macros Params Returns See_Also Standards Throws Version
types=

[lexer_properties]
fold.d.comment.explicit=0

[settings]
# default extension used when saving files
extension=d

# MIME type
mime_type=text/x-dsrc

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=//
# multiline comments
comment_open=/*
comment_close=*/
# or alternatively
#comment_open=/+
#comment_close=+/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=dmd -w -c "%f"
linker=dmd -w -of"%e" "%f"
# you can also use the gdc compiler, please use the "gdmd" wrapper script(included with gdc)
#compiler=gdmd -w -c "%f"
#linker=gdmd -w -of"%e" "%f"

run_cmd="./%e"


