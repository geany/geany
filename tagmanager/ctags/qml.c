/*
 *	Copyright (c) 2013, Tory Gaurnier
 *
 *	This source code is released for free distribution under the terms of the
 *	GNU General Public License.
 *
 *	This module creates tags for QML source files.
 */

#include "general.h"    /* always include first */
#include "entry.h"
#include "parse.h"      /* always include */
#include "read.h"       /* to define file fileReadLine() */

#include <string.h>     /* to declare strxxx() functions */


typedef struct QMLTag {
	boolean			set; // Is tag set?
	boolean 		supported; // Is tag supported?
	boolean			added; // Has the tag already been created?
	vString			*id;
	vString			*name;
	unsigned long	line_num;
	MIOPos			file_pos;
	int				kind;
	struct QMLTag	*parent;
	struct QMLTag	*child;
	struct QMLTag	*prev;
	struct QMLTag	*next;
} QMLTag;


typedef enum QMLKind {
	QML_OBJECT,
	JS_FUNCTION
} QMLKind;


static kindOption QMLKinds[] = {
	{ TRUE,	'o',	"other",	"objects"	},
	{ TRUE, 'f',	"function",	"functions"	}
};


/*
 * Malloc and return initialized QMLTag ptr
 */
QMLTag* qmlTagNew() {
	QMLTag *tag		=	malloc(sizeof(QMLTag));
	tag->set		=	FALSE;
	tag->added		=	FALSE;
	tag->supported	=	FALSE;
	tag->name		=	vStringNew();
	tag->id			=	vStringNew();
	tag->kind		=	-1;
	tag->parent		=	NULL;
	tag->child		=	NULL;
	tag->prev		=	NULL;
	tag->next		=	NULL;

	return tag;
}


QMLTag* qmlTagSet(QMLTag *tag, vString *word, QMLKind kind) {
	if(tag->set) {
		tag->child			=	qmlTagNew();
		tag->child->parent	=	tag;
		tag					=	tag->child;
	}

	vStringCopy(tag->name, word);
	tag->kind		=	kind;
	tag->line_num	=	File.lineNumber;
	tag->file_pos	=	File.filePosition;
	tag->set		=	TRUE;
	tag->supported	=	TRUE;

	return tag;
}


/*
 * Free all memory from tag
 */
void qmlTagDelete(QMLTag *tag) {
	// Free vStrings
	vStringDelete(tag->name);
	vStringDelete(tag->id);

	// Make links point to NULL
	if(tag->parent != NULL && tag->parent->child == tag) tag->parent->child = NULL;
	if(tag->prev != NULL) tag->prev->next = NULL;

	// Free QMLTag
	free(tag);
}


/*
 * Iterate through QMLTag linked list freeing each
 */
void freeQMLTags(QMLTag *tag) {
	while(tag != NULL) {
		// Else if tag has child, recurse down
		if(tag->child != NULL) tag = tag->child;

		// If tag has next sibling, move to it
		else if(tag->next != NULL) tag = tag->next;

		// Else delete tag and move back or up
		else {
			// If prev exists, delete and move back
			if(tag->prev != NULL) {
				QMLTag *prev = tag->prev;
				qmlTagDelete(tag);
				tag = prev;
			}

			// Else delete tag and move up to parent
			else {
				QMLTag *parent = tag->parent;
				qmlTagDelete(tag);
				tag = parent;
			}
		}
	}
}


/*
 * Skip past unsupported block, returning remainder of line, or NULL if end not found within line.
 * unsigned int *i is a pointer to in_unsupported_block, stores number of recursions in unsupported
 * blocks (for example, recursive while loops), and acts as a boolean.
 */
const unsigned char* skipUnsupportedBlock(const unsigned char *line, unsigned int *i) {
	while(*i > 0 && *line != '\0') {
		switch(*line) {
			case '}':
				(*i)--;
				break;

			case '{':
				(*i)++;
				break;
		}

		line++;
	}

	// If end of unsupported blocks is found return remainder of line
	if(!*i) return line;

	else return NULL;
}


/*
 * Skip past multiline comment in line, returning remainder of line, or NULL if end not found
 */
const unsigned char* skipMultilineComment(const unsigned char *line) {
	boolean found = FALSE;

	while(*line != '\0') {
		if(strncmp(line, "*/", 2) == 0) {
			line += 2;
			found = TRUE;
			break;
		}

		line++;
	}

	// If end of comment is found return remainder of line
	if(found) return line;

	else return NULL;
}


/*
 * Create QML tag with current values
 */
void makeTag(QMLTag *tag) {
	tagEntryInfo entry;

	// If cur is QML_OBJECT, and ID is not empty, then set name with ID
	if(tag->kind == QML_OBJECT && strcmp(tag->id->buffer, "") != 0) {
		vStringCatS(tag->name, ": ");
		vStringCat(tag->name, tag->id);
	}

	initTagEntry(&entry, vStringValue(tag->name));
	entry.lineNumber	=	tag->line_num;
	entry.filePosition	=	tag->file_pos;
	entry.isFileScope	=	TRUE;
	entry.kindName		=	QMLKinds[tag->kind].name;
	entry.kind			=	QMLKinds[tag->kind].letter;

	if(tag->parent != NULL) {
		QMLTag *cur = tag->parent;
		vString *scope = vStringNew();
		vStringCopy(scope, cur->name);

		while((cur = cur->parent) != NULL) {
			vString *tmp = vStringNew();
			vStringCopy(tmp, scope);
			vStringCopy(scope, cur->name);
			vStringCatS(scope, ".");
			vStringCat(scope, tmp);
			vStringDelete(tmp);
		}

		entry.extensionFields.scope[0] = vStringValue(tag->name);
		entry.extensionFields.scope[1] = vStringValue(scope);
	}

	tag->added = TRUE;
	makeTagEntry(&entry);
}


/*
 * Iterate through QMLTag linked list, creating CTags tag for each.
 * Create tags in order from parent to child, from left to right.
 */
void makeTags(QMLTag *tag) {
	while(tag != NULL) {
		// If tag is supported and hasn't been added, then create it
		if(tag->supported && !tag->added) makeTag(tag);
		// If tag's not supported, set added to TRUE so it skips it later (even though it's a lie ;)
		else if(!tag->supported) tag->added = TRUE;
		// If tag has children that haven't already been added, recurse down
		if(tag->child != NULL && !tag->child->added) tag = tag->child;
		// Else if tag has next siblings that hasn't been added, move to it
		else if(tag->next != NULL && !tag->next->added) tag = tag->next;
		// Else move back to parent
		else tag = tag->parent;
	}
}


/*
 * Copy alphanumeric chars from line to word until non-alphanumeric is reached.
 * Line is incremented past word and whitespace after word.
 */
void getNextWord(const unsigned char **line, vString *word) {
	while(isspace(**line)) (*line)++; // First skip any whitespace
	// Next copy all alphanumerics and '_' to word
	while(isalnum(**line) || **line == '_' || **line == '.') {
		// If we have a period and it's not a signal function/expression, then don't add '.'
		// Signal function/expression will have "on" and then a capital letter
		if(**line == '.' && !(strncmp(*line, ".on", 3) == 0 && isupper((*line)[3])))
			vStringPut(word, ' ');

		else vStringPut(word, **line);
		(*line)++;
	}

	while(isspace(**line)) (*line)++; // Next skip any trailing whitespace

	vStringTerminate(word);
}


/*
 * Scan line by line for QML tags
 */
static void findTags(void) {
	const unsigned char *line			=	NULL;
	QMLTag *const root					=	qmlTagNew();
	QMLTag *cur							=	root;
	boolean in_multiline_comment		=	FALSE; // True if we are inside a multiline comment
	unsigned int in_unsupported_block	=	0; // > 0 if we are inside an unsupported block
	vString *word						=	vStringNew();

	// Main loop
	while((line = fileReadLine()) != NULL) {
		// Loop through line until end of line is reached
		while(*line != '\0') {
			// If in middle of multiline comment, skip through till end
			if(in_multiline_comment) {
				if((line = skipMultilineComment(line)) == NULL) break;
				else in_multiline_comment = FALSE;
			}

			// Skip whitespace and comments
			while(isspace(*line)) line++;
			if(strncmp(line, "//", 2) == 0) {
				line = NULL;
				break;
			}

			// Check if start of multiline comment
			if(strncmp(line, "/*", 2) == 0) {
				in_multiline_comment = TRUE;
				if((line = skipMultilineComment(line)) == NULL) break;
				else in_multiline_comment = FALSE;
			}

			// If in unsupported block, skip through till end
			if(in_unsupported_block)
				if((line = skipUnsupportedBlock(line, &in_unsupported_block)) == NULL) break;

			// Get next word in line
			getNextWord(&line, word);

			// If word is not blank, check for supported tags
			if(vStringLength(word) > 0) {
				// If word is "function", then there is a Javascript function
				if(strcmp(vStringValue(word), "function") == 0)
					cur = qmlTagSet(cur, word, JS_FUNCTION);

				// If ':' is next in line, check for function or ID
				else if(*line == ':') {
					line++; // Increment past ':'

					// If word is "id" then set ID
					if(strcmp(vStringValue(word), "id") == 0) {
						// If cur is an active tag, set ID
						if(cur->set) getNextWord(&line, cur->id);
						// Else set ID to parent
						else if(cur->parent != NULL)
							getNextWord(&line, cur->parent->id);
					}

					// Else check for function
					else {
						while(isspace(*line)) line++;
						// If '{' is next in line, then we have a signal function or a function
						// bound to a property
						if(*line == '{') {
							line++; // Inc past '{'
							cur = qmlTagSet(cur, word, JS_FUNCTION);
						}
					}
				}

				// If '{' is next in line, then there may be a QML Object
				else if(*line == '{') {
					line++; // Inc past '{'
					// If word is "else", then we are in an unsupported block
					if(strcmp(vStringValue(word), "else") == 0) {
						in_unsupported_block++;
						if((line = skipUnsupportedBlock(line, &in_unsupported_block)) == NULL) {
							vStringClear(word);
							break;
						}
					}

					// Else we are in a QML Object
					else cur = qmlTagSet(cur, word, QML_OBJECT);
				}
			}

			// Else if word is blank, and '{' is next, then we are likely following a () from an
			// unsupported block
			else if(*line == '{') {
				line++; // Increment past '{'
				in_unsupported_block++;
				if((line = skipUnsupportedBlock(line, &in_unsupported_block)) == NULL) {
					vStringClear(word);
					break;
				}
			}

			// Else if word is blank, check for '}' to close tag
			else if(*line == '}') {
				line++; // Skip past '}'

				// If in tag switch to cur->next
				if(cur->set) {
					cur->next			=	qmlTagNew();
					cur->next->prev		=	cur;
					cur->next->parent	=	cur->parent;
					cur					=	cur->next;
				}

				// Else we must have switched to cur->next but there is no next, just another '}'
				// Delete blank tag and switch to parent->next
				else if(cur->parent != NULL) {
					QMLTag *tmp = cur;
					cur->parent->next			=	qmlTagNew();
					cur->parent->next->parent	=	cur->parent->parent;
					cur->parent->next->prev		=	cur->parent;
					cur							=	cur->parent->next;
					qmlTagDelete(tmp);
				}
			}

			// Else increment line by one character to avoid infinite loop on unrecognized chars
			else line++;

			vStringClear(word);
		}
	}

	// When finished building QMLTags linked list, make sure root->next is NULL
	// Then create tags, and free QMLTags
	if(root->next != NULL) {
		qmlTagDelete(root->next);
		root->next = NULL;
	}

	makeTags(root);
	freeQMLTags(root);
}


/*
 * QML parser definition
 */
extern parserDefinition *QMLParser(void) {
	parserDefinition* def = parserNew("QML");
	static const char *const extensions [] = { "qml", NULL };

	def->kinds		=	QMLKinds;
	def->kindCount	=	KIND_COUNT(QMLKinds);
	def->extensions	=	extensions;
	def->parser		=	findTags;
	def->regex		=	FALSE;

	return def;
}