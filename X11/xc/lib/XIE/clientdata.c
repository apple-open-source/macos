/* $Xorg: clientdata.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/XIE/clientdata.c,v 1.5 2001/12/14 19:54:33 dawes Exp $ */

#include "XIElibint.h"

#include <stdio.h>


void
XiePutClientData (
	Display      	*display,
	unsigned long  	name_space,
	unsigned long  	flo_id,
	XiePhototag	element,
	Bool         	final,
	unsigned     	band_number,
	unsigned char  	*data,
	unsigned     	nbytes)
{
    xiePutClientDataReq	*req;
    char		*pBuf;

    LockDisplay (display);

    GET_REQUEST (PutClientData, pBuf);

    BEGIN_REQUEST_HEADER (PutClientData, pBuf, req);

    STORE_REQUEST_EXTRA_HEADER (PutClientData, nbytes, req);
    req->nameSpace = name_space;
    req->floID = flo_id;
    req->element = element;
    req->final = final;
    req->bandNumber = band_number;
    req->byteCount = nbytes;

    END_REQUEST_HEADER (PutClientData, pBuf, req);

    Data (display, (char *) data, nbytes);

    UnlockDisplay (display);
    SYNC_HANDLE (display);
}


Status
XieGetClientData (
	Display      	*display,
	unsigned long  	name_space,
	unsigned long  	flo_id,
	XiePhototag	element,
	unsigned  	max_bytes,
	Bool		terminate,
	unsigned     	band_number,
	XieExportState 	*new_state_ret,
	unsigned char   **data_ret,
	unsigned     	*nbytes_ret)
{
    xieGetClientDataReq		*req;
    xieGetClientDataReply	rep;
    char			*pBuf;

    LockDisplay (display);

    GET_REQUEST (GetClientData, pBuf);

    BEGIN_REQUEST_HEADER (GetClientData, pBuf, req);

    STORE_REQUEST_HEADER (GetClientData, req);
    req->nameSpace = name_space;
    req->floID = flo_id;
    req->maxBytes = max_bytes;
    req->element = element;
    req->terminate = terminate;
    req->bandNumber = band_number;

    END_REQUEST_HEADER (GetClientData, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
	/* V4.13 moved the data length word into the reply */
    {
        UnlockDisplay (display);
	SYNC_HANDLE (display);

   	*nbytes_ret = 0;
	*data_ret = NULL;

	return (0);
    }


    *new_state_ret = rep.newState;
    *nbytes_ret = rep.byteCount;

    *data_ret = (unsigned char *) Xmalloc (PADDED_BYTES (rep.byteCount));

    _XReadPad (display, (char *) *data_ret, rep.byteCount);


    UnlockDisplay (display);
    SYNC_HANDLE (display);

    return (1);
}
