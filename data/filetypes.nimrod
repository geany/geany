# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
commentline=comment_line
number=number_1
string=string_1
character=character
word=keyword_1
triple=string_2
tripledouble=string_2
classname=type
defname=function
operator=operator
identifier=identifier_1
commentblock=comment
stringeol=string_eol
word2=keyword_2
decorator=decorator

[keywords]
# all items must be in one line
primary=addr and as asm atomic bind block break case cast const continue converter discard distinct div do elif else end enum except export finally for from generic if import in include interface is isnot iterator lambda let macro method mixin mod nil not notin object of or out proc ptr raise ref return shared shl shr static template try tuple type var when while with without xor yield
# additional keywords, will be highlighted with style "word2"
# these are the builtins for Python 2.7 created with ' '.join(dir(__builtins__))
identifiers=

[lexer_properties]
fold.comment.python=1
fold.quotes.python=1

[settings]
# default extension used when saving files
extension=nim

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comment char, like # in this file
comment_single=#
# multiline comments
#comment_open="""
#comment_close="""

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
width=2
# 0 is spaces, 1 is tabs, 2 is tab & spaces
type=0

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=nimrod c "%f"
run_cmd="%f"
