/* $Xorg: miConvert.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems,
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miConvert.c,v 1.9 2001/12/14 19:57:20 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miRender.h"
#include "pexos.h"


typedef	void	 (*ColorConversionTableType)();
static void PEXIndexedColour_to_PEXRdrColourModelRGB();
static void PEXRgb8Colour_to_PEXRdrColourModelRGB();
static void PEXRgb16Colour_to_PEXRdrColourModelRGB();
static void NoChange();

/*
 * Color conversion jump table for miConvertVertexColors,
 * miConvertFacetColors and miConvertColor.
 *
 * Note that only conversions supported are 
 *
 *		indexed ->rgbFloat
 *		Rgb8    ->rgbFloat
 *		Rgb16   ->rgbFloat
 *
 */

static 
ColorConversionTableType
ColourConversionRoutine[(PEXRdrColourModelHLS+1)*(PEXMaxColour+1)] = {
/* Convert to Implementation dependant Color Model */
  PEXIndexedColour_to_PEXRdrColourModelRGB,	/* Indexed -> ImpDep */
  NoChange,					/* RgbFloat -> ImpDep */
  NULL,						/* Cie -> ImpDep */
  NULL,						/* Hsv -> ImpDep */
  NULL,						/* Hls -> ImpDep */
  PEXRgb8Colour_to_PEXRdrColourModelRGB,	/* Rgb8 -> ImpDep */
  PEXRgb16Colour_to_PEXRdrColourModelRGB,	/* Rgb16 -> ImpDep */
/* Convert to Rgb Float */
  PEXIndexedColour_to_PEXRdrColourModelRGB,	/* Indexed -> RgbFloat */
  NoChange,					/* RgbFloat -> RgbFloat */
  NULL,						/* Cie -> RgbFloat */
  NULL,						/* Hsv -> RgbFloat */
  NULL,						/* Hls -> RgbFloat */
  PEXRgb8Colour_to_PEXRdrColourModelRGB,	/* Rgb8 -> RgbFloat */
  PEXRgb16Colour_to_PEXRdrColourModelRGB,	/* Rgb16 -> RgbFloat */
/* Convert to Cie Float */
  NULL,						/* Indexed -> CieFloat */
  NULL,						/* RgbFloat -> CieFloat */
  NULL,						/* Cie -> CieFloat */
  NULL,						/* Hsv -> CieFloat */
  NULL,						/* Hls -> CieFloat */
  NULL,						/* Rgb8 -> CieFloat */
  NULL,						/* Rgb16 -> CieFloat */
/* Convert to Hsv Float */
  NULL,						/* Indexed -> HsvFloat */
  NULL,						/* RgbFloat -> HsvFloat */
  NULL,						/* Cie -> HsvFloat */
  NULL,						/* Hsv -> HsvFloat */
  NULL,						/* Hls -> HsvFloat */
  NULL,						/* Rgb8 -> HsvFloat */
  NULL,						/* Rgb16 -> HsvFloat */
/* Convert to Hls Float */
  NULL,						/* Indexed -> HlsFloat */
  NULL,						/* RgbFloat -> HlsFloat */
  NULL,						/* Cie -> HlsFloat */
  NULL,						/* Hsv -> HlsFloat */
  NULL,						/* Hls -> HlsFloat */
  NULL,						/* Rgb8 -> HlsFloat */
  NULL,						/* Rgb16 -> HlsFloat */
};

/*++
 |
 |  Function Name:	PEXIndexedColour_to_PEXRdrColourModelRGB
 |
 |  Function Description:
 |	 Convert vertex colors to the specified rendering color model
 |
 |  Note(s):
 |
 --*/

static
void
PEXIndexedColour_to_PEXRdrColourModelRGB(pRend, in_col, out_col)
ddRendererPtr		pRend;		/* renderer handle */
ddIndexedColour 	**in_col;
ddRgbFloatColour	**out_col;
{
    miColourEntry			*pintcolour;
    ddUSHORT		     	 	status;

    InquireLUTEntryAddress (PEXColourLUT, pRend->lut[PEXColourLUT],
			    ((*in_col)++)->index,
			    &status, 
			    (ddPointer *)&pintcolour);

    /* Insure that LUT entry is in correct color model */
    if (pintcolour->entry.colourType != PEXRgbFloatColour)
     ColourConversionRoutine[pintcolour->entry.colourType*PEXRdrColourModelRGB]
			(pRend, &pintcolour->entry.colour.rgbFloat, out_col);
    else *((*out_col)++) = pintcolour->entry.colour.rgbFloat;

}

/*++
 |
 |  Function Name:	PEXRgb8Colour_to_PEXRdrColourModelRGB
 |
 |  Function Description:
 |	 Convert vertex colors to the specified rendering color model
 |
 |  Note(s):
 |
 --*/

static
void
PEXRgb8Colour_to_PEXRdrColourModelRGB(pRend, in_col, out_col)
ddRendererPtr		pRend;		/* renderer handle */
ddRgb8Colour	 	**in_col;
ddRgbFloatColour	**out_col;
{
    (*out_col)->red = (*in_col)->red;
    (*out_col)->green = (*in_col)->green;
    ((*out_col)++)->blue = ((*in_col)++)->blue;
}

/*++
 |
 |  Function Name:	PEXRgb16Colour_to_PEXRdrColourModelRGB
 |
 |  Function Description:
 |	 Convert vertex colors to the specified rendering color model
 |
 |  Note(s):
 |
 --*/

static
void
PEXRgb16Colour_to_PEXRdrColourModelRGB(pRend, in_col, out_col)
ddRendererPtr		pRend;		/* renderer handle */
ddRgb16Colour	 	**in_col;
ddRgbFloatColour	**out_col;
{
    (*out_col)->red = (*in_col)->red;
    (*out_col)->green = (*in_col)->green;
    ((*out_col)++)->blue = ((*in_col)++)->blue;
}

/*++
 |
 |  Function Name:	NoChange
 |
 |  Function Description:
 |	 Dummy label to indicate no color change needed.
 |
 |  Note(s):
 |
 --*/

static
void
NoChange(pRend, in_col, out_col)
ddRendererPtr		pRend;		/* renderer handle */
ddRgb16Colour	 	**in_col;
ddRgbFloatColour	**out_col;
{
}

/*++
 |
 |  Function Name:	miConvertColor
 |
 |  Function Description:
 |	 Converts a ddColourSpecifier to the specified rendering color model.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miConvertColor(pRend, cinput, rdrColourModel, coutput)
/* in */
ddRendererPtr		pRend;		/* renderer handle */
ddColourSpecifier	*cinput;	/* input color */
ddSHORT			rdrColourModel;	/* output color model */
/* out */
ddColourSpecifier	*coutput;	/* output color */
{
/* uses */
    ColorConversionTableType	convert;
    ddSHORT			input_color;
    char			*icolptr;
    char			*ocolptr;

    /* find proper conversion routine */
    convert = ColourConversionRoutine[cinput->colourType*rdrColourModel];

    /* convert is 1 if input and output color model are the same */
    if (convert == NoChange) {
      /* no conversion necessary */
      *coutput = *cinput;
      return Success;
    }

    /* convert is NULL if output color model not supported */
    if (convert == NULL) {
      return 1;
    }

    /* set output color type */
    switch(rdrColourModel) {
     case PEXRdrColourModelImpDep:
	coutput->colourType = PEXRgbFloatColour;
	break;
     case PEXRdrColourModelRGB:
	coutput->colourType = PEXRgbFloatColour;
	break;
     case PEXRdrColourModelHSV:
	coutput->colourType = PEXHsvFloatColour;
	break;
     case PEXRdrColourModelHLS:
	coutput->colourType = PEXHlsFloatColour;
	break;
     case PEXRdrColourModelCIE:
	coutput->colourType = PEXCieFloatColour;
	break;
    }

    /* convert color data */
    icolptr = (char *)&(cinput->colour.indexed);
    ocolptr = (char *)&(coutput->colour.indexed);
    (*convert)(pRend, &icolptr, &ocolptr);

    return (Success);
}

/*++
 |
 |  Function Name:	miConvertVertexColors
 |
 |  Function Description:
 |	 Convert vertex colors to the specified rendering color model
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miConvertVertexColors(pRend, vinput, rdrColourModel, voutput)
/* in */
ddRendererPtr		pRend;		/* renderer handle */
miListHeader		*vinput;	/* input vertex list */
ddSHORT			rdrColourModel;	/* output color model */
/* out */
miListHeader		**voutput;	/* output vertex list */
{
/* uses */
    ddPointUnion		in_pt, out_pt;
    miListHeader		*output;
    listofddPoint		*pddilist;
    listofddPoint		*pddolist;
    miDDContext			*pddc = (miDDContext *)pRend->pDDContext;
    int				list_count = 0;
    int				vert_count;
    int				point_size, out_point_size;
    int				coord_size;
    ddPointType			type;
    int				i, j;
    ColorConversionTableType	convert;
    ddSHORT			input_color;

    /* if no vertex colors, done! */
    if (!DD_IsVertColour(vinput->type)) {
      *voutput = vinput;
      return Success;
    }

    /* extract vertex color type */
    if (DD_IsVertIndexed(vinput->type)) input_color = PEXIndexedColour;
    else if (DD_IsVertRGBFLOAT(vinput->type)) input_color = PEXRgbFloatColour;
    else if (DD_IsVertRGB8(vinput->type)) input_color = PEXRgb8Colour;
    else if (DD_IsVertRGB16(vinput->type)) input_color = PEXRgb16Colour;
    else if (DD_IsVertHSV(vinput->type)) input_color = PEXHsvFloatColour;
    else if (DD_IsVertHLS(vinput->type)) input_color = PEXHlsFloatColour;
    else if (DD_IsVertCIE(vinput->type)) input_color = PEXCieFloatColour;

    /* find proper conversion routine */
    convert = ColourConversionRoutine[input_color*rdrColourModel];

    /* convert is 1 if input and output color model are the same */
    if (convert == NoChange) {
      /* no conversion necessary */
      *voutput = vinput;
      return Success;
    }

    /* convert is NULL if output color model not supported */
    if (convert == NULL) {
      return 1;
    }

    /* Initialize output list */
    output = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(output, MI_ROUND_LISTHEADERCOUNT(vinput->numLists))
    if (!output->ddList) return(BadAlloc);

    type = vinput->type;
    DD_VertPointSize( (type & (DDPT_SHORT | DDPT_4D)), coord_size );

    /* set output color type */
    switch(rdrColourModel) {
     case PEXRdrColourModelImpDep:
     case PEXRdrColourModelRGB:
	DD_SetVertRGBFLOAT(type);
	break;
     case PEXRdrColourModelHSV:
	DD_SetVertHSV(type);
	break;
     case PEXRdrColourModelHLS:
	DD_SetVertHLS(type);
	break;
     case PEXRdrColourModelCIE:
	DD_SetVertCIE(type);
	break;
    }
    output->type = type;
    DD_VertPointSize( type, out_point_size );

    pddilist = vinput->ddList;
    pddolist = output->ddList;

    /* 
     * Traverse each list.
     */
    for (i = 0; i < vinput->numLists; i++) {

       	  if ((vert_count = pddolist->numPoints = pddilist->numPoints) <= 1) {
	    pddilist++;
	    continue;
          }

	  /* Insure sufficient room for each vertex */
	  MI_ALLOCLISTOFDDPOINT(pddolist, vert_count+1, out_point_size);
	  if (!pddolist->pts.p2DSpt) return(BadAlloc);

	  /* 
	   * Copy each point and initialize the edges.
	   * Note that the edge flag is always the last
	   * ddULONG of a vertex. Thus incrementing the
	   * destination pointer by the size of the input
	   * point "automatically" places the pointer
	   * at the start of the edge flag field.
	   */
          in_pt = pddilist->pts;
          out_pt = pddolist->pts;

	  for (j = 0; j < vert_count; j++) {

	     /* Copy the coordinate data to the output list */
             memcpy( out_pt.ptr, in_pt.ptr, coord_size);
	     in_pt.ptr += coord_size;
	     out_pt.ptr += coord_size;

	     /* convert the color */
	     (*convert)(pRend, &(in_pt.ptr), &(out_pt.ptr));

	     /* Copy the normal data to the output list */
	     if (DD_IsVertNormal(vinput->type))
		*(out_pt.pNormal++) = *(in_pt.pNormal++);

	     /* Copy the edge flag data to the output list */
	     if (DD_IsVertEdge(vinput->type))
		*(out_pt.pEdge++) = *(in_pt.pEdge++);
	  }

	  /* Now, skip to next input list */
          pddilist++;
          pddolist++;
	  list_count++;
    }

    output->numLists = list_count;
    *voutput = output;
    return (Success);
}

/*++
 |
 |  Function Name:	miConvertFacetColors
 |
 |  Function Description:
 |	 Convert vertex colors to the specified rendering color model
 |
 |  Note(s):
 |	 Currently, this will ONLY convert from indexed -> RGBFLOAT
 |
 --*/

ddpex3rtn
miConvertFacetColors(pRend, finput, rdrColourModel, foutput)
/* in */
ddRendererPtr		pRend;		/* renderer handle */
listofddFacet		*finput;	/* input facet list */
ddSHORT			rdrColourModel;	/* output color model */
/* out */
listofddFacet		**foutput;	/* output facet list */
{
    listofddFacet       	*fct_list;
    ddFacetUnion        	in_fct;
    ddFacetUnion        	out_fct;
    miDDContext			*pddc = (miDDContext *)pRend->pDDContext;
    int                 	j;
    int                 	facet_size;
    ColorConversionTableType	convert;
    ddSHORT			input_color;

    /* if no vertex colors, done! */
    if ((!DD_IsFacetColour(finput->type)) || (finput->type == DD_FACET_NONE)) {
      *foutput = finput;
      return Success;
    }

    /* extract facet color type */
    switch(finput->type) {
	case DD_FACET_INDEX:
	case DD_FACET_INDEX_NORM:
	   input_color = PEXIndexedColour;
	   break;
	case DD_FACET_RGB8:
	case DD_FACET_RGB8_NORM:
	   input_color = PEXRgb8Colour;
	   break;
	case DD_FACET_RGB16:
	case DD_FACET_RGB16_NORM:
	   input_color = PEXRgb16Colour;
	   break;
	case DD_FACET_RGBFLOAT:
	case DD_FACET_RGBFLOAT_NORM:
	   input_color = PEXRgbFloatColour;
	   break;
	case DD_FACET_HSV:
	case DD_FACET_HSV_NORM:
	   input_color = PEXHsvFloatColour;
	   break;
	case DD_FACET_HLS:
	case DD_FACET_HLS_NORM:
	   input_color = PEXHlsFloatColour;
	   break;
	case DD_FACET_CIE:
	case DD_FACET_CIE_NORM:
	   input_color = PEXCieFloatColour;
	   break;
    }

    /* find proper conversion routine */
    convert = ColourConversionRoutine[input_color*rdrColourModel];

    /* convert is 1 if input and output color model are the same */
    if (convert == NoChange) {
      /* no conversion necessary */
      *foutput = finput;
      return Success;
    }

    /* convert is NULL if output color model not supported */
    if (convert == NULL) {
      return 1;
    }

    /* Get next free facet list header */
    fct_list = MI_NEXTTEMPFACETLIST(pddc);

    /* set output color type */
    switch(rdrColourModel) {
     case PEXRdrColourModelImpDep:
     case PEXRdrColourModelRGB:
	if (DD_IsFacetNormal(finput->type)) 
	  fct_list->type = DD_FACET_RGBFLOAT_NORM;
	else fct_list->type = DD_FACET_RGBFLOAT;
	break;
     case PEXRdrColourModelHSV:
	if (DD_IsFacetNormal(finput->type)) 
	  fct_list->type = DD_FACET_HSV_NORM;
	else fct_list->type = DD_FACET_HSV;
	break;
     case PEXRdrColourModelHLS:
	if (DD_IsFacetNormal(finput->type)) 
	  fct_list->type = DD_FACET_HLS_NORM;
	else fct_list->type = DD_FACET_HLS;
	break;
     case PEXRdrColourModelCIE:
	if (DD_IsFacetNormal(finput->type)) 
	  fct_list->type = DD_FACET_CIE_NORM;
	else fct_list->type = DD_FACET_CIE;
	break;
    }

    /*
     * Allocate storage for the facet list
     */
    DDFacetSIZE(fct_list->type, facet_size);
    MI_ALLOCLISTOFDDFACET(fct_list, finput->numFacets, facet_size);
    if (!(out_fct.pNoFacet = fct_list->facets.pNoFacet)) return(BadAlloc);

    in_fct = finput->facets;

    /* Remember, facet data is of the form:
     *
     * |--------------|--------------------------|
     *   color (opt)         normal (opt)
     */

    for (j = 0; j < finput->numFacets; j++) {

	/* convert the color */
	(*convert)(pRend, &(in_fct.pNoFacet), &(out_fct.pNoFacet));

	/* Copy the input normal */
	if (DD_IsFacetNormal(finput->type))
	  *(out_fct.pFacetN++) = *(in_fct.pFacetN++);

    }
 
    fct_list->numFacets = finput->numFacets;
    *foutput = fct_list;
 
    return(Success);

}

/*++
 |
 |  Function Name:	miColourtoIndex
 |
 |  Function Description:
 |	 Convert a direct color to an index using the 
 |	 color approximation table.
 |
 |  Note(s):
 |	 Dithering is ignored as there is no screen data with which to dither.
 |
 --*/

ddpex3rtn
miColourtoIndex(pRend, colourApproxIndex, directcolour, colourindex)
/* in */
ddRendererPtr		pRend;		/* renderer handle */
ddTableIndex		colourApproxIndex;	/* colour approx table index */
ddColourSpecifier	*directcolour;	/* Direct colour input */
/* out */
ddULONG			*colourindex;	/* output colour index */
{
      miColourApproxEntry	*pLUT;
      ddColourApproxEntry	*pentry;
      ddUSHORT			status;

      /* Fetch current color approximation table entry */
      if ((InquireLUTEntryAddress (PEXColourApproxLUT,
				   pRend->lut[PEXColourApproxLUT],
				   colourApproxIndex,
				   &status, (ddPointer *)&pLUT))
	   == PEXLookupTableError)
	return (PEXLookupTableError);

      pentry = &pLUT->entry;

      /****************************************** 
       * Need color mode conversion code here!!! 
       * for now, do nothing....
       *****************************************/
      if (directcolour->colourType != pentry->approxModel) {}

      /* now perform direct -> index conversion. */
      if (pentry->approxType == PEXColourSpace) {
	switch (pentry->approxModel) {

	  case PEXRgbFloatColour:
	  case PEXCieFloatColour:
	  case PEXHsvFloatColour:
	  case PEXHlsFloatColour:
	   {
	    *colourindex = 
		((ddULONG)(directcolour->colour.rgbFloat.red*pentry->max1))
			   * pentry->mult1;
	    
	    *colourindex +=
		((ddULONG)(directcolour->colour.rgbFloat.green*pentry->max2))
			   * pentry->mult2;
	    
	    *colourindex += 
		((ddULONG)(directcolour->colour.rgbFloat.blue*pentry->max3))
			   * pentry->mult3;
	    
	    *colourindex += pentry->basePixel;
	    break;
	   }

	  case PEXRgb8Colour:
	    *colourindex = 
		((ddULONG)(directcolour->colour.rgb8.red*pentry->max1))
			   * pentry->mult1;
	    
	    *colourindex += 
		((ddULONG)(directcolour->colour.rgb8.green*pentry->max2))
			   * pentry->mult2;
	    
	    *colourindex += 
		((ddULONG)(directcolour->colour.rgb8.blue*pentry->max3))
			   * pentry->mult3;
	    
	    *colourindex += pentry->basePixel;
	    break;

	  case PEXRgb16Colour:
	    *colourindex = 
		((ddULONG)(directcolour->colour.rgb16.red*pentry->max1))
			   * pentry->mult1;
	    
	    *colourindex += 
		((ddULONG)(directcolour->colour.rgb16.green*pentry->max2))
			   * pentry->mult2;
	    
	    *colourindex += 
		((ddULONG)(directcolour->colour.rgb16.blue*pentry->max3))
			   * pentry->mult3;
	    
	    *colourindex += pentry->basePixel;
	    break;
	}
      } else /* if (pentry->approxType == PEXColourRange) */ {
	ddFLOAT floatindex;
	ddFLOAT nw1, nw2, nw3;

	/* use floatindex as temp var */
	floatindex = pentry->weight1 + pentry->weight2 + pentry->weight3;
	nw1 = pentry->weight1 / floatindex;
	nw2 = pentry->weight2 / floatindex;
	nw3 = pentry->weight3 / floatindex;

	switch (pentry->approxModel) {

	  case PEXRgbFloatColour:
	  case PEXCieFloatColour:
	  case PEXHsvFloatColour:
	  case PEXHlsFloatColour:
	   {
	    floatindex = directcolour->colour.rgbFloat.red * nw1;
	    floatindex += directcolour->colour.rgbFloat.green * nw2;
	    floatindex += directcolour->colour.rgbFloat.blue * nw3;
	    
	    floatindex *= pentry->max1;
	    *colourindex =  (ddULONG)(floatindex * pentry->mult1) +
	                    (ddULONG)(floatindex * pentry->mult2) +
	                    (ddULONG)(floatindex * pentry->mult3) +
			     pentry->basePixel;
	    break;
	   }

	  case PEXRgb8Colour:
	    floatindex = directcolour->colour.rgb8.red * nw1;
	    floatindex += directcolour->colour.rgb8.green * nw2;
	    floatindex += directcolour->colour.rgb8.blue * nw3;
	    
	    floatindex *= pentry->max1;
	    *colourindex =  (ddULONG)(floatindex * pentry->mult1) +
	                    (ddULONG)(floatindex * pentry->mult2) +
	                    (ddULONG)(floatindex * pentry->mult3) +
			     pentry->basePixel;
	    break;

	  case PEXRgb16Colour:
	    floatindex = directcolour->colour.rgb16.red * nw1;
	    floatindex += directcolour->colour.rgb16.green * nw2;
	    floatindex += directcolour->colour.rgb16.blue * nw3;
	    
	    floatindex *= pentry->max1;
	    *colourindex =  (ddULONG)(floatindex * pentry->mult1) +
	                    (ddULONG)(floatindex * pentry->mult2) +
	                    (ddULONG)(floatindex * pentry->mult3) +
			     pentry->basePixel;
	    break;
	}
      }

      return (Success);
}

/*++
 |
 |  Function Name:	miAddEdgeFlag
 |
 |  Function Description:
 |	 Add edge visibility flags to a list of vertices.
 |	 Performs not operation if there already are edge
 |	 flags in the data. Note all edges are set to "visible".
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miAddEdgeFlag(pddc, vinput, voutput)
/* in */
	miDDContext	*pddc;
        miListHeader    *vinput;
        miListHeader    **voutput;
{
/* uses */
    char		*in_pt, *out_pt;
    ddULONG		*edge_ptr;
    miListHeader	*output;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			list_count = 0;
    int			vert_count;
    int			point_size, out_point_size;
    int			i, j;

    /* If already have vertices, then simply return */
    if (DD_IsVertEdge(vinput->type)) {
      *voutput = vinput;
      return(Success);
    }

    /* Initialize output list */
    output = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(output, MI_ROUND_LISTHEADERCOUNT(vinput->numLists))
    if (!output->ddList) return(BadAlloc);

    DD_VertPointSize(vinput->type, point_size);
    output->type = vinput->type;
    DD_SetVertEdge(output->type);
    DD_VertPointSize(output->type, out_point_size);

    pddilist = vinput->ddList;
    pddolist = output->ddList;

    /* 
     * Traverse each list.
     */
    for (i = 0; i < vinput->numLists; i++) {

       	  if ((vert_count = pddolist->numPoints = pddilist->numPoints) <= 1) {
	    pddilist++;
	    continue;
          }

	  /* Insure sufficient room for each vertex */
	  MI_ALLOCLISTOFDDPOINT(pddolist, vert_count+1, out_point_size);
	  if (!pddolist->pts.p2DSpt) return(BadAlloc);

	  /* 
	   * Copy each point and initialize the edges.
	   * Note that the edge flag is always the last
	   * ddULONG of a vertex. Thus incrementing the
	   * destination pointer by the size of the input
	   * point "automatically" places the pointer
	   * at the start of the edge flag field.
	   */
          in_pt = pddilist->pts.ptr;
          out_pt = pddolist->pts.ptr;

	  for (j = 0; j < vert_count; j++) {
             memcpy( out_pt, in_pt, point_size);
	     in_pt += point_size;
	     out_pt += point_size;
	     edge_ptr = (ddULONG *)out_pt;
	     *(edge_ptr++) = ~0;
	     out_pt = (char *)edge_ptr;
	  }

	  /* Now, skip to next input list */
          pddilist++;
          pddolist++;
	  list_count++;
    }

    output->numLists = list_count;
    *voutput = output;
    return (Success);
}


/*++
 |
 |  Function Name:	miRemoveInvisibleEdges
 |
 |  Function Description:
 |	 Checks the edge flags of each edge in the input
 |	 vertex list and removes the "invisible" ones.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miRemoveInvisibleEdges(pddc, vinput, voutput)
/* in */
	miDDContext	*pddc;
        miListHeader    *vinput;
        miListHeader    **voutput;
{
/* uses */
    char		*in_pt, *out_pt;
    ddULONG		*edge_ptr;
    miListHeader	*output;
    listofddPoint	*pddilist;
    listofddPoint	*pddolist;
    int			list_count = 0;
    int			vert_count, counter;
    int			point_size;
    int			edge_offset;
    int			i, j;

    /* If already have vertices, then simply return */
    if (!(DD_IsVertEdge(vinput->type))) {
      *voutput = vinput;
      return(Success);
    }

    /* Initialize output list */
    output = MI_NEXTTEMPDATALIST(pddc);
    MI_ALLOCLISTHEADER(output, MI_ROUND_LISTHEADERCOUNT(vinput->numLists))
    if (!output->ddList) return(BadAlloc);

    output->type = vinput->type;
    output->numLists = vinput->numLists;
    output->flags =  vinput->flags;

    DD_VertPointSize(vinput->type, point_size);
    DD_VertOffsetEdge(vinput->type, edge_offset);

    pddilist = vinput->ddList;
    pddolist = output->ddList;

    /* 
     * Traverse each list.
     */
    for (i = 0; i < vinput->numLists; i++) {

       	  if ((vert_count = pddilist->numPoints) <= 1) {
	    pddilist++;
	    continue;
          }

	  /* Insure sufficient room for each vertex */
	  MI_ALLOCLISTOFDDPOINT(pddolist, vert_count, point_size);
	  if (!pddolist->pts.p2DSpt) return(BadAlloc);

	  /* 
	   * Copy each point and initialize the edges.
	   * Note that the edge flag is always the last
	   * ddULONG of a vertex. Thus incrementing the
	   * destination pointer by the size of the input
	   * point "automatically" places the pointer
	   * at the start of the edge flag field.
	   */
          in_pt = pddilist->pts.ptr;
          out_pt = pddolist->pts.ptr;

	  for (j = 0, counter = 0; j < vert_count; j++) {
	     edge_ptr = (ddULONG *)(in_pt + edge_offset);
	     if (*edge_ptr) {
		memcpy( out_pt, in_pt, point_size);
		out_pt += point_size;
		counter++;
	     } else if (counter) {
		/* if edge is invisible, start new list */

		/* First, end last edge of previous edge */
		memcpy( out_pt, in_pt, point_size);
		pddolist->numPoints = counter + 1;
		counter = 0;

		/* Insure enough room for new list header */
		list_count++;
		MI_ALLOCLISTHEADER(output, 
				MI_ROUND_LISTHEADERCOUNT(list_count));
		if (!output->ddList) return(BadAlloc);
		pddolist = &output->ddList[list_count];

		/* Next, insure enough room for vertices in new list */
		MI_ALLOCLISTOFDDPOINT(pddolist, vert_count - j, point_size);
		if (!(out_pt = pddolist->pts.ptr)) return(BadAlloc);
	     }

	     in_pt += point_size;
	  }

	  /* Now, skip to next input list */
          pddilist++;
	  if (counter > 1) {
	    pddolist->numPoints = counter;
	    list_count++;
	    MI_ALLOCLISTHEADER(output, MI_ROUND_LISTHEADERCOUNT(list_count));
	    if (!output->ddList) return(BadAlloc); 
	    pddolist = &output->ddList[list_count]; 
	  }
    }

    output->numLists = list_count;
    *voutput = output;
    return (Success);
}

