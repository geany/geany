# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
word=0x00007f;0xffffff;true;false
identifier=0x000000;0xffffff;false;false
number=0x007f00;0xffffff;false;false
delimiter=0x301010;0xffffff;false;false
character=0xff901e;0xffffff;false;false
charactereol=0x000000;0xe0c0e0;false;false
string=0xff901e;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
label=0xaaaaaa;0xffffff;false;true
commentline=0xd00000;0xffffff;false;false
illegal=0xff0000;0xffffff;false;true

[keywords]
# all items must be in one line
primary=abort abs abstract accept access aliased all and array at begin body case constant declare delay delta digits do else elsif end entry exception exit for function generic goto if in interface is limited loop mod new not null of or others out overriding package pragma private procedure protected raise range record rem renames requeue return reverse select separate subtype synchronized tagged task terminate then type until use when while with xor


[settings]
# default extension used when saving files
extension=adb

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=--
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


[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=gcc -Wall -c "%f"
linker=gnatmake "%e"
run_cmd="./%e"


