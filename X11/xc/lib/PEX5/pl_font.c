/* $Xorg: pl_font.c,v 1.4 2001/02/09 02:03:27 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"


PEXFont
PEXLoadFont (display, fontname)

INPUT Display	*display;
INPUT char	*fontname;

{
    register pexLoadFontReq	*req;
    char			*pBuf;
    PEXFont			id;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (LoadFont, pBuf);

    BEGIN_REQUEST_HEADER (LoadFont, pBuf, req);

    PEXStoreReqHead (LoadFont, req);
    req->numBytes = strlen (fontname);
    req->font = id = XAllocID (display);
    req->length += (req->numBytes + 3) >> 2;

    END_REQUEST_HEADER (LoadFont, pBuf, req);

    Data (display, fontname, req->numBytes);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (id);
}


void
PEXUnloadFont (display, font)

INPUT Display	*display;
INPUT PEXFont	font;

{
    register pexResourceReq 	*req;
    char			*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (UnloadFont, pBuf);

    BEGIN_REQUEST_HEADER (Resource, pBuf, req);

    PEXStoreReqHead (UnloadFont, req);
    req->id = font;

    END_REQUEST_HEADER (Resource, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXFontInfo *
PEXQueryFont (display, font)

INPUT Display		*display;
INPUT PEXFont		font;

{
    register pexQueryFontReq	*req;
    char			*pBuf, *pBufSave;
    pexQueryFontReply 		rep;
    PEXFontInfo			*fontInfo;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (QueryFont, pBuf);

    BEGIN_REQUEST_HEADER (QueryFont, pBuf, req);

    PEXStoreReqHead (QueryFont, req);
    req->font = font;

    END_REQUEST_HEADER (QueryFont, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (NULL);            /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    fontInfo = (PEXFontInfo *) Xmalloc (sizeof (PEXFontInfo));

    EXTRACT_FONTINFO (pBuf, (*fontInfo));

    fontInfo->props = (PEXFontProp *) Xmalloc (
	(unsigned) (fontInfo->count * sizeof (PEXFontProp)));

    EXTRACT_LISTOF_FONTPROP (fontInfo->count, pBuf, fontInfo->props);

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (fontInfo);
}


char **
PEXListFonts (display, pattern, maxNames, countReturn)

INPUT Display		*display;
INPUT char		*pattern;
INPUT unsigned int	maxNames;
OUTPUT unsigned long	*countReturn;

{
    register pexListFontsReq   	*req;
    char			*pBuf, *pBufSave;
    pexListFontsReply 		rep;
    char			**names;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (ListFonts, pBuf);

    BEGIN_REQUEST_HEADER (ListFonts, pBuf, req);

    PEXStoreReqHead (ListFonts, req);
    req->maxNames = maxNames;
    req->numChars = strlen (pattern);
    req->length += ((int) req->numChars + 3) >> 2;

    END_REQUEST_HEADER (ListFonts, pBuf, req);

    Data (display, pattern, req->numChars);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*countReturn = 0;
	return (NULL);            /* return an error */
    }

    *countReturn = rep.numStrings;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    names = (char **) Xmalloc ((unsigned) (rep.numStrings * sizeof (char *)));

    EXTRACT_LISTOF_STRING (rep.numStrings, pBuf, names);

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (names);
}


char **
PEXListFontsWithInfo (display, pattern, maxNames, countReturn, fontInfoReturn)

INPUT Display		*display;
INPUT char		*pattern;
INPUT unsigned int	maxNames;
OUTPUT unsigned long	*countReturn;
OUTPUT PEXFontInfo	**fontInfoReturn;

{
    register pexListFontsWithInfoReq	*req;
    char				*pBuf, *pBufSave;
    pexListFontsWithInfoReply		rep;
    PEXFontInfo				*pInfoRet;
    char				**names;
    CARD32				count;
    int					i;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (ListFontsWithInfo, pBuf);

    BEGIN_REQUEST_HEADER (ListFontsWithInfo, pBuf, req);

    PEXStoreReqHead (ListFontsWithInfo, req);
    req->maxNames = maxNames;
    req->numChars = strlen (pattern);
    req->length += ((int) req->numChars + 3) >> 2;

    END_REQUEST_HEADER (ListFontsWithInfo, pBuf, req);

    Data (display, (char *) pattern, req->numChars);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
        PEXSyncHandle (display);
	*countReturn = 0;
        return (NULL);                /* return an error */
    }

    *countReturn = rep.numStrings;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the font names to pass back to the client.
     */

    names = (char **) Xmalloc ((unsigned) (rep.numStrings * sizeof (char *)));

    EXTRACT_LISTOF_STRING (rep.numStrings, pBuf, names);


    /*
     * Allocate a buffer for the font info to pass back to the client.
     */

    EXTRACT_CARD32 (pBuf, count);

    *fontInfoReturn = pInfoRet = (PEXFontInfo *)
	Xmalloc ((unsigned) (count * sizeof (PEXFontInfo)));

    for (i = 0; i < count; i++, pInfoRet++)
    {
        EXTRACT_FONTINFO (pBuf, (*pInfoRet));

	pInfoRet->props = (PEXFontProp *)
	    Xmalloc ((unsigned) (pInfoRet->count * sizeof (PEXFontProp)));

        EXTRACT_LISTOF_FONTPROP (pInfoRet->count, pBuf, pInfoRet->props);
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (names);
}


PEXTextExtent *
PEXQueryTextExtents (display, id, fontGroup, path, expansion, spacing, height, 
    halign, valign, count, text)

INPUT Display			*display;
INPUT XID			id;
INPUT unsigned int		fontGroup;
INPUT int			path;
INPUT double			expansion;
INPUT double			spacing;
INPUT double			height;
INPUT int			halign;
INPUT int			valign;
INPUT unsigned long		count;
INPUT PEXStringData		*text;
{
    register pexQueryTextExtentsReq	*req;
    char				*pBuf, *pBufSave;
    pexQueryTextExtentsReply 		rep;
    pexMonoEncoding 			monoEncoding;
    int					numEncodings, i;
    PEXTextExtent			*textExtents;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (QueryTextExtents, pBuf);

    BEGIN_REQUEST_HEADER (QueryTextExtents, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (QueryTextExtents, fpFormat, req);
    req->textPath = path;
    req->id = id;
    req->fontGroupIndex = (pexTableIndex) fontGroup;
    req->alignment_vertical = valign;
    req->alignment_horizontal = halign;
    req->numStrings = count;

    if (fpConvert)
    {
	FP_CONVERT_DHTON (expansion, req->charExpansion, fpFormat);
	FP_CONVERT_DHTON (spacing, req->charSpacing, fpFormat);
	FP_CONVERT_DHTON (height, req->charHeight, fpFormat);
    }
    else
    {
	req->charExpansion = expansion;
	req->charSpacing = spacing;
	req->charHeight = height;
    }

    req->length += (count * (LENOF (CARD32) + LENOF (pexMonoEncoding)));
    for (i = 0; i < count; i++)
	req->length += (((int) text[i].length + 3) >> 2);

    END_REQUEST_HEADER (QueryTextExtents, pBuf, req);


    /*
     * Put the text in the request.
     */

    monoEncoding.characterSet = (INT16) 1;
    monoEncoding.characterSetWidth = (CARD8) PEXCSByte;
    monoEncoding.encodingState = 0;  

    numEncodings = 1;

    for (i = 0; i < count; i++)
    {
	XDATA_CARD32 (display, numEncodings);
	monoEncoding.numChars = (CARD16) (text[i].length);
	Data (display, (char *) &monoEncoding, SIZEOF (pexMonoEncoding));
	Data (display, text[i].ch, text[i].length);
    }

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (NULL);            /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    textExtents = (PEXTextExtent *) Xmalloc (
	(unsigned) (count * sizeof (PEXTextExtent)));

    EXTRACT_LISTOF_EXTENT_INFO (count, pBuf, textExtents, fpConvert, fpFormat)

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (textExtents);
}


PEXTextExtent *
PEXQueryEncodedTextExtents (display, id, fontGroup, path, expansion,
    spacing, height, halign, valign, count, encoded_text)

INPUT Display			*display;
INPUT XID			id;
INPUT unsigned int		fontGroup;
INPUT int			path;
INPUT double			expansion;
INPUT double			spacing;
INPUT double			height;
INPUT int			halign;
INPUT int			valign;
INPUT unsigned long		count;
INPUT PEXListOfEncodedText    	*encoded_text;

{
    register pexQueryTextExtentsReq	*req;
    char				*pBuf, *pBufSave;
    pexQueryTextExtentsReply 		rep;
    PEXEncodedTextData      		*string;
    PEXTextExtent			*textExtents;
    int					i, j;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (QueryTextExtents, pBuf);

    BEGIN_REQUEST_HEADER (QueryTextExtents, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (QueryTextExtents, fpFormat, req);
    req->textPath = path;
    req->id = id;
    req->fontGroupIndex = (pexTableIndex) fontGroup;
    req->alignment_vertical = valign;
    req->alignment_horizontal = halign;
    req->numStrings = count;

    if (fpConvert)
    {
	FP_CONVERT_DHTON (expansion, req->charExpansion, fpFormat);
	FP_CONVERT_DHTON (spacing, req->charSpacing, fpFormat);
	FP_CONVERT_DHTON (height, req->charHeight, fpFormat);
    }
    else
    {
	req->charExpansion = expansion;
	req->charSpacing = spacing;
	req->charHeight = height;
    }

    req->length += (count * LENOF (CARD32));
    for (i = 0; i < count; i++)
    {
	string = encoded_text[i].encoded_text;
	for (j = 0; j < (int) encoded_text[i].count; j++, string++)
	{
	    req->length += LENOF (pexMonoEncoding);
	    if (string->character_set_width == PEXCSLong) 
		req->length += string->length;
	    else if (string->character_set_width == PEXCSShort) 
		req->length += ((int) string->length + 1) >> 1;
	    else /* string->character_set_width == PEXCSByte) */ 
		req->length += ((int) string->length + 3) >> 2;
	} 
    }

    END_REQUEST_HEADER (QueryTextExtents, pBuf, req);


    /*
     * Put the encoded text in the request.
     */

    for (i = 0; i < count; i++)
    {
	unsigned long numEncodings = encoded_text[i].count;
	string = encoded_text[i].encoded_text;

	XDATA_CARD32 (display, numEncodings);

	for (j = 0; j < (int) numEncodings; j++, string++)
	{
	    XDATA_MONOENCODING (display, string);

	    if (string->character_set_width == PEXCSLong) 
	    {
		Data (display, string->ch, string->length * SIZEOF (long));
	    }
	    else if (string->character_set_width == PEXCSShort) 
	    {
		Data (display, string->ch, string->length * SIZEOF (short));
	    }
	    else /* string->character_set_width == PEXCSByte) */ 
	    {
		Data (display, string->ch, string->length);
	    }
	}
    }


    /*
     * Get a reply.
     */

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (NULL);            /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    textExtents = (PEXTextExtent *) Xmalloc (
	(unsigned) (count * sizeof (PEXTextExtent)));

    EXTRACT_LISTOF_EXTENT_INFO (count, pBuf, textExtents, fpConvert, fpFormat)

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (textExtents);
}
