/* $Xorg: utils.c,v 1.4 2001/02/09 02:03:42 xorgcvs Exp $ */

/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/lib/XIE/utils.c,v 1.5 2001/12/14 19:54:34 dawes Exp $ */

#include "XIElibint.h"


Status
_XieRegisterTechFunc (int group, int technique, XieTechFuncPtr techfunc)
{
    XieTechFuncRec *ptr = _XieTechFuncs[group - 1];
    XieTechFuncRec *prev = NULL, *newrec;
    int	status = 1;

    newrec = (XieTechFuncRec *) Xmalloc (sizeof (XieTechFuncRec));
    newrec->technique = technique;
    newrec->techfunc = techfunc;

    if (ptr == NULL)
    {
	_XieTechFuncs[group - 1] = newrec;
	newrec->next = NULL;
    }
    else
    {
	while (ptr && ptr->technique < technique)
	{
	    prev = ptr;
	    ptr = ptr->next;
	}

	if (ptr == NULL)
	{
	    prev->next = newrec;
	    newrec->next = NULL;
	}
	else
	{
	    if (ptr->technique == technique)
	    {
		Xfree ((char *) newrec);
		status = 0;
	    }
	    else if (prev == NULL)
	    {
		newrec->next = _XieTechFuncs[group - 1];
		_XieTechFuncs[group - 1] = newrec;
	    }
	    else
	    {
		newrec->next = prev->next;
		prev->next = newrec;
	    }
	}
    }

    return (status);
}



XieTechFuncPtr
_XieLookupTechFunc (int group, int technique)
{
    XieTechFuncRec *ptr = _XieTechFuncs[group - 1];
    int found = 0;

    while (ptr && !found)
    {
	if (ptr->technique == technique)
	    found = 1;
	else
	    ptr = ptr->next;
    }

    if (found)
	return (ptr->techfunc);
    else
	return (NULL);
}


