/*
*   Copyright (c) 2019, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Declares parsers used by ctags
*/
#ifndef TM_PARSERS_H
#define TM_PARSERS_H

/* This file is included by ctags by defining EXTERNAL_PARSER_LIST_FILE inside
 * ctags/Makefile.am */

/* Keep in sync with tm_parser.h */
#define EXTERNAL_PARSER_LIST \
	CParser, \
	CppParser, \
	JavaParser, \
	MakefileParser, \
	PascalParser, \
	PerlParser, \
	PhpParser, \
	PythonParser, \
	TexParser, \
	AsmParser, \
	IniconfParser, \
	SqlParser, \
	DocBookParser, \
	ErlangParser, \
	CssParser, \
	RubyParser, \
	TclParser, \
	ShParser, \
	DParser, \
	FortranParser, \
	GDScriptParser, \
	DiffParser, \
	VhdlParser, \
	LuaParser, \
	JavaScriptParser, \
	HaskellParser, \
	CsharpParser, \
	BasicParser,\
	HaxeParser,\
	RstParser, \
	HtmlParser, \
	AdaParser, \
	CUDAParser, \
	MatLabParser, \
	ValaParser, \
	FlexParser, \
	NsisParser, \
	MarkdownParser, \
	Txt2tagsParser, \
	AbcParser, \
	VerilogParser, \
	RParser, \
	CobolParser, \
	ObjcParser, \
	AsciidocParser, \
	AbaqusParser, \
	RustParser, \
	GoParser, \
	JsonParser, \
	ZephirParser, \
	PowerShellParser, \
	JuliaParser, \
	BibtexParser, \
	CPreProParser, \
	TclOOParser, \
	ClojureParser, \
	LispParser, \
	TypeScriptParser, \
	DosBatchParser, \
	AutoItParser, \
	Perl6Parser, \
	OcamlParser, \
	LdScriptParser, \
	ForthParser, \
	MesonParser, \
	SystemVerilogParser

#endif
