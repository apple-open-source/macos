/*
 * $Xorg: setvalues.c,v 1.4 2001/02/09 02:05:30 xorgcvs Exp $
 *
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
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */
/* $XFree86: xc/programs/editres/setvalues.c,v 1.5 2001/12/14 20:00:43 dawes Exp $ */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xresource.h>

#include <stdio.h>

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Cardinals.h>	
#include <X11/Xfuncs.h>
#include <X11/Xos.h>
#include "editresP.h"

#define RESOURCE_NAME ("name")
#define RESOURCE_CLASS ("Class")

/*	Function Name: PrintSetValuesError
 *	Description: Allow the SetValues error to be printed.
 *	Arguments: event - the set values call that caused this event.
 *	Returns: str - a string contining the errors.
 */

char *
PrintSetValuesError(event)
Event * event;
{
    char * errors = NULL;
    WNode * node;
    int i;
    SetValuesEvent * sv_event = (SetValuesEvent *) event;
    char buf[BUFSIZ];

    if (sv_event->num_entries == 0) 
	return(XtNewString("SetValues was Successful."));

    for (i = 0 ; i < (int)sv_event->num_entries ; i++) {
	node = FindNode(global_tree_info->top_node,
			sv_event->info[i].widgets.ids, 
			sv_event->info[i].widgets.num_widgets);

	if (node == NULL) {
	    sprintf(buf, "Editres Internal Error: Unable to FindNode.\n");
	    AddString(&errors, buf); 
	    continue;
	}

	sprintf(buf, "%s(0x%lx) - %s\n", node->name, node->id,
		sv_event->info[i].message);
	AddString(&errors, buf);
    }
    return(errors);
}

/*	Function Name: GetResourceValueForSetValues(node);
 *	Description: Returns the value that should be sent to SetValues.
 *	Arguments: node - the node which contains the resource box.
 *	Returns: value - allocated value.
 */

char *
GetResourceValueForSetValues(node, size)
WNode * node;
unsigned short * size;
{
    Arg args[1];
    char *ptr, *temp;
    XrmDatabase db = NULL;
    XrmValue value;

    XtSetArg(args[0], XtNstring, &ptr);
    XtGetValues(node->resources->res_box->value_wid, args, ONE);

    /*
     * This makes sure that exactly the same thing happens during a set
     * values, that would happend of we were to insert this value into
     * the resource database.
     */

    temp = XtMalloc(sizeof(char) * (strlen(ptr) + strlen(RESOURCE_NAME) + 2));
    sprintf(temp, "%s:%s", RESOURCE_NAME, ptr);
    XrmPutLineResource(&db, temp);
    XtFree(temp);

    XrmGetResource(db, RESOURCE_NAME, RESOURCE_CLASS, &temp, &value);

    ptr = XtMalloc(sizeof(char) * value.size);
    memmove( ptr, value.addr, value.size);
    XrmDestroyDatabase(db);
    
    *size = (unsigned short) value.size;
    return(ptr);
}
