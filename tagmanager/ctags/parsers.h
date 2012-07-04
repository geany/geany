/*
*
*   Copyright (c) 2000-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   External interface to all language parsing modules.
*
*   To add a new language parser, you need only modify this single source
*   file to add the name of the parser definition function.
*/
#ifndef _PARSERS_H
#define _PARSERS_H

/* Add the name of any new parser definition function here */
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
    ObjcParser
/*
langType of each parser
 0	CParser
 1	CppParser
 2	JavaParser
 3	MakefileParser
 4	PascalParser
 5	PerlParser
 6	PhpParser
 7	PythonParser
 8	LaTeXParser
 9	AsmParser
10	ConfParser
11	SqlParser
12	DocBookParser
13	CssParser
14	RubyParser
15	TclParser
16	ShParser
17	DParser
18	FortranParser
19	FeriteParser
20	DiffParser
21	VhdlParser
22	LuaParser
23	JavaScriptParser
24	HaskellParser
25	CsharpParser
26	FreeBasicParser
27  HaxeParser
28  RestParser
29  HtmlParser
30  F77Parser
31  GLSLParser
32  MatlabParser
33  ValaParser
34  ActionScriptParser
35  NsisParser
36  MarkdownParser
37  Txt2tagsParser
38  AbcParser
39  Verilog
40	RParser
41	CobolParser
42	ObjcParser
*/
#endif	/* _PARSERS_H */

/* vi:set tabstop=8 shiftwidth=4: */
