# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false
commentline=0xff0000;0xffffff;false;false
number=0x400080;0xffffff;false;false
string=0x008000;0xffffff;false;false
character=0x008000;0xffffff;false;false
word=0x111199;0xffffff;true;false
global=0x111199;0xffffff;false;false
symbol=0x008020;0xffffff;false;false
classname=0x7f0000;0xffffff;true;false
defname=0x7f0000;0xffffff;false;false
operator=0x000000;0xffffff;false;false
identifier=0x000000;0xffffff;false;false
modulename=0x111199;0xffffff;true;false
backticks=0x000000;0xe0c0e0;false;false
instancevar=0x000000;0xffffff;false;true
classvar=0x000000;0xffffff;true;true
datasection=0x000000;0xffffff;false;false
heredelim=0x000000;0xffffff;false;false
worddemoted=0x111199;0xffffff;false;false

[keywords]
# all items must be in one line
primary=__FILE__ load define_method attr_accessor attr_writer attr_reader and def end in or self unless __LINE__ begin defined? ensure module redo super until BEGIN break do false next rescue then when END case else for nil include require retry true while alias class elsif if not return undef yield


[settings]
# the following characters are these which a "word" can contains, see documentation
wordchars=_#&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=

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
compiler=ruby -c "%f"
run_cmd=ruby "%f"
