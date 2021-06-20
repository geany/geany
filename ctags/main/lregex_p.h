/*
*   Copyright (c) 2000-2003, Darren Hiebert
*   Copyright (c) 2017, Red Hat, Inc.
*   Copyright (c) 2017, Masatake YAMATO
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for applying regular expression matching.
*
*   The code for utilizing the Gnu regex package with regards to processing the
*   regex option and checking for regex matches was adapted from routines in
*   Gnu etags.
*/
#ifndef CTAGS_MAIN_LREGEX_PRIVATE_H
#define CTAGS_MAIN_LREGEX_PRIVATE_H

/*
*   INCLUDE FILES
*/
#include "general.h"
#include "kind_p.h"
#include "lregex.h"

/*
*   DATA DECLARATIONS
*/
enum regexParserType {
	REG_PARSER_SINGLE_LINE,
	REG_PARSER_MULTI_LINE,
	REG_PARSER_MULTI_TABLE,
};

struct lregexControlBlock;

/*
*   FUNCTION PROTOTYPES
*/
extern struct lregexControlBlock* allocLregexControlBlock (parserDefinition *parser);
extern void freeLregexControlBlock (struct lregexControlBlock* lcb);

extern void processTagRegexOption (struct lregexControlBlock *lcb,
								   enum regexParserType,
								   const char* const parameter);
extern void addTagRegex (struct lregexControlBlock *lcb, const char* const regex,
						 const char* const name, const char* const kinds, const char* const flags,
						 bool *disabled);
extern void addTagMultiLineRegex (struct lregexControlBlock *lcb, const char* const regex,
								  const char* const name, const char* const kinds, const char* const flags,
								  bool *disabled);
extern void addTagMultiTableRegex(struct lregexControlBlock *lcb,
								  const char* const table_name,
								  const char* const regex,
								  const char* const name, const char* const kinds, const char* const flags,
								  bool *disabled);

extern bool matchRegex (struct lregexControlBlock *lcb, const vString* const line);
extern bool doesExpectCorkInRegex (struct lregexControlBlock *lcb);
extern void addCallbackRegex (struct lregexControlBlock *lcb,
							  const char* const regex,
							  const char* const flags,
							  const regexCallback callback,
							  bool *disabled,
							  void * userData);
extern bool regexNeedsMultilineBuffer (struct lregexControlBlock *lcb);
extern bool matchMultilineRegex (struct lregexControlBlock *lcb, const vString* const allLines);
extern bool matchMultitableRegex (struct lregexControlBlock *lcb, const vString* const allLines);

extern void notifyRegexInputStart (struct lregexControlBlock *lcb);
extern void notifyRegexInputEnd (struct lregexControlBlock *lcb);

extern void addRegexTable (struct lregexControlBlock *lcb, const char *name);
extern void extendRegexTable (struct lregexControlBlock *lcb, const char *src, const char *dist);

extern void printMultitableStatistics (struct lregexControlBlock *lcb);

#endif	/* CTAGS_MAIN_LREGEX_PRIVATEH */
