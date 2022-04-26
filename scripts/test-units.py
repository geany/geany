#!/usr/bin/env python3

import os

LANGS = {
	'C': '*.c *.h',
	'CPP': '*.cpp *.hpp',
	'JAVA': '*.java',
	'MAKEFILE': '*.mak',
	'PASCAL': '*.pas',
	'PERL': '*.pl *.pm',
	'PHP': '*.php',
	'PYTHON': '*.py *.pyx',
	'LATEX': '*.tex',
	'ASM': '*.asm',
	'CONF': '*.conf',
	'SQL': '*.sql',
# not in ctags
#	'DOCBOOK': '*.docbook',
	'ERLANG': '*.erl',
	'CSS': '*.css',
	'RUBY': '*.rb',
	'TCL': '*.tcl',
	'SH': '*.sh *.ksh',
	'D': '*.d',
# like f77 - once new parser merged, keep FORTRAN only
#	'FORTRAN': '',
	'GDSCRIPT': '*.gd',
	'DIFF': '*.diff',
	'VHDL': '*.vhd',
	'LUA': '*.lua',
	'JAVASCRIPT': '*.js',
	'HASKELL': '*.hs',
	'CSHARP': '*.cs',
	'FREEBASIC': '*.bas',
	'HAXE': '*.hx',
	'REST': '*.rst',
	'HTML': '*.html',
	'F77': '*.f90 *.f *.for *.f95',
# like C
#	'CUDA': '',
	'MATLAB': '*.m',
# not in ctags
#	'VALA': '*.vala',
	'ACTIONSCRIPT': 'actionscript/*.as',
	'NSIS': '*.nsi',
	'MARKDOWN': '*.md',
	'TXT2TAGS': '*.t2t',
	'ABC': '*.abc',
	'VERILOG': '*.v',
# missing unit tests
#	'R': '',
	'COBOL': 'cobol/*.cbl',
	'OBJC': '*.mm',
	'ASCIIDOC': '*.asciidoc',
	'ABAQUS': '*.inp',
	'RUST': '*.rs',
	'GO': '*.go',
	'JSON': '*.json',
# like PHP
#	'ZEPHIR': '*.zep',
	'POWERSHELL': '*.ps1',
	'JULIA': '*.jl',
# missing unit tests
#	'BIBTEX': '',
# "virtual" parser used by C/C++
#	'CPREPROCESSOR': '',
	'CLOJURE': '*.clj',
	'OCAML': '*.ml',
	'LISP': '*.lisp',
	'TYPESCRIPT': '*.ts',
	'ADA': '*.ads *.adb',
	'RAKU': '*.raku',
	'BATCH': '*.bat',
}

TAGFILE='test_units.tags'

# get all kinds from a tags file
def get_used_kinds():
	with open(TAGFILE, encoding="cp1252") as f:
		lines = f.readlines()
	used = set()
	for line in lines:
		comps = line.split(";\"\t")
		if len(comps) > 1:
			used.add(comps[1][0])
	return used

# get all kinds mapped in tm_parser.c
def get_mapped_kinds(lang):
	with open("src/tagmanager/tm_parser.c") as f:
		lines = f.readlines()
	found_decl = False
	mapped = set()
	for line in lines:
		s = 'static TMParserMapEntry map_' + lang
		if s in line or (lang == 'COMMON_C' and '#define COMMON_C' in line):
			found_decl = True
		elif found_decl:
			if len(line) < 10:
				break
			if line.startswith("\t{'") and 'tm_tag_undef_t' not in line:
				mapped.add(line[3])
	return mapped

# get all kinds mapped in tm_parser.c but not present in the tags file
def get_diff(lang):
	pattern = LANGS[lang]
	os.system('cd tests/ctags && ctags -o ../../' + TAGFILE + ' --kinds-all=* ' + pattern)
	used = get_used_kinds()
	mapped = get_mapped_kinds(lang)
	if lang == 'C' or lang == 'CPP':
		mapped = mapped.union(get_mapped_kinds('COMMON_C'))
	return mapped - used


for lang in LANGS:
	diff = get_diff(lang)
	if len(diff) > 0:
		print(lang + ': ' + str(sorted(list(diff))))
