# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment
kword=keyword_1
operator=operator
basekword=keyword_2
otherkword=keyword_3
number=number_1
string=string_1
string2=string_2
identifier=identifier
infix=function
infixeol=function

[keywords]
# all items must be in one line
# use same keywords as in RStudio
# https://github.com/rstudio/rstudio/blob/master/src/gwt/acesupport/acemode/r_highlight_rules.js
primary=attach break detach else for function if in library new next repeat require return setClass setGeneric setGroupGeneric setMethod setRefClass source stop switch try tryCatch warning while
# use same buildinConstants as in RStudio
package=F FALSE Inf NA NA_integer_ NA_real_ NA_character_ NA_complex_ NaN NULL T TRUE
package_other=

[settings]
# default extension used when saving files
extension=R

# the following characters are these which a "word" can contains, see documentation
#wordchars=_.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
   #command_example();
# setting to false would generate this
#  command_example();
# This setting works only for single line comments
comment_use_indent=false

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

