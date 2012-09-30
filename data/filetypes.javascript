# For complete documentation of this file, please see Geany's main documentation
[styling=C]

[keywords]
# all items must be in one line
primary=break case catch const class continue debugger default delete do else enum export extends finally for if implements import in instanceof interface let new package private protected public return static super switch throw try typeof while with yield false NaN null true undefined void
secondary=function var prototype
classes=Array Boolean Date Function Math Number Object String RegExp EvalError Error RangeError ReferenceError SyntaxError TypeError URIError decodeURI decodeURIComponent encodeURI encodeURIComponent eval isFinite isNaN parseFloat parseInt console document navigator window this constructor

[settings]
# experimenting ActionScript lexer for Javascript, better syntax highlighting
lexer_filetype=ActionScript

# default extension used when saving files
extension=js

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=//
# multiline comments
comment_open=/*
comment_close=*/

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
compiler=
run=


