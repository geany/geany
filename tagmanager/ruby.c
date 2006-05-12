/*
*   $Id: ruby.c,v 1.2 2001/12/18 04:30:18 darren Exp $
*
*   Copyright (c) 2000-2001, Thaddeus Covert <sahuagin@mediaone.net>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Ruby language
*   files.
*
*   Copyright (c) 2002, Matthias Veit <matthias_veit@yahoo.de>
*   Enable parsing of class (in ruby: singleton) methods and mixins.
*
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_CLASS, K_METHOD, K_SINGLETON, K_MIXIN, K_VARIABLE, K_MEMBER
} rubyKind;

static kindOption RubyKinds [] = {
                                     { TRUE, 'c', "class",    "classes" },
                                     { TRUE, 'f', "function", "methods" },
                                     { TRUE, 'm', "member", "singleton_methods" },
                                     { TRUE, 'd', "macro",    "mixins" },
                                     { TRUE, 'v', "variable",  "variable" },
                                     { TRUE, 's', "struct",  "member" }
                                 };


/*
*   FUNCTION DEFINITIONS
*/

static void findRubyTags (void) {
    vString *name = vStringNew ();
    const unsigned char *line;
    boolean inMultilineString = FALSE;

    while ((line = fileReadLine ()) != NULL) {
        const unsigned char *cp = line;
        boolean is_singleton = FALSE;

        while (*cp != '\0')
        {
            if (*cp=='"' &&
                    strncmp ((const char*) cp, "\"\"\"", (size_t) 3) == 0) {
                inMultilineString = (boolean) !inMultilineString;
                cp += 3;
            }
            /* make sure you include the comments */
            if(*cp=='=' && strncmp((const char*)cp, "==begin", (size_t)7) == 0) {
                inMultilineString = (boolean)!inMultilineString;
                cp +=3;
            }
            /* mark the end of a comment */
            if( *cp=='=' && strncmp((const char*)cp, "==end", (size_t)5) == 0) {
                inMultilineString = (boolean)0;
                cp+=5;
            }
            if (inMultilineString  ||  isspace ((int) *cp))
                ++cp;
            else if (*cp == '#')
                break;
            else if (*cp == '=' && *(cp+1) != '=' && (isspace((int) *(cp-1)) || isalnum((int) *(cp-1)))) {

            	// try to detect a variable by the = sign - enrico
            	// exlude all what not look like ' = ' or 'l=b'
            	const unsigned char *cur_pos = cp;  // store the current position because
            	                                    // we are going backwards with cp
            	                                    // (think about what happens if we don't do this ;-))
            	int is_member = 0;
            	cp--;

            	while (isspace ((int) *cp)) --cp;

            	while (isalnum ((int) *cp) || *cp == '_') cp--;

				if (*cp == '@') is_member = 1; // handle @...
				else if (!isspace((int) *cp))
				{
					cp = cur_pos + 1;
					continue;
				}

				// cp points to the char before the variable name, so go forward
				cp++;

            	while (cp != cur_pos && ! isspace((int) *cp))
            	{
                    vStringPut (name, (int) *cp);
                    cp++;
            	}
                vStringTerminate (name);
                if (vStringLength (name) > 0)
                {
                	if (is_member) makeSimpleTag (name, RubyKinds, K_MEMBER);
                	else makeSimpleTag (name, RubyKinds, K_VARIABLE);
                }
                vStringClear (name);
                cp = cur_pos + 1;

            }
            else if (strncmp ((const char*) cp, "module", (size_t) 6) == 0) {
                cp += 6;
                if (isspace ((int) *cp)) {
                    while (isspace ((int) *cp))
                        ++cp;
                    while (isalnum ((int) *cp)  ||  *cp == '_'  ||  *cp == ':') {
                        vStringPut (name, (int) *cp);
                        ++cp;
                    }
                    vStringTerminate (name);
                    makeSimpleTag (name, RubyKinds, K_MIXIN);
                    vStringClear (name);
                }
            } else if (strncmp ((const char*) cp, "class", (size_t) 5) == 0) {
                cp += 5;
                if (isspace ((int) *cp)) {
                    while (isspace ((int) *cp))
                        ++cp;
                    while (isalnum ((int) *cp)  ||  *cp == '_'  ||  *cp == ':') {
                        vStringPut (name, (int) *cp);
                        ++cp;
                    }
                    vStringTerminate (name);
                    makeSimpleTag (name, RubyKinds, K_CLASS);
                    vStringClear (name);
                }
            } else if (strncmp ((const char*) cp, "def", (size_t) 3) == 0) {
                cp += 3;
                if (isspace ((int) *cp)) {
                    while (isspace ((int) *cp))
                        ++cp;

                    /* Put the valid characters allowed in a variable name
                     * in here. In ruby a variable name ENDING in ! means
                     * it changes the underlying data structure in place.
                     * A variable name ENDING in ? means that the function
                     * returns a bool. Functions should not start with these
                     * characters.
                     */
                    while (isalnum ((int) *cp)  ||  *cp == '_' || *cp == '!' || *cp =='?' || *cp=='.') {
                        /* classmethods are accesible only via class instance instead
                         * of object instance. This difference has to be outlined.
                         */
                        if (*cp == '.') {
                            //class method
                            is_singleton = TRUE;
                            vStringTerminate (name);
                            vStringClear(name);
                        } else {
                            vStringPut (name, (int) *cp);
                        }
                        ++cp;
                    }
                    vStringTerminate (name);
                    if (is_singleton) {
                        makeSimpleTag (name, RubyKinds, K_SINGLETON);
                    } else {
                        makeSimpleTag (name, RubyKinds, K_METHOD);
                    }
                    vStringClear (name);
                }
            } else if (*cp != '\0') {
                do
                    ++cp;
                while (isalnum ((int) *cp)  ||  *cp == '_');
            }
        }
    }
    vStringDelete (name);
}

extern parserDefinition* RubyParser (void)
{
    static const char *const extensions [] = { "rb", "rhtml", NULL };
    parserDefinition* def = parserNew ("Ruby");
    def->kinds      = RubyKinds;
    def->kindCount  = KIND_COUNT (RubyKinds);
    def->extensions = extensions;
    def->parser     = findRubyTags;
    return def;
}
