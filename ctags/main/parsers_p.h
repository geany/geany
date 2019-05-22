/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to all language parsing modules.
*
*   To add a new language parser, you need only modify this single input
*   file to add the name of the parser definition function.
*/
#ifndef CTAGS_MAIN_PARSERS_H
#define CTAGS_MAIN_PARSERS_H

#ifdef HAVE_LIBXML
#define XML_PARSER_LIST \
	DbusIntrospectParser, \
	GladeParser,  \
	Maven2Parser, \
	PlistXMLParser, \
	RelaxNGParser, \
	SvgParser, \
	XmlParser, \
	XsltParser
#else
#define XML_PARSER_LIST
#endif

#ifdef HAVE_LIBYAML
#define YAML_PARSER_LIST						\
	YamlParser,									\
	AnsiblePlaybookParser
#else
#define YAML_PARSER_LIST
#endif

#ifdef HAVE_PACKCC
#define PEG_PARSER_LIST						\
	VarlinkParser
#else
#define PEG_PARSER_LIST
#endif

/* Add the name of any new parser definition function here */
#define PARSER_LIST \
	AdaParser, \
	AntParser, \
	AsciidocParser, \
	AsmParser, \
	AspParser, \
	AutoconfParser, \
	AutoItParser, \
	AutomakeParser, \
	AwkParser, \
	BasicParser, \
	BetaParser, \
	ClojureParser, \
	CMakeParser, \
	CParser, \
	CppParser, \
	CPreProParser, \
	CssParser, \
	CsharpParser, \
	CtagsParser, \
	CobolParser, \
	CUDAParser, \
	DParser, \
	DiffParser, \
	DtdParser, \
	DTSParser, \
	DosBatchParser, \
	EiffelParser, \
	ElixirParser, \
	ElmParser, \
	ErlangParser, \
	FalconParser, \
	FlexParser, \
	FortranParser, \
	FyppParser,	   \
	GdbinitParser, \
	GoParser, \
	HtmlParser, \
	IniconfParser, \
	InkoParser, \
	ITclParser, \
	JavaParser, \
	JavaPropertiesParser, \
	JavaScriptParser, \
	JsonParser, \
	LdScriptParser, \
	LispParser, \
	LuaParser, \
	M4Parser, \
	ManParser, \
	MakefileParser, \
	MarkdownParser, \
	MatLabParser, \
	MooseParser, \
	MyrddinParser, \
	ObjcParser, \
	OldCppParser, \
	OldCParser, \
	OcamlParser, \
	PasswdParser, \
	PascalParser, \
	PerlParser, \
	Perl6Parser, \
	PhpParser, \
	PodParser, \
	ProtobufParser, \
	PuppetManifestParser, \
	PythonParser, \
	PythonLoggingConfigParser, \
	QemuHXParser, \
	QtMocParser, \
	RParser, \
	RSpecParser, \
	RexxParser, \
	RobotParser, \
	RpmSpecParser, \
	RstParser, \
	RubyParser, \
	RustParser, \
	SchemeParser, \
	ShParser, \
	SlangParser, \
	SmlParser, \
	SqlParser, \
	SystemdUnitParser, \
	SystemTapParser, \
	TclParser, \
	TclOOParser, \
	TexParser, \
	TTCNParser, \
	TypeScriptParser, \
	VeraParser, \
	VerilogParser, \
	SystemVerilogParser, \
	UCtagsAspellParser, \
	VhdlParser, \
	VimParser, \
	WindResParser, \
	YaccParser, \
	YumRepoParser, \
	ZephirParser

#endif  /* CTAGS_MAIN_PARSERS_H */
