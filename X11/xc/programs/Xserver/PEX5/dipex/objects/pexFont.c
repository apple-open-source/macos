/* $Xorg: pexFont.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexFont.c,v 3.9 2001/12/14 19:57:43 dawes Exp $ */


/*++    pexFont.c
 *		PEXOpenFont
 *		PEXCloseFont
 *		PEXQueryFont
 *		PEXListFonts
 *		PEXListFontsWithInfo
 *		PEXQueryTextExtents
 --*/

#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "dipex.h"
#include "pexLookup.h"
#define NEED_OS_LIMITS
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif

static dipexFont *FindPEXFontEntry();

/*++	PEXOpenFont
 *
 * DESCRIPTION:
 *
 * This request loads the specified PEX font, if necessary, and associates
 * identifier f_id with it. The font name should use the ISO Latin-1 encoding,
 * and upper/lower case does not matter.  PEXfonts are not associated with a
 * particular screen, and can be used with any renderer or PHIGS workstation
 * resources. 
 --*/
ErrorCode
PEXOpenFont (cntxtPtr, strmPtr)
pexContext      *cntxtPtr;
pexOpenFontReq  *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode FreePEXFont ();
    unsigned char fName[PATH_MAX];
    dipexFont *dif;
    extern void CopyISOLatin1Lowered();

    /* oh, who cares if it's already been opened under this id */
	if (!LegalNewID(strmPtr->font,  cntxtPtr->client)) 
	    PEX_ERR_EXIT(BadIDChoice,strmPtr->font,cntxtPtr);

	if (strmPtr->numBytes > PATH_MAX - 1)
	    PEX_ERR_EXIT(BadLength,0,cntxtPtr);

	/* has this server already loaded this font */
	CopyISOLatin1Lowered(	fName, (unsigned char *)(strmPtr+1),
				(int)(strmPtr->numBytes));

	dif = FindPEXFontEntry(fName);

	if (dif) {
	    if (dif->refcnt > 0) {
		dif->refcnt += 1; }
	} else {

	    dif = (dipexFont *) xalloc ((unsigned long)sizeof(dipexFont));
	    if (!dif) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
	    dif->ddFont.id = strmPtr->font;
	    dif->refcnt = 1;

	    err = OpenPEXFont(	(ddULONG)(strmPtr->numBytes), 
				(ddUCHAR *)(strmPtr + 1), &(dif->ddFont));
	    if (err) {
		xfree((pointer)dif);
		PEX_ERR_EXIT(err,0,cntxtPtr);
	    }

	/*
         * Note that fonts resources are stored with the type (dipexFont *),
         * and they are referenced sometimes in the DD layer as diFontHandle
         * (which is a pointer to a ddFontResource).  Since the first part
         * of the dipexFont structure consists of a ddFontResource, this
         * works.  Even though it is ugly, it's best not to start changing
         * all of the font code at this time (right before a public release),
         * and hopefully, it will get cleaned up for PEX 6.0.
	 */

	ADDRESOURCE(strmPtr->font, PEXFontType, dif);

	} 

    return(err);

} /* end-PEXOpenFont() */

static dipexFont *
FindPEXFontEntry(fname)
unsigned char *fname;
{
    return (0);		    /* stub */
}

/*++	PEXCloseFont
 --*/
ErrorCode
PEXCloseFont (cntxtPtr, strmPtr)
pexContext      *cntxtPtr;
pexCloseFontReq *strmPtr;
{
    ErrorCode err = Success;
    diFontHandle pf = 0;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXFontError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_PEXFONT(strmPtr->id, pf);

    FreeResource(strmPtr->id, RT_NONE);

    return(err);

} /* end-PEXCloseFont() */

/*++	PEXQueryFont
 --*/
ErrorCode
PEXQueryFont( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexQueryFontReq    	*strmPtr;
{
    ErrorCode err = Success;
    diFontHandle pf = 0;
    extern ddBuffer *pPEXBuffer;

    LU_PEXFONT(strmPtr->font, pf);

    SETUP_INQ(pexQueryFontReply);

    err = QueryPEXFont (pf, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexQueryFontReply);
	reply->lengthFontInfo = pPEXBuffer->dataSize;
	WritePEXBufferReply(pexQueryFontReply);
    }
    return( err );

} /* end-PEXQueryFont() */

/*++	PEXListFonts
 --*/
ErrorCode
PEXListFonts( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexListFontsReq 	*strmPtr;
{
    ErrorCode err = Success;
    extern ddBuffer *pPEXBuffer;
    CARD32 numStrings;

    SETUP_INQ(pexListFontsReply);

    err = ListPEXFonts(	strmPtr->numChars, (CARD8 *)(strmPtr+1),
			strmPtr->maxNames, &numStrings,	pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexListFontsReply);
	reply->numStrings = numStrings;
	WritePEXBufferReply(pexListFontsReply);
    }
    return( err );

} /* end-PEXListFonts() */

/*++	PEXListFontsWithInfo
 --*/
ErrorCode
PEXListFontsWithInfo( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexListFontsWithInfoReq *strmPtr;
{
    ErrorCode err = Success;
    extern ddBuffer *pPEXBuffer;
    CARD32 numStrings;

    SETUP_INQ(pexListFontsWithInfoReply);

    err = ListPEXFontsPlus( strmPtr->numChars, (CARD8 *)(strmPtr+1),
			    strmPtr->maxNames, &numStrings, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexListFontsWithInfoReply);
	reply->numStrings = numStrings;
	WritePEXBufferReply(pexListFontsWithInfoReply);
    }
    return( err );

} /* end-PEXListFontsWithInfo() */

/*++	PEXQueryTextExtents
 --*/
ErrorCode
PEXQueryTextExtents( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexQueryTextExtentsReq  *strmPtr;
{
    ErrorCode err = Success;
    extern ddBuffer *pPEXBuffer;
    diResourceHandle ptr;
    ddResourceType what;

    if (! (ptr = (diResourceHandle) LookupIDByType (strmPtr->id, PEXLutType)))
	if (! (ptr = (diResourceHandle) LookupIDByType (strmPtr->id, PEXWksType)))
	    if (! (ptr = (diResourceHandle) LookupIDByType (strmPtr->id, PEXRendType)))
		PEX_ERR_EXIT(BadValue,strmPtr->id,cntxtPtr)
	    else what = RENDERER_RESOURCE;
	else what = WORKSTATION_RESOURCE;
    else {
	what = LOOKUP_TABLE_RESOURCE;
	if (((ddLUTResource *)ptr)->lutType != PEXTextFontLUT) {
	    PEX_ERR_EXIT(BadMatch,strmPtr->id,cntxtPtr);
	}
    }

    SETUP_INQ(pexQueryTextExtentsReply);

    err = QueryPEXTextExtents(	ptr, what, strmPtr->fontGroupIndex,
				strmPtr->textPath, strmPtr->charExpansion,
				strmPtr->charSpacing, strmPtr->charHeight,
				&(strmPtr->textAlignment), strmPtr->numStrings,
				(ddPointer)(strmPtr + 1), pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexQueryTextExtentsReply);
	WritePEXBufferReply(pexQueryTextExtentsReply);
    }
    return( err );

} /* end-PEXQueryTextExtents() */
/*++
 *
 *	End of File
 *
 --*/
