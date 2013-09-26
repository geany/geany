#include "general.h"    /* always include first */
#include "entry.h"
#include "parse.h"      /* always include */
#include "read.h"       /* to define file fileReadLine() */

#include <string.h>     /* to declare strxxx() functions */

typedef struct {
	vString			*id;
	vString			*name;
	unsigned long	line_num;
	MIOPos			file_pos;
	int				kind;
} QMLObject;

typedef enum {
	QML_OBJECT,
	JS_FUNCTION,
	KIND_COUNT
} QMLKind;

static kindOption QMLKinds[KIND_COUNT] = {
	{ TRUE,	'o',	"other",	"objects"	},
	{ TRUE, 'f',	"function",	"functions"	}
};

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

	if(found) line = NULL;

	return line;
}

void createQMLObjectTag(QMLObject *qml_obj, boolean *is_qml_object) {
	tagEntryInfo entry;
	initTagEntry(&entry, vStringValue(qml_obj->name));

	entry.lineNumber	=	qml_obj->line_num;
	entry.filePosition	=	qml_obj->file_pos;
	entry.kindName		=	QMLKinds[qml_obj->kind].name;
	entry.kind			=	QMLKinds[qml_obj->kind].letter;

	makeTagEntry(&entry);
	vStringClear(qml_obj->name);
	vStringDelete(qml_obj->name);

	*is_qml_object = FALSE;
}

vString* getName(const unsigned char **str_ptr) {
	vString *name = vStringNew();
	const unsigned char *str = *str_ptr;

	while(isalnum(*str) || *str == '_') {
		vStringPut(name, *str);
		str++;
	}

	*str_ptr = str;
	vStringTerminate(name);
	return name;
}

// Get ID from str, then append ": ID" to name and return
vString *getId(const unsigned char *str, vString *name) {
	vString *id = vStringNew();

	while(strstr(str, "id:") != NULL) {
		if(strncmp(str, "id:", 3) == 0) {
			str += 3;
			while(isspace(*str)) str++;
			id = getName(&str);
			vStringCatS(name, ": ");
			vStringCat(name, id);
		}

		else str++;
	}

	vStringClear(id);
	vStringDelete(id);

	return name;
}

static void findQMLTags(void) {
	QMLObject *qml_obj			 =	malloc(sizeof(QMLObject));
	boolean is_multiline_comment =	FALSE;
	boolean is_qml_object		 =	FALSE;
	const unsigned char *line;

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

		// If in middle of QML Object { } check for ID and '}'
		if(is_qml_object && (strstr(line, "id:") != NULL || strchr(line, '}') != NULL)) {
			if(strstr(line, "id:") != NULL)
				qml_obj->name = getId(line, qml_obj->name);

			else createQMLObjectTag(qml_obj, &is_qml_object);
		}

		// If '{' in line, but '(' not in line, then there might be a QML Object
		if(strchr(line, '{') != NULL && strchr(line, '(') == NULL) {
			int i = 0;
			while(isspace(*line)) line++; // Increment ptr past any whitespace before 'Object {'
			// Search past whole word of 'Object'
			while(!isspace(line[i])) {
				if(line[i] == '{' || line[i] == ':') break;
				i++;
			}

			while(isspace(line[i])) i++; // Search past any space between 'Object' and '{'

			// If '{' is after potential Object name and any whitespace, prepare tag to be added
			if(line[i] == '{') {
				qml_obj->name		 =	getName(&line);
				qml_obj->line_num	 =	File.lineNumber;
				qml_obj->file_pos	 =	File.filePosition;
				qml_obj->kind		 =	QML_OBJECT;
				is_qml_object		 =	TRUE;

				// If ID is on same line
				if(strstr(line, "id:") != NULL) {
					while(strncmp(line, "id:", 3) != 0) line++;
					qml_obj->name = getId(line, qml_obj->name);
				}

				// If single line Object
				if(strchr(line, '}') != NULL)
					createQMLObjectTag(qml_obj, &is_qml_object);
			}
		}

		// If line contains a function
		// Using while to prepare for unlikely event that "function" appears more than once in line
		while(strstr(line, "function") != NULL) {
			while(strncmp(line, "function", 8) != 0) line++;

			// If whitespace is after "function"
			if(isspace(line[8])) {
				line += 8; // Skip past "function"

				while(isspace(*line)) line++;

				vString *name = getName(&line);

				while(isspace(*line)) line++;

				// If '(' is after "function" and whitespace, then add tag as JS_FUNCTION
				if(*line == '(') makeSimpleTag(name, QMLKinds, JS_FUNCTION);

				vStringClear(name);
				vStringDelete(name);

				break;
			}

			else line++;
		}
	}

	free(qml_obj);
}

extern parserDefinition *QMLParser(void) {
	parserDefinition* def = parserNew("QML");
	static const char *const extensions [] = { "qml", NULL };

	def->kinds		=	QMLKinds;
	def->kindCount	=	KIND_COUNT(QMLKinds);
	def->extensions	=	extensions;
	def->parser		=	findQMLTags;
	def->regex		=	FALSE;

	return def;
}