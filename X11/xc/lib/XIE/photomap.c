/* $Xorg: photomap.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/XIE/photomap.c,v 1.5 2001/12/14 19:54:34 dawes Exp $ */

#include "XIElibint.h"


XiePhotomap
XieCreatePhotomap (Display *display)
{
    xieCreatePhotomapReq	*req;
    char			*pBuf;
    XiePhotomap			id;

    LockDisplay (display);

    id = XAllocID (display);

    GET_REQUEST (CreatePhotomap, pBuf);

    BEGIN_REQUEST_HEADER (CreatePhotomap, pBuf, req);

    STORE_REQUEST_HEADER (CreatePhotomap, req);
    req->photomap = id;

    END_REQUEST_HEADER (CreatePhotomap, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (id);
}


void
XieDestroyPhotomap (Display *display, XiePhotomap photomap)
{
    xieDestroyPhotomapReq	*req;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (DestroyPhotomap, pBuf);

    BEGIN_REQUEST_HEADER (DestroyPhotomap, pBuf, req);

    STORE_REQUEST_HEADER (DestroyPhotomap, req);
    req->photomap = photomap;

    END_REQUEST_HEADER (DestroyPhotomap, pBuf, req);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}


Status
XieQueryPhotomap (
	Display      		*display,
	XiePhotomap    		photomap,
	Bool         		*populated_ret,
	XieDataType   		*datatype_ret,
	XieDataClass		*dataclass_ret,
	XieDecodeTechnique	*decode_technique_ret,
	XieLTriplet     	width_ret,
	XieLTriplet     	height_ret,
	XieLTriplet     	levels_ret)
{
    xieQueryPhotomapReq		*req;
    xieQueryPhotomapReply	rep;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (QueryPhotomap, pBuf);

    BEGIN_REQUEST_HEADER (QueryPhotomap, pBuf, req);

    STORE_REQUEST_HEADER (QueryPhotomap, req);
    req->photomap = photomap;

    END_REQUEST_HEADER (QueryPhotomap, pBuf, req);

    if (_XReply (display, (xReply *)&rep,
	(SIZEOF (xieQueryPhotomapReply) - 32) >> 2, xTrue) == 0)
    {
        UnlockDisplay (display);
	SYNC_HANDLE (display);

	return (0);
    }

    *populated_ret 	  = rep.populated;
    *datatype_ret         = rep.dataType;
    *dataclass_ret	  = rep.dataClass;
    *decode_technique_ret = rep.decodeTechnique;
    width_ret[0]      	  = rep.width0;
    width_ret[1]      	  = rep.width1;
    width_ret[2]          = rep.width2;
    height_ret[0]         = rep.height0;
    height_ret[1]         = rep.height1;
    height_ret[2]         = rep.height2;
    levels_ret[0]         = rep.levels0;
    levels_ret[1]         = rep.levels1;
    levels_ret[2]         = rep.levels2;

    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (1);
}
