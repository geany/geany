# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0xd00000;0xffffff;false;false
commentline=0xd00000;0xffffff;false;false
commentdoc=0x3f5fbf;0xffffff;false;false
commentdocnested=0x3f5fbf;0xffffff;false;false
number=0x007f00;0xffffff;false;false
word=0x00007f;0xffffff;true;false
word2=0x991111;0xffffff;true;false
word3=0x991111;0xffffff;true;false
typedef=0x0000d0;0xffffff;false;false
string=0xff901e;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
character=0xff901e;0xffffff;false;false
operator=0x301010;0xffffff;false;false
identifier=0x000000;0xffffff;false;false
commentlinedoc=0x3f5fbf;0xffffff;true;false
commentdockeyword=0x3f5fbf;0xffffff;true;true
commentdockeyworderror=0x3f5fbf;0xffffff;false;false

[keywords]
# all items must be in one line
primary=__FILE__ __LINE__ __DATA__ __TIME__ __TIMESTAMP__ abstract alias align asm assert auto body bool break byte case cast catch cdouble cent cfloat char class const continue creal dchar debug default delegate delete deprecated do double else enum export extern false final finally float for foreach function goto idouble if ifloat import in inout int interface invariant ireal is long mixin module new null out override package pragma private protected public real return scope short static struct super switch synchronized template this throw true try typedef typeof ubyte ucent uint ulong union unittest ushort version void volatile wchar while with
secondary=
# documentation keywords for D, currently not working
docComment=Authors Bugs Copyright Date Deprecated Examples History License Macros Params Returns See_Also Standards Throws Version
types=

[settings]
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
# indention of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=dmd -w -c "%f"
# the -of option is automatically added by Geany
linker=dmd -w "%f"
# you can also use the gdc compiler, please use the "gdmd" wrapper script(included with gdc)
#compiler=gdmd -w -c "%f"
#linker=gdmd -w "%f"

run_cmd="./%e"


