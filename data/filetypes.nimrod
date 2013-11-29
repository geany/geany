# For complete documentation of this file, please see Geany's main documentation
[styling=Python]

[keywords]
# all items must be in one line
primary=addr and as asm atomic bind block break case cast const continue converter discard distinct div do elif else end enum except export finally for from generic if import in include interface is isnot iterator lambda let macro method mixin mod nil not notin object of or out proc ptr raise ref return shared shl shr static template try tuple type var when while with without xor yield

[lexer_properties]
fold.comment.nimrod=1
fold.quotes.nimrod=1

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
