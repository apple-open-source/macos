/* $Xorg: XMakeAssoc.c,v 1.4 2001/02/09 02:04:05 xorgcvs Exp $ */
/*

Copyright 1985, 1998 The Open Group.

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/oldX/XMakeAssoc.c,v 1.3 2001/01/17 19:43:38 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "X11/Xlibint.h"
#include "X10.h"

/*
 * XMakeAssoc - Insert data into an XAssocTable keyed on an XId.
 * Data is inserted into the table only once.  Redundant inserts are
 * meaningless (but cause no problems).  The queue in each association
 * bucket is sorted (lowest XId to highest XId).
 */
int
XMakeAssoc(dpy, table, x_id, data)
	register Display *dpy;
	register XAssocTable *table;
	register XID x_id;
	register XPointer data;
{
	int hash;
	register XAssoc *bucket;
	register XAssoc *Entry;
	register XAssoc *new_entry;
	
	/* Hash the XId to get the bucket number. */
	hash = x_id & (table->size - 1);
	/* Look up the bucket to get the entries in that bucket. */
	bucket = &table->buckets[hash];
	/* Get the first entry in the bucket. */
	Entry = bucket->next;

	/* If (Entry != bucket), the bucket is empty so make */
	/* the new entry the first entry in the bucket. */
	/* if (Entry == bucket), the we have to search the */
	/* bucket. */
	if (Entry != bucket) {
		/* The bucket isn't empty, begin searching. */
		/* If we leave the for loop then we have either passed */
		/* where the entry should be or hit the end of the bucket. */
		/* In either case we should then insert the new entry */
		/* before the current value of "Entry". */
		for (; Entry != bucket; Entry = Entry->next) {
			if (Entry->x_id == x_id) {
				/* Entry has the same XId... */
				if (Entry->display == dpy) {
					/* Entry has the same Display... */
					/* Therefore there is already an */
					/* entry with this XId and Display, */
					/* reset its data value and return. */
					Entry->data = data;
					return 0;
				}
				/* We found an association with the right */
				/* id but the wrong display! */
				continue;
			}
			/* If the current entry's XId is greater than the */
			/* XId of the entry to be inserted then we have */
			/* passed the location where the new XId should */
			/* be inserted. */
			if (Entry->x_id > x_id) break;
		}
        }

	/* If we are here then the new entry should be inserted just */
	/* before the current value of "Entry". */
	/* Create a new XAssoc and load it with new provided data. */
	new_entry = (XAssoc *)Xmalloc(sizeof(XAssoc));
	if (new_entry == NULL) {
		/* Malloc failed! */
		errno = ENOMEM;
		_XIOError(dpy);
	}
	new_entry->display = dpy;
	new_entry->x_id = x_id;
	new_entry->data = data;

	/* Insert the new entry. */
	new_entry->prev = Entry->prev;
	new_entry->next = Entry;
	Entry->prev->next = new_entry;
	Entry->prev = new_entry;
	return 0;
}

