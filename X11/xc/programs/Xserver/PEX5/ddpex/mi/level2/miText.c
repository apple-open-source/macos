/* $Xorg: miText.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miText.c,v 3.8 2001/12/14 19:57:30 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miStruct.h"
#include "PEXprotost.h"
#include "ddpex3.h"
#include "ddpex2.h"
#include "miRender.h"
#include "miFont.h"
#include "miText.h"
#include "miClip.h"
#include "gcstruct.h"
#include "pexos.h"

#ifndef PADDING
#define PADDING(n) ( (n)%4 ? (4 - ((n)%4)) : 0)
#endif 

extern ddpex3rtn ComputeMCVolume();

ddpex2rtn
tx_el_to_path(pRend, pddc, numFragments, pString, numChars, tx_el,
	align_pt, count_ret)
/* in */
    ddRendererPtr           pRend;        /* Renderer handle */
    miDDContext             *pddc;        /* Context handle */
    ddUSHORT                numFragments; /* # of mono encodings */
    pexMonoEncoding         *pString;	  /* Pointer to ISTRING */
    ddULONG                 numChars;     /* Total # of chars in ISTRING */
/* out */
    miTextElement           *tx_el;       /* text element data */
    ddCoord2D               *align_pt;    /* text alignment */
    ddULONG		    *count_ret;   /* return character count */
{
/* Define required temporary variables */

    miCharPath              *CharPtr;
    ddUSHORT                 fontIndex;
    ddUSHORT                 path;
    ddFLOAT                  expansion;
    ddFLOAT                  spacing;
    ddTextAlignmentData      *pAlignment;
    diLUTHandle		     fontTable;
    register ddPointer	     ptr, save_ptr;
    miTextFontEntry          *ptr1;
    pexMonoEncoding	    *mono_enc;
    int			     fragnum, charnum, some_characters, signum, bytes;
    CARD32		     charval;
    diFontHandle	     font_handle;
    miFontHeader	    *font;
    Ch_stroke_data	    *char_data;
    ddFLOAT		     sp; 
    Meta_font		     meta_font;
    pexCoord2D		     cur, end, cpt;
    float	     	     xmin, xmax, ymin, ymax;
    ddTextFontEntry	    *fontEntry;
    ddUSHORT                 es, clip_mode;
    extern void              micalc_cpt_and_align();
    
    *count_ret = 0;

    fontIndex = pddc->Static.attrs->textFont;
    expansion = ABS(pddc->Static.attrs->charExpansion);
    spacing = pddc->Static.attrs->charSpacing;
    path = pddc->Static.attrs->textPath;
    sp = spacing * FONT_COORD_HEIGHT;
    pAlignment = &(pddc->Static.attrs->textAlignment);

    /* Get the handle for font table */

    fontTable = pRend->lut[PEXTextFontLUT];
    
    /* Inquire the Font table to get the ddTextFontEntry member */

    if ((InquireLUTEntryAddress (PEXTextFontLUT, fontTable, fontIndex, &es, 
				(ddPointer *)&ptr1)) == PEXLookupTableError)
      return (PEXLookupTableError);

    fontEntry = &ptr1->entry;
    
    /* Allocate space for stroke definitions of all chars in ISTRING */

    if (!((tx_el->paths) = (miCharPath *) xalloc(numChars *
						sizeof(miCharPath))))
      return (BadAlloc);

    /* signum is used later on to encapsulate addition vs. subtraction */

    if (path == PEXPathRight || path == PEXPathUp)
	signum = 1;
    else
	signum = -1;
		    
    ptr = (ddPointer) pString;
    
    /* Process the input ISTRING */

    meta_font.top = -1.0e20;
    meta_font.bottom = 1.0e20;
    meta_font.width = 1.0e-20;
	
    xmin = xmax = ymin = ymax = 0.0;
    cpt.x = cpt.y = 0.0;
    cur.x = end.x = cur.y = end.y = 0.0;
	
    some_characters = 0; /* Make TRUE when a valid character is found */
	
    save_ptr = ptr;        /* Save this for later use */
	
    /* First determine the largest character box size within ISTRING */
    /* Do for each MONO_ENCODING fragment within the ISTRING */

    for (fragnum = 0; fragnum < numFragments; fragnum++) {

      mono_enc = (pexMonoEncoding *)ptr;
      ptr += sizeof(pexMonoEncoding);
	    
      if (mono_enc->characterSet < 1 ||
	  mono_enc->characterSet > fontEntry->numFonts)
	  mono_enc->characterSet = 1;

      font_handle = fontEntry->fonts[mono_enc->characterSet - 1];
	    
      /* This is the font that this MONO_ENCODING would be rendered with  */

      font = (miFontHeader *)(font_handle->deviceData);
	    
      /* Bump up ISTRINGS extremes if appropriate */

      if (font->top > meta_font.top) 
	meta_font.top = font->top;
      if (font->bottom < meta_font.bottom)
	meta_font.bottom = font->bottom;
      if (font->max_width > meta_font.width)
	meta_font.width = font->max_width;

      bytes = mono_enc->numChars *
	  ((mono_enc->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((mono_enc->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));

      ptr += (bytes + PADDING (bytes));
    }

    /* Now get the character definition and the required per character */
    /* translation component required to be applied during rendering.  */

    ptr = save_ptr;   /* Restore the ptr */
    CharPtr = tx_el->paths; /* Initialize the pointer to character data */

    for (fragnum = 0; fragnum < numFragments; fragnum++) {
	
      mono_enc = (pexMonoEncoding *)ptr;
      ptr += sizeof(pexMonoEncoding);

      font_handle = fontEntry->fonts[mono_enc->characterSet - 1];
      font = (miFontHeader *)(font_handle->deviceData);

      /* Do for each character within the MONO_ENCODING */

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
		
	if (!(font->ch_data[charval]))	/* undefined character */
	  if (font->font_info.defaultGlyph == 0 &&
	      font->font_info.firstGlyph > 0)  /* no default */
	    /* no extent info is calculated for undefined indices
	     * in charsets where there is no default glyph */
	    continue;   
	  else
	    charval = font->font_info.defaultGlyph;

	some_characters = 1;
	char_data = font->ch_data[charval];  /* Get strokes for char */

	switch (path) {
		
	   case PEXPathRight :
	   case PEXPathLeft :
	     end.x = cur.x + signum*(char_data->right)*expansion;
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
	     cur.x += -char_data->right * 0.5 * expansion;
	     break;
	}

	/* Save the stroke definitions for the character */

	CharPtr->path = &(char_data->strokes);

	/* Save the translation per character */

	(CharPtr->trans).x = cur.x;
	(CharPtr->trans).y = cur.y;

	/* Set cur.x back to lower left corner of character box  */
	/* for the cases of PEXPathUp and PEXPathDown.           */

	if (path==PEXPathUp || path==PEXPathDown)
	  cur.x -= -char_data->right * 0.5 * expansion;
		
	CharPtr++; (*count_ret)++;  /* Update pointer and count */

      }  /* for each character */
	    
      ptr += PADDING(mono_enc->numChars * 
		     ((mono_enc->characterSetWidth == PEXCSByte) 
		      ? sizeof(CARD8) 
		      : ((mono_enc->characterSetWidth == PEXCSShort) 
			 ? sizeof(CARD16) 
			 : sizeof(CARD32))));
				
    } /* for each MONO_ENCODING (fragment) */
	
    /* Compute the alignment and concatenation point; however, */
    /* the concatenation point (cpt) can be ignored here !     */

    if (some_characters) {
	    
      micalc_cpt_and_align(&meta_font, &xmin, &xmax, &ymin, &ymax, path, 
			 expansion, pAlignment, &cpt, align_pt);
						       
    } else {
      /* no valid characters */
      xmin = xmax = ymin = ymax = 0.0;
      cpt.x = cpt.y = align_pt->x = align_pt->y = 0.0;
    }
	
    return (Success);
}				 


ddpex2rtn
atx_el_to_path(pRend, pddc, numFragments, pString, numChars, tx_el,
	align_pt, count_ret)
/* in */
    ddRendererPtr           pRend;        /* Renderer handle */
    miDDContext             *pddc;        /* Context handle */
    ddUSHORT                numFragments; /* # of mono encodings */
    pexMonoEncoding         *pString;	  /* Pointer to ISTRING */
    ddULONG                 numChars;     /* Total # of chars in ISTRING */
/* out */
    miTextElement           *tx_el;       /* text element data */
    ddCoord2D               *align_pt;    /* text alignment */
    ddULONG		    *count_ret;   /* return character count */
{
/* Define required temporary variables */

    miCharPath              *CharPtr;
    ddUSHORT                 fontIndex;
    ddUSHORT                 path;
    ddFLOAT                  expansion;
    ddFLOAT                  spacing;
    ddTextAlignmentData      *pAlignment;
    diLUTHandle		     fontTable;
    register ddPointer	     ptr, save_ptr;
    miTextFontEntry          *ptr1;
    pexMonoEncoding	    *mono_enc;
    int			     fragnum, charnum, some_characters, signum, bytes;
    CARD32		     charval;
    diFontHandle	     font_handle;
    miFontHeader	    *font;
    Ch_stroke_data	    *char_data;
    ddFLOAT		     sp;
    Meta_font		     meta_font;
    pexCoord2D		     cur, end, cpt;
    float	     	     xmin, xmax, ymin, ymax;
    ddTextFontEntry	    *fontEntry;
    ddUSHORT                 es;
    extern void              micalc_cpt_and_align();
    
    *count_ret = 0;

    fontIndex = pddc->Static.attrs->textFont;
    expansion = ABS(pddc->Static.attrs->charExpansion);
    spacing = pddc->Static.attrs->charSpacing;
    path = pddc->Static.attrs->atextPath;
    sp = spacing * FONT_COORD_HEIGHT;
    pAlignment = &(pddc->Static.attrs->atextAlignment);

    /* Get the handle for font table */

    fontTable = pRend->lut[PEXTextFontLUT];
    
    /* Inquire the Font table to get the ddTextFontEntry member */

    if ((InquireLUTEntryAddress (PEXTextFontLUT, fontTable, fontIndex, &es, 
				(ddPointer *)&ptr1)) == PEXLookupTableError)
      return (PEXLookupTableError);

    fontEntry = &ptr1->entry;
    
    /* Allocate space for stroke definitions of all chars in ISTRING */

    if (!((tx_el->paths) = (miCharPath *) xalloc(numChars *
						sizeof(miCharPath))))
      return (BadAlloc);

    /* signum is used later on to encapsulate addition vs. subtraction */

    if (path == PEXPathRight || path == PEXPathUp)
	signum = 1;
    else
	signum = -1;
		    
    ptr = (ddPointer) pString;
    
    /* Process the input ISTRING */

    meta_font.top = -1.0e20;
    meta_font.bottom = 1.0e20;
    meta_font.width = 1.0e-20;
	
    xmin = xmax = ymin = ymax = 0.0;
    cpt.x = cpt.y = 0.0;
    cur.x = end.x = cur.y = end.y = 0.0;
	
    some_characters = 0; /* Make TRUE when a valid character is found */
	
    save_ptr = ptr;        /* Save this for later use */
	
    /* First determine the largest character box size within ISTRING */
    /* Do for each MONO_ENCODING fragment within the ISTRING */

    for (fragnum = 0; fragnum < numFragments; fragnum++) {
	
      mono_enc = (pexMonoEncoding *)ptr;
      ptr += sizeof(pexMonoEncoding);
	    
      if (mono_enc->characterSet < 1 ||
	  mono_enc->characterSet > fontEntry->numFonts)
	  mono_enc->characterSet = 1;

      font_handle = fontEntry->fonts[mono_enc->characterSet - 1];

      /* This is the font that this MONO_ENCODING would be rendered with  */

      font = (miFontHeader *)(font_handle->deviceData);
	    
      /* Bump up ISTRINGS extremes if appropriate */

      if (font->top > meta_font.top) 
	meta_font.top = font->top;
      if (font->bottom < meta_font.bottom)
	meta_font.bottom = font->bottom;
      if (font->max_width > meta_font.width)
	meta_font.width = font->max_width;

      bytes = mono_enc->numChars *
	  ((mono_enc->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((mono_enc->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));

      ptr += (bytes + PADDING (bytes));
    }

    /* Now get the character definition and the required per character */
    /* translation component required to be applied during rendering.  */

    ptr = save_ptr;   /* Restore the ptr */
    CharPtr = tx_el->paths; /* Initialize the pointer to character data */

    for (fragnum = 0; fragnum < numFragments; fragnum++) {
	
      mono_enc = (pexMonoEncoding *)ptr;
      ptr += sizeof(pexMonoEncoding);
	    
      font_handle = fontEntry->fonts[mono_enc->characterSet - 1];
      font = (miFontHeader *)(font_handle->deviceData);

      /* Do for each character within the MONO_ENCODING */

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
		
	if (!(font->ch_data[charval]))	/* undefined character */
	  if (font->font_info.defaultGlyph == 0 &&
	      font->font_info.firstGlyph > 0)  /* no default */
	    /* no extent info is calculated for undefined indices
	     * in charsets where there is no default glyph */
	    continue;   
	  else
	    charval = font->font_info.defaultGlyph;

	some_characters = 1;
	char_data = font->ch_data[charval];  /* Get strokes for char */

	switch (path) {
		
	   case PEXPathRight :
	   case PEXPathLeft :
	     end.x = cur.x + signum*(char_data->right)*expansion;
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
	     cur.x += -char_data->right * 0.5 * expansion;
	     break;
	}

	/* Save the stroke definitions for the character */

	CharPtr->path = &(char_data->strokes);

	/* Save the translation per character */

	(CharPtr->trans).x = cur.x;
	(CharPtr->trans).y = cur.y;

	/* Set cur.x back to lower left corner of character box  */
	/* for the cases of PEXPathUp and PEXPathDown.           */

	if (path==PEXPathUp || path==PEXPathDown)
	  cur.x -= -char_data->right * 0.5 * expansion;
		
	CharPtr++; (*count_ret)++;  /* Update pointer and count */

      }  /* for each character */
	    
      ptr += PADDING(mono_enc->numChars * 
		     ((mono_enc->characterSetWidth == PEXCSByte) 
		      ? sizeof(CARD8) 
		      : ((mono_enc->characterSetWidth == PEXCSShort) 
			 ? sizeof(CARD16) 
			 : sizeof(CARD32))));
				
    } /* for each MONO_ENCODING (fragment) */
	
    /* Compute the alignment and concatenation point; however, */
    /* the concatenation point (cpt) can be ignored here !     */

    if (some_characters) {
	    
      micalc_cpt_and_align(&meta_font, &xmin, &xmax, &ymin, &ymax, path, 
			 expansion, pAlignment, &cpt, align_pt);
						       
    } else {
      /* no valid characters */
      xmin = xmax = ymin = ymax = 0.0;
      cpt.x = cpt.y = align_pt->x = align_pt->y = 0.0;
    }
	
    return (Success);
}				 


void
text2_xform( pos, attrs, align, xf, aflag)
    ddCoord2D		*pos;
    miDDContextRendAttrs *attrs;
    ddVector2D		*align;
    register ddFLOAT	xf[4][4];
    ddUSHORT            aflag;
{
    ddFLOAT		ht_scale, inv_mag;
    ddCoord2D	        vup;
    ddCoord2D	        vbase;
    register ddFLOAT     a, b;
    ddFLOAT		temp[4][4], temp1[4][4];

    /* Get the text or annotation text attribute values */

    if (aflag == 0) {
      ht_scale = ABS(attrs->charHeight / HEIGHT);
      vup.x = attrs->charUp.x;
      vup.y = attrs->charUp.y;
    }
    else {
      ht_scale = ABS(attrs->atextHeight / HEIGHT);
      vup.x = attrs->atextUp.x;
      vup.y = attrs->atextUp.y;
    }

    inv_mag = 1.0 / sqrt( vup.x * vup.x + vup.y * vup.y);
    vup.x *= inv_mag;
    vup.y *= inv_mag;

    /* Compute base vector = up vector rotated by 90 in Z */

    vbase.x = vup.y;
    vbase.y = -vup.x;

    inv_mag = 1.0 / sqrt( vbase.x * vbase.x + vbase.y * vbase.y);
    vbase.x *= inv_mag;
    vbase.y *= inv_mag;

    a = -ht_scale * align->x;
    b = -ht_scale * align->y;

    /* Initialize temp to identity */

    miMatIdent (temp);

    /* Store the scaling components */

    temp[0][0] = temp[1][1] = ht_scale;

    /* Store the translation components */

    temp[0][3] = a;
    temp[1][3] = b;

    /* Let temp1 hold the base vector, the up vector, and the
       text position. */

    temp1[0][0] = vbase.x;
    temp1[0][1] = vup.x;
    temp1[0][2] = 0.0;
    temp1[0][3] = pos->x;
   
    temp1[1][0] = vbase.y;
    temp1[1][1] = vup.y;
    temp1[1][2] = 0.0;
    temp1[1][3] = pos->y;

    temp1[2][0] = temp1[3][0] = 0.0;
    temp1[2][1] = temp1[3][1] = 0.0;
    temp1[2][2] = temp1[3][3] = 1.0;
    temp1[2][3] = 0.0;
    temp1[3][2] = 0.0;

    miMatMult( xf, temp, temp1 );
}


void
text3_xform( pos, u, v, attrs, align, xf, aflag)
    ddCoord3D		*pos;
    register ddVector3D	*u, *v;
    miDDContextRendAttrs *attrs;
    ddVector2D		*align;
    register ddFLOAT	xf[4][4];
    ddUSHORT            aflag;
{
    ddFLOAT		ht_scale, inv_mag;
    ddCoord3D		vup;
    ddCoord3D	        vbase; 
    register ddFLOAT	a, b;
    ddFLOAT		temp[4][4], temp1[4][4];
    ddFLOAT             temp2[4][4], temp3[4][4];
    ddVector3D		e_one, e_two, e_three;
    register ddVector3D	*e3 = &e_three, *e2 = &e_two, *e1 = &e_one;

    /* Get the text or annotation text attribute values */

    if (aflag == 0) {
      ht_scale = ABS(attrs->charHeight / HEIGHT);
      vup.x = attrs->charUp.x;
      vup.y = attrs->charUp.y;
      vup.z = 0.0;
    }
    else {
      ht_scale = ABS(attrs->atextHeight / HEIGHT);
      vup.x = attrs->atextUp.x;
      vup.y = attrs->atextUp.y;
      vup.z = 0.0;
    }

    inv_mag = 1.0 / sqrt( vup.x * vup.x + vup.y * vup.y);
    vup.x *= inv_mag;
    vup.y *= inv_mag;

    /* Compute base vector = up vector rotated by 90 in Z */

    vbase.x = vup.y;
    vbase.y = -vup.x;
    vbase.z = 0.0;

    inv_mag = 1.0 / sqrt( vbase.x * vbase.x + vbase.y * vbase.y);
    vbase.x *= inv_mag;
    vbase.y *= inv_mag;

    a = -align->x * ht_scale;
    b = -align->y * ht_scale;

    /* Initialize temp to identity */

    miMatIdent (temp);

    /* Store the scaling components */

    temp[0][0] = temp[1][1] = ht_scale;

    /* Store the translation components */

    temp[0][3] = a;
    temp[1][3] = b;

    /* Let temp1 hold the base vector and the up vector */

    temp1[0][0] = vbase.x;
    temp1[0][1] = vup.x;
    temp1[0][2] = 0.0;
    temp1[0][3] = 0.0;

    temp1[1][0] = vbase.y;
    temp1[1][1] = vup.y;
    temp1[1][2] = 0.0;
    temp1[1][3] = 0.0;

    temp1[2][0] = temp1[3][0] = 0.0;
    temp1[2][1] = temp1[3][1] = 0.0;
    temp1[2][2] = temp1[3][3] = 1.0;
    temp1[2][3] = 0.0;
    temp1[3][2] = 0.0;

    /* e3 is the cross-product of direction vectors u and v */

    e3->x = u->y * v->z - u->z * v->y;
    e3->y = u->z * v->x - u->x * v->z;
    e3->z = u->x * v->y - u->y * v->x;
    
    inv_mag = sqrt(e3->x * e3->x + e3->y * e3->y + e3->z * e3->z);

    /* See if the direction is valid or if we can get by with a 2D 
       transform, i.e. if u and v are collinear. */

    if ( MI_ZERO_MAG( inv_mag) ) {
	miMatMult( xf, temp, temp1 );

    } else {	/* Build a 3D transform. */

/* The rotation matrix, temp2, for orienting the text 
   coordinate space consists of the row vectors, e1, e2 and e3,
   and the position. */

/* Normalized vector e3 is row 3 of the rotation matrix */

	inv_mag = 1.0 / inv_mag;
	temp2[0][2] = e3->x*inv_mag; 
	temp2[1][2] = e3->y*inv_mag; 
	temp2[2][2] = e3->z*inv_mag;
	temp2[3][2] = 0.0;

/* e1 is the normalized u vector and is row 1 of the rotation
   matrix. */

	inv_mag = 1.0 / sqrt(u->x * u->x + u->y * u->y + u->z * u->z);
	temp2[0][0] = e1->x = u->x * inv_mag;
	temp2[1][0] = e1->y = u->y * inv_mag;
	temp2[2][0] = e1->z = u->z * inv_mag;
	temp2[3][0] = 0.0;

/* e2 is the normalized v vector, (u X v) X u, and is row 2
   of the rotation matrix. */

	e2->x = e3->y * e1->z - e3->z * e1->y; 
	e2->y = e3->z * e1->x - e3->x * e1->z;
	e2->z = e3->x * e1->y - e3->y * e1->x; 
	inv_mag = 1.0 / sqrt(e2->x * e2->x + e2->y * e2->y + e2->z * e2->z);
	temp2[0][1] = (e2->x *= inv_mag);
	temp2[1][1] = (e2->y *= inv_mag); 
	temp2[2][1] = (e2->z *= inv_mag);
	temp2[3][1] = 0.0;

	temp2[0][3] = pos->x;
	temp2[1][3] = pos->y;
	temp2[2][3] = pos->z;
	temp2[3][3] = 1.0;

/* The final transformation matrix, xf, is the product of the
   3 matrices: temp2 x temp1 x temp. */

        miMatMult( temp3, temp1, temp2 );
	miMatMult( xf, temp, temp3 ); 
    }
}


/*++
 |
 |  Function Name:	miText3D
 |
 |  Function Description:
 |	 Handles the Text 3D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miText3D(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
/* local */
    miTextStruct	*ddText = (miTextStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddUSHORT            numEncodings = ddText->numEncodings;
    ddCoord3D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddCoord3D           *pDirections = ddText->pDirections;/* orientation */
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();

/* Define required temporary variables */

    ddULONG             numChars; /* Needed for xalloc */
    pexMonoEncoding     *pMono;
    ddCoord2D           align;    /* alignment info */
    ddFLOAT             tc_to_cc_xform[4][4];
    ddFLOAT             tc_to_mc_xform[4][4];
    ddFLOAT             buf_xform[4][4];
    miDDContext         *pddc;
    ddFLOAT             exp, tx, ty;
    ddFLOAT             ptx, pty, ptx_first;
    int                 i, j, k;
    int                 count;  /* Count of characters to be rendered */
    ddFLOAT             ei0cc, ei1cc, ei3cc;
    ddFLOAT             ei0mc, ei1mc, ei3mc;
    miCharPath          *save_ptr;
    miListHeader	*mc_path, *mclip_path, *cc_path, *clip_path, *dc_path;
    listofddPoint	*sp;
    XID		        temp;
    int			status;
    ddUSHORT            aflag, clip_mode;

    /* Set the annotation text flag to zero */

    aflag = 0;

    /* Get the DDContext handle for local use */

    pddc = (miDDContext *)pRend->pDDContext;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
	return (Success);


    /* Convert text string into required paths */

    if ((status = tx_el_to_path (pRend, pddc, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text3_xform (pOrigin, pDirections, (pDirections+1),
		 pddc->Static.attrs, &align, text_el.xform, aflag);

    /* Render the paths in text_el as polylines */

    /* Set up the new composite transform first */

    miMatMult (buf_xform, text_el.xform,
	       pddc->Dynamic->mc_to_cc_xform);

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pddc->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    /* Check for modelling clip and set up the volume if required */

    if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {
	ComputeMCVolume(pRend, pddc);    /* Get the current model clip volume */
    }

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Render characters one by one */

      /* Check if the character is not renderable, for e.g., space char. */
      /* If so just skip to next character in the ISTRING and continue.  */

      if (!(text_el.paths->path->ddList)) {
	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);
	text_el.paths++;
	continue;
      }

      /* Modify the composite transform by the previous translation */
      /* and the current scaling in x realizing the char expansion  */

      tx = ptx;
      ty = pty;

      ptx = (ddFLOAT)(((text_el.paths)->trans).x);
      pty = (ddFLOAT)(((text_el.paths)->trans).y);

      /* Check to see if this is the very first character and the text */
      /* path is Up or Down. If so, we need to modify tx by the first  */
      /* character translation to align with the rest of the characters*/
      /* in the string.                                                */

      if ((pddc->Static.attrs->textPath == PEXPathUp ||
	   pddc->Static.attrs->textPath == PEXPathDown) && k == 0)
	tx += ptx;        

      /* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE FIRST  */
      /* CHARACTER IN THE STRING. ptx FOR ALL OTHER CHARACTERS WILL BE */
      /* RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE REQUIRED */
      /* EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS NOT A PROBLEM HERE*/
      /* SINCE WE NEED THIS SPECIAL CASE ONLY FOR THE FIRST CHARACTER. */
      /*                                                               */
      /* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE OF +=   */

      /* Check to see if the text path is Left. If so, we need to modify */
      /* tx by the first character width so as to start the string to the*/
      /* left of the text origin.                                        */

      if (k == 0) {
	ptx_first = ptx; /* Get the first character translation */

	/* Adjust the translation by character spacing factor to get */
	/* just the character width.                                 */

	ptx_first += (pddc->Static.attrs->charSpacing) * FONT_COORD_HEIGHT;
      }

      if (pddc->Static.attrs->textPath == PEXPathLeft)
	tx += ptx_first;

      /* Check to see if modelling clip is required. If so, apply it */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Buffer the tc_to_mc_xform first */

	  memcpy( (char *)tc_to_mc_xform, (char *)text_el.xform, 16*sizeof(ddFLOAT));

	  /* Apply the per character translation and scaling by directly */
	  /* modifying the concerned matrix elements.                    */

	  for (i=0; i<4; ++i) {
	      /* Buffer the element values */
	      ei0mc = tc_to_mc_xform[i][0];
	      ei1mc = tc_to_mc_xform[i][1];
	      ei3mc = tc_to_mc_xform[i][3];
	      /* Modify the transform */
	      tc_to_mc_xform[i][0] = exp * ei0mc;
	      tc_to_mc_xform[i][3] = tx * ei0mc + ty * ei1mc + ei3mc;
	  }
	  /* Transform the character strokes into Model space */

	  if (status = miTransform(pddc, text_el.paths->path, &mc_path, 
			 	   tc_to_mc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

	  if (status = miClipPolyLines(pddc, mc_path, &mclip_path, MI_MCLIP))
	      return (status);

      }
      else {
	  mclip_path = text_el.paths->path;
      }

      /* Buffer the tc_to_cc_xform first */

      memcpy( (char *)tc_to_cc_xform, (char *)buf_xform, 16*sizeof(ddFLOAT));

      /* Apply the per character translation and scaling by directly */
      /* modifying the concerned matrix elements.                    */

      for (i=0; i<4; ++i) {
	  /* Buffer the element values */
	  ei0cc = tc_to_cc_xform[i][0];
	  ei1cc = tc_to_cc_xform[i][1];
	  ei3cc = tc_to_cc_xform[i][3];
	  /* Modify the transform */
	  tc_to_cc_xform[i][0] = exp * ei0cc;
	  tc_to_cc_xform[i][3] = tx * ei0cc + ty * ei1cc + ei3cc;
      }

      /* Transform and clip the paths corresponding to current character */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Note that we are already in Model space here */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   pddc->Dynamic->mc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }
      else {

	  /* Note that we are still in text local space here ! */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   tc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }

      clip_mode = MI_VCLIP;
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* if nothing left, then update pointers and continue */
      if (clip_path->numLists <= 0) {
	  text_el.paths++;
	  continue;
      }

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
          return (status);

      /* Render Text to screen */

      pddc->Static.RenderProcs[TEXT_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);
      /* Update the pointer to next character */

      text_el.paths++;
    }

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}

/*++
 |
 |  Function Name:	miText2D
 |
 |  Function Description:
 |	 Handles the Text 2D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miText2D(pRend, pExecuteOC)
/* in */ 
    ddRendererPtr       pRend;	  /* renderer handle */ 
    miGenericStr       *pExecuteOC;
/* out */
{
/* local */
    miText2DStruct	*ddText = (miText2DStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddCoord2D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddUSHORT            numEncodings = ddText->numEncodings;
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();

/* Define required temporary variables */

    ddULONG             numChars; /* Needed for xalloc */
    pexMonoEncoding     *pMono;
    ddCoord2D           align;    /* alignment info */
    ddFLOAT             tc_to_cc_xform[4][4];
    ddFLOAT             tc_to_mc_xform[4][4];
    ddFLOAT             buf_xform[4][4];
    miDDContext         *pddc;
    ddFLOAT             exp, tx, ty;
    ddFLOAT             ptx, pty, ptx_first;
    int                 i, j, k;
    int                 count;  /* Count of characters to be rendered */
    ddFLOAT             ei0cc, ei1cc, ei3cc;
    ddFLOAT             ei0mc, ei1mc, ei3mc;
    miCharPath          *save_ptr;
    miListHeader	*mc_path, *mclip_path, *cc_path, *clip_path, *dc_path;
    listofddPoint	*sp;
    XID		        temp;
    int			status;
    ddUSHORT            aflag, clip_mode;

    /* Set the annotation text flag to zero */

    aflag = 0;

    /* Get the DDContext handle for local use */

    pddc = (miDDContext *)pRend->pDDContext;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
	return (Success);


    /* Convert text string into required paths */

    if ((status = tx_el_to_path (pRend, pddc, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text2_xform (pOrigin, pddc->Static.attrs, &align, text_el.xform, aflag);

    /* Render the paths in text_el as polylines */

    /* Set up the new composite transform first */

    miMatMult (buf_xform, text_el.xform,
	       pddc->Dynamic->mc_to_cc_xform);

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pddc->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    /* Check for modelling clip and set up the volume if required */

    if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {
	ComputeMCVolume(pRend, pddc);    /* Get the current model clip volume */
    }

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Render characters one by one */

      /* Check if the character is not renderable, for e.g., space char. */
      /* If so just skip to next character in the ISTRING and continue.  */

      if (!(text_el.paths->path->ddList)) {
	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);
	text_el.paths++;
	continue;
      }

      /* Modify the composite transform by the previous translation */
      /* and the current scaling in x realizing the char expansion  */

      tx = ptx;
      ty = pty;

      ptx = (ddFLOAT)(((text_el.paths)->trans).x);
      pty = (ddFLOAT)(((text_el.paths)->trans).y);

      /* Check to see if this is the very first character and the text */
      /* path is Up or Down. If so, we need to modify tx by the first  */
      /* character translation to align with the rest of the characters*/
      /* in the string.                                                */

      if ((pddc->Static.attrs->textPath == PEXPathUp ||
	   pddc->Static.attrs->textPath == PEXPathDown) && k == 0)
	tx += ptx;

      /* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE FIRST  */
      /* CHARACTER IN THE STRING. ptx FOR ALL OTHER CHARACTERS WILL BE */
      /* RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE REQUIRED */
      /* EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS NOT A PROBLEM HERE*/
      /* SINCE WE NEED THIS SPECIAL CASE ONLY FOR THE FIRST CHARACTER. */
      /*                                                               */
      /* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE OF +=   */

      /* Check to see if the text path is Left. If so, we need to modify */
      /* tx by the first character width so as to start the string to the*/
      /* left of the text origin.                                        */

      if (k == 0) {
	ptx_first = ptx; /* Get the first character translation */

	/* Adjust the translation by character spacing factor to get */
	/* just the character width.                                 */

	ptx_first += (pddc->Static.attrs->charSpacing) * FONT_COORD_HEIGHT;
      }

      if (pddc->Static.attrs->textPath == PEXPathLeft)
	tx += ptx_first;

      /* Check to see if modelling clip is required. If so, apply it */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Buffer the tc_to_mc_xform first */

	  memcpy( (char *)tc_to_mc_xform, (char *)text_el.xform, 16*sizeof(ddFLOAT));

	  /* Apply the per character translation and scaling by directly */
	  /* modifying the concerned matrix elements.                    */

	  for (i=0; i<4; ++i) {
	      /* Buffer the element values */
	      ei0mc = tc_to_mc_xform[i][0];
	      ei1mc = tc_to_mc_xform[i][1];
	      ei3mc = tc_to_mc_xform[i][3];
	      /* Modify the transform */
	      tc_to_mc_xform[i][0] = exp * ei0mc;
	      tc_to_mc_xform[i][3] = tx * ei0mc + ty * ei1mc + ei3mc;
	  }
	  /* Transform the character strokes into Model space */

	  if (status = miTransform(pddc, text_el.paths->path, &mc_path, 
				   tc_to_mc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

	  if (status = miClipPolyLines(pddc, mc_path, &mclip_path, MI_MCLIP))
	      return (status);
      }
      else {
	  mclip_path = text_el.paths->path;
      }

      /* Buffer the tc_to_cc_xform first */

      memcpy( (char *)tc_to_cc_xform, (char *)buf_xform, 16*sizeof(ddFLOAT));

      /* Apply the per character translation and scaling by directly */
      /* modifying the concerned matrix elements.                    */

      for (i=0; i<4; ++i) {
	  /* Buffer the element values */
	  ei0cc = tc_to_cc_xform[i][0];
	  ei1cc = tc_to_cc_xform[i][1];
	  ei3cc = tc_to_cc_xform[i][3];
	  /* Modify the transform */
	  tc_to_cc_xform[i][0] = exp * ei0cc;
	  tc_to_cc_xform[i][3] = tx * ei0cc + ty * ei1cc + ei3cc;
      }

      /* Transform and clip the paths corresponding to current character */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Note that we are already in Model space here */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   pddc->Dynamic->mc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

      }
      else {

	  /* Note that we are still in text local space here ! */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   tc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }

      clip_mode = MI_VCLIP;
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* if nothing left, then update pointers and continue */
      if (clip_path->numLists <= 0) {
	  text_el.paths++;
	  continue;
      }

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
          return (status);

      /* Render Text to screen */

      pddc->Static.RenderProcs[TEXT_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);

      /* Update the pointer to next character */

      text_el.paths++;
    }

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}


/*++
 |
 |  Function Name:	miAnnoText3D
 |
 |  Function Description:
 |	 Handles the  Annotation text 3D  ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miAnnoText3D(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    /* local */
    miAnnoTextStruct	*ddText = (miAnnoTextStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddUSHORT            numEncodings = ddText->numEncodings;
    ddCoord3D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddCoord3D           *pOffset = ddText->pOffset;
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();

/* Define required temporary variables */

    ddULONG             numChars; /* Needed for xalloc */
    pexMonoEncoding     *pMono;
    ddCoord2D           align;    /* alignment info */
    ddFLOAT             tc_to_cc_xform[4][4];
    ddFLOAT             tc_to_mc_xform[4][4];
    ddFLOAT             buf_xform[4][4];
    ddFLOAT             buf1_xform[4][4];
    miDDContext         *pddc;
    ddFLOAT             exp, tx, ty;
    ddFLOAT             ptx, pty, ptx_first, pty_first;
    int                 i, j, k;
    int                 count;  /* Count of characters to be rendered */
    ddFLOAT             ei0cc, ei1cc, ei3cc;
    ddFLOAT             ei0mc, ei1mc, ei3mc;
    miCharPath          *save_ptr;
    miListHeader	*mc_path, *mclip_path, *cc_path, *clip_path, *dc_path;
    listofddPoint	*sp;
    XID		        temp;
    int			status;
    ddUSHORT            aflag;
    static ddVector3D   Directions[2] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    ddCoord3D           *pDirections = (ddCoord3D *)Directions;
    miListHeader        Connector;
    ddCoord4D           CC_Offset;  /* Offset in Clipping Coordinates */
    ddCoord4D           MC_Origin, CC_Origin, NPC_Origin;
    ddUSHORT            oc;         /* Outcode for 4D point clipping */
    ddUSHORT            clip_mode;  /* distinguish model from view " */

    /* Get the DDContext handle for local use */

    pddc = (miDDContext *)pRend->pDDContext;

    /* Transform and clip the text origin first to see if any rendering*/
    /* needs to be done at all. If the NPC sub-volume does not contain */
    /* the origin, the annotation text is not rendered.                */

    MC_Origin.x = pOrigin->x;
    MC_Origin.y = pOrigin->y;
    MC_Origin.z = pOrigin->z;
    MC_Origin.w = 1.0;

    if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

      ComputeMCVolume(pRend, pddc);      /* Compute  modelling coord version
                                   of clipping volume */
      clip_mode = MI_MCLIP;
      CLIP_POINT4D(&MC_Origin, oc, clip_mode);

      if (oc) return (Success); /* origin model clipped out */
    }

    miTransformPoint (&MC_Origin, pddc->Dynamic->mc_to_cc_xform,
		      &CC_Origin);

    clip_mode = MI_VCLIP;
    CLIP_POINT4D(&CC_Origin, oc, clip_mode);
    if (oc) {
      return (Success);  /* origin view clipped out */
    }

    /* Compute the NPC_Origin for later use */

    miMatMult (buf_xform, pddc->Dynamic->mc_to_wc_xform,
	       pddc->Dynamic->wc_to_npc_xform);
    miTransformPoint (&MC_Origin, buf_xform, &NPC_Origin);

    /* Set the annotation text flag to one */

    aflag = 1;

    /* Transform the NPC Offset to CC. Note that we are transforming a */
    /* vector. i.e., only scaling components of the viewport transform */
    /* need be applied. This is simply done by multiplication of the x */
    /* and y components by the respective scale factors.               */

    CC_Offset.x = pOffset->x * pddc->Dynamic->npc_to_cc_xform[0][0];
    CC_Offset.y = pOffset->y * pddc->Dynamic->npc_to_cc_xform[1][1];
    CC_Offset.z = pOffset->z * pddc->Dynamic->npc_to_cc_xform[2][2];
    CC_Offset.w = 0.0;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
	return (Success);


    /* Convert text string into required paths */

    if ((status = atx_el_to_path (pRend, pddc, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text3_xform (pOrigin, pDirections, (pDirections+1),
		 pddc->Static.attrs, &align, text_el.xform, aflag);

    /* Set up the new composite transform first. Note that in the case */
    /* of annotation text, only the text origin is transformed by the  */
    /* complete pipeline transform. The text itself is affected only by*/
    /* the transformed origin in NPC, the NPC offset , npc_to_cc, and  */
    /* the workstation transform.                                      */

    /* Now compute the initial composite transform for the first char.  */
    /* The required transforms for characters are - text space to model */
    /* space transform, transformation of the annotation text origin, if*/
    /* any, and the npc to cc transform.                                */

    /* Get the translation due to the transformation of the annotation  */
    /* text origin by mc_to_npc_xform into buf1_xform.                  */

    memcpy( (char *)buf1_xform, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    buf1_xform[0][3] += NPC_Origin.x - MC_Origin.x;
    buf1_xform[1][3] += NPC_Origin.y - MC_Origin.y;
    buf1_xform[2][3] += NPC_Origin.z - MC_Origin.z;

    miMatMult (buf_xform, text_el.xform, buf1_xform);
    miMatMult (buf_xform, buf_xform, pddc->Dynamic->npc_to_cc_xform);

    /* Add the offset in CC */

    buf_xform[0][3] += CC_Offset.x;
    buf_xform[1][3] += CC_Offset.y;
    buf_xform[2][3] += CC_Offset.z;

    /******** Render the text string first, and then the connector ********/

    /* Render the paths in text_el as polylines */

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pddc->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Render characters one by one */

      /* Check if the character is not renderable, for e.g., space char. */
      /* If so just skip to next character in the ISTRING and continue.  */

      if (!(text_el.paths->path->ddList)) {
	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);
	text_el.paths++;
	continue;
      }

      /* Modify the composite transform by the previous translation */
      /* and the current scaling in x realizing the char expansion  */

      tx = ptx;
      ty = pty;

      ptx = (ddFLOAT)(((text_el.paths)->trans).x);
      pty = (ddFLOAT)(((text_el.paths)->trans).y);

      /* Check to see if this is the very first character and the text */
      /* path is Up or Down. If so, we need to modify tx by the first  */
      /* character translation to align with the rest of the characters*/
      /* in the string.                                                */

      if ((pddc->Static.attrs->atextPath == PEXPathUp ||
	   pddc->Static.attrs->atextPath == PEXPathDown) && k == 0)
	tx += ptx;

      /* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE FIRST  */
      /* CHARACTER IN THE STRING. ptx FOR ALL OTHER CHARACTERS WILL BE */
      /* RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE REQUIRED */
      /* EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS NOT A PROBLEM HERE*/
      /* SINCE WE NEED THIS SPECIAL CASE ONLY FOR THE FIRST CHARACTER. */
      /*                                                               */
      /* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE OF +=   */

      if (k == 0) {
	ptx_first = ptx; /* Get the first character translation */

	/* Adjust the translation by character spacing factor to get */
	/* just the character width.                                 */

	ptx_first += (pddc->Static.attrs->charSpacing) * FONT_COORD_HEIGHT;

	pty_first = pty; /* Save first character height */
      }

      /* Check to see if the text path is Left. If so, we need to modify */
      /* tx by the first character width so as to start the string to the*/
      /* left of the text origin.                                        */

      if (pddc->Static.attrs->atextPath == PEXPathLeft)
	tx += ptx_first;

      /* Check to see if modelling clip is required. If so, apply it */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Buffer the tc_to_mc_xform first */

	  memcpy( (char *)tc_to_mc_xform, (char *)text_el.xform, 16*sizeof(ddFLOAT));

	  /* Apply the per character translation and scaling by directly */
	  /* modifying the concerned matrix elements.                    */

	  for (i=0; i<4; ++i) {
	      /* Buffer the element values */
	      ei0mc = tc_to_mc_xform[i][0];
	      ei1mc = tc_to_mc_xform[i][1];
	      ei3mc = tc_to_mc_xform[i][3];
	      /* Modify the transform */
	      tc_to_mc_xform[i][0] = exp * ei0mc;
	      tc_to_mc_xform[i][3] = tx * ei0mc + ty * ei1mc + ei3mc;
	  }
	  /* Transform the character strokes into Model space */

	  if (status = miTransform(pddc, text_el.paths->path, &mc_path, 
				   tc_to_mc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

	  if (status = miClipPolyLines(pddc, mc_path, &mclip_path, MI_MCLIP))
	      return (status);
      }
      else {
	  mclip_path = text_el.paths->path;
      }

      /* Buffer the tc_to_cc_xform first */
      memcpy( (char *)tc_to_cc_xform, (char *)buf_xform, 16*sizeof(ddFLOAT));

      /* Apply the per character translation and scaling by directly */
      /* modifying the concerned matrix elements.                    */

      for (i=0; i<4; ++i) {
	/* Buffer the element values */
	ei0cc = tc_to_cc_xform[i][0];
	ei1cc = tc_to_cc_xform[i][1];
	ei3cc = tc_to_cc_xform[i][3];
	/* Modify the transform */
	tc_to_cc_xform[i][0] = exp * ei0cc;
	tc_to_cc_xform[i][3] = tx * ei0cc + ty * ei1cc + ei3cc;
      }

      /* Transform and clip the paths corresponding to current character */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Note that we are already in Model space here */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   pddc->Dynamic->mc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }
      else {

	  /* Note that we are still in text local space here ! */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   tc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }

      /* Clip */

      clip_mode = MI_VCLIP; 
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* if nothing left, then update pointers and continue */
      if (clip_path->numLists <= 0) {
	  text_el.paths++;
	  continue;
      }

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
          return (status);

      /* Render Text to screen */

      pddc->Static.RenderProcs[TEXT_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);

      /* Update the pointer to next character */

      text_el.paths++;
    }

    /* Check the annotation style and draw a connecting line to the text */

    if (pddc->Static.attrs->atextStyle == 2) {

      /* Use the offset and text origin to build a polyline */

      Connector.type = DD_3D_POINT;
      Connector.numLists = 1;
      if (!(Connector.ddList = (listofddPoint *) 
	                       xalloc(sizeof(listofddPoint))))
	return (BadAlloc);
      Connector.ddList->numPoints = 2;
      if (!((Connector.ddList->pts.p3Dpt) = (ddCoord3D *) 
	                                    xalloc(sizeof(ddCoord3D) * 2)))
      return (BadAlloc);

      Connector.ddList->pts.p3Dpt->x = pOrigin->x;
      Connector.ddList->pts.p3Dpt->y = pOrigin->y;
      Connector.ddList->pts.p3Dpt->z = pOrigin->z;
      Connector.ddList->pts.p3Dpt++;
      Connector.ddList->pts.p3Dpt->x = pOrigin->x;
      Connector.ddList->pts.p3Dpt->y = pOrigin->y;
      Connector.ddList->pts.p3Dpt->z = pOrigin->z;
      Connector.ddList->pts.p3Dpt--;   /* Reset pointer to head of the list */

      /* Render the connector as a polyline */

      /* Transform and clip the connector polyline */

      if (status = miTransform(pddc, &Connector, &cc_path, 
			       pddc->Dynamic->mc_to_cc_xform,
			       NULL4x4,
			       DD_HOMOGENOUS_POINT))
	return (status);

      /* Modify the second point by the amount of transformed offset.   */

      cc_path->ddList->pts.p4Dpt++;
      cc_path->ddList->pts.p4Dpt->x += CC_Offset.x;
      cc_path->ddList->pts.p4Dpt->y += CC_Offset.y;
      cc_path->ddList->pts.p4Dpt->z += CC_Offset.z;
      cc_path->ddList->pts.p4Dpt--; /* Reset pointer to head of the list */

      /* Clip */

      clip_mode = MI_VCLIP; 
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
	  return (status);

      /* Render Connector to screen */

      pddc->Static.RenderProcs[POLYLINE_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);

    }  /* if atextStyle == 2 */

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}

/*++
 |
 |  Function Name:	miAnnoText2D
 |
 |  Function Description:
 |	 Handles the Annotation text 2D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miAnnoText2D(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    /* local */
    miAnnoText2DStruct	*ddText = (miAnnoText2DStruct *)(pExecuteOC+1);
    miTextElement       text_el;            /* text element */
    ddUSHORT            numEncodings = ddText->numEncodings;
    ddCoord2D           *pOrigin = ddText->pOrigin;  /* string origin */
    ddCoord2D           *pOffset = ddText->pOffset;
    pexMonoEncoding     *pText = ddText->pText;	  /* text string */

/* calls */
    extern ddpex3rtn    miTransform();
    extern ddpex3rtn    miClipPolyLines();

/* Define required temporary variables */

    ddULONG             numChars; /* Needed for xalloc */
    pexMonoEncoding     *pMono;
    ddCoord2D           align;    /* alignment info */
    ddFLOAT             tc_to_cc_xform[4][4];
    ddFLOAT             tc_to_mc_xform[4][4];
    ddFLOAT             buf_xform[4][4];
    ddFLOAT             buf1_xform[4][4];
    miDDContext         *pddc;
    ddFLOAT             exp, tx, ty;
    ddFLOAT             ptx, pty, ptx_first, pty_first;
    int                 i, j, k;
    int                 count;  /* Count of characters to be rendered */
    ddFLOAT             ei0cc, ei1cc, ei3cc;
    ddFLOAT             ei0mc, ei1mc, ei3mc;
    miCharPath          *save_ptr;
    miListHeader	*mc_path, *mclip_path, *cc_path, *clip_path, *dc_path;
    listofddPoint	*sp;
    XID		        temp;
    int			status;
    ddUSHORT            aflag;
    miListHeader        Connector;
    ddCoord4D           CC_Offset;  /* Offset in Clipping Coordinates */
    ddCoord4D           MC_Origin, CC_Origin, NPC_Origin;
    ddUSHORT            oc;         /* Outcode for 4D point clipping */
    ddUSHORT            clip_mode;

    /* Get the DDContext handle for local use */

    pddc = (miDDContext *)pRend->pDDContext;

    /* Transform and clip the text origin first to see if any rendering*/
    /* needs to be done at all. If the NPC sub-volume does not contain */
    /* the origin, the annotation text is not rendered.                */

    MC_Origin.x = pOrigin->x;
    MC_Origin.y = pOrigin->y;
    MC_Origin.z = 0.0;
    MC_Origin.w = 1.0;

    if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

      ComputeMCVolume(pRend, pddc);	/*  Compute  modelling coord version
					    of clipping volume */
      clip_mode = MI_MCLIP;
      CLIP_POINT4D(&MC_Origin, oc, clip_mode);

      if (oc) return (Success); /* origin model clipped out */
    }

    miTransformPoint (&MC_Origin, pddc->Dynamic->mc_to_cc_xform,
		      &CC_Origin);

    clip_mode = MI_VCLIP;
    CLIP_POINT4D(&CC_Origin, oc, clip_mode);
    if (oc) {
      return (Success);  /* Don't render anything; origin clipped out */
    }

    /* Compute the NPC_Origin for later use */

    miMatMult (buf_xform, pddc->Dynamic->mc_to_wc_xform,
	       pddc->Dynamic->wc_to_npc_xform);
    miTransformPoint (&MC_Origin, buf_xform, &NPC_Origin);

    /* Set the annotation text flag to one */

    aflag = 1;

    /* Transform the NPC Offset to CC. Note that we are transforming a */
    /* vector. i.e., only scaling components of the viewport transform */
    /* need be applied. This is simply done by multiplication of the x */
    /* and y components by the respective scale factors.               */

    CC_Offset.x = pOffset->x * pddc->Dynamic->npc_to_cc_xform[0][0];
    CC_Offset.y = pOffset->y * pddc->Dynamic->npc_to_cc_xform[1][1];
    CC_Offset.z = 0.0;
    CC_Offset.w = 0.0;

    /* Determine the total number of characters in the ISTRING */

    numChars = 0;
    pMono = pText;
    for (i=0; i<numEncodings; i++) {
      int bytes = pMono->numChars * ((pMono->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pMono->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));
      numChars += (ddULONG)pMono->numChars;
      pMono = (pexMonoEncoding *) ((char *) (pMono + 1) +
	  bytes + PADDING (bytes));
    }

    if (numChars == 0)
	return (Success);


    /* Convert text string into required paths */

    if ((status = atx_el_to_path (pRend, pddc, numEncodings, pText,
	numChars, &text_el, &align, &count)) != Success) {
      return (status);
    }

    /* Compute the required Character Space to Modelling Space Transform */

    text2_xform (pOrigin, pddc->Static.attrs, &align, text_el.xform, aflag);

    /* Set up the new composite transform first. Note that in the case */
    /* of annotation text, only the text origin is transformed by the  */
    /* complete pipeline transform. The text itself is affected only by*/
    /* the transformed origin in NPC, the NPC offset , npc_to_cc, and  */
    /* the workstation transform.                                      */

    /* Now compute the initial composite transform for the first char.  */
    /* The required transforms for characters are - text space to model */
    /* space transform, transformation of the annotation text origin, if*/
    /* any, and the npc to cc transform.                                */

    /* Get the translation due to the transformation of the annotation  */
    /* text origin by mc_to_npc_xform into buf1_xform.                  */

    memcpy( (char *)buf1_xform, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    buf1_xform[0][3] += NPC_Origin.x - MC_Origin.x;
    buf1_xform[1][3] += NPC_Origin.y - MC_Origin.y;

    miMatMult (buf_xform, text_el.xform, buf1_xform);
    miMatMult (buf_xform, buf_xform, pddc->Dynamic->npc_to_cc_xform);

    /* Add the offset in CC */

    buf_xform[0][3] += CC_Offset.x;
    buf_xform[1][3] += CC_Offset.y;

    /******** Render the text string first, and then the connector ********/

    /* Render the paths in text_el as polylines */

    /* Get the current character expansion factor */

    exp = ABS((ddFLOAT)pddc->Static.attrs->charExpansion);

    /* Save the pointer to the beginning of the character data */

    save_ptr = text_el.paths;

    /* Do for all characters (paths) in the text_el */

    /* Initialize the previous translation components */

    ptx = pty = 0.0;

    for (k=0; k<count; k++) {  /* Render characters one by one */

      /* Check if the character is not renderable, for e.g., space char. */
      /* If so just skip to next character in the ISTRING and continue.  */

      if (!(text_el.paths->path->ddList)) {
	ptx = (ddFLOAT)(((text_el.paths)->trans).x);
	pty = (ddFLOAT)(((text_el.paths)->trans).y);
	text_el.paths++;
	continue;
      }

      /* Modify the composite transform by the previous translation */
      /* and the current scaling in x realizing the char expansion  */

      tx = ptx;
      ty = pty;

      ptx = (ddFLOAT)(((text_el.paths)->trans).x);
      pty = (ddFLOAT)(((text_el.paths)->trans).y);

      /* Check to see if this is the very first character and the text */
      /* path is Up or Down. If so, we need to modify tx by the first  */
      /* character translation to align with the rest of the characters*/
      /* in the string.                                                */

      if ((pddc->Static.attrs->atextPath == PEXPathUp ||
	   pddc->Static.attrs->atextPath == PEXPathDown) && k == 0)
	tx += ptx;

      /* NOTE THAT THE ABOVE COMPUTATION WILL ONLY WORK FOR THE FIRST  */
      /* CHARACTER IN THE STRING. ptx FOR ALL OTHER CHARACTERS WILL BE */
      /* RELATIVE TO THE TEXT ORIGIN AND SO WILL NOT GIVE THE REQUIRED */
      /* EFFECTIVE CHARACTER WIDTH. HOWEVER, THIS IS NOT A PROBLEM HERE*/
      /* SINCE WE NEED THIS SPECIAL CASE ONLY FOR THE FIRST CHARACTER. */
      /*                                                               */
      /* FURTHER, NOTE THAT ptx WILL BE NEGATIVE AND HENCE USE OF +=   */

      if (k == 0) {
	ptx_first = ptx; /* Get the first character translation */

	/* Adjust the translation by character spacing factor to get */
	/* just the character width.                                 */

	ptx_first += (pddc->Static.attrs->charSpacing) * FONT_COORD_HEIGHT;

	pty_first = pty; /* Save first character height */

	/* Adjust the translation by character spacing factor to get */
	/* just the character height.                                */

	pty_first += (pddc->Static.attrs->charSpacing) * FONT_COORD_HEIGHT;
      }

      /* Check to see if the text path is Left. If so, we need to modify */
      /* tx by the first character width so as to start the string to the*/
      /* left of the text origin.                                        */

      if (pddc->Static.attrs->atextPath == PEXPathLeft)
	tx += ptx_first;

      /* Check to see if modelling clip is required. If so, apply it */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Buffer the tc_to_mc_xform first */

	  memcpy( (char *)tc_to_mc_xform, (char *)text_el.xform, 16*sizeof(ddFLOAT));

	  /* Apply the per character translation and scaling by directly */
	  /* modifying the concerned matrix elements.                    */

	  for (i=0; i<4; ++i) {
	      /* Buffer the element values */
	      ei0mc = tc_to_mc_xform[i][0];
	      ei1mc = tc_to_mc_xform[i][1];
	      ei3mc = tc_to_mc_xform[i][3];
	      /* Modify the transform */
	      tc_to_mc_xform[i][0] = exp * ei0mc;
	      tc_to_mc_xform[i][3] = tx * ei0mc + ty * ei1mc + ei3mc;
	  }
	  /* Transform the character strokes into Model space */

	  if (status = miTransform(pddc, text_el.paths->path, &mc_path, 
				   tc_to_mc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

	  if (status = miClipPolyLines(pddc, mc_path, &mclip_path, MI_MCLIP))
	      return (status);
      }
      else {
	  mclip_path = text_el.paths->path;
      }

      /* Buffer the tc_to_cc_xform first */

      memcpy( (char *)tc_to_cc_xform, (char *)buf_xform, 16*sizeof(ddFLOAT));

      /* Apply the per character translation and scaling by directly */
      /* modifying the concerned matrix elements.                    */

      for (i=0; i<4; ++i) {
	/* Buffer the element values */
	ei0cc = tc_to_cc_xform[i][0];
	ei1cc = tc_to_cc_xform[i][1];
	ei3cc = tc_to_cc_xform[i][3];
	/* Modify the transform */
	tc_to_cc_xform[i][0] = exp * ei0cc;
	tc_to_cc_xform[i][3] = tx * ei0cc + ty * ei1cc + ei3cc;
      }

      /* Transform and clip the paths corresponding to current character */

      if (pddc->Dynamic->pPCAttr->modelClip == PEXClip) {

	  /* Note that we are already in Model space here */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   pddc->Dynamic->mc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);

      }
      else {

	  /* Note that we are still in text local space here ! */

	  if (status = miTransform(pddc, mclip_path, &cc_path, 
				   tc_to_cc_xform,
				   NULL4x4,
				   DD_HOMOGENOUS_POINT))
	      return (status);
      }

      /* Clip */

      clip_mode = MI_VCLIP; 
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* if nothing left, then update pointers and continue */
      if (clip_path->numLists <= 0) {
	  text_el.paths++;
	  continue;
      }

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
          return (status);

      /* Render Text to screen */

      pddc->Static.RenderProcs[TEXT_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);

      /* Update the pointer to next character */

      text_el.paths++;
    }

    /* Check the annotation style and draw a connecting line to the text */

    if (pddc->Static.attrs->atextStyle == 2) {

      /* Use the offset and the text origin to build a polyline */

      Connector.type = DD_2D_POINT;
      Connector.numLists = 1;
      if (!(Connector.ddList = (listofddPoint *) xalloc(sizeof(listofddPoint))))
	return (BadAlloc);
      Connector.ddList->numPoints = 2;
      if (!((Connector.ddList->pts.p2Dpt) = (ddCoord2D *) 
	                                     xalloc(sizeof(ddCoord2D) * 2)))
	return (BadAlloc);
      Connector.ddList->pts.p2Dpt->x = pOrigin->x;
      Connector.ddList->pts.p2Dpt->y = pOrigin->y;
      Connector.ddList->pts.p2Dpt++;
      Connector.ddList->pts.p2Dpt->x = pOrigin->x;
      Connector.ddList->pts.p2Dpt->y = pOrigin->y;
      Connector.ddList->pts.p2Dpt--;   /* Reset pointer to head of the list */

      /* Render the connector as a polyline */

      /* Transform and clip the connector polyline */

      if (status = miTransform(pddc, &Connector, &cc_path, 
			       pddc->Dynamic->mc_to_cc_xform, 
			       NULL4x4, 
			       DD_HOMOGENOUS_POINT))
	return (status);

      /* Modify the second point by the amount of transformed offset.   */

      cc_path->ddList->pts.p4Dpt++;
      cc_path->ddList->pts.p4Dpt->x += CC_Offset.x;
      cc_path->ddList->pts.p4Dpt->y += CC_Offset.y;
      cc_path->ddList->pts.p4Dpt--; /* Reset pointer to head of the list */

      /* Clip */

      clip_mode = MI_VCLIP; 
      if (status = miClipPolyLines(pddc, cc_path, &clip_path, clip_mode))
          return (status);

      /* Transform to DC coordinates */

      if (status = miTransform(pddc, clip_path, &dc_path, 
			       pddc->Dynamic->cc_to_dc_xform,
			       NULL4x4,
			       DD_2DS_POINT))
	  return (status);

      /* Render Connector to screen */

      pddc->Static.RenderProcs[POLYLINE_RENDER_TABLE_INDEX](pRend, 
							pddc, 
							dc_path);
    }  /* if atextStyle == 2 */

    /* Free up space allocated for text stroke data */

    xfree ((char *)save_ptr);

    return (Success);
}

