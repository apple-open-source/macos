/* $Xorg: colorlist.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

/*

Copyright 1993, 1994, 1998  The Open Group

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
/* $XFree86: xc/lib/XIE/colorlist.c,v 1.6 2001/12/14 19:54:33 dawes Exp $ */

#include "XIElibint.h"


XieColorList
XieCreateColorList (Display *display)
{
    xieCreateColorListReq	*req;
    char			*pBuf;
    XieColorList		id;

    LockDisplay (display);

    id = XAllocID (display);

    GET_REQUEST (CreateColorList, pBuf);

    BEGIN_REQUEST_HEADER (CreateColorList, pBuf, req);

    STORE_REQUEST_HEADER (CreateColorList, req);
    req->colorList = id;

    END_REQUEST_HEADER (CreateColorList, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (id);
}


void
XieDestroyColorList (Display *display, XieColorList color_list)
{
    xieDestroyColorListReq	*req;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (DestroyColorList, pBuf);

    BEGIN_REQUEST_HEADER (DestroyColorList, pBuf, req);

    STORE_REQUEST_HEADER (DestroyColorList, req);
    req->colorList = color_list;

    END_REQUEST_HEADER (DestroyColorList, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}


void
XiePurgeColorList (Display *display, XieColorList color_list)
{
    xiePurgeColorListReq	*req;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (PurgeColorList, pBuf);

    BEGIN_REQUEST_HEADER (PurgeColorList, pBuf, req);

    STORE_REQUEST_HEADER (PurgeColorList, req);
    req->colorList = color_list;

    END_REQUEST_HEADER (PurgeColorList, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}


Status
XieQueryColorList (
	Display    	*display,
	XieColorList  	color_list,
	Colormap   	*colormap_ret,
	unsigned   	*ncolors_ret,
	unsigned long  	**colors_ret)
{
    xieQueryColorListReq	*req;
    xieQueryColorListReply	rep;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (QueryColorList, pBuf);

    BEGIN_REQUEST_HEADER (QueryColorList, pBuf, req);

    STORE_REQUEST_HEADER (QueryColorList, req);
    req->colorList = color_list;

    END_REQUEST_HEADER (QueryColorList, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
	SYNC_HANDLE (display);

	*colormap_ret = 0;
	*ncolors_ret = 0;
	*colors_ret = NULL;

	return (0);
    }

    *colormap_ret = rep.colormap;
    *ncolors_ret = rep.length;

    if (*ncolors_ret)
    {
      *colors_ret = (unsigned long *) Xmalloc (
	rep.length * sizeof (unsigned long));

      _XRead32 (display, (long *)(*colors_ret), rep.length << 2);
    }
    else
	*colors_ret = NULL;

    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (1);
}
