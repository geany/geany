# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x00002f;0xffffff;false;false
comment=0xd00000;0xffffff;false;false
preprocessor=0x007f7f;0xffffff;false;false
identifier=0x007f00;0xffffff;false;false
operator=0x301010;0xffffff;false;false
target=0x0000ff;0xffffff;false;false
ideol=0x008000;0xffffff;false;false

# there are no keywords available otherwise mail me


[settings]
# default extension used when saving files
extension=mak

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=
