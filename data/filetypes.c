# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file intead
default=default
comment=comment
commentline=comment
commentdoc=comment_doc
number=number_1
word=keyword_1
word2=keyword_2
string=string_1
character=string_1
uuid=other
preprocessor=preprocessor
operator=operator
identifier=identifier_1
stringeol=string_eol
verbatim=string_2
regex=regex
commentlinedoc=comment_doc
commentdockeyword=comment_doc_keyword
commentdockeyworderror=comment_doc_keyword_error
globalclass=class
# """verbatim"""
tripleverbatim=string_2

[keywords]
# all items must be in one line
primary=asm auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while FALSE NULL TRUE
secondary=
# these are some doxygen keywords (incomplete)
docComment=attention author brief bug class code date def enum example exception file fn namespace note param remarks return see since struct throw todo typedef var version warning union

[lexer_properties]
styling.within.preprocessor=1
lexer.cpp.track.preprocessor=0
preprocessor.symbol.$(file.patterns.cpp)=#
preprocessor.start.$(file.patterns.cpp)=if ifdef ifndef
preprocessor.middle.$(file.patterns.cpp)=else elif
preprocessor.end.$(file.patterns.cpp)=endif

[settings]
# default extension used when saving files
extension=c

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
compiler=gcc -Wall -c "%f"
linker=gcc -Wall -o "%e" "%f"
run_cmd="./%e"


