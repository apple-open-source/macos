/* $Xorg: pl_escape.c,v 1.4 2001/02/09 02:03:27 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

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

#include "PEXlib.h"
#include "PEXlibint.h"


void
PEXEscape (display, escapeID, escapeDataSize, escapeData)

INPUT Display		*display;
INPUT unsigned long  	escapeID;
INPUT int		escapeDataSize;
INPUT char		*escapeData;

{
    register pexEscapeReq	*req;
    char			*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (Escape, pBuf);

    BEGIN_REQUEST_HEADER (Escape, pBuf, req);

    PEXStoreReqExtraHead (Escape, escapeDataSize, req);
    req->escapeID = escapeID;

    END_REQUEST_HEADER (Escape, pBuf, req);

    Data (display, escapeData, escapeDataSize);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


char *
PEXEscapeWithReply (display, escapeID, escapeDataSize,
    escapeData, escapeOutDataSize)

INPUT Display		*display;
INPUT unsigned long  	escapeID;
INPUT int		escapeDataSize;
INPUT char		*escapeData;
OUTPUT unsigned long	*escapeOutDataSize;

{
    register pexEscapeWithReplyReq	*req;
    char				*pBuf;
    pexEscapeWithReplyReply		rep;
    char				*escRepData;
    char				*escRepDataRet;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (EscapeWithReply, pBuf);

    BEGIN_REQUEST_HEADER (EscapeWithReply, pBuf, req);

    PEXStoreReqExtraHead (EscapeWithReply, escapeDataSize, req);
    req->escapeID = escapeID;

    END_REQUEST_HEADER (EscapeWithReply, pBuf, req);

    Data (display, escapeData, escapeDataSize);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*escapeOutDataSize = 0;
        return (NULL);               /* return an error */
    }

    *escapeOutDataSize = 20 + (rep.length << 2);


    /*
     * Allocate a buffer for the reply escape data
     */

    escRepData = escRepDataRet = Xmalloc ((unsigned) *escapeOutDataSize);

    memcpy (escRepData, rep.escape_specific, 20);
    escRepData += 20;

    if (rep.length)
	_XRead (display, (char *) escRepData, (long) (rep.length << 2));


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (escRepDataRet);
}


void PEXSetEchoColor (display, renderer, color_type, color)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
INPUT int		color_type;
INPUT PEXColor		*color;

{
    char			*escapeData;
    unsigned			escapeSize;
    pexEscapeSetEchoColorData	*header;
    char			*ptr;
    int				fpConvert;
    int				fpFormat;


    /*
     * Fill in the escape record.
     */

    escapeSize = SIZEOF (pexEscapeSetEchoColorData) +
	SIZEOF (pexColorSpecifier) + GetColorSize (color_type);

    escapeData = Xmalloc (escapeSize);

    fpFormat = PEXGetProtocolFloatFormat (display);
    fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    header = (pexEscapeSetEchoColorData *) escapeData;
    header->fpFormat = fpFormat;
    header->rdr = renderer;

    ptr = escapeData + SIZEOF (pexEscapeSetEchoColorData);
    STORE_INT16 (color_type, ptr);
    ptr += 2;
    STORE_COLOR_VAL (color_type, (*color), ptr, fpConvert, fpFormat);


    /*
     * Generate the escape.
     */

    PEXEscape (display, PEXEscapeSetEchoColor, (int) escapeSize, escapeData);

    Xfree (escapeData);
}
