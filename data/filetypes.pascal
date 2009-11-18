# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
identifier=0x000000;0xffffff;false;false
comment=0xd00000;0xffffff;false;false
comment2=0x3f5fbf;0xffffff;false;false
commentline=0xd00000;0xffffff;false;false
preprocessor=0x007f7f;0xffffff;false;false
preprocessor2=0x007f7f;0xffffff;false;false
number=0x007F00;0xffffff;false;false
hexnumber=0x007F00;0xffffff;false;false
word=0x111199;0xffffff;true;false
string=0xff901e;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
character=0x404000;0xffffff;false;false
operator=0x301010;0xffffff;false;false
asm=0x804080;0xffffff;false;false


[keywords]
# all items must be in one line
primary=absolute abstract add and array as asm assembler automated begin byte case cdecl char class const constructor contains default deprecated destructor dispid dispinterface div do downto dynamic else end except export exports external far file final finalization finally for forward function goto if implementation implements in index inherited initialization inline integer interface is label library message mod name near nil nodefault not object of on or out overload override package packed pascal platform private procedure program property protected public published raise read readonly real record register reintroduce remove repeat requires resourcestring safecall sealed set shl shr static stdcall stored strict string then threadvar to try type unit unsafe until uses var varargs virtual while with word write writeonly xor

[lexer_properties]
# only highlight keywords like read,write if in appropriate context
lexer.pascal.smart.highlighting=1

[settings]
# default extension used when saving files
#extension=pas

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
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

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=fpc "%f"
run_cmd="./%e"
