# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead

default=default
commentline=comment_line
commentblock=comment
string=string_1
stringmultiline=string_1
escapechar=regex
identifier=default
operator=operator
operatorstring=regex
number=number_1
key=tag
keyword1=keyword_1
keyword2=keyword_1
keyword3=keyword_2
keyword4=keyword_2
path=preprocessor

[keywords]
# all items must be in one line
keywords1=assert else if in inherit let or rec then with
keywords2=false null true
keywords3=abort add addDrvOutputDependencies all any attrNames attrValues baseNameOf bitAnd bitOr bitXor
 break builtins catAttrs
ceil compareVersions concatLists concatMap concatStringsSep convertHash currentSystem currentTime
deepSeq derivation dirOf div elem 
elemAt fetchClosure fetchGit fetchTarball fetchTree fetchurl filter filterSource findFile 
flakeRefToString 
floor foldl' fromJSON fromTOML functionArgs genList genericClosure getAttr getContext getEnv getFlake 
groupBy hasAttr hasContext hashFile hashString head import intersectAttrs isAttrs isBool isFloat 
isFunction isInt isList isNull isPath isString langVersion 
length lessThan listToAttrs map mapAttrs match mul nixPath nixVersion
outputOf parseDrvName parseFlakeRef partition path pathExists placeholder readDir readFile 
readFileType removeAttrs replaceStrings seq sort split splitVersion storeDir storePath stringLength sub 
substring tail throw toFile toJSON toPath toString toXML trace traceVerbose tryEval typeOf 
unsafeDiscardOutputDependency unsafeDiscardStringContext warn zipAttrsWith
keywords4=

[lexer_properties]

[settings]
# default extension used when saving files
extension=nix

# MIME type
mime_type=text/x-nix

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comment char, like # in this file
comment_single=#
# multiline comments
#comment_open=/*
#comment_close=*/

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
FT_00_LB=_Compile
FT_00_CM=python -m py_compile "%f"
FT_00_WD=
FT_02_LB=_Lint
FT_02_CM=pycodestyle --max-line-length=80 "%f"
FT_02_WD=
error_regex=(.+):([0-9]+):([0-9]+)
EX_00_LB=_Execute
EX_00_CM=python "%f"
EX_00_WD=
