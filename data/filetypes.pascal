# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x0000ff;0xffffff;false;false
comment=0x0000ff;0xffffff;false;false
number=0x007F00;0xffffff;false;false
word=0x991111;0xffffff;true;false
string=0x1E90FF;0xffffff;false;false
character=0x004040;0xffffff;false;false
preprocessor=0x7F7F00;0xffffff;false;false
operator=0x101030;0xffffff;false;false
identifier=0x991111;0xffffff;true;false
regex=0x13631B;0xffffff;false;false
commentline=0x0000ff;0xffffff;false;false
commentdoc=0x0000ff;0xffffff;false;false

[keywords]
primary=absolute and array asm begin break case const constructor continue destructor div do downto dynamic else end end. file for function goto if implementation in inherited inline interface label message mod nil not object of on operator or overload outpacked procedure program record reintroduce repeat self set shl shr string then to type unit until uses var while with xor as class except exports finalization finally initialization is library on property raise threadvar try dispose exit false new true absolute abstract alias assembler cdecl cppdecl default export external far far16 forward index name near oldfpccall override pascal private protected public published read register safecall softfloat stdcall virtual write


[settings]
# the following characters are these which a "word" can contains, see documentation
wordchars=_#&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open={
comment_close=}

# set to false if a comment character/string should start a column 0 of a line, true uses any
# indention of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=fpc "%f" 1>&2
run_cmd="./%e"
