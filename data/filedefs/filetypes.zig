# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment_line=comment_line
comment_line_doc=comment_doc
comment_line_top=comment_doc
number=number_1
operator=operator
character=character
string=string_1
multistring=string_1
escapechar=string_1
identifier=identifier_1
function=function
builtin_function=keyword_2
kw_primary=keyword_1
kw_secondary=keyword_2
kw_tertiary=keyword_2
kw_type=class
identifer_string=preprocessor

[keywords]
# all items must be in one line
primary=addrspace align allowzero and anyframe anytype asm async await break callconv catch comptime const continue defer else enum errdefer error export extern fn for if inline linksection noalias noinline nosuspend opaque or orelse packed pub resume return struct suspend switch test threadlocal try union unreachable usingnamespace var volatile while true false null undefined
secondary=i8 u8 i16 u16 i32 u32 i64 u64 i128 u128 isize usize c_char c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong c_longdouble f16 f32 f64 f80 f128 bool anyopaque void noreturn type anyerror comptime_int comptime_float

[lexer_properties]

[settings]
# default extension used when saving files
extension=zig

# MIME type
mime_type=text/x-zig

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comment char, like # in this file
comment_single=//
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
# 		#command_example();
# setting to false would generate this
# #		command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=0

[build-menu]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
FT_00_LB=_Build
FT_00_CM=zig build-exe %f
FT_00_WD=
FT_02_LB=Te_st
FT_02_CM=zig test %f
FT_02_WD=
EX_00_LB=_Run
EX_00_CM=zig run %f
EX_00_WD=
