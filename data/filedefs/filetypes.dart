# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead

default=default
commentline=comment_line
commentlinedoc=comment_doc
commentblock=comment
commentblockdoc=comment_doc
stringsq=string_1
stringdq=string_1
triplestringsq=string_1
triplestringdq=string_1
rawstringsq=string_1
rawstringdq=string_1
triplerawstringsq=string_1
triplerawstringdq=string_1
escapechar=string_2
identifier=default
identifierstring=string_2
operator=operator
operatorstring=string_2
symbolidentifier=preprocessor
symboloperator=preprocessor
number=number_1
key=other
metadata=decorator
kwprimary=keyword_1
kwsecondary=keyword_1
kwtertiary=keyword_2
kwtype=class
stringeol=string_eol

[keywords]
# all items must be in one line
primary=abstract as assert async await base break case catch class const continue covariant default deferred do else enum export extends extension external factory false final finally for get hide if implements import in interface is late library mixin native new null of on operator part required rethrow return sealed set show static super switch sync this throw true try type typedef var when while with yield
secondary=Function Never bool double dynamic int num void
tertiary=BigInt Comparable Comparator Completer DateTime Deprecated Directory DoubleLinkedQueue Duration Enum Error Exception Expando File FileLock FileMode FileStat FileSystemEntity FileSystemEvent Future FutureOr HashMap HashSet IOException Invocation Iterable IterableBase IterableMixin Iterator LinkedHashMap LinkedHashSet LinkedList LinkedListEntry List ListBase ListMixin ListQueue Map MapBase MapEntry MapMixin MapView Match Null OSError Object Pattern Platform Point Process Queue Random RawSocket RawSocketEvent Record Rectangle RegExp RegExpMatch RuneIterator Runes ServerSocket Set SetBase SetMixin Sink Socket SocketException SplayTreeMap SplayTreeSet StackTrace Stopwatch Stream String StringBuffer StringSink Symbol SystemHash Timer Type Uri UriData WeakReference

[lexer_properties]

[settings]
tag_parser=Java

# default extension used when saving files
extension=dart

# MIME type
mime_type=text/x-dart

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comment char, like # in this file
comment_single=//
# multiline comments
comment_open=/*
comment_close=*/

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
#width=2
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=0

[build-menu]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
FT_00_LB=_Compile
FT_00_CM=dart compile exe "%f"
FT_00_WD=
FT_01_LB=_Analyze
FT_01_CM=dart analyze "%f"
FT_01_WD=
FT_02_LB=_Test
FT_02_CM=dart test "%f"
FT_02_WD=
EX_00_LB=Dart _Run
EX_00_CM=dart run "%f"
EX_00_WD=%p
EX_01_LB=Flutter R_un
EX_01_CM=flutter run "%f"
EX_01_WD=%p
