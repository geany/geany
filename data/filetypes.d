# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=default
comment=comment
commentline=comment
commentdoc=commentdoc
commentdocnested=commentdoc
number=number
word=keyword
word2=keyword2
word3=keyword2
typedef=type
string=string
stringeol=stringeol
character=string
operator=operator
identifier=default
commentlinedoc=commentdoc,bold
commentdockeyword=commentdoc,bold,italic
commentdockeyworderror=commentdoc

[keywords]
# all items must be in one line
primary=__FILE__ __LINE__ __DATA__ __TIME__ __TIMESTAMP__ abstract alias align asm assert auto body bool break byte case cast catch cdouble cent cfloat char class const continue creal dchar debug default delegate delete deprecated do double else enum export extern false final finally float for foreach function goto idouble if ifloat immutable import in inout int interface invariant ireal is long macro mixin module new nothrow null out override package pragma private protected public pure real ref return scope shared short static struct super switch synchronized template this throw true try typedef typeof ubyte ucent uint ulong union unittest ushort version void volatile wchar while with
secondary=
# documentation keywords for D, currently not working
docComment=Authors Bugs Copyright Date Deprecated Examples History License Macros Params Returns See_Also Standards Throws Version
types=

[settings]
# default extension used when saving files
extension=d

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=//
comment_close=
# this is an alternative way, so multiline comments are used
#comment_open=/*
#comment_close=*/
# this is alternative way, so multiline comments are used
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


