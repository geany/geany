# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
word=keyword_1
identifier=identifier_1
number=number
delimiter=operator
character=character
charactereol=string_eol
string=string_1
stringeol=string_eol
label=label
commentline=comment_line
illegal=error

[keywords]
# all items must be in one line
primary=abort abs abstract accept access aliased all and array at begin body case constant declare delay delta digits do else elsif end entry exception exit for function generic goto if in interface is limited loop mod new not null of or others out overriding package pragma private procedure protected raise range record rem renames requeue return reverse select separate subtype synchronized tagged task terminate then type until use when while with xor


[settings]
# default extension used when saving files
extension=adb

# MIME type
mime_type=text/x-adasrc

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=--
# multiline comments
#comment_open=
#comment_close=

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
compiler=gcc -Wall -c "%f"
linker=gnatmake "%e"
run_cmd="./%e"


