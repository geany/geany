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
	{ TRUE, 'f',	"function",	"functions"		}
};

/*
 * Malloc and return initialized QMLTag ptr
 */
QMLTag* qmlTagNew() {
	QMLTag *tag	=	malloc(sizeof(QMLTag));
	tag->set	=	FALSE;
	tag->added	=	FALSE;
	tag->name	=	vStringNew();
	tag->id		=	vStringNew();
	tag->kind	=	-1;
	tag->parent	=	NULL;
	tag->child	=	NULL;
	tag->prev	=	NULL;
	tag->next	=	NULL;

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
		entry.extensionFields.scope[0] = QMLKinds[tag->parent->kind].name;
		entry.extensionFields.scope[1] = vStringValue(tag->parent->name);
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
		// If tag is not supported, set added to TRUE so it skips it later (even though it's a lie;)
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
 * Line is incremented past word.
 */
void getFirstWord(const unsigned char **line, vString *word) {
	while(isspace(**line)) (*line)++; // First skip any whitespace
	// Next copy all alphanumerics and '_' to word
	while(isalnum(**line) || **line == '_' || **line == '.') {
		vStringPut(word, **line);
		(*line)++;
	}

	vStringTerminate(word);
}

/*
 *  Get ID from line and store in vString *id
 */
void getId(const unsigned char *line, vString *id) {
	while(strstr(line, "id:") != NULL) {
		if(strncmp(line, "id:", 3) == 0) {
			line += 3; // Skip past "id:" to word
			getFirstWord(&line, id);
		}

		else line++;
	}
}

/*
 * Scan line by line for QML tags
 */
static void findTags(void) {
	const unsigned char *line		=	NULL;
	QMLTag *const root				=	qmlTagNew();
	QMLTag *cur						=	root;
	boolean is_multiline_comment	=	FALSE;

//AFTER REFRESHING SYMBOL LIST NULL ITEMS ARE CREATED, FIND OUT WHYYYYY!!!!!!!!!
	// Main loop
	while((line = fileReadLine()) != NULL) {
		// If in middle of multiline comment, skip through till end
		if(is_multiline_comment) {
			if((line = skipMultilineComment(line)) == NULL) continue;
			else is_multiline_comment = FALSE;
		}

		// Skip whitespace and comments
		while(isspace(*line)) line++;
		if(strncmp(line, "//", 2) == 0) continue;
		if(strncmp(line, "/*", 2) == 0) {
			is_multiline_comment = TRUE;
			if((line = skipMultilineComment(line)) == NULL) continue;
			else is_multiline_comment = FALSE;
		}

		// Check for '}' to close tag
		if(strchr(line, '}')) {
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

			continue;
		}

		// Check for QML Object or JS Function
		// If '{' in line, then there might be a QML Object or JS Function
		if(strchr(line, '{') != NULL) {
			// If cur->set, then we are already in a tag, so create child and switch to it
			if(cur->set) {
				cur->child			=	qmlTagNew();
				cur->child->parent	=	cur;
				cur					=	cur->child;
			}

			// Skip whitespace and check for "function"
			while(isspace(*line)) line++;
			if(strncmp(line, "function", 8) == 0) {
				line += 8;
				cur->kind = JS_FUNCTION;
				cur->supported = TRUE;
			}

			// Else check for ':' and '{'
			else if(strchr(line, ':') != NULL && strchr(line, '{') != NULL) {
				int i = 0;
				int words = 0;
				while(i <= strlen(line)) {
					if(isspace(line[i])) words++; // If we hit a space, inc num of words
					// After if(line[i] == ':'), inc i (for parent loop and incing past ':')
					if(line[i++] == (unsigned char)':' && words <= 1) {
						while(isspace(line[i])) i++;
						if(line[i] == (unsigned char)'{') {
							cur->kind = JS_FUNCTION;
							cur->supported = TRUE;
						}
					}
				}
			}

			// Get info for tag
			getFirstWord(&line, cur->name);
			cur->line_num	=	File.lineNumber;
			cur->file_pos	=	File.filePosition;
			cur->set		=	TRUE;

			while(isspace(*line)) line++; // Skip whitespace after word

			// If '{' is after word and any whitespace, then tag is a QML object
			if(*line == '{') {
				cur->kind		=	QML_OBJECT;
				cur->supported	=	TRUE;
			}
		}

		// Check for ID
		if(strstr(line, "id:") != NULL) {
			// If cur is an active tag, set ID
			if(cur->set) getId(line, cur->id);
			// Else set ID to parent
			else getId(line, cur->parent->id);
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