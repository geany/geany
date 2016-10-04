/*
*   Copyright (c) 1998-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*/
#ifndef CTAGS_MAIN_KIND_H
#define CTAGS_MAIN_KIND_H

#include "general.h"

#define KIND_NULL    '\0'

#define KIND_FILE_ALT '!'

#define KIND_FILE_DEFAULT 'F'
#define KIND_FILE_DEFAULT_LONG "file"

typedef struct sKindOption {
	bool enabled;          /* are tags for kind enabled? */
	char  letter;               /* kind letter */
	const char* name;		  /* kind name */
	const char* description;	  /* displayed in --help output */
} kindOption;

#endif	/* CTAGS_MAIN_KIND_H */
