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
qoperator=operator

[keywords]
# all items must be in one line
keywords=a abort abs absolute access action ada add admin after aggregate alias all allocate also alter always analyse analyze and any are array as asc asensitive assertion assignment asymmetric at atomic attribute attributes audit authorization auto_increment avg avg_row_length backup backward before begin bernoulli between bigint binary bit bit_length bitvar blob bool boolean both breadth break browse bulk by c cache call called cardinality cascade cascaded case cast catalog catalog_name ceil ceiling chain change char char_length character character_length character_set_catalog character_set_name character_set_schema characteristics characters check checked checkpoint checksum class class_origin clob close cluster clustered coalesce cobol collate collation collation_catalog collation_name collation_schema collect column column_name columns command_function command_function_code comment commit committed completion compress compute condition condition_number connect connection connection_name constraint constraint_catalog constraint_name constraint_schema constraints constructor contains containstable continue conversion convert copy corr corresponding count covar_pop covar_samp create createdb createrole createuser cross csv cube cume_dist current current_date current_default_transform_group current_path current_role current_time current_timestamp current_transform_group_for_type current_user cursor cursor_name cycle data database databases date datetime datetime_interval_code datetime_interval_precision day day_hour day_microsecond day_minute day_second dayofmonth dayofweek dayofyear dbcc deallocate dec decimal declare default defaults deferrable deferred defined definer degree delay_key_write delayed delete delimiter delimiters dense_rank deny depth deref derived desc describe descriptor destroy destructor deterministic diagnostics dictionary disable disconnect disk dispatch distinct distinctrow distributed div do domain double drop dual dummy dump dynamic dynamic_function dynamic_function_code each element else elseif enable enclosed encoding encrypted end end-exec enum equals errlvl escape escaped every except exception exclude excluding exclusive exec execute existing exists exit exp explain external extract false fetch fields file fillfactor filter final first float float4 float8 floor flush following for force foreign fortran forward found free freetext freetexttable freeze from full fulltext function fusion g general generated get global go goto grant granted grants greatest group grouping handler having header heap hierarchy high_priority hold holdlock host hosts hour hour_microsecond hour_minute hour_second identified identity identity_insert identitycol if ignore ilike immediate immutable implementation implicit in include including increment index indicator infile infix inherit inherits initial initialize initially inner inout input insensitive insert insert_id instance instantiable instead int int1 int2 int3 int4 int8 integer intersect intersection interval into invoker is isam isnull isolation iterate join k key key_member key_type keys kill lancompiler language large last last_insert_id lateral leading least leave left length less level like limit lineno lines listen ln load local localtime localtimestamp location locator lock login logs long longblob longtext loop low_priority lower m map match matched max max_rows maxextents maxvalue mediumblob mediumint mediumtext member merge message_length message_octet_length message_text method middleint min min_rows minus minute minute_microsecond minute_second minvalue mlslabel mod mode modifies modify module month monthname more move multiset mumps myisam name names national natural nchar nclob nesting new next no no_write_to_binlog noaudit nocheck nocompress nocreatedb nocreaterole nocreateuser noinherit nologin nonclustered none normalize normalized nosuperuser not nothing notify notnull nowait null nullable nullif nulls number numeric object octet_length octets of off offline offset offsets oids old on online only open opendatasource openquery openrowset openxml operation operator optimize option optionally options or order ordering ordinality others out outer outfile output over overlaps overlay overriding owner pack_keys pad parameter parameter_mode parameter_name parameter_ordinal_position parameter_specific_catalog parameter_specific_name parameter_specific_schema parameters partial partition pascal password path pctfree percent percent_rank percentile_cont percentile_disc placing plan pli position postfix power preceding precision prefix preorder prepare prepared preserve primary print prior privileges proc procedural procedure process processlist public purge quote raid0 raiserror range rank raw read reads readtext real recheck reconfigure recursive ref references referencing regexp regr_avgx regr_avgy regr_count regr_intercept regr_r2 regr_slope regr_sxx regr_sxy regr_syy reindex relative release reload rename repeat repeatable replace replication require reset resignal resource restart restore restrict result return returned_cardinality returned_length returned_octet_length returned_sqlstate returns revoke right rlike role rollback rollup routine routine_catalog routine_name routine_schema row row_count row_number rowcount rowguidcol rowid rownum rows rule save savepoint scale schema schema_name schemas scope scope_catalog scope_name scope_schema scroll search second second_microsecond section security select self sensitive separator sequence serializable server_name session session_user set setof sets setuser share show shutdown signal similar simple size smallint some soname source space spatial specific specific_name specifictype sql sql_big_result sql_big_selects sql_big_tables sql_calc_found_rows sql_log_off sql_log_update sql_low_priority_updates sql_select_limit sql_small_result sql_warnings sqlca sqlcode sqlerror sqlexception sqlstate sqlwarning sqrt ssl stable start starting state statement static statistics status stddev_pop stddev_samp stdin stdout storage straight_join strict string structure style subclass_origin sublist submultiset substring successful sum superuser symmetric synonym sysdate sysid system system_user table table_name tables tablesample tablespace temp template temporary terminate terminated text textsize than then ties time timestamp timezone_hour timezone_minute tinyblob tinyint tinytext to toast top top_level_count trailing tran transaction transaction_active transactions_committed transactions_rolled_back transform transforms translate translation treat trigger trigger_catalog trigger_name trigger_schema trim true truncate trusted tsequal type uescape uid unbounded uncommitted under undo unencrypted union unique unknown unlisten unlock unnamed unnest unsigned until update updatetext upper usage use user user_defined_type_catalog user_defined_type_code user_defined_type_name user_defined_type_schema using utc_date utc_time utc_timestamp vacuum valid validate validator value values var_pop var_samp varbinary varchar varchar2 varcharacter variable variables varying verbose view volatile waitfor when whenever where while width_bucket window with within without work write writetext x509 xor year year_month zerofill zone

[settings]
# default extension used when saving files
extension=sql

# MIME type
mime_type=text/x-sql

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=--\s
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
