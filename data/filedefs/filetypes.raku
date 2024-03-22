# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
error=error
commentline=comment_line
commentembed=comment
pod=comment_doc
character=character
heredoc_qq=here_doc
string=string_1
string_q=string_2
string_qq=string_2
string_q_lang=string_2
string_var=string_2
regex=regex
regex_var=identifier_2
adverb=default
number=number_1
preprocessor=preprocessor
operator=operator
word=keyword_1
function=function
identifier=identifier
typedef=default
mu=default
positional=default
associative=default
callable=default
grammar=default
class=class

[keywords]
keywords=BEGIN CATCH CHECK CONTROL END ENTER EVAL FIRST INIT KEEP LAST LEAVE NEXT POST PRE START TEMP UNDO after also andthen as async augment bag before but category circumfix class cmp complex constant contend default defer div does dynamic else elsif enum eq eqv extra fail fatal ff fff for gather gcd ge given grammar gt handles has if infix is lcm le leave leg let lift loop lt macro make maybe method mix mod module multi ne not o only oo or orelse orwith postcircumfix postfix prefix proto regex repeat require return-rw returns role rule size_t slang start str submethod subset supersede take temp term token trusts try unit unless until when where while with without x xor xx
functions=ACCEPTS AT-KEY EVALFILE EXISTS-KEY Filetests IO STORE abs accept acos acosec acosech acosh acotan acotanh alarm and antipairs asec asech asin asinh atan atan2 atanh base bind binmode bless break caller ceiling chars chdir chmod chomp chop chr chroot chrs cis close closedir codes comb conj connect contains continue cos cosec cosech cosh cotan cotanh crypt dbm defined die do dump each elems eof exec exists exit exp expmod fc fcntl fileno flat flip flock floor fmt fork formats functions get getc getpeername getpgrp getppid getpriority getsock gist glob gmtime goto grep hyper import index int invert ioctl is-prime iterator join keyof keys kill kv last lazy lc lcfirst lines link list listen local localtime lock log log10 lsb lstat map match mkdir msb msg my narrow new next no of open ord ords our pack package pairs path pick pipe polymod pop pos pred print printf prototype push quoting race rand read readdir readline readlink readpipe recv redo ref rename requires reset return reverse rewinddir rindex rmdir roots round samecase say scalar sec sech seek seekdir select semctl semget semop send set setpgrp setpriority setsockopt shift shm shutdown sign sin sinh sleep sockets sort splice split sprintf sqrt srand stat state study sub subst substr substr-rw succ symlink sys syscall system syswrite tan tanh tc tclc tell telldir tie time times trans trim trim-leading trim-trailing truncate uc ucfirst unimatch uniname uninames uniprop uniprops unival unlink unpack unpolar unshift untie use utime values wait waitpid wantarray warn wordcase words write
types_basic=AST Any Block Bool CallFrame Callable Code Collation Compiler Complex ComplexStr Cool CurrentThreadScheduler Date DateTime Dateish Distribution Distribution::Hash Distribution::Locally Distribution::Path Duration Encoding Encoding::Registry Endian FatRat ForeignCode HyperSeq HyperWhatever Instant Int IntStr Junction Label Lock::Async Macro Method Mu Nil Num NumStr Numeric ObjAt Parameter Perl PredictiveIterator Proxy RaceSeq Rat RatStr Rational Real Routine Routine::WrapHandle Scalar Sequence Signature Str StrDistance Stringy Sub Submethod Telemetry Telemetry::Instrument::Thread Telemetry::Instrument::ThreadPool Telemetry::Instrument::Usage Telemetry::Period Telemetry::Sampler UInt ValueObjAt Variable Version Whatever WhateverCode atomicint bit bool buf buf1 buf16 buf2 buf32 buf4 buf64 buf8 int int1 int16 int2 int32 int4 int64 int8 long longlong num num32 num64 rat rat1 rat16 rat2 rat32 rat4 rat64 rat8 uint uint1 uint16 uint2 uint32 uint4 uint64 uint8 utf16 utf32 utf8
types_composite=Array Associative Bag BagHash Baggy Blob Buf Capture Enumeration Hash Iterable Iterator List Map Mix MixHash Mixy NFC NFD NFKC NFKD Pair Positional PositionalBindFailover PseudoStash QuantHash Range Seq Set SetHash Setty Slip Stash Uni utf8
types_domain=Attribute Cancellation Channel CompUnit CompUnit::Repository CompUnit::Repository::FileSystem CompUnit::Repository::Installation Distro Grammar IO IO::ArgFiles IO::CatHandle IO::Handle IO::Notification IO::Path IO::Path::Cygwin IO::Path::QNX IO::Path::Unix IO::Path::Win32 IO::Pipe IO::Socket IO::Socket::Async IO::Socket::INET IO::Spec IO::Spec::Cygwin IO::Spec::QNX IO::Spec::Unix IO::Spec::Win32 IO::Special Kernel Lock Match Order Pod::Block Pod::Block::Code Pod::Block::Comment Pod::Block::Declarator Pod::Block::Named Pod::Block::Para Pod::Block::Table Pod::Defn Pod::FormattingCode Pod::Heading Pod::Item Proc Proc::Async Promise Regex Scheduler Semaphore Supplier Supplier::Preserving Supply Systemic Tap Thread ThreadPoolScheduler VM
types_exceptions=Backtrace Backtrace::Frame CX::Done CX::Emit CX::Last CX::Next CX::Proceed CX::Redo CX::Return CX::Succeed CX::Take CX::Warn Exception Failure X::AdHoc X::Anon::Augment X::Anon::Multi X::Assignment::RO X::Attribute::NoPackage X::Attribute::Package X::Attribute::Required X::Attribute::Undeclared X::Augment::NoSuchType X::Bind X::Bind::NativeType X::Bind::Slice X::Caller::NotDynamic X::Channel::ReceiveOnClosed X::Channel::SendOnClosed X::Comp X::Composition::NotComposable X::Constructor::Positional X::Control X::ControlFlow X::ControlFlow::Return X::DateTime::TimezoneClash X::Declaration::Scope X::Declaration::Scope::Multi X::Does::TypeObject X::Dynamic::NotFound X::Eval::NoSuchLang X::Export::NameClash X::IO X::IO::Chdir X::IO::Chmod X::IO::Copy X::IO::Cwd X::IO::Dir X::IO::DoesNotExist X::IO::Link X::IO::Mkdir X::IO::Move X::IO::Rename X::IO::Rmdir X::IO::Symlink X::IO::Unlink X::Inheritance::NotComposed X::Inheritance::Unsupported X::Method::InvalidQualifier X::Method::NotFound X::Method::Private::Permission X::Method::Private::Unqualified X::Mixin::NotComposable X::NYI X::NoDispatcher X::Numeric::Real X::OS X::Obsolete X::OutOfRange X::Package::Stubbed X::Parameter::Default X::Parameter::MultipleTypeConstraints X::Parameter::Placeholder X::Parameter::Twigil X::Parameter::WrongOrder X::Phaser::Multiple X::Phaser::PrePost X::Placeholder::Block X::Placeholder::Mainline X::Pod X::Proc::Async X::Proc::Async::AlreadyStarted X::Proc::Async::BindOrUse X::Proc::Async::CharsOrBytes X::Proc::Async::MustBeStarted X::Proc::Async::OpenForWriting X::Proc::Async::TapBeforeSpawn X::Proc::Unsuccessful X::Promise::CauseOnlyValidOnBroken X::Promise::Vowed X::Redeclaration X::Role::Initialization X::Scheduler::CueInNaNSeconds X::Seq::Consumed X::Sequence::Deduction X::Signature::NameClash X::Signature::Placeholder X::Str::Numeric X::StubCode X::Syntax X::Syntax::Augment::WithoutMonkeyTyping X::Syntax::Comment::Embedded X::Syntax::Confused X::Syntax::InfixInTermPosition X::Syntax::Malformed X::Syntax::Missing X::Syntax::NegatedPair X::Syntax::NoSelf X::Syntax::Number::RadixOutOfRange X::Syntax::P5 X::Syntax::Perl5Var X::Syntax::Regex::Adverb X::Syntax::Regex::SolitaryQuantifier X::Syntax::Reserved X::Syntax::Self::WithoutObject X::Syntax::Signature::InvocantMarker X::Syntax::Term::MissingInitializer X::Syntax::UnlessElse X::Syntax::Variable::Match X::Syntax::Variable::Numeric X::Syntax::Variable::Twigil X::Temporal X::Temporal::InvalidFormat X::TypeCheck X::TypeCheck::Assignment X::TypeCheck::Binding X::TypeCheck::Return X::TypeCheck::Splice X::Undeclared
adverbs=D a array b backslash c closure delete double exec exists f function h hash heredoc k kv p q qq quotewords s scalar single sym to v val w words ww x

[lexer_properties]
styling.within.preprocessor=1

[settings]
# default extension used when saving files
extension=raku

# MIME type
mime_type=text/x-perl6

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
#comment_open=#`((
#comment_close=))

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
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)

compiler=raku -c "%f"

run_cmd=raku "%f"

# Parse syntax check error messages and warnings, examples:
# Alphanumeric character is not allowed as a delimiter
# at foo.raku:1
error_regex=.*at (.+):([0-9]+).*
