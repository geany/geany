# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment
commentline=comment_line
commentdoc=comment_doc
number=number_1
word=keyword_1
word2=keyword_2
uuid=string_2
string=string_1
character=character
operator=operator
identifier=identifier_1
quotedidentifier=identifier_2
preprocessor=preprocessor

[keywords]
# all items must be in one line
#
#  three keywords are supported for cobol:
#      primary,secondary and extended_keywords
#
#  the styling keywords are:
#         primary   -> [styling] word
#         secondary -> [styling] word2
# extended_keywords -> [styling] uuid
#
primary=accept access add address advancing after all allowing alphabet alphabetic alphabetic-lower alphabetic-upper alphanumeric alphanumeric-edited also alter alternate and any are area areas ascending assign at author beep before bell binary blank block bottom by call cancel cbll cd cf ch character characters class clock-units close cobol code code-set col color colour collating column comma common communications computational compute condition-code configuration content continue control converting copy corr corresponding count currency data date date-compiled date-written day day-of-week de debugging debug-contents debug-item debug-line debug-name debug-sub-1 debug-sub-2 debug-sub-3 decimal-point delaratives delete delimited delimiter depending descending destination detail disable display divide division down duplicates dynamic egi else emi enabled end end-accept end-add end-call end-chain end-compute end-delete end-display end-divide end-evaluate end-if end-inspect end-multiply end-of-page end-perform end-read end-receive end-return end-rewrite end-search end-start end-string end-subtract end-unstring end-write enter environment eop equal error esi evaluate every exception exit extend external false fd file file-control filler final first footing for from function generate giving global go goback greater group heading high high-value high-values identification if in index indexed indicate initial initialize initiate input input-output inspect installation into invalid i-o i-o-control is just justified key label last leading left less limit limits linage linage-counter line line-counter lines linkage lock low-value low-values memory merge message mode modules move multiple multiply national native negative next no not number numeric numeric-edited object-computer occurs of off omitted on open optional or order organization other output overflow packed-decimal padding page page-counter perform pf ph pic picture plus pointer pos position positive printing procedure procedures proceed program program-id purge queue quote quotes rd read receive record records redefines reel reference references relative release remainder removal renames replace replacing report reporting reports rerun reserve reset return returning return-code return-unsigned reversed rewind rewrite rf rh right rollback rounded run same sd search section security segment segment-limit select send sentence separate sequence sequential set signed-int signed-long signed-short size sort sort-merge source source-computer space spaces special-names standard standard-1 standard-2 start status stop string sub-queue-1 sub-queue-2 sub-queue-3 subtract suppress symbolic sync synchronized table tallying tape terminal terminate test text than then through thru time times to top trailing true type unit unsigned-int unsigned-long unsigned-short unstring until up upon usage use using value values varying when with words working-storage write zero zeroes zeros
secondary=abs absolute-value acos annuity asin atan byte-length char combined-datetime concatenate cos current-date date-of-integer date-to-yyyymmdd day-of-integer day-to-yyyymmdd e exception-file exception-location exception-statement exception-status exp exp10 fraction-part factorial integer integer-of-date integer-of-day integer-part length locale-date locale-time locale-time-from-secs log log10 lower-case max min mean median midrange mod numval numval-c ord ord-max ord-min pi present-value random range rem reverse seconds-from-formatted-time seconds-past-midnight sign sin sqrt standard-deviation stored-char-length substitute substitute-case sum tan test-date-yyyymmdd test-day-yyyymmdd trim upper-case variance when-compiled year-to-yyyymmdd
extended_keywords=

[settings]
# default extension used when saving files
extension=cob

# MIME type
mime_type=text/x-cobol

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=*>
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=false

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1
