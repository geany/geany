#include "general.h"    /* always include first */
#include "entry.h"
#include "parse.h"      /* always include */
#include "read.h"       /* to define file fileReadLine() */

#include <string.h>     /* to declare strxxx() functions */

typedef struct QMLTag {
	boolean			set; // Is tag set?
	boolean 		supported; // Is tag supported?
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
	vStringDelete(tag->name);
	vStringDelete(tag->id);
	free(tag);
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
void createTag(QMLTag *tag) {
	tagEntryInfo entry;
	initTagEntry(&entry, vStringValue(tag->name));

	// If cur is QML_OBJECT, and ID is not empty, then set name with ID
	if(tag->kind == QML_OBJECT && strcmp(tag->id->buffer, "") != 0) {
		vStringCatS(tag->name, ": ");
		vStringCat(tag->name, tag->id);
	}

	entry.lineNumber	=	tag->line_num;
	entry.filePosition	=	tag->file_pos;
	entry.kindName		=	QMLKinds[tag->kind].name;
	entry.kind			=	QMLKinds[tag->kind].letter;

	makeTagEntry(&entry);
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
	QMLTag *cur						=	qmlTagNew();
	boolean is_multiline_comment	=	FALSE;

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

		// Check for QML Object or JS Function
		// If '{' in line, then there might be a QML Object or JS Function
		if(strchr(line, '{') != NULL) {
			// If cur->set, then we are already in a tag, so create child and make cur
			if(cur->set) {
				QMLTag *child	=	qmlTagNew();
				cur->child		=	child;
				child->parent	=	cur;
				cur				=	child;
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
				while(i <= strlen(line)) {
					static int words = 0;

					if(isspace(line[i])) words++; // If we hit a space, inc num of words
					// After if(), inc i (for parent loop and incing past ':')
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
			cur->set = TRUE;
			getFirstWord(&line, cur->name);
			cur->line_num = File.lineNumber;
			cur->file_pos = File.filePosition;

			while(isspace(*line)) line++; // Skip whitespace after word

			// If '{' is after word and any whitespace, then tag is a QML object
			if(*line == '{') {
				cur->kind = QML_OBJECT;
				cur->supported = TRUE;
			}
		}

		// Check for ID
		if(strstr(line, "id:") != NULL) {
			// If cur is an active tag, set ID
			if(cur->set) getId(line, cur->id);
			// Else set ID to parent
			else getId(line, cur->parent->id);
		}

		// Check for '}' to close tag
		if(strchr(line, '}')) {
			QMLTag *tmp = cur; // Store cur while we switch to next or child

			// If in tag, then check if supported to create, then switch to cur->next
			if(cur->set) {
				// If kind is supported, then create tag
				if(cur->supported) createTag(cur);

				// Then move cur to next
				tmp->next	=	qmlTagNew();
				cur			=	tmp->next;
				cur->parent	=	tmp->parent;
			}

			// Else we must have switched to cur->next but there is no next, just another '}'
			// Therefore we need to create a tag for parent (if it's supported).
			// Then switch to parent->next
			else {
				if(cur->parent->supported) createTag(cur->parent);
				cur->parent->next	=	qmlTagNew();
				cur 				=	tmp->parent->next;
				cur->parent			=	tmp->parent->parent;
			 }

			qmlTagDelete(tmp); // Delete QMLTag and free it's memory
		}
	}
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