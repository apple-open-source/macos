/* $Xorg: miTestOCs.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */
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

#include "mipex.h"
#include "ddpex3.h"
#include "miStruct.h"
#include "miRender.h"
#include "PEXErr.h"
#include "PEXprotost.h"


/* Level III Output Command Primitives */

static char        *ptTypeNames[] = {
    "DD_2D_POINT",		  /* 2D point */
    "DD_3D_POINT",		  /* 3D point */
    "DD_INDEX_POINT",		  /* 3D point w/ colour */
    "DD_RGB8_POINT",		  /* 3D point w/ colour */
    "DD_RGB16_POINT",		  /* 3D point w/ colour */
    "DD_RGBFLOAT_POINT",	  /* 3D point w/ colour */
    "DD_HSV_POINT",		  /* 3D point w/ colour */
    "DD_HLS_POINT",		  /* 3D point w/ colour */
    "DD_CIE_POINT",		  /* 3D point w/ colour */
    "DD_NORM_POINT",		  /* 3D point w/ normal */
    "DD_EDGE_POINT",		  /* 3D point w/ edge flag */
    "DD_INDEX_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_RGB8_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_RGB16_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_RGBFLOAT_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_HSV_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_HLS_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_CIE_NORM_POINT",	  /* 3D point w/ colour & normal */
    "DD_INDEX_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_RGB8_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_RGB16_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_RGBFLOAT_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_HSV_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_HLS_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_CIE_EDGE_POINT",	  /* 3D point w/ colour & edge flag */
    "DD_NORM_EDGE_POINT",	  /* 3D point w/ normal & edge flag */
    "DD_INDEX_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_RGB8_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_RGB16_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_RGBFLOAT_NORM_EDGE_POINT",/* 3D point w/ colour, normal & edge */
    "DD_HSV_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_HLS_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_CIE_NORM_EDGE_POINT",	  /* 3D point w/ colour, normal & edge */
    "DD_HOMOGENOUS_POINT"	  /* homogenous point (4D) */
};

static char        *pfTypeNames[] = {
    "DD_FACET_NONE",		  /* no facet attributes */
    "DD_FACET_INDEX",		  /* facet colour */
    "DD_FACET_RGB8",		  /* facet colour */
    "DD_FACET_RGB16",		  /* facet colour */
    "DD_FACET_RGBFLOAT",	  /* facet colour */
    "DD_FACET_HSV",		  /* facet colour */
    "DD_FACET_HLS",		  /* facet colour */
    "DD_FACET_CIE",		  /* facet colour */
    "DD_FACET_NORM",		  /* facet normal */
    "DD_FACET_INDEX_NORM",	  /* facet colour & normal */
    "DD_FACET_RGB8_NORM",	  /* facet colour & normal */
    "DD_FACET_RGB16_NORM",	  /* facet colour & normal */
    "DD_FACET_RGBFLOAT_NORM",	  /* facet colour & normal */
    "DD_FACET_HSV_NORM",	  /* facet colour & normal */
    "DD_FACET_HLS_NORM",	  /* facet colour & normal */
    "DD_FACET_CIE_NORM"		  /* facet colour & normal */
};

static char        *piTypeNames[] = {
    "DD_VERTEX",
    "DD_VERTEX_EDGE"
};

static	int	test_print_flag = 0;

#define PRINT_POINT_INFO( pt ) \
\
	ErrorF( "\tPoint Type: %d %s\tNum Lists: %d\n",  \
	(pt)->type, ptTypeNames[(int)((pt)->type)], (pt)->numLists )

#define PRINT_FACET_INFO( pf ) \
\
	ErrorF( "\tFacet Type: %d %s\tNum Facets: %d\n",  \
	(pf)->type, pfTypeNames[(int)((pf)->type)], (pf)->numFacets )

#define PRINT_INDEX_INFO( pi ) \
\
	ErrorF( "\tIndex Type: %d %s\tNum Index: %d\n",  \
	(pi)->type, piTypeNames[(int)((pi)->type)], (pi)->numIndex )

/*++
 |
 |  Function Name:	miTestNurbSurface
 |
 |  Function Description:
 |	 Handles the  Non-uniform B-spline surfac ocs.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestNurbSurface(pRend, pSurf, numCurves, pTrimCurve)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    ddNurbSurface      *pSurf;	  /* surface data */
    ddULONG             numCurves;/* number of trim curves */
    listofTrimCurve    *pTrimCurve;	/* trim curve */
/* out */
{
    if (test_print_flag) ErrorF( "miTestNurbSurface\n");
    return (Success);
}

/*++
 |
 |  Function Name:	miTestCellArrays
 |
 |  Function Description:
 |	 Handles the Cell array 3D, Cell array 2D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestCellArrays(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    if (test_print_flag) ErrorF( "miTestCellArrays\n");
    return (Success);
}

/*++
 |
 |  Function Name:	miTestExtCellArray
 |
 |  Function Description:
 |	 Handles the Extended Cell array ocs
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestExtCellArray(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    if (test_print_flag) ErrorF( "miTestExtCellArray\n");
    return (Success);
}

/*++
 |
 |  Function Name:	miTestGDP
 |
 |  Function Description:
 |	 Handles the GDP 3D, GDP 2D ocs.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestGDP(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
   if (test_print_flag) ErrorF( "miTestGDP\n");
   return (Success);
}

/*++
 |
 |  Function Name:	miTestSetAttribute
 |
 |  Function Description:
 |	 Handles the  All Other ocs (to set attributes).
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestSetAttribute(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;	  /* output command */
/* out */
{
   if (test_print_flag) ErrorF( "miTestSetAttribute\n");
   return (Success);
}



/*++
 |
 |  Function Name:	miTestColourOC
 |
 |  Function Description:
 |	 Handles the Colour setting OC's.
 |
 |  Note(s):
 |
 --*/

ddpex2rtn
miTestColourOC(pRend, pExecuteOC)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    miGenericStr       *pExecuteOC;
/* out */
{
    if (test_print_flag) ErrorF( "miTestColourOC\n");
    return (Success);
}

