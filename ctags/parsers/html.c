/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for HTML language
*   files.
*/

#include "general.h"

#include <string.h>
#include <ctype.h>

#include "entry.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "keyword.h"
#include "promise.h"

/* The max. number of nested elements - prevents further recursion if the limit
 * is exceeded and avoids stack overflow for invalid input containing too many
 * open tags */
#define MAX_DEPTH 1000


typedef enum {
	K_ANCHOR,
	K_HEADING1,
	K_HEADING2,
	K_HEADING3
} htmlKind;


static kindDefinition HtmlKinds [] = {
	{ true, 'a', "anchor",		"named anchors" },
	{ true, 'h', "heading1",	"H1 headings" },
	{ true, 'i', "heading2",	"H2 headings" },
	{ true, 'j', "heading3",	"H3 headings" }
};

typedef enum {
	KEYWORD_h1,
	KEYWORD_h2,
	KEYWORD_h3,
	KEYWORD_a,
	KEYWORD_script,
	KEYWORD_style,
	KEYWORD_name,

	/* void elements */
	KEYWORD_area,
	KEYWORD_base,
	KEYWORD_br,
	KEYWORD_col,
	KEYWORD_command,
	KEYWORD_embed,
	KEYWORD_hr,
	KEYWORD_img,
	KEYWORD_input,
	KEYWORD_keygen,
	KEYWORD_link,
	KEYWORD_meta,
	KEYWORD_param,
	KEYWORD_source,
	KEYWORD_track,
	KEYWORD_wbr
} keywordId;

static const keywordTable HtmlKeywordTable[] = {
	{"h1", KEYWORD_h1},
	{"h2", KEYWORD_h2},
	{"h3", KEYWORD_h3},
	{"a", KEYWORD_a},
	{"script", KEYWORD_script},
	{"style", KEYWORD_style},
	{"name", KEYWORD_name},

	/* void elements */
	{"area", KEYWORD_area},
	{"base", KEYWORD_base},
	{"br", KEYWORD_br},
	{"col", KEYWORD_col},
	{"command", KEYWORD_command},
	{"embed", KEYWORD_embed},
	{"hr", KEYWORD_hr},
	{"img", KEYWORD_img},
	{"input", KEYWORD_input},
	{"keygen", KEYWORD_keygen},
	{"link", KEYWORD_link},
	{"meta", KEYWORD_meta},
	{"param", KEYWORD_param},
	{"source", KEYWORD_source},
	{"track", KEYWORD_track},
	{"wbr", KEYWORD_wbr},
};

typedef enum {
	TOKEN_EOF,
	TOKEN_NAME,			/* tag and attribute names */
	TOKEN_STRING,		/* single- or double-quoted attribute value */
	TOKEN_TEXT,
	TOKEN_TAG_START,	/* <  */
	TOKEN_TAG_START2,	/* </ */
	TOKEN_TAG_END,		/* >  */
	TOKEN_TAG_END2,		/* /> */
	TOKEN_EQUAL,
	TOKEN_COMMENT,
	TOKEN_OTHER
} tokenType;

#ifdef DEBUG
const char *tokenTypes[] = {
#define E(X) [TOKEN_##X] = #X
	E(EOF),
	E(NAME),
	E(STRING),
	E(TEXT),
	E(TAG_START),
	E(TAG_START2),
	E(TAG_END),
	E(TAG_END2),
	E(EQUAL),
	E(COMMENT),
	E(OTHER),
#undef E
};
#endif

typedef struct {
	tokenType type;
	vString *string;
} tokenInfo;


static int Lang_html;


static void readTag (tokenInfo *token, vString *text, int depth);

#ifdef DEBUG
static void dumpToken (tokenInfo *token, const char *context, const char* extra_context)
{
	fprintf (stderr, "[%7s] %-20s@%s.%s\n",
			 tokenTypes[token->type], vStringValue(token->string),
			 context, extra_context? extra_context: "_");
}
#endif

static void readTokenText (tokenInfo *const token, bool collectText)
{
	int c;
	int lastC = 'X';  /* whatever non-space character */

	vStringClear (token->string);

getNextChar:

	c = getcFromInputFile ();

	switch (c)
	{
		case EOF:
			token->type = TOKEN_EOF;
			break;

		case '<':
			ungetcToInputFile (c);
			token->type = TOKEN_TEXT;
			break;

		default:
			if (collectText)
			{
				if (isspace (c))
					c = ' ';
				if (c != ' ' || lastC != ' ')
				{
					vStringPut (token->string, c);
					lastC = c;
				}
			}

			goto getNextChar;
	}
}

static void readToken (tokenInfo *const token, bool skipComments)
{
	int c;

	vStringClear (token->string);

getNextChar:

	c = getcFromInputFile ();
	while (isspace (c))
		c = getcFromInputFile ();

	switch (c)
	{
		case EOF:
			token->type = TOKEN_EOF;
			break;

		case '<':
		{
			int d = getcFromInputFile ();

			if (d == '!')
			{
				d = getcFromInputFile ();
				if (d == '-')
				{
					d = getcFromInputFile ();
					if (d == '-')
					{
						int e = ' ';
						int f = ' ';
						do
						{
							d = e;
							e = f;
							f = getcFromInputFile ();
						}
						while (f != EOF && ! (d == '-' && e == '-' && f == '>'));

						if (skipComments)
							goto getNextChar;
						else
						{
							token->type = TOKEN_COMMENT;
							break;
						}
					}
				}
				ungetcToInputFile (d);
				token->type = TOKEN_OTHER;
			}
			else if (d == '?')
				token->type = TOKEN_OTHER;
			else if (d == '/')
				token->type = TOKEN_TAG_START2;
			else
			{
				ungetcToInputFile (d);
				token->type = TOKEN_TAG_START;
			}
			break;
		}
		case '/':
		{
			int d = getcFromInputFile ();
			if (d == '>')
				token->type = TOKEN_TAG_END2;
			else
			{
				ungetcToInputFile (d);
				token->type = TOKEN_OTHER;
			}
			break;
		}
		case '>':
			token->type = TOKEN_TAG_END;
			break;

		case '=':
			token->type = TOKEN_EQUAL;
			break;

		case '"':
		case '\'':
		{
			const int delimiter = c;
			c = getcFromInputFile ();
			while (c != EOF && c != delimiter)
			{
				vStringPut (token->string, c);
				c = getcFromInputFile ();
			}
			token->type = TOKEN_STRING;
			break;
		}

		default:
		{
			do
			{
				vStringPut (token->string, tolower (c));
				c = getcFromInputFile ();
			}
			while (!isspace (c) && c != '<' && c != '>' && c != '/' &&
				   c != '=' && c != '\'' && c != '"' && c != EOF);
			if (c != EOF)
				ungetcToInputFile (c);
			token->type = TOKEN_NAME;
			break;
		}
	}
}

static void appendText (vString *text, vString *appendedText)
{
	if (text != NULL && vStringLength (appendedText) > 0)
	{
		if (vStringLength (text) > 0 && vStringLast (text) == ' ' &&
			vStringLength (appendedText) > 0 && vStringChar (appendedText, 0) == ' ')
		{
			vStringStripTrailing (text);
		}
		vStringCat (text, appendedText);
	}
}

static bool readTagContent (tokenInfo *token, vString *text, long *line, long *lineOffset, int depth)
{
	tokenType type;

	readTokenText (token, text != NULL);
	appendText (text, token->string);

	do
	{
		*line = getInputLineNumber ();
		*lineOffset = getInputLineOffset ();
		readToken (token, false);
		type = token->type;
		if (type == TOKEN_TAG_START)
			readTag (token, text, depth + 1);
		if (type == TOKEN_COMMENT || type == TOKEN_TAG_START)
		{
			readTokenText (token, text != NULL);
			appendText (text, token->string);
		}
	}
	while (type == TOKEN_COMMENT || type == TOKEN_TAG_START);

	return type == TOKEN_TAG_START2;
}

static bool skipScriptContent (tokenInfo *token, long *line, long *lineOffset)
{
	bool found_start = false;
	bool found_script = false;

	long line_tmp[2];
	long lineOffset_tmp[2];

	tokenType type;

	do
	{
		line_tmp[0] = getInputLineNumber ();
		lineOffset_tmp[0] = getInputLineOffset ();

		readToken (token, false);
		type = token->type;

		if (type == TOKEN_TAG_START2)
		{
			found_start = true;
			line_tmp[1] = line_tmp[0];
			lineOffset_tmp[1] = lineOffset_tmp[0];
		}
		else if (found_start
				 && type == TOKEN_NAME
				 && lookupKeyword (vStringValue (token->string), Lang_html) == KEYWORD_script)
		{
			found_script = true;
			*line = line_tmp[1];
			*lineOffset = lineOffset_tmp[1];
		}
		else
			found_start = false;
	}
	while ((type != TOKEN_EOF) && (!found_script));

	return found_script;
}

static void readTag (tokenInfo *token, vString *text, int depth)
{
	bool textCreated = false;

	readToken (token, true);
	if (token->type == TOKEN_NAME)
	{
		keywordId startTag;
		bool isHeading;
		bool isVoid;

		startTag = lookupKeyword (vStringValue (token->string), Lang_html);
		isHeading = (startTag == KEYWORD_h1 || startTag == KEYWORD_h2 || startTag == KEYWORD_h3);
		isVoid = (startTag >= KEYWORD_area && startTag <= KEYWORD_wbr);
		if (text == NULL && isHeading)
		{
			text = vStringNew ();
			textCreated = true;
		}

		do
		{
			readToken (token, true);
			if (startTag == KEYWORD_a && token->type == TOKEN_NAME)
			{
				keywordId attribute = lookupKeyword (vStringValue (token->string), Lang_html);

				if (attribute == KEYWORD_name)
				{
					readToken (token, true);
					if (token->type == TOKEN_EQUAL)
					{
						readToken (token, true);
						if (token->type == TOKEN_STRING || token->type == TOKEN_NAME)
							makeSimpleTag (token->string, K_ANCHOR);
					}
				}
			}
		}
		while (token->type != TOKEN_TAG_END && token->type != TOKEN_TAG_END2 &&
			   token->type != TOKEN_EOF);

		if (!isVoid && token->type == TOKEN_TAG_END && depth < MAX_DEPTH)
		{
			long startSourceLineNumber = getSourceLineNumber ();
			long startLineNumber = getInputLineNumber ();
			long startLineOffset = getInputLineOffset ();
			long endLineNumber;
			long endLineOffset;
			bool tag_start2;

			if (startTag == KEYWORD_script)
			{
				bool script = skipScriptContent (token, &endLineNumber, &endLineOffset);
				if (script)
					makePromise ("JavaScript", startLineNumber, startLineOffset,
								 endLineNumber, endLineOffset, startSourceLineNumber);
				readToken (token, true);
				goto out;
			}

			tag_start2 = readTagContent (token, text, &endLineNumber, &endLineOffset, depth);
			if (tag_start2)
			{
				readToken (token, true);
				if (isHeading && textCreated && vStringLength (text) > 0)
				{
					keywordId endTag = lookupKeyword (vStringValue (token->string), Lang_html);
					if (startTag == endTag)
					{
						htmlKind headingKind;

						if (startTag == KEYWORD_h1)
							headingKind = K_HEADING1;
						else if (startTag == KEYWORD_h2)
							headingKind = K_HEADING2;
						else
							headingKind = K_HEADING3;

						vStringStripLeading (text);
						vStringStripTrailing (text);
						makeSimpleTag (text, headingKind);
					}
				}
				else if (startTag == KEYWORD_style)
				{
					keywordId endTag = lookupKeyword (vStringValue (token->string), Lang_html);
					if (startTag == endTag)
						makePromise ("CSS", startLineNumber, startLineOffset,
									 endLineNumber, endLineOffset, startSourceLineNumber);
				}

				readToken (token, true);
			}
		}
	}

 out:
	if (textCreated)
		vStringDelete (text);
}

static void findHtmlTags (void)
{
	tokenInfo token;

	token.string = vStringNew ();

	do
	{
		readToken (&token, true);
		if (token.type == TOKEN_TAG_START)
			readTag (&token, NULL, 0);
	}
	while (token.type != TOKEN_EOF);

	vStringDelete (token.string);
}

static void initialize (const langType language)
{
	Lang_html = language;
}

/* parser definition */
extern parserDefinition* HtmlParser (void)
{
	static const char *const extensions [] = { "htm", "html", NULL };
	parserDefinition* def = parserNew ("HTML");
	def->kindTable        = HtmlKinds;
	def->kindCount    = ARRAY_SIZE (HtmlKinds);
	def->extensions   = extensions;
	def->parser       = findHtmlTags;
	def->initialize   = initialize;
	def->keywordTable = HtmlKeywordTable;
	def->keywordCount = ARRAY_SIZE (HtmlKeywordTable);
	return def;
}
