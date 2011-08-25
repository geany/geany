# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=default
comment=commentdoc
commentml=comment
identifier=default
control=0x301010;0xffffff;true;false
keyword=0x301060;0xffffff;true;false
defword=0x000080;;false;true
preword1=0x000000;0xe0c0e0;false;false
preword2=0xaaaaaa;0xffffff;false;true
number=0x007f00;0xffffff;false;false
string=string
locale=0xff0000;0xffffff;false;true

[keywords]
# all items must be in one line
primary=ABORT EXIT DO LOOP UNLOOP BEGIN UNTIL WHILE REPEAT EXIT IF ELSE THEN CASE ENDCASE OF ENDOF


[settings]
# default extension used when saving files
extension=fs

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=\\ 
# multiline comments
comment_open=(
comment_close= )

# comment_open=\
# comment_close=

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
