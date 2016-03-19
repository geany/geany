# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
commentline=comment_line
number=number_1
word=keyword_1
string=string_1
character=character
operator=operator
identifier=identifier
backticks=backticks
param=parameter
scalar=identifier_1
error=error
here_delim=here_doc
here_q=here_doc

[keywords]
primary=break case continue do done elif else esac eval exit export fi for function goto if in integer return set shift then until while


[settings]
# default extension used when saving files
extension=sh

# MIME type
mime_type=application/x-shellscript

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start a column 0 of a line, true uses any
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
FT_02_LB=_Lint
FT_02_CM=shellcheck --format=gcc "%f"
FT_02_WD=
EX_00_LB=_Execute
EX_00_CM="./%f"
EX_00_WD=
