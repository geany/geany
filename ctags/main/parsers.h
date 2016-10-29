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

/* Add the name of any new parser definition function here */
/* keep src/tagmanager/tm_parser.h in sync */
#define PARSER_LIST \
	CParser, \
	CppParser, \
	JavaParser, \
	MakefileParser, \
	PascalParser, \
	PerlParser, \
	PhpParser, \
	PythonParser, \
	LaTeXParser, \
	AsmParser, \
	ConfParser, \
	SqlParser, \
	DocBookParser, \
	ErlangParser, \
	CssParser, \
	RubyParser, \
	TclParser, \
	ShParser, \
	DParser, \
	FortranParser, \
	FeriteParser, \
	DiffParser, \
	VhdlParser, \
	LuaParser, \
	JavaScriptParser, \
	HaskellParser, \
	CsharpParser, \
	FreeBasicParser,\
	HaxeParser,\
	RestParser, \
	HtmlParser, \
	F77Parser, \
	GLSLParser, \
	MatlabParser, \
	ValaParser, \
	ActionScriptParser, \
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
	PowerShellParser

#endif  /* CTAGS_MAIN_PARSERS_H */
