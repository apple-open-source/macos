/* $Xorg: miFont.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 
All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miFont.c,v 3.7 2001/12/14 19:57:37 dawes Exp $ */

#include "miFont.h"
#include "miLUT.h"
#include "miWks.h"
#include "PEX.h"
#include "PEXErr.h"
#include "pexExtract.h"
#include "pexUtils.h"
#include "pexos.h"


extern void CopyISOLatin1Lowered();
extern ErrorCode LoadPEXFontFile();
extern int pex_get_matching_names();

extern  diFontHandle defaultPEXFont;

/*  Level 4/3 Shared Resources  */
/*  PEX Font Procedures  */

/* get_lowered_truncated_entry takes a directory entry, strips off the .phont 
 * suffix, and puts in all in lower case ISO Latin 1.  If the entry doesn't 
 * have a .phont suffix, 0 is returned.
 */
int
get_lowered_truncated_entry(before, after)
char *before;			/* in */
char *after;			/* out */
{
    char    *suffix_ptr;
    
    suffix_ptr = before + strlen(before) - strlen(".phont");
    if (strncmp(suffix_ptr, ".phont", strlen(".phont")) != 0)
	return 0;

    CopyISOLatin1Lowered((unsigned char *)after, (unsigned char *)before,
			 strlen(before) - strlen(".phont"));
    
    return 1;
}

/*++
 |
 |  Function Name:	OpenPEXFont
 |
 |  Function Description:
 |	 Handles the PEXOpenFont request.
 |
 --*/
ddpex43rtn
OpenPEXFont(strLen, pName, pFont)
/* in */
    ddULONG             strLen;	  /* length of name */
    ddUCHAR            *pName;	  /* font name */
    diFontHandle        pFont;	  /* font handle - we fill in device_data */
/* out */
{
    miFontHeader       *font;
    register int        i;
    Ch_stroke_data **ch_data;
    ddpex43rtn	    err = Success;

    font = (miFontHeader *)xalloc((unsigned long)(sizeof(miFontHeader)));
    if (font == NULL) return (BadAlloc);
    pFont->deviceData = (ddPointer)font;
    font->lutRefCount = 0;
    font->freeFlag = MI_FALSE;
    font->properties = 0;
    font->font_info.numProps = 0;
    font->ch_data = 0;
    font->num_ch = 0;
    font->top = 0.0;
    font->bottom = 0.0;
    font->max_width = 0.0;

    err = LoadPEXFontFile(strLen, pName, pFont);
    if (err != Success) {
	xfree(font);
	return (err); }

    /*
     * Now, make a pass from the first glyph to the last glyph, seeing if 
     * all are defined.
     */
    font->font_info.allExist = 1;
    ch_data = font->ch_data + font->font_info.firstGlyph;
    for (   i = font->font_info.firstGlyph; 
	    i < font->num_ch && font->font_info.allExist;
	    i++, ch_data++ ) 
	if (*ch_data == NULL || (*ch_data)->strokes.numLists <= 0) {
	    font->font_info.allExist = 0;
	    break; }


    /*	For now, let the default glyph be an asterisk */
    font->font_info.defaultGlyph = (CARD16)'*';

    /* It's a stroke font */
    font->font_info.strokeFont = 1;

    return (Success);

}				  /* OpenPEXFont */

/*++
 |
 |  Function Name:	FreePEXFont
 |
 |  Function Description:
 |	Deletes all storage used by the font.
 |
 --*/
static void
really_free_font(pFont)
    diFontHandle        pFont;	  /* font handle */
{
    miFontHeader		 *font = (miFontHeader *) pFont->deviceData;
    register Ch_stroke_data	**ch_data, *firstChar = 0;
    int				  j;

    if (font->properties)
	xfree((char *) font->properties);

    if (font->ch_data) {
	for (j = 0, ch_data = font->ch_data; j < font->num_ch; j++, ch_data++) {
	    if (*ch_data != NULL) {
		MI_FREELISTHEADER(&((*ch_data)->strokes));
		if (!firstChar) firstChar = *ch_data; } }

	xfree((char *) (firstChar));
	xfree((char *) (font->ch_data));
    }

    xfree((char *) font);
    xfree((char *) pFont);

} 


ddpex43rtn
FreePEXFont(pFont, Fid)
/* in */
    diFontHandle        pFont;	  /* font handle */
    ddResourceId        Fid;	  /* font resource id */
/* out */
{
    miFontHeader       *font = (miFontHeader *) pFont->deviceData;
    
    if (pFont == defaultPEXFont) return (Success);

    font->freeFlag = MI_TRUE;
    pFont->id = PEXAlreadyFreed;
    
    if (font->lutRefCount == 0)
	really_free_font(pFont);

    return (Success);
}

/*++
 |
 |  Function Name: QueryPEXFont
 |
 |  Function Description:
 |	 Handles the PEXQueryFont request.
 |
 |  Input Description:
 |	diFontHandle	pFont;		 font handle
 |
 |  Output Description:
 |	ddBufferPtr	pBuffer;	 buffer with fontinfo
 |
 --*/
ddpex43rtn
QueryPEXFont(pFont, pBuffer)
    diFontHandle        pFont;	  /* font handle */
    ddBufferPtr         pBuffer;  /* font info */
{
    miFontHeader	*font = (miFontHeader *)pFont->deviceData;
    ddPointer    pbuf;
    ddULONG	 data_size;
    
    data_size = sizeof(pexFontInfo) +
		font->font_info.numProps * sizeof(pexFontProp);
			
    PU_CHECK_BUFFER_SIZE(pBuffer, data_size);
    pbuf = pBuffer->pBuf;
			
    /* copy actual font info into buffer */		
    memcpy( (char *)pbuf, (char *)&(font->font_info), sizeof(pexFontInfo));
    pbuf += sizeof(pexFontInfo);
    
    /* copy property info into buffer */
    if (font->font_info.numProps > 0)
	PACK_LISTOF_STRUCT( font->font_info.numProps, pexFontProp,
			    font->properties, pbuf);
			
    pBuffer->dataSize = data_size;
    			    
    return (Success);
    
} /* QueryPEXFont */

/*++
 |
 |  Function Name:	ListPEXFonts
 |
 |  Function Description:
 |	 Handles the PEXListFonts request.
 |
 --*/
ddpex43rtn
ListPEXFonts(patLen, pPattern, maxNames, pNumNames, pBuffer)
/* in */
    ddUSHORT            patLen;	  /* number of chars in pattern */
    ddUCHAR            *pPattern; /* pattern */
    ddUSHORT            maxNames; /* maximum number of names to return */
/* out */
    ddULONG            *pNumNames;/* number of names in reply */
    ddBufferPtr         pBuffer;  /* list of names */
{

    ddPointer	     pbuf;
    ddULONG	     total_space, n;
    char	   **names;	/* a list of strings */
    CARD16	    *valCARD16;
    int		     i;
    
    if (!pex_get_matching_names(patLen, pPattern, maxNames, &n, &names))
	return (BadAlloc);
	
    /* figure out how much space is needed by these strings */
    total_space = 0;
    for (i = 0; i < n; i++) {
	total_space += 2 + strlen(names[i]) + PADDING(2 + strlen(names[i]));
    }

    PU_CHECK_BUFFER_SIZE(pBuffer, total_space);
    
    pbuf = pBuffer->pBuf;
    for (i = 0; i < n; i++) {
	valCARD16 = (CARD16 *)pbuf;
	*valCARD16 = strlen(names[i]);
	pbuf += sizeof(CARD16);
	memcpy( (char *)pbuf, names[i], (int)(strlen(names[i])));
	pbuf += strlen(names[i]) + PADDING(2 + strlen(names[i]));
	xfree(names[i]);
    }
    xfree(names);
    
    *pNumNames = n;
    pBuffer->dataSize = total_space;
    return (Success);
}

/*++
 |
 |  Function Name:	ListPEXFontsPlus
 |
 |  Function Description:
 |	 Handles the PEXListFontsWithInfo request.
 |
 --*/
 
/* we don't ever expect to have more than this number of properties per font 
   but let's not create any bugs we don't have to, so this is just a 
   guestimate, not an enforced maximum
*/
#define EST_MAX_FONT_PROPS  10	

ddpex43rtn
ListPEXFontsPlus(patLen, pPattern, maxNames, pNumNames, pBuffer)
/* in */
    ddUSHORT            patLen;	  /* number of chars in pattern */
    ddUCHAR            *pPattern; /* pattern */
    ddUSHORT            maxNames; /* maximum number of names to return */
/* out */
    ddULONG            *pNumNames;/* number of names in reply */
    ddBufferPtr         pBuffer;  /* font names and info */
{

    ddPointer	    pBuf;
    ddULONG	    guess_size = 0, n;
    char	    **names;	/* a list of strings */
    int		    i, j, len;
    ddpex43rtn	    err = Success;
    ddFontResource  ddFont;
    miFontHeader    fontData;
    Ch_stroke_data  **ch_data;
    
    /* lookup names */
    if (!pex_get_matching_names(patLen, pPattern, maxNames, &n, &names))
	return (BadAlloc);
	
    /* guess at a large number of bytes for the reply, and make sure
       we have this many (can always realloc later) */
    for (i=0; i<n; i++)
	guess_size += (strlen(names[i]) + 4);
    guess_size += (sizeof(CARD32) + (n * sizeof(pexFontInfo)));
    guess_size += (n * EST_MAX_FONT_PROPS * sizeof(pexFontProp));
    if (PU_BUF_TOO_SMALL(pBuffer, guess_size))
	if (puBuffRealloc(pBuffer, guess_size) != Success) goto free_names;

    /* write names into reply buffer */
    pBuf = pBuffer->pBuf;
    pBuffer->dataSize = 0;
    for (i = 0; i < n; i++) {
	len = strlen(names[i]);
	PACK_CARD16(len, pBuf);
	PACK_LISTOF_STRUCT(len, CARD8, names[i], pBuf);
	SKIP_PADDING(pBuf, PADDING(sizeof(CARD16) + len));
	pBuffer->dataSize += sizeof(CARD16) + len +
	    PADDING(sizeof(CARD16) + len);
    }


    /* read in the font info, write it into the reply buffer */
    ddFont.deviceData = (ddPointer)&(fontData);
    fontData.properties = 0;
    PACK_CARD32(n, pBuf);
    pBuffer->dataSize += sizeof(CARD32);
    for (i = 0; i < n; i++) {
    
	err = LoadPEXFontFile(	(ddULONG)(strlen(names[i])),
				(ddUCHAR *)(names[i]),
				(diFontHandle)&ddFont);
	if (err) goto free_names;
	
	pBuffer->dataSize += sizeof(pexFontInfo)
		    + sizeof(pexFontProp) * fontData.font_info.numProps;
	if (PU_BUF_TOO_SMALL(pBuffer, pBuffer->dataSize))
	    if (puBuffRealloc(pBuffer, pBuffer->dataSize) != Success)
		goto free_names;

	/*
	 * Now, make a pass from the first glyph to the last glyph, seeing if 
	 * all are defined.
	 */
	fontData.font_info.allExist = 1;
	ch_data = fontData.ch_data + fontData.font_info.firstGlyph;
	for (j = fontData.font_info.firstGlyph; 
	     j < fontData.num_ch && fontData.font_info.allExist;
	     j++, ch_data++ ) 
	    if (*ch_data == NULL || (*ch_data)->strokes.numLists <= 0) {
		fontData.font_info.allExist = 0;
		break; }

	/* For now, let the default glyph be an asterisk */
	fontData.font_info.defaultGlyph = (CARD16)'*';
    
	/* It's a stroke font */
	fontData.font_info.strokeFont = 1;

	PACK_STRUCT(pexFontInfo, &(fontData.font_info), pBuf);
	if (fontData.font_info.numProps > 0) {
	    PACK_LISTOF_STRUCT(	fontData.font_info.numProps, pexFontProp,
				fontData.properties, pBuf);
	    xfree(fontData.properties);
	    fontData.properties = 0; }

	if (fontData.ch_data) {
	    for (   j=0, ch_data = fontData.ch_data;
		    j< fontData.num_ch; 
		    j++, ch_data++) {
		if (*ch_data) {
		    MI_FREELISTHEADER(&	((*ch_data)->strokes));
		    xfree((char *)(*ch_data)); } }
	    xfree((char *) (fontData.ch_data)); }

	xfree(names[i]);
    }

    xfree(names);

    *pNumNames = n;
    pBuffer->pBuf = pBuf;
    return (Success);

free_names:
    for (i = 0; i < n; i++) xfree(names[i]);
    xfree(names);
    pBuffer->dataSize = 0;
    if (err) return(err);
    return(BadAlloc);
}				  /* ListPEXFontsPlus */
 
/*
 * Given the extremes of all of the character sets used in composing
 * an ISTRING, and given the extremes of the ISTRING itself, along
 * with path, expansion and alignment, calculate the correct
 * concatenation point and alignment point.  The updated extreme values
 * are returned.
 */
void
micalc_cpt_and_align(meta_font, extent_xmin, extent_xmax,
    extent_ymin, extent_ymax, path, exp, pAlignment, cpt, align)
Meta_font	    *meta_font;
float		    *extent_xmin, *extent_xmax;
float		    *extent_ymin, *extent_ymax;
ddUSHORT	     path;
ddTextAlignmentData *pAlignment;
ddFLOAT		     exp;
register pexCoord2D *cpt;
register pexCoord2D *align;
{

    register float  xmin = *extent_xmin,
                    xmax = *extent_xmax,
                    ymin = *extent_ymin,
                    ymax = *extent_ymax;
    pexCoord2D	    temp;
    
    /* some of the necessary info may not be calculated yet */
    switch (path) {
	case PEXPathRight:
	    if (xmin < 0) {
		temp.x = xmax;
		xmax = xmin;
		xmin = temp.x;
	    }
	    cpt->x = xmax;
	    ymin = meta_font->bottom;
	    ymax = meta_font->top;
	    break;
	    
	case PEXPathLeft:
	    if (xmax <= 0.0)
		cpt->x = xmin;
	    else
		cpt->x = xmax;
	    ymin = meta_font->bottom;
	    ymax = meta_font->top;
	    break;
	    
	case PEXPathUp:
	    if (ymin < 0.0) {
		temp.y = ymax;
		ymax = ymin + meta_font->bottom;
		ymin = temp.y + meta_font->bottom;
	    } else {
		ymin = meta_font->bottom;
		ymax += meta_font->bottom;
	    }
	    cpt->y = ymax;
	    xmax = meta_font->width * 0.5 * exp;
	    xmin = - xmax;
	    break;
	    
	case PEXPathDown:
	    if (ymax > 0.0) {
		temp.y = ymax;
		ymax = ymin;
		ymin = temp.y;
	    } else {
		ymin += meta_font->top;
		ymax = meta_font->top;
	    }
	    cpt->y = ymin;
	    xmax = meta_font->width * 0.5 * exp;
	    xmin = - xmax;
	    break;
    }
    
    /* now do the vertical stuff */
    switch (path) {
    
	case PEXPathRight :
	case PEXPathLeft :
	
	    switch (pAlignment->vertical) {
		case PEXValignNormal :
		case PEXValignBase :	
		    cpt->y = 0.0;
		    align->y = ymin - meta_font->bottom;
		    break;
		case PEXValignBottom :	
		    cpt->y = align->y = ymin;
		    break;
		case PEXValignTop :	
		    cpt->y = align->y = ymax;
		    break;
		case PEXValignCap :	
		    cpt->y = FONT_COORD_CAP;
		    align->y = ymax - (meta_font->top - FONT_COORD_CAP);
		    break;
		case PEXValignHalf :
		    cpt->y = align->y = FONT_COORD_HALF;
		    break;
	    }
	    
	    break;
    
	case PEXPathUp :
	case PEXPathDown :
	    switch (pAlignment->vertical) {
	    
		case PEXValignBase :	
		    align->y = ymin - meta_font->bottom;
		    break;
		case PEXValignBottom :	
		    align->y = ymin;
		    break;
		case PEXValignTop :	
		    align->y = ymax;
		    break;
		case PEXValignCap :	
		    align->y = ymax - (meta_font->top - FONT_COORD_CAP);
		    break;
		case PEXValignHalf :
		    align->y = FONT_COORD_HALF + 0.5*(ymin - meta_font->bottom);
		    break;
		    
		case PEXValignNormal :
		    if (path == PEXPathUp) {
			/* for PathUp, NORMAL == BASE */
			align->y = ymin - meta_font->bottom;
		    } else {	/* path == PEXPathDown */
			align->y = ymax;
		    }
		    break;
	    }
	    
	    break;
    }
    
    /* now do the horizontal stuff */
    switch (path) {
    
	case PEXPathRight:
	
	    switch (pAlignment->horizontal) {
		case PEXHalignNormal :
		case PEXHalignLeft :
		    align->x = xmin;
		    break;
		case PEXHalignCenter :
		    align->x = 0.5 * (xmin + xmax);
		    break;
		case PEXHalignRight :
		    align->x = xmax;
		    break;
	    }
	    break;

	case PEXPathLeft:
	
	    switch (pAlignment->horizontal) {
		case PEXHalignLeft :
		    if (xmax <= 0.0)
			align->x = xmin;
		    else
			align->x = xmax + xmin;
		    break;
		    
		case PEXHalignCenter :
		    align->x = 0.5 * (xmin + xmax);
		    break;
		    
		case PEXHalignNormal :
		case PEXHalignRight :
		    align->x = (xmax > 0.0 ? 0.0 : xmax);
		    break;
	    }
	    break;

	case PEXPathUp:
	case PEXPathDown:
	
	    switch (pAlignment->horizontal) {
		case PEXHalignLeft :
		    align->x = cpt->x = xmin;
		    break;
		case PEXHalignNormal :
		case PEXHalignCenter :
		    align->x = cpt->x = 0.5 * (xmin + xmax);
		    break;
		case PEXHalignRight :
		    align->x = cpt->x = xmax;
		    break;
	    }
	    break;
	
    }

    *extent_xmin = xmin;
    *extent_xmax = xmax;
    *extent_ymin = ymin;
    *extent_ymax = ymax;
}



/*++
 |
 |  Function Name:	QueryPEXTextExtents
 |
 |  Function Description:
 |	 Handles the PEXQueryTextExtents request.
 |
 |  Note(s):
 |
 --*/
ddpex43rtn
QueryPEXTextExtents(resource, resourceType, fontIndex, path, expansion, 
		spacing, height, pAlignment, numStrings, pStrings, pBuffer)
/* in */
    ddPointer		    resource;	    /* what it is depends on next arg */
    ddResourceType	    resourceType;   /* renderer, wks, or lut */
    ddUSHORT		    fontIndex;	    /* index into font table */
    ddUSHORT		    path;	    /* text path */
    ddFLOAT		    expansion;	    /* character expansion */
    ddFLOAT		    spacing;	    /* character spacing */
    ddFLOAT		    height;	    /* character height */
    ddTextAlignmentData	   *pAlignment;	    /* text alignment */
    ddULONG		    numStrings;	    /* num strings */
    ddPointer		    pStrings;	    /* list of ISTRINGS */
/* out */
    ddBufferPtr		    pBuffer;	    /* extent info */
{
    diLUTHandle		     fontTable;
    register ddPointer	     ptr;
    pexMonoEncoding	    *mono_enc;
    int			     i, fragnum, charnum, some_characters, signum;
    CARD32		     numFragments, charval;
    diFontHandle	     font_handle;
    miFontHeader	    *font;
    pexExtentInfo	    *extent;
    Ch_stroke_data	    *char_data;
    ddFLOAT		     sp = spacing * FONT_COORD_HEIGHT;
    Meta_font		     meta_font;
    pexCoord2D		     cur, end, cpt, align;
    float	     	     xmin, xmax, ymin, ymax;
    float		     ht_scale = height / FONT_COORD_HEIGHT;
    extern unsigned long     PEXFontType;
    miTextFontEntry	    *miFontTable;
    ddTextFontEntry	    *fontEntry;
    ddpex43rtn		     err;
    ddUSHORT		     status;
    
    switch (resourceType) {
	case WORKSTATION_RESOURCE : {
	    miWksPtr	pwks = (miWksPtr)(((diWKSHandle)resource)->deviceData);
	    fontTable = pwks->pRend->lut[PEXTextFontLUT];
	    break;

	case LOOKUP_TABLE_RESOURCE :
	    fontTable = (diLUTHandle )resource;
	    if (fontTable->lutType != PEXTextFontLUT) return (BadMatch);
	    break;

	case RENDERER_RESOURCE :
	    fontTable = ((ddRendererPtr )resource)->lut[PEXTextFontLUT];
	    break;

	default: return(BadValue);
	}
	
    }
    
    
    /* get ddTextFontEntry member */
    err = InquireLUTEntryAddress(   PEXTextFontLUT, fontTable, fontIndex,
				    &status, (ddPointer *)(&miFontTable));
    if (err != Success) return(err);
    fontEntry = &miFontTable->entry;
    
    PU_CHECK_BUFFER_SIZE(pBuffer, numStrings * sizeof(pexExtentInfo));
    pBuffer->dataSize = numStrings * sizeof(pexExtentInfo);
    

    /* signum is used later on to encapsulate addition vs. subtraction */
    if (path == PEXPathRight || path == PEXPathUp)
	signum = 1;
    else
	signum = -1;
		    
    ptr = pStrings;
    extent = (pexExtentInfo *)pBuffer->pBuf;
    
    /* for each ISTRING */
    for (i = 0; i < numStrings; i++, extent++) {
    
	meta_font.top = -1.0e20;
	meta_font.bottom = 1.0e20;
	meta_font.width = 1.0e-20;
	
	xmin = xmax = ymin = ymax = 0.0;
	cpt.x = cpt.y = 0.0;
	cur.x = end.x = cur.y = end.y = 0.0;
	
	some_characters = 0;	/* make TRUE when a valid character is found */
	
	numFragments = *(CARD32 *)ptr;
	ptr += sizeof(CARD32);
	
	/* for each MONO_ENCODING fragment within the ISTRING */
	for (fragnum = 0; fragnum < numFragments; fragnum++) {
	
	    mono_enc = (pexMonoEncoding *)ptr;
	    ptr += sizeof(pexMonoEncoding);
	    
	    if (mono_enc->characterSet < 1 ||
		mono_enc->characterSet > fontEntry->numFonts)
		mono_enc->characterSet = 1;

	    font_handle = fontEntry->fonts[mono_enc->characterSet - 1];
	    
	    /* this is the font that this MONO_ENCODING would be rendered
	     * with, thus we use it to base our extents on */
	    font = (miFontHeader *)(font_handle->deviceData);
	    
	    /* bump up ISTRINGS extremes if appropriate */
	    if (font->top > meta_font.top) 
		meta_font.top = font->top;
	    if (font->bottom < meta_font.bottom)
		meta_font.bottom = font->bottom;
	    if (font->max_width > meta_font.width)
		meta_font.width = font->max_width;
	    
	    /* for each character within the MONO_ENCODING */
	    for (charnum = 0; charnum < mono_enc->numChars; charnum++) {

		switch (mono_enc->characterSetWidth) {
		    case PEXCSByte :
			charval = (CARD32)(*(CARD8 *)ptr);
			ptr += sizeof(CARD8);
			break;
		    case PEXCSShort :
			charval = (CARD32)(*(CARD16 *)ptr);
			ptr += sizeof(CARD16);
			break;
		    case PEXCSLong :
			charval = *(CARD32 *)ptr;
			ptr += sizeof(CARD32);
			break;
		}
		
		if (	(charval < font->font_info.firstGlyph)
		    ||	(charval > font->font_info.lastGlyph)
		    ||	!(font->ch_data[(int)charval]))	/* undefined char */
		    if (font->font_info.defaultGlyph == 0 &&
			font->font_info.firstGlyph > 0)  /* no default */
			/* no extent info is calculated for undefined indices
			 * in charsets where there is no default glyph */
			continue;   
		    else
			charval = font->font_info.defaultGlyph;

		some_characters = 1;
		char_data = font->ch_data[(int)charval];
		
		switch (path) {
		
		    case PEXPathRight :
		    case PEXPathLeft :
			end.x = cur.x + signum * char_data->right * expansion;
			if (cur.x > xmax) xmax = cur.x;
			if (cur.x < xmin) xmin = cur.x;
			if (end.x > xmax) xmax = end.x;
			if (end.x < xmin) xmin = end.x;
			cur.x = end.x + signum * sp;
			break;
		    
		    case PEXPathUp :
		    case PEXPathDown :
			end.y = cur.y + signum * (meta_font.top -
						  meta_font.bottom);
			if (cur.y > ymax) ymax = cur.y;
			if (cur.y < ymin) ymin = cur.y;
			if (end.y > ymax) ymax = end.y;
			if (end.y < ymin) ymin = end.y;
			cur.y = end.y + signum * sp;
			break;
			
		}
	    }
	    
	    ptr += PADDING(mono_enc->numChars * 
		    ((mono_enc->characterSetWidth == PEXCSByte) 
			? sizeof(CARD8) 
			: ((mono_enc->characterSetWidth == PEXCSShort) 
				? sizeof(CARD16) 
				: sizeof(CARD32))));
				
				
	} /* for each MONO_ENCODING */
	
	if (some_characters) {
	    
	    micalc_cpt_and_align( &meta_font, &xmin, &xmax, &ymin, &ymax,
				  path, expansion, pAlignment, &cpt, &align);
						       
	} else {
	    /* no valid characters */
	    xmin = xmax = ymin = ymax = 0.0;
	    cpt.x = cpt.y = align.x = align.y = 0.0;
	}
	
	extent->lowerLeft.x = ht_scale * (xmin - align.x);
	extent->lowerLeft.y = ht_scale * (ymin - align.y);
	extent->upperRight.x = ht_scale * (xmax - align.x);
	extent->upperRight.y = ht_scale * (ymax - align.y);
	extent->concatpoint.x = ht_scale * (cpt.x - align.x);
	extent->concatpoint.y = ht_scale * (cpt.y - align.y);
	
    }	/* for each ISTRING */
    
    return (Success);
}				  /* QueryPEXTextExtents */


/*++
 |
 |  Function Name:	UpdateFontRefs
 |
 |  Function Description:
 |	The font resource knows how many LUTs are referencing it.  If
 |	that number drops to zero, and FreePEXFont has already been
 |	called, then we really release the storage used by the font.
 |
 |  Note(s):
 |
 --*/
ddpex43rtn
UpdateFontRefs(pFont, pResource, action)
/* in */
    diFontHandle        pFont;	  /* font handle */
    diLUTHandle         pResource;/* lut handle */
    ddAction            action;	  /* add or remove */
/* out */
{

    miFontHeader *font = (miFontHeader *) pFont->deviceData;

    if (action == ADD) font->lutRefCount++;
    else font->lutRefCount--;

    if ((font->freeFlag == MI_TRUE) && (font->lutRefCount == 0))
	really_free_font (pFont);

    return (Success);
}				  /* UpdateFontRefs */

