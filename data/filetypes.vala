# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=default
comment=comment
commentline=comment
commentdoc=commentdoc
number=number
word=keyword
word2=keyword2
string=string
character=string
uuid=extra
preprocessor=preprocessor
operator=operator
identifier=default
stringeol=stringeol
# @"verbatim"
verbatim=extra
# (/regex/)
regex=extra
commentlinedoc=commentdoc,bold
commentdockeyword=commentdoc,bold,italic
commentdockeyworderror=commentdoc
globalclass=type

[keywords]
# all items must be in one line
primary=abstract as async base bool break callback case catch char class const constpointer construct continue default delegate delete do double dynamic else ensures enum errordomain extern false finally float for foreach generic get global if in inline int int16 int32 int64 int8 interface internal is lock long namespace new null out override owned private protected public ref requires return set sealed short signal size_t sizeof ssize_t static string struct switch this throw throws time_t true try typeof uchar uint uint16 uint32 uint64 uint8 ulong unichar unowned ushort using value var virtual void weak while yield
#secondary=
# these are some doxygen and valadoc keywords (incomplete)
docComment=attention author brief bug class code date def deprecated enum example exception file fn inheritDoc link namespace note param remarks return see since struct throw throws todo typedef union var version warning

[lexer_properties]
styling.within.preprocessor=1
lexer.cpp.track.preprocessor=0
preprocessor.symbol.$(file.patterns.cpp)=#
preprocessor.start.$(file.patterns.cpp)=if
preprocessor.middle.$(file.patterns.cpp)=else elif
preprocessor.end.$(file.patterns.cpp)=endif
lexer.cpp.triplequoted.strings=1

[settings]
lexer_filetype=C

# default extension used when saving files
extension=vala

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
#comment_use_indent=true

# context action command (please see Geany's main documentation for details)
#context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=valac -c "%f"
linker=valac "%f"
run_cmd=./"%e"
