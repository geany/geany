# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0x0000ff;0xffffff;false;false
commentline=0x0000ff;0xffffff;false;false
commentdoc=0x0000ff;0xffffff;false;false
number=0x007f00;0xffffff;false;false
word=0x991111;0xffffff;true;false
word2=0x00007F;0xffffff;true;false
string=0x1E90FF;0xffffff;false;false
character=0x1E90FF;0xffffff;false;false
uuid=0x804040;0xffffff;false;false
preprocessor=0x7F7F00;0xffffff;false;false
operator=0x101030;0xffffff;false;false
identifier=0x100000;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
verbatim=0x101030;0xffffff;false;false
regex=0x905010;0xffffff;false;false
commentlinedoc=0x0000ff;0xffffff;true;false
commentdockeyword=0x0000ff;0xffffff;true;true
globalclass=0xbb1111;0xffffff;true;false
# whether arguments of preprocessor commands should be styled (only first argument is used)
# 1 to enable, 0 to disable
styling_within_preprocessor=1;0;false;false

[keywords]
# all items must be in one line
primary=volatile default export goto sizeof typename asm enum bool union NULL FALSE TRUE typedef struct char int float double void unsigned signed long short extern static register auto const if else switch for while do break continue return class new delete inline case
docComment=TODO FIXME

[settings]
# the following characters are these which a "word" can contains, see documentation
wordchars=_#&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=/*
comment_close=*/
# this is an alternative way, so multiline comments are used
#comment_open=/*
#comment_close=*/

# set to false if a comment character/string should start a column 0 of a line, true uses any
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
compiler=gcc -Wall -c "%f"
# the -o option is automatically added by Geany
linker=gcc -Wall "%f"
run_cmd="./%e"


