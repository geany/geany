/*
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 *   This module contains functions for generating tags for source files
 *   for inp files (Abaqus).
 */

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <ctype.h>
#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_PART,
	K_ASSEMBLY,
	K_STEP
} AbaqusKind;

static kindOption AbaqusKinds[] = {
     { TRUE, 'c', "class",      "Parts" },
     { TRUE, 'm', "member",      "Assembly" },
     { TRUE, 'n', "namespace",      "Steps" }
};

/*
*   FUNCTION DEFINITIONS
*/

static int getWord(const char *ref, const char **ptr)
{
	const char *p = *ptr;

	while ((*ref != '\0') && (*p != '\0') && (tolower(*ref) == tolower(*p))) ref++, p++;

	if (*ref) return FALSE;

	*ptr = p;
	return TRUE;
}


static void createTag(AbaqusKind kind, const char *buf)
{
	vString *name;

	if (*buf == '\0') return;

	buf = strstr(buf, "=");
	if (buf == NULL) return;

	buf += 1;

	if (*buf == '\0') return;

	name = vStringNew();

	do
	{
		vStringPut(name, (int) *buf);
		++buf;
	} while ((*buf != '\0') && (*buf != ','));
	vStringTerminate(name);
	makeSimpleTag(name, AbaqusKinds, kind);
	vStringDelete(name);
}


static void findAbaqusTags(void)
{
	const char *line;

	while ((line = (const char*)fileReadLine()) != NULL)
	{
		const char *cp = line;

		for (; *cp != '\0'; cp++)
		{
			if (*cp == '*')
			{
				cp++;

				/* Parts*/
				if (getWord("part", &cp))
				{
					createTag(K_PART, cp);
					continue;
				}
				/* Assembly */
				if (getWord("assembly", &cp))
				{
					createTag(K_ASSEMBLY, cp);
					continue;
				}
				/* Steps */
				if (getWord("step", &cp))
				{
					createTag(K_STEP, cp);
					continue;
				}
			}
		}
	}
}


extern parserDefinition* AbaqusParser (void)
{
	static const char *const extensions [] = { "inp", NULL };
	parserDefinition * def = parserNew ("Abaqus");
	def->kinds      = AbaqusKinds;
	def->kindCount  = KIND_COUNT (AbaqusKinds);
	def->extensions = extensions;
	def->parser     = findAbaqusTags;
	return def;
}
