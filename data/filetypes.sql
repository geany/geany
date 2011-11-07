# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment
commentline=comment_line
commentlinedoc=comment_line_doc
commentdoc=comment_doc
commentdockeyword=comment_doc_keyword
commentdockeyworderror=comment_doc_keyword_error
number=number_1
word=keyword_1
word2=keyword_2
string=string_1
character=character
operator=operator
identifier=identifier_1
sqlplus=default
sqlplus_prompt=default
sqlplus_comment=comment
quotedidentifier=identifier_2

[keywords]
# all items must be in one line
keywords=absolute action add admin after aggregate alias all allocate alter and any are array as asc assertion at authorization before begin bfile bigint binary bit blob bool boolean both breadth by call cascade cascaded case cast catalog char character check class clob close cluster collate collation column commit completion connect connection constraint constraints constructor continue corresponding create cross cube current current_date current_path current_role current_time current_timestamp current_user cursor cycle data date day deallocate dec decimal declare default deferrable deferred delete depth deref desc describe descriptor destroy destructor deterministic diagnostics dictionary dimension disconnect diskgroup distinct domain double drop dynamic each else end end-exec equals escape every except exception exec execute exists explain external false fetch first fixed flashback float for foreign found from free full function general get global go goto grant group grouping having host hour identity if ignore immediate in index indextype indicator initialize initially inner inout input insert int integer intersect interval into is isolation iterate join key language large last lateral leading left less level like limit local localtime localtimestamp locator long map match materialized mediumblob mediumint mediumtext merge middleint minus minute modifies modify module month names national natural nchar nclob new next no noaudit none not null numeric nvarchar2 object of off old on only open operation option or order ordinality out outer output package pad parameter parameters partial path postfix precision prefix preorder prepare preserve primary prior privileges procedure profile public purge read reads real recursive ref references referencing regexp regexp_like relative rename restrict result return returning returns revoke right role rollback rollup routine row rows savepoint schema scroll scope search second section select sequence session session_user set sets size smallint some space specific specifictype sql sqlexception sqlstate sqlwarning start state statement static structure synonym system_user table tablespace temporary terminate than then time timestamp timezone_hour timezone_minute tinyint to trailing transaction translation  treat trigger true truncate type under union unique unknown unnest update usage user using value values varchar varchar2 variable varying view when whenever where with without work write year zone


[settings]
# default extension used when saving files
extension=sql

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=--
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
