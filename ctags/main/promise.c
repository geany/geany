/*
 *
 *  Copyright (c) 2016, Red Hat, Inc.
 *  Copyright (c) 2016, Masatake YAMATO
 *
 *  Author: Masatake YAMATO <yamato@redhat.com>
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 */

#include "general.h"
#include "promise.h"
#include "debug.h"
#include "xtag.h"


struct promise {
	langType lang;
	unsigned long startLine;
	int startCharOffset;
	unsigned long endLine;
	int endCharOffset;
	unsigned long sourceLineOffset;
};

static struct promise *promises;
static int promise_count;
static int promise_allocated;

int  makePromise   (const char *parser,
		    unsigned long startLine, int startCharOffset,
		    unsigned long endLine, int endCharOffset,
		    unsigned long sourceLineOffset)
{
	struct promise *p;
	int r;
	langType lang;

/* GEANY DIFF */
/*
	if (!isXtagEnabled (XTAG_TAGS_GENERATED_BY_SUB_PARSERS))
		return -1;
*/
/* GEANY DIFF END */

	lang = getNamedLanguage (parser, 0);
	if (lang == LANG_IGNORE)
		return -1;

	if ( promise_count == promise_allocated)
	{
		size_t c = promise_allocated? (promise_allocated * 2): 8;
		promises = xRealloc (promises, c, struct promise);
		promise_allocated = c;
	}

	p = promises + promise_count;

	p->lang = lang;
	p->startLine = startLine;
	p->startCharOffset = startCharOffset;
	p->endLine = endLine;
	p->endCharOffset = endCharOffset;
	p->sourceLineOffset = sourceLineOffset;

	r = promise_count;
	promise_count++;
	return r;
}

void breakPromisesAfter (int promise)
{
	Assert (promise_count >= promise);

	promise_count = promise;
}

bool forcePromises (void)
{
	int i;
	bool tagFileResized = false;

	for (i = 0; i < promise_count; ++i)
	{
		struct promise *p = promises + i;
		tagFileResized = runParserInNarrowedInputStream (p->lang,
								 p->startLine,
								 p->startCharOffset,
								 p->endLine,
								 p->endCharOffset,
								 p->sourceLineOffset)
			? true
			: tagFileResized;
	}

	promise_count = 0;
	return tagFileResized;
}


int getLastPromise (void)
{
	return promise_count - 1;
}
