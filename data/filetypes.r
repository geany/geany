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
primary=function if in break next repeat else for return switch while try tryCatch stop warning require library attach detach source setMethod setGeneric setGroupGeneric setClass setRefClass new
# use same buildinConstants as in RStudio
package=NULL NA TRUE FALSE T F Inf NaN NA_integer_ NA_real_ NA_character_ NA_complex_
#package_other=acme aids aircondit amis aml banking barchart barley beaver bigcity boot brambles breslow bs bwplot calcium cane capability cav censboot channing city claridge cloth cloud coal condense contourplot control corr darwin densityplot dogs dotplot ducks empinf envelope environmental ethanol fir frets gpar grav gravity grob hirose histogram islay knn larrows levelplot llines logit lpoints lsegments lset ltext lvqinit lvqtest manaus melanoma melanoma motor multiedit neuro nitrofen nodal ns nuclear oneway parallel paulsen poisons polar qq qqmath remission rfs saddle salinity shingle simplex singer somgrid splom stripplot survival tau tmd tsboot tuna unit urine viewport wireframe wool xyplot

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

