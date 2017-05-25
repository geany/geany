# For complete documentation of this file, please see Geany's main documentation

[styling=C]

[keywords]
# all items must be in one line
primary=break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var true false iota nil
secondary=bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 uint16 uint32 uint64 uintptr

# these are the Doxygen keywords
docComment=a addindex addtogroup anchor arg attention author authors b brief bug c callergraph callgraph category cite class code cond copybrief copydetails copydoc copyright date def defgroup deprecated details dir dontinclude dot dotfile e else elseif em endcode endcond enddot endhtmlonly endif endinternal endlatexonly endlink endmanonly endmsc endrtfonly endverbatim endxmlonly enum example exception extends file fn headerfile hideinitializer htmlinclude htmlonly if ifnot image implements include includelineno ingroup interface internal invariant latexonly li line link mainpage manonly memberof msc mscfile n name namespace nosubgrouping note overload p package page par paragraph param post pre private privatesection property protected protectedsection protocol public publicsection ref related relatedalso relates relatesalso remark remarks result return returns retval rtfonly sa section see short showinitializer since skip skipline snippet struct subpage subsection subsubsection tableofcontents test throw throws todo tparam typedef union until var verbatim verbinclude version warning weakgroup xmlonly xrefitem

[lexer_properties=C]
lexer.cpp.backquoted.strings=1
lexer.cpp.allow.dollars=0
fold.preprocessor=0

[settings]

# default extension used when saving files
extension=go

# MIME type
mime_type=text/x-go

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

[build-menu]
FT_00_LB=_Build
FT_00_CM=go build %f
FT_00_WD=
FT_02_LB=Te_st
FT_02_CM=go test
FT_02_WD=
EX_00_LB=_Run
EX_00_CM=go run %f
EX_00_WD=
