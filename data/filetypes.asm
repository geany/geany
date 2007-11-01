# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0x808080;0xffffff;false;false
number=0x007f00;0xffffff;false;false
string=0xff901e;0xffffff;false;false
operator=0x000000;0xffffff;false;false
identifier=0x880000;0xffffff;false;false
cpuinstruction=0x111199;0xffffff;true;false
mathinstruction=0x7f0000;0xffffff;true;false
register=0x000000;0xffffff;true;false
directive=0x3d670f;0xffffff;true;false
directiveoperand=0xff901e;0xffffff;false;false
commentblock=0x808080;0xffffff;false;false
character=0xff901e;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
extinstruction=0x007f7f;0xffffff;false;false

[keywords]
# all items must be in one line
# this is by default a very simple instruction set; not of Intel or so
instructions=hlt lad spi add sub mul div jmp jez jgz jlz swap jsr ret pushac popac addst subst mulst divst lsa lds push pop cli ldi ink lia dek ldx
registers=
directives=org list nolist page equivalent word text


[settings]
# default extension used when saving files
#extension=asm

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=;
comment_close=

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
compiler=nasm "%f"

