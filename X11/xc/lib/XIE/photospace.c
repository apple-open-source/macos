/* $Xorg: photospace.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/XIE/photospace.c,v 1.5 2001/12/14 19:54:34 dawes Exp $ */

#include "XIElibint.h"



XiePhotospace
XieCreatePhotospace (Display *display)
{
    xieCreatePhotospaceReq	*req;
    char			*pBuf;
    XiePhotospace		id;

    LockDisplay (display);

    id = XAllocID (display);

    GET_REQUEST (CreatePhotospace, pBuf);

    BEGIN_REQUEST_HEADER (CreatePhotospace, pBuf, req);

    STORE_REQUEST_HEADER (CreatePhotospace, req);
    req->nameSpace = id;

    END_REQUEST_HEADER (CreatePhotospace, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (id);
}


void
XieDestroyPhotospace (Display *display, XiePhotospace photospace)
{
    xieDestroyPhotospaceReq	*req;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (DestroyPhotospace, pBuf);

    BEGIN_REQUEST_HEADER (DestroyPhotospace, pBuf, req);

    STORE_REQUEST_HEADER (DestroyPhotospace, req);
    req->nameSpace = photospace;

    END_REQUEST_HEADER (DestroyPhotospace, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}


void
XieExecuteImmediate (
	Display		*display,
	XiePhotospace	photospace,
	unsigned long	flo_id,
	Bool		notify,
	XiePhotoElement	*elem_list,
	int		elem_count)
{
    xieExecuteImmediateReq	*req;
    char			*pBuf, *pStart;
    unsigned			size;
    int				i;

    LockDisplay (display);

    size = _XiePhotofloSize (elem_list, elem_count);

    GET_REQUEST (ExecuteImmediate, pBuf);

    BEGIN_REQUEST_HEADER (ExecuteImmediate, pBuf, req);

    STORE_REQUEST_EXTRA_HEADER (ExecuteImmediate, size, req);
    req->nameSpace = photospace;
    req->floID = flo_id;
    req->numElements = elem_count;
    req->notify = notify;

    END_REQUEST_HEADER (ExecuteImmediate, pBuf, req);

    pBuf = pStart = _XAllocScratch (display, size);

    for (i = 0; i < elem_count; i++)
	(*_XieElemFuncs[elem_list[i].elemType - 1]) (&pBuf, &elem_list[i]);

    Data (display, pStart, size);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}
