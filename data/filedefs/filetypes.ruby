# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
commentline=comment_line
number=number_1
string=string_1
character=character
word=keyword_1
global=type
symbol=preprocessor
classname=class
defname=function
operator=operator
identifier=identifier_1
modulename=type
backticks=backticks
instancevar=default
classvar=default
datasection=default
heredelim=operator
worddemoted=keyword_1
stdin=default
stdout=default
stderr=default
regex=regex
here_q=here_doc
here_qq=here_doc
here_qx=here_doc
string_q=string_2
string_qq=string_2
string_qx=string_2
string_qr=string_2
string_qw=string_2
upper_bound=default
error=error
pod=comment_doc

[keywords]
# all items must be in one line
primary=__FILE__ load define_method attr_accessor attr_writer attr_reader and def end in or self unless __LINE__ begin defined? ensure module redo super until BEGIN break do false next rescue then when END case else for nil include require require_relative retry true while alias class elsif if not return undef yield


[settings]
# default extension used when saving files
extension=rb

# MIME type
mime_type=application/x-ruby

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
#comment_open==begin
#comment_close==end

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

[build-menu]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
FT_00_LB=_Compile
FT_00_CM=ruby -wc "%f"
FT_00_WD=
EX_00_LB=_Execute
EX_00_CM=ruby "%f"
EX_00_WD=
