/*
 *      queue.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * This provides a simple single linked list, with some functons to modify it.
 * Being a queue, you can append data only to the end of the list, end retrieve
 * data from the beginning. Only the first node is directly visible, but with the several foreach
 * functions you can iterate through the entire list.
 */


#include "geany.h"

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"


/* Allocates memory for queue_start, and sets next and data members to NULL */
GeanyQueue *queue_init(void)
{
	return g_new0(GeanyQueue, 1);
}


/* Returns true if q_node is the last node in a queue */
static gboolean queue_is_last_node(const GeanyQueue *q_node)
{
    return (q_node->next == NULL);
}


/* Appends a data in a new node at the end of the lst */
void queue_append(GeanyQueue *queue_start, gpointer data)
{
	GeanyQueue *temp, *next;

	if (queue_start == NULL || data == NULL)
		return;

	if (queue_start->data == NULL)
	{
		queue_start->data = data;
		return;
	}

	temp = g_new0(GeanyQueue, 1);
	temp->data = data;
	temp->next = NULL;

	next = queue_start;
	while (! queue_is_last_node(next))
		next = next->next;
	next->next = temp;
}


/* Removes and frees the first node in queue_start, and writes the data of the
 * removed node into data.
 * Returns a pointer to the new first item */
GeanyQueue *queue_delete(GeanyQueue *queue_start, gpointer *data, const gboolean free_data)
{
	GeanyQueue *ret;

	if (NULL == queue_start)
		return NULL;

	if (data != NULL)
		*data = queue_start->data;

	if (free_data)
		g_free(queue_start->data);

	ret = queue_start->next;
	g_free(queue_start);

	return ret;
}


/* Removes and frees the entire queue staring at queue_start */
void queue_destroy(GeanyQueue *queue_start)
{
	while ((queue_start = queue_delete(queue_start, NULL, FALSE)));
}

typedef void (*ForeachFunc) (GeanyQueue *queue_start, gpointer data);

/* Iterates through param, and calls func with the node queue_start and each node in param */
static void queue_foreach_data_2(GeanyQueue *queue_start, ForeachFunc func, GeanyQueue *param)
{
	GeanyQueue *temp = param;

	if (! queue_start || ! param)
		return;

	do
	{
		(*func) (queue_start, (temp->data));
	}
	while ((temp = temp->next));
}


/* Copies the data of each node in q1, then the data of each node in q2 to a newly
 * created queue, using queue_append. Frees q1 and q2.
 * Returns a pointer to the created queue. */
GeanyQueue *queue_concat_copy(GeanyQueue *q1, GeanyQueue *q2)
{
	/*		q1 +	q2		=	q3			*
	 *  ->1->2 + ->4->5->6  = 4->5->6->1->2 */
	GeanyQueue *ret = queue_init();

	queue_foreach_data_2(ret, queue_append, q1);
	queue_foreach_data_2(ret, queue_append, q2);

	queue_destroy(q1);
	queue_destroy(q2);

	return ret;
}

