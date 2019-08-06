/*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains code for generating tags for GIBIANE scripts.
*   Copyright 2019 Thibault Lindecker (Communication & Systemes) - thibault.lindecker@c-s.fr
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "debug.h"
#include "main.h"
#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "keyword.h"
#include "entry.h"
#include "routines.h"
#include <string.h>

typedef enum {
	K_PROCEDURE,
	K_VARIABLE,
	COUNT_KIND
} GIBIANEKind;

static kindDefinition GIBIANEKinds[COUNT_KIND] = {
	{ true, 'p', "procedure",  "procedures" },
	{ true, 'v', "variable" ,  "variables"  }
};

typedef enum eGIBIANELineType {
	LTYPE_UNDETERMINED,
	LTYPE_COMMENT
} lineType;


typedef enum eTokenType {
	TOKEN_UNDEFINED,
	TOKEN_IDENTIFIER,
	TOKEN_EOF,
	TOKEN_SEMICOLON,
	TOKEN_PROCEDURE,
	TOKEN_PROCEDURE_FIN,
	TOKEN_VARIABLE,
	TOKEN_INSTRUCTION
} tokenType;

typedef struct {
	tokenType       type;
	vString *       string;
	unsigned long   lineNumber;
	MIOPos          filePosition;
} tokenInfo;

static void initGIBIANEEntry (tagEntryInfo *const e, const tokenInfo *const token,
							  const GIBIANEKind kind)
{
	initTagEntry (e, vStringValue (token->string), kind);
	e->lineNumber = token->lineNumber;
	e->filePosition = token->filePosition;
}

static void makeTag (const tokenInfo *const token, const vString *const arglist, const vString *const returnlist,
	const GIBIANEKind kind)
{

	if (GIBIANEKinds[kind].enabled)
	{
		tagEntryInfo e;

		initGIBIANEEntry (&e, token, kind);

		if (arglist)
			e.extensionFields.signature = vStringValue (arglist);  //Ce que mange la procedure
		if (returnlist)
			e.extensionFields.varType   = vStringValue (returnlist);

		makeTagEntry (&e);
	}

}

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);
	token->type            = TOKEN_UNDEFINED;
	token->string          = vStringNew ();
	token->lineNumber      = getInputLineNumber ();
	token->filePosition    = getInputFilePosition ();
	return token;
}

static void deleteToken (tokenInfo *const token)
{
	vStringDelete (token->string);
	eFree (token);
}

static void copyToken (tokenInfo *const dest, const tokenInfo *const src)
{
	dest->lineNumber   = src->lineNumber;
	dest->filePosition = src->filePosition;
	dest->type         = src->type;
	vStringCopy (dest->string, src->string);
}

static bool isIdentChar (const int c)
{
	return (isalnum (c) || c == ':' || c == '_' || c == '-' ||
			c >= 0x80 || c == '.' || c == ' ');
}

static bool isSpace (int c)
{
	return (c == '\t' || c == ' ' || c == '\v' || c == '\f');
}

static bool isQuote (int c)
{
	return (c == '\'');
}

static void parseIdentifier (vString *const string, const int firstChar)
{
	if(firstChar == '='){ //parse variable
		vStringPut (string, (char) firstChar); return;
	}

	int c = firstChar;
	do
	{
		vStringPut (string, (char) c);
		c = getcFromInputFile ();
	} while (isIdentChar(c) && c != ';' && c != '=' && !isQuote(c) && c != EOF);

	ungetcToInputFile (c);
}

static int skipSpaceQuote (int c)
{
	while(isSpace(c) || isQuote(c))
		c = getcFromInputFile ();
	return c;
}

static void readToken (tokenInfo *const token)
{
	int c;
	static bool newline = false;
	static bool comment_line = false;
	static bool debut_proc = false;

	token->type   = TOKEN_UNDEFINED;
	vStringClear (token->string);

	c = getcFromInputFile ();
	c = skipSpaceQuote (c);

	/* we ignore comments line (start with '*') */
	if(newline && c != '*'){
		newline = false;
	}

	if(c == '\n' || c == '\r'){
		newline = true;
	}
	else if(c == '*' && newline){
		comment_line = true;
		while(comment_line){
			comment_line = false;
			do
			{
				c = getcFromInputFile ();
			} while (c != '\n' && c != '\r' && c != EOF);
			if(c == '*'){
				comment_line = true;
			}
		}
	}
	else if(c == ';'){
		vStringPut (token->string, (char) c);
		token->type = TOKEN_SEMICOLON;
		debut_proc = false;
	}
	else if(c == EOF){
		token->type = TOKEN_EOF; /*End of file */
	}
	else{
		vString *find_variable = vStringNew ();
		vString *find_procedure = vStringNew ();
		vString *find_instruction = vStringNew ();
		while(1)
		{
			vStringPut (find_variable, (char) c);
			vStringPut (find_procedure, (char) c);
			vStringPut (find_instruction, (char) c);

			c = getcFromInputFile ();
			if(c == '='){
				vStringCat(token->string, find_variable);
				token->type = TOKEN_VARIABLE;
				break;
			}

			/* In GIBIANE, the first 4 letters are enough to call an operator, but user can specify more than 4 letters.
			 * Exemple : for calling 'DEBPROC' operator, user can use : DEBP, DEBPR, DEBPRO and DEBPROC. */
			int keyword_min_size = ((int)find_procedure->length < 4) ? 4 : (int)find_procedure->length;

			if(strncasecmp (vStringValue (find_procedure), "DEBPROC", keyword_min_size) == 0){
				if(isQuote(c) || c == ' '){
					vStringCat(token->string, find_procedure);
					token->type = TOKEN_PROCEDURE;
					debut_proc = true;
					break;
				}
			}else if(strncasecmp (vStringValue (find_procedure), "FINPROC", keyword_min_size) == 0){
				if(isQuote(c) || c == ' ' || c == ';'){
					vStringCat(token->string, find_procedure);
					token->type = TOKEN_PROCEDURE_FIN;
					break;
				}
			}

			if(isQuote(c) || c == ' '){
				vStringClear (find_procedure);
			}

			if((!isIdentChar(c) || c == ';' || c == EOF || c == '\n' || c == '\r' || (debut_proc && c == ' ')) && !isQuote(c)){
				vStringCat(token->string, find_instruction);
				token->type = TOKEN_INSTRUCTION;
				break;
			}
		}
		ungetcToInputFile (c);
		token->lineNumber   = getInputLineNumber ();
		token->filePosition = getInputFilePosition ();

	}

}

/* We enter here because we have detected 'DEBPROC' operator in readToken().
 * We will read the name of the procedur, its arguments and its return values :
 * Exemple :
 *    DEBPROC myprocedure toto*'TABL';
 *      do something
 *    FINPROC titi*'MOTS' tata*'ENTIER'
 * For this example, we would have :
 *  name : myprocedure
 *  arguments : toto*'TABL'
 *  return values : titi*'MOTS' tata*'ENTIER' */
static void parseProcedure (tokenInfo *const token)
{
	tokenInfo *nameFree = NULL;
	bool nextline = false;
	bool finproc  = false;
	vString *returnlist = vStringNew ();
	vString *procedur_name = vStringNew ();
	vString *arglist = vStringNew ();
	vString *espace = vStringNewInit (" ");
	vString *egal = vStringNewInit ("=");

	/* We read the name of the procedur */
	readToken (token);
	nameFree = newToken ();
	copyToken (nameFree, token);

	do
	{
		readToken (token);

		if(token->type == TOKEN_SEMICOLON){
			nextline = true;
			if(finproc == true)
				break;
		}
		/* arguments of the procedur */
		if(nextline == false){
			vStringCat (arglist, token->string);
			vStringCat (arglist, espace);
		}
		/* return values of the procedur */
		if(finproc == true){
			vStringCat (returnlist, token->string);
			vStringCat (returnlist, espace);
		}
		if(token->type == TOKEN_PROCEDURE_FIN)
			finproc = true;

	}
	while (token->type != TOKEN_EOF);

	if(returnlist->length > 0){
		vStringCat (returnlist, egal);
	}

	/* We make tag */
	makeTag(nameFree, arglist, returnlist, K_PROCEDURE);
}

/* We enter here because we have detected '=' char in readToken().
 * Because we are just after the '=' char, we have already read the variable name. */
static void parseVariable (tokenInfo *const token)
{
	/* Make a copy of the token */
	tokenInfo *nameFree = NULL;
	nameFree = newToken ();
	copyToken (nameFree, token);

	/* We make tag */
	makeTag(nameFree, NULL, NULL, K_VARIABLE);

	deleteToken (nameFree);
}

/* main loop on the GIBIANE language file */
static void findGIBIANETags (void)
{
	tokenInfo *const token = newToken ();
	readToken (token);
	while (token->type != TOKEN_EOF)
	{
		switch (token->type)
		{
			case TOKEN_PROCEDURE:
				parseProcedure (token);
				break;

			case TOKEN_VARIABLE:
				parseVariable (token);
				break;

			default: break;
		}
		readToken (token);
	}
	deleteToken (token);
}

extern parserDefinition* GIBIANEParser (void)
{
	static const char *const extensions [] = { "dgibi", "procedur", NULL };
	parserDefinition* def                  = parserNew ("GIBIANE");
	def->kindTable                         = GIBIANEKinds;
	def->kindCount                         = ARRAY_SIZE (GIBIANEKinds);
	def->extensions                        = extensions;
	def->parser                            = findGIBIANETags;
	return def;
}
