/*
*   Copyright (c) 2016, Masatake YAMATO <yamato@redhat.com>
*   Copyright (c) 2016, Red Hat, Inc.
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for Python language
*   files.
*/

#include "general.h"  /* must always come first */
#include "mio.h"
#include "objpool.h"
#include "vstring.h"

#ifndef CTAGS_MAIN_TOKEN_H
#define CTAGS_MAIN_TOKEN_H

struct tokenClass;
struct tokenTypePair;

typedef short tokenType;
typedef short tokenKeyword;

typedef struct sTokenInfo {
	tokenType type;
	tokenKeyword keyword;
	vString *string;
	struct tokenInfoClass *klass;
	unsigned long lineNumber;
	MIOPos filePosition;
} tokenInfo;

struct tokenTypePair {
	tokenType start;
	tokenType end;
};

#define TOKEN(X)  ((tokenInfo *)X)
#define TOKENX(X,T)  ((T *)(((char *)TOKEN(X)) + sizeof (tokenInfo)))

struct tokenInfoClass {
	unsigned int nPreAlloc;
	tokenType typeForUndefined;
	tokenKeyword keywordNone;
	tokenType typeForKeyword;
	tokenType typeForEOF;
	size_t extraSpace;
	struct tokenTypePair   *pairs;
	unsigned int        pairCount;
	void (*init)   (tokenInfo *token, void *data);
	void (*read)   (tokenInfo *token, void *data);
	void (*clear)  (tokenInfo *token);
	void (*destroy) (tokenInfo *token);
	void (*copy)   (tokenInfo *dest, tokenInfo *src, void *data);
	objPool *pool;
	ptrArray *backlog;
};

void *newToken       (struct tokenInfoClass *klass);
void *newTokenFull   (struct tokenInfoClass *klass, void *data);
void *newTokenByCopying (tokenInfo *src);
void *newTokenByCopyingFull (tokenInfo *src, void *data);

void  flashTokenBacklog (struct tokenInfoClass *klass);
void  tokenDestroy    (tokenInfo *token);

void tokenReadFull   (tokenInfo *token, void *data);
void tokenRead       (tokenInfo *token);
void tokenUnreadFull (tokenInfo *token, void *data); /* DATA passed to copy method internally. */
void tokenUnread     (tokenInfo *token);


void tokenCopyFull   (tokenInfo *dest, tokenInfo *src, void *data);
void tokenCopy       (tokenInfo *dest, tokenInfo *src);

/* Helper macro & functions */

#define tokenIsType(TKN,T)     ((TKN)->type == TOKEN_##T)
#define tokenIsKeyword(TKN,K)  ((TKN)->type == TKN->klass->typeForKeyword \
									&& (TKN)->keyword == KEYWORD_##K)
#define tokenIsEOF(TKN)      ((TKN)->type == (TKN)->klass->typeForEOF)

#define tokenString(TKN)	   (vStringValue ((TKN)->string))
#define tokenPutc(TKN,C)      (vStringPut ((TKN)->string, C))

/* return true if t is found. In that case token holds an
   language object type t.
   return false if it reaches EOF. */
bool tokenSkipToType (tokenInfo *token, tokenType t);
bool tokenSkipToTypeFull (tokenInfo *token, tokenType t, void *data);
bool tokenSkipOverPair (tokenInfo *token);
bool tokenSkipOverPairFull (tokenInfo *token, void *data);

#endif
