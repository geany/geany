# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
identifier=identifier_1
comment=comment
comment2=comment_doc
commentline=comment_line
preprocessor=preprocessor
preprocessor2=preprocessor
number=number_1
hexnumber=number_1
word=keyword_1
string=string
stringeol=string_eol
character=character
operator=operator
asm=number_2


[keywords]
# all items must be in one line
primary=absolute abstract add and array as asm assembler automated begin boolean break byte case cdecl char class const constructor contains default deprecated destructor dispid dispinterface div do downto dynamic else end except export exports external far file final finalization finally for forward function goto if implementation implements in index inherited initialization inline integer interface is label library message mod name near nil nodefault not object of on or out overload override package packed pascal platform private procedure program property protected public published raise read readonly real record register reintroduce remove repeat requires resourcestring safecall sealed set shl shr static stdcall stored strict string then threadvar to try type unit unsafe until uses var varargs virtual while with word write writeonly xor

[lexer_properties]
# only highlight keywords like read,write if in appropriate context
lexer.pascal.smart.highlighting=1

[settings]
# default extension used when saving files
extension=pas

# MIME type
mime_type=text/x-pascal

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
#comment_single=
# multiline comments
comment_open={
comment_close=}

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
compiler=fpc "%f"
run_cmd="./%e"
