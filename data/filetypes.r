# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
#comment=0x000fff;0xffffff;true;false
#kword=0x66ff33;0xffffff;true;false
#operator=0x660000;0xffffff;false;false
#basekword=0x66ff33;0xffffff;false;false
#otherkword=0x66ff33;0xffffff;false;false
#number=0xcc00ff;0xffffff;false;false
#string=0xcc00ff;0xffffff;false;false
#string2=0xcc00ff;0xffffff;false;false
#identifier=0x6600ff;0xffffff;false;false
#infix=0xcc00ff;0xffffff;false;false
#infixeol=0xcc00ff;0xffffff;false;false

[keywords]
# all items must be in one line
#primary=source if else for cbind rbind break array matrix
#secondary=

[settings]
# default extension used when saving files
extension=R

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=
# this is an alternative way, so multiline comments are used
#comment_open=/*
#comment_close=*/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
   #command_example();
# setting to false would generate this
#  command_example();
# This setting works only for single line comments
comment_use_indent=false

# context action command (please see Geany's main documentation for details)
context_action_cmd=

