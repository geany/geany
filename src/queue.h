/*
 *      queue.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009 kugel. aka Thomas Martitz <thomas47(at)arcor(dot)de>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

/* WARNING: Do not use this in new code, use GQueue or GList instead - this code may be
 * removed. */

#ifndef __QUEUE_H__
#define __QUEUE_H__


typedef struct _GeanyQueue
{
	gpointer data;
	struct _GeanyQueue *next;
} GeanyQueue;


GeanyQueue *queue_init(void);

void queue_append(GeanyQueue *queue_start, gpointer data);

GeanyQueue *queue_delete(GeanyQueue *queue_start, gpointer *data, const gboolean free_data);

GeanyQueue *queue_concat_copy(GeanyQueue *q1, GeanyQueue *q2);

void queue_destroy(GeanyQueue *queue_start);


#endif /* __QUEUE_H__ */
