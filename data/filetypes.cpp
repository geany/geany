# For complete documentation of this file, please see Geany's main documentation
[styling=C]

[keywords]
# all items must be in one line
primary=alignas alignof and and_eq asm auto bitand bitor bool break case catch char char16_t char32_t class compl const const_cast constexpr continue decltype default delete do double dynamic_cast else enum explicit export extern false final float for friend goto if inline int int8_t int16_t int32_t int64_t long mutable namespace new noexcept not not_eq nullptr operator or or_eq override private protected ptrdiff_t public register reinterpret_cast return short signed sizeof size_t static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq
secondary=
# these are the Doxygen keywords
docComment=a addindex addtogroup anchor arg attention author authors b brief bug c callergraph callgraph category cite class code cond copybrief copydetails copydoc copyright date def defgroup deprecated details dir dontinclude dot dotfile e else elseif em endcode endcond enddot endhtmlonly endif endinternal endlatexonly endlink endmanonly endmsc endrtfonly endverbatim endxmlonly enum example exception extends file fn headerfile hideinitializer htmlinclude htmlonly if ifnot image implements include includelineno ingroup interface internal invariant latexonly li line link mainpage manonly memberof msc mscfile n name namespace nosubgrouping note overload p package page par paragraph param post pre private privatesection property protected protectedsection protocol public publicsection ref related relatedalso relates relatesalso remark remarks result return returns retval rtfonly sa section see short showinitializer since skip skipline snippet struct subpage subsection subsubsection tableofcontents test throw throws todo tparam typedef union until var verbatim verbinclude version warning weakgroup xmlonly xrefitem

[lexer_properties]
styling.within.preprocessor=1
lexer.cpp.track.preprocessor=0
preprocessor.symbol.$(file.patterns.cpp)=#
preprocessor.start.$(file.patterns.cpp)=if ifdef ifndef
preprocessor.middle.$(file.patterns.cpp)=else elif
preprocessor.end.$(file.patterns.cpp)=endif

[settings]
lexer_filetype=C

# default extension used when saving files
extension=cpp

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
compiler=g++ -Wall -c "%f"
linker=g++ -Wall -o "%e" "%f"
run_cmd="./%e"

