/*****************************************************************************/
/*

Copyright 1989, 1998  The Open Group

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
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    name of Evans & Sutherland not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND    **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/
/* $XFree86: xc/programs/twm/list.c,v 1.8 2002/09/24 21:00:27 tsi Exp $ */


/**********************************************************************
 *
 * $Xorg: list.c,v 1.4 2001/02/09 02:05:36 xorgcvs Exp $
 *
 * TWM code to deal with the name lists for the NoTitle list and
 * the AutoRaise list
 *
 * 11-Apr-88 Tom LaStrange        Initial Version.
 *
 **********************************************************************/

#include <stdio.h>
#include "twm.h"
#include "screen.h"
#include "gram.h"
#include "util.h"

struct name_list_struct
{
    name_list *next;		/* pointer to the next name */
    char *name;			/* the name of the window */
    char *ptr;			/* list dependent data */
};

/***********************************************************************
 *
 *  Procedure:
 *	AddToList - add a window name to the appropriate list
 *
 *  Inputs:
 *	list	- the address of the pointer to the head of a list
 *	name	- a pointer to the name of the window 
 *	ptr	- pointer to list dependent data
 *
 *  Special Considerations
 *	If the list does not use the ptr value, a non-null value 
 *	should be placed in it.  LookInList returns this ptr value
 *	and procedures calling LookInList will check for a non-null 
 *	return value as an indication of success.
 *
 ***********************************************************************
 */

void
AddToList(list_head, name, ptr)
name_list **list_head;
char *name;
char *ptr;
{
    name_list *nptr;

    if (!list_head) return;	/* ignore empty inserts */

    nptr = (name_list *)malloc(sizeof(name_list));
    if (nptr == NULL)
    {
	twmrc_error_prefix();
	fprintf (stderr, "unable to allocate %ld bytes for name_list\n",
		 (unsigned long)sizeof(name_list));
	Done(0);
    }

    nptr->next = *list_head;
    nptr->name = name;
    nptr->ptr = (ptr == NULL) ? (char *)TRUE : ptr;
    *list_head = nptr;
}    

/***********************************************************************
 *
 *  Procedure:
 *	LookInList - look through a list for a window name, or class
 *
 *  Returned Value:
 *	the ptr field of the list structure or NULL if the name 
 *	or class was not found in the list
 *
 *  Inputs:
 *	list	- a pointer to the head of a list
 *	name	- a pointer to the name to look for
 *	class	- a pointer to the class to look for
 *
 ***********************************************************************
 */

char *
LookInList(list_head, name, class)
name_list *list_head;
char *name;
XClassHint *class;
{
    name_list *nptr;

    /* look for the name first */
    for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	if (strcmp(name, nptr->name) == 0)
	    return (nptr->ptr);

    if (class)
    {
	/* look for the res_name next */
	for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	    if (strcmp(class->res_name, nptr->name) == 0)
		return (nptr->ptr);

	/* finally look for the res_class */
	for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	    if (strcmp(class->res_class, nptr->name) == 0)
		return (nptr->ptr);
    }
    return (NULL);
}

char *
LookInNameList(list_head, name)
name_list *list_head;
char *name;
{
    return (LookInList(list_head, name, NULL));
}

/***********************************************************************
 *
 *  Procedure:
 *	GetColorFromList - look through a list for a window name, or class
 *
 *  Returned Value:
 *	TRUE if the name was found
 *	FALSE if the name was not found
 *
 *  Inputs:
 *	list	- a pointer to the head of a list
 *	name	- a pointer to the name to look for
 *	class	- a pointer to the class to look for
 *
 *  Outputs:
 *	ptr	- fill in the list value if the name was found
 *
 ***********************************************************************
 */

int GetColorFromList(list_head, name, class, ptr)
name_list *list_head;
char *name;
XClassHint *class;
Pixel *ptr;
{
    int save;
    name_list *nptr;

    for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	if (strcmp(name, nptr->name) == 0)
	{
	    save = Scr->FirstTime;
	    Scr->FirstTime = TRUE;
	    GetColor(Scr->Monochrome, ptr, nptr->ptr);
	    Scr->FirstTime = save;
	    return (TRUE);
	}

    if (class)
    {
	for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	    if (strcmp(class->res_name, nptr->name) == 0)
	    {
		save = Scr->FirstTime;
		Scr->FirstTime = TRUE;
		GetColor(Scr->Monochrome, ptr, nptr->ptr);
		Scr->FirstTime = save;
		return (TRUE);
	    }

	for (nptr = list_head; nptr != NULL; nptr = nptr->next)
	    if (strcmp(class->res_class, nptr->name) == 0)
	    {
		save = Scr->FirstTime;
		Scr->FirstTime = TRUE;
		GetColor(Scr->Monochrome, ptr, nptr->ptr);
		Scr->FirstTime = save;
		return (TRUE);
	    }
    }
    return (FALSE);
}

/***********************************************************************
 *
 *  Procedure:
 *	FreeList - free up a list
 *
 ***********************************************************************
 */

void FreeList(list)
name_list **list;
{
    name_list *nptr;
    name_list *tmp;

    for (nptr = *list; nptr != NULL; )
    {
	tmp = nptr->next;
	free((char *) nptr);
	nptr = tmp;
    }
    *list = NULL;
}
