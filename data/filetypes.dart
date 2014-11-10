# For complete documentation of this file, please see Geany's main documentation
[styling=C]

[keywords]
primary=abstract assert break case catch class const continue default do else enum extends false final finally for function future generic get goto if implements import in Infinity inner instanceof interface let NaN native new null num outer package private protected public rest return set static strictfp super switch synchronized this throw throws transient true try typeof undefined var void volatile while with yeild prototype
secondary=boolean byte char double float int long short Array Boolean Date Function Math Number Object String RegExp EvalError Error RangeError ReferenceError SyntaxError TypeError URIError decodeURI decodeURIComponent encodeURI encodeURIComponent eval isFinite isNaN parseFloat parseInt
# documentation keywords for javadoc
doccomment=author deprecated exception param return see serial serialData serialField since throws todo version
typedefs=

[settings]
# default extension used when saving files
extension=dart

# MIME type
mime_type=application/dart

# Set Lexer
lexer_filetype=Java

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
# command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=2
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=0

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=
run=
