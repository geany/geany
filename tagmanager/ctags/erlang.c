/*
*
*   Copyright (c) 2015, Beng Tan
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for parsing and scanning Erlang source files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"    /* must always come first */

#include <ctype.h>
#include <string.h>

#include "parse.h"
#include "read.h"

typedef enum {
    K_NONE = -1,
    K_DEFINE,
    K_RECORD,
    K_TYPE,
    K_CALLBACK,
    K_FUNCTION
} erlangKind;

static kindOption ErlangKinds [] = {
    { TRUE,  'd', "macro", "Macros"},
    { TRUE,  's', "struct", "Records"},
    { TRUE,  't', "typedef", "Types"},
    { TRUE,  'i', "interface", "Callback definitions"},
    { TRUE,  'f', "function", "Functions"},
};

static int getWord(const char *ref, const char **ptr)
{
    const char *p = *ptr;

    while ((*ref != '\0') && (*p != '\0') && (*ref == *p)) ref++, p++;

    if (*ref) return FALSE;

    *ptr = p;
    return TRUE;
}

static int isDelimiter(char c, char *delimiters)
{
    for (; *delimiters != '\0'; delimiters++) {
        if (c == *delimiters) {
            return 1;
        }
    }
    return 0;
}

static void vStringPutUntil(vString *name, const char *buf, char *delimiters)
{
    while ((*buf != '\0') && !isDelimiter(*buf, delimiters))
    {
        vStringPut(name, (int) *buf);
        ++buf;
    }
}

static void findErlangTags(void)
{
    const char *line;
    vString *name = vStringNew();

    while ((line = (const char*)fileReadLine()) != NULL)
    {
        const char *cp = line;

        if (getWord("-define(", &cp))
        {
            vStringClear(name);
            vStringPutUntil(name, cp, ",(");
            vStringTerminate(name);
            makeSimpleTag(name, ErlangKinds, K_DEFINE);
            continue;
        }

        if (getWord("-record(", &cp))
        {
            vStringClear(name);
            vStringPutUntil(name, cp, ",");
            vStringTerminate(name);
            makeSimpleTag(name, ErlangKinds, K_RECORD);
            continue;
        }

        if (getWord("-type ", &cp))
        {
            vStringClear(name);
            vStringPutUntil(name, cp, "(");
            vStringTerminate(name);
            makeSimpleTag(name, ErlangKinds, K_TYPE);
            continue;
        }

        if (getWord("-callback ", &cp))
        {
            vStringClear(name);
            vStringPutUntil(name, cp, ")");
            vStringPut(name, (int) ')');
            vStringTerminate(name);
            makeSimpleTag(name, ErlangKinds, K_CALLBACK);
            continue;
        }

        // Search for functions
        if (isalnum(*cp) || *cp == '_') {
            // Scan past function name
            while (isalnum(*cp) || *cp == '_') {
                cp++;
            }

            // Scan past whitespace between function name and brackets
            if (*cp == ' ') {
                while (*cp == ' ') {
                    cp++;
                }
            }

            if (*cp == '(') {
                vStringClear(name);
                vStringPutUntil(name, line, "(");
                vStringTerminate(name);
                makeSimpleTag(name, ErlangKinds, K_FUNCTION);
                continue;
            }
            else {
                // Reset cp
                cp = line;
            }
        }
    }

    vStringDelete(name);
}

extern parserDefinition* ErlangParser (void)
{
    static const char *const extensions [] = { "hrl", "erl", NULL };
    parserDefinition* def = parserNew ("Erlang");
    def->extensions = extensions;
    def->kinds      = ErlangKinds;
    def->kindCount  = KIND_COUNT (ErlangKinds);
    def->parser     = findErlangTags;

    return def;
}
