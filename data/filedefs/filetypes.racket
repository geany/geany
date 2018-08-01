# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment_line
multicomment=comment
number=number_1
keyword=keyword_1
keywordkw=keyword_2
symbol=keyword_2
string=string_1
stringeol=string_eol
identifier=identifier_1
operator=operator
special=function

[keywords]
# all items must be in one line
keywords=boolean? not equal? eqv? eq? equal?/recur immutable? gen:equal+hash prop:equal+hash symbol=? boolean=? false? nand nor implies xor number? complex? real? rational? integer? exact-integer? exact-nonnegative-integer? exact-positive-integer? inexact-real? fixnum? flonum? double-flonum? single-flonum? zero? positive? negative? even? odd? exact? inexact? inexact->exact exact->inexact real->single-flonum real->double-flonum + - * / quotient remainder quotient/remainder modulo add1 sub1 abs max min gcd lcm round floor ceeiling truncate numerator denominator rationalize = < <= > >= sqrt integer-sqrt integer-sqrt/remainder expt exp log sin cos tan asin acos atan make-rectangular make-polar real-part imag-part magnitude angle bitwise-ior bitwise-and bitwise-xor bitwise-not bitwise-bit-set? bitwise-bit-field arithmetic-shift integer-length random random-seed make-pseudo-random-generator pseudo-random-generator? current-pseudo-random-generator pseudo-random-generator->vector vector->pseudo-random-generator pseudo-random-generator-vector? crypto-random-types random-ref random-sample number->string string->number real->decimal-string integer-bytes->integer integer->integer-bytes floating-point-bytes->real real->floating-point-bytes system-big-endian? pi pi.f degrees->radians radians->degrees sqr sgn conjugate sinh cosh tanh exact-round exact-floor exact-ceiling exact-truncate order-of-magnitude nan? infinite? positive-integer? negative-integer? nonpositive-integer? nonnegative-integer? natural? fl+ fl- fl* fl/ flabs fl= fl< fl> fl<= fl>= flmin flmax flround flfloor flceiling fltruncate flsin flcos fltan flasin flacos flatan fllog flexp flsqrt flexpt ->fl fl->exact-integer make-flrectangular flrandom flvector? flvector make-flvector flvector-length flvector-ref flvector-set! flvector-copy in-flvector shared-flvector make-shared-flvector fx+ fx- fx* fxquotient fxremainder fxmodulo fxabs fxand fxior fxxor fxnot fxlshift fxrshift fx= fx< fx> fx<= fx>= fxmin fxmax fx->fl fl->fx fxvector? fxvector make-fxvector fxvector-length fxvector-ref fxvector-set! fxvector-copy in-fxvector shared-fxvector make-shared-fxvector extflonum? extflonum-available? extfl+ extfl- extfl* extfl/ extflabs extfl= extfl< extfl> extfl<= extfl>= extflmin extflmax extflsin extflcos extfltan extflasin extflacos extfnatan extfllog extflexp extflsqrt extflexpt
->extfl extfl->exact-integer real->extfl extfl->exact extfl->inexact pi.t extflvector? extflvector make-extflvector extflvector-length extflvector-ref extflvector-set! extflvector-copy in-extflvector make-shared-extflvector floating-point-bytes->extfl extfl->floating-point-bytes string? make-string string string->immutable-string string-length string-ref string-set! substring string-copy string-copy! string-fill! string-append string->list list->string build-string string=? string<? string<=? string>? string>=? string-ci=? string-ci<? string-ci<=? string-ci>? string-ci>=? string-upcase string-downcase string-titlecase string-foldcase string-normalize-nfd string-normalize-nfkd string-normalize-nfc string-normalize-nfkc string-locale=? string-locale<? string-locale>? string-locale-ci=? string-locale-ci<? string-locale-upcase string-locale-downcase string-append* string-join string-normalize-spaces string-replace string-split string-trim non-empty-string? string-contains? string-prefix? string-suffix? ~a ~v ~s ~e ~r ~.a ~.v ~.s
special_keywords=define class let let* for for/list for*/fold require module true false #f #t for/flvector for*/flvector for/fxvector for*/fxvector for/extflvector for*/extflvector

[settings]
# default extension used when saving files
extension=rkt
# MIME type
#mime_type=text/x-racket

# the following characters are these which a "word" can contains, see documentation
wordchars=!#%*-:<=>?@\\abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
whitespace_chars=\s\t\"$&'()+,./;[]^`{|}~_

# single comments, like # in this file
comment_single=;
# multiline comments
comment_open=#|
comment_close=|#

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=xdg-open "http://docs.racket-lang.org/search/index.html?q=%s"

[indentation]
width=2
# 0 is spaces, 1 is tabs, 2 is tab & spaces
type=0

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compile=raco make "%e"
run_cmd=racket "%f"
