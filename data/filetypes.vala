# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=default
comment=comment
commentline=comment
commentdoc=commentdoc
number=number
word=word
word2=word2
string=string
character=string
uuid=0x404080
preprocessor=preprocessor
operator=operator
identifier=default
stringeol=stringeol
# @"verbatim"
verbatim=0x101030
# (/regex/)
regex=0x105090
commentlinedoc=commentdoc,bold
commentdockeyword=commentdoc,bold,italic
commentdockeyworderror=commentdoc
globalclass=type

[keywords]
# all items must be in one line
primary=else if switch case default break continue return for foreach in do while is try catch finally throw namespace interface class struct enum signal errordomain construct callback get set base const static var weak virtual abstract override inline extern public protected private delegate out ref lock using true false null generic new delete base this value typeof sizeof throws requires ensures void bool char uchar int uint short ushort long ulong size_t ssize_t int8 uint8 int16 uint16 int32 uint32 int64 uint64 float double unichar string constpointer time_t
#secondary=
# these are some doxygen keywords (incomplete)
docComment=attention author brief bug class code date def enum example exception file fn namespace note param remarks return see since struct throw todo typedef var version warning union

[lexer_properties]
styling.within.preprocessor=1
preprocessor.symbol.$(file.patterns.cpp)=#
preprocessor.start.$(file.patterns.cpp)=if
preprocessor.middle.$(file.patterns.cpp)=else elif
preprocessor.end.$(file.patterns.cpp)=endif

[settings]
lexer_filetype=C

# default extension used when saving files
#extension=vala

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
#comment_open=//
#comment_close=
# this is an alternative way, so multiline comments are used
#comment_open=/*
#comment_close=*/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
#comment_use_indent=true

# context action command (please see Geany's main documentation for details)
#context_action_cmd=

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=valac -c "%f"
linker=valac "%f"
run_cmd=./"%e"
