/* $Xorg: miDDCtoGC.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level1/miDDCtoGC.c,v 3.8 2001/12/14 19:57:16 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "miRender.h"
#include "gcstruct.h"
#include "miLineDash.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	miDDC_to_GC_polyline
 |
 |  Function Description:
 |	 Initializes the attributes in a GC in order to correctly
 |	 render a polyline. Currently, therefore, the following
 |	 attributes are set:
 |
 |		line width
 |		dashing
 |
 |  Note(s):
 |	 This routine currently performs no optimization of this
 |	 process. For example, no effort is made to globally check
 |	 whether this process is even necessary. 
 |	 Note that this routine assumes that the proper defaults
 |	 are set for the GC attributes not used by PEX (attributes
 |	 such as line join style or end cap style).
 |
 --*/

ddpex3rtn
miDDC_to_GC_polyline(pRend, pddc, pgc)
/* in */
ddRendererPtr	pRend;	/* renderer handle */
miDDContext	*pddc;	/* dd Context handle */
GCPtr		pgc;	/* X GC handle */
{
      ddLONG		gcmask = 0;
      ddUSHORT		status;
      ddColourSpecifier	linecolour;
      miColourEntry	*plinecolour;
      ddULONG		colourindex;
      ddSHORT		linewidth;


      /*
       * Set line colour. 
       * The colour must be processed according
       * to the contents of the current colour approximation table
       * entry to compute the proper direct colour.
       */

      /*
       * Calculate final color index.
       */
      if (pddc->Static.attrs->echoMode == PEXEcho) 
	    linecolour = pddc->Static.attrs->echoColour;
      else 
	    linecolour = pddc->Static.attrs->lineColour;

      miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
			&linecolour, &colourindex);

      /* Only set GC value if necessary */
      if (colourindex != pgc->fgPixel) {
	gcmask |= GCForeground;
	pgc->fgPixel = colourindex;
      }

      /*
       * Set line width. PEX line width is defined as
       * the product of the line width scale factor (lineWidth in
       * the PC) and the workstation nominal line width (1 for X).
       * therefore the linewidth is simply equal to the line width
       * scale factor. Note that this code sets lineWidth of 1
       * to ddx linewidth 0. Although this is technically wrong,
       * it sure does speed things up!
       */
      if (pddc->Static.attrs->lineWidth <= 1.0) linewidth = 0;
      else linewidth = (ddSHORT)pddc->Static.attrs->lineWidth;

      /* Only set GC value if necessary */
      if (linewidth != pgc->lineWidth) {
	gcmask |= GCLineWidth;
	pgc->lineWidth = linewidth;
      }

      /*
       * Set the line dash. PEX defines three line dash
       * types: solid (no dashing), dashed (equally spaced dashes)
       * dotted (equally spaced dots) and dash dot (alternating
       * dots and dashes). Note that the default dashing is
       * defined in miLineDash.h.
       */
      /* Only set dashes if necessary */
      switch (pddc->Static.attrs->lineType) {

	case PEXLineTypeSolid:
	   if (pgc->lineStyle != LineSolid) {
	     gcmask |= GCLineStyle;
	     pgc->lineStyle = LineSolid;
	   }
	   break;

	case PEXLineTypeDashed:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dashed_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashed, 
		    sizeof(mi_line_dashed));

	   } else if (pgc->dash != mi_line_dashed) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dashed_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashed, 
		    sizeof(mi_line_dashed));
	   }
	   break;

	case PEXLineTypeDotted:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dotted_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dotted, 
		    sizeof(mi_line_dotted));

	   } else if (pgc->dash != mi_line_dotted) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dotted_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dotted, 
		    sizeof(mi_line_dotted));
	   }
	   break;

	case PEXLineTypeDashDot:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dashdot_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashdot, 
		    sizeof(mi_line_dashdot));

	   } else if (pgc->dash != mi_line_dashdot) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dashdot_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashdot, 
		    sizeof(mi_line_dashdot));
	   }
	   break;
      }

      /* Register changes with ddx */
      if (gcmask) {
	pgc->serialNumber |= GC_CHANGE_SERIAL_BIT;
	pgc->stateChanges |= gcmask;
	(*pgc->funcs->ChangeGC)(pgc, gcmask);
      }

      /* Clear polyline GC change flag */
      pddc->Static.misc.flags &= ~POLYLINEGCFLAG;

      return (Success);
}

/*++
 |
 |  Function Name:	miDDC_to_GC_edges
 |
 |  Function Description:
 |	 Initializes the attributes in a GC in order to correctly
 |	 render the edges of a fill area. Currently, therefore, 
 |	 the following attributes are set:
 |
 |		line width
 |		dashing
 |
 |  Note(s):
 |	 Note, that no colour initialization is performed - this attribute
 |	 changes too often within the rendering pipeline.
 |	 Lastly, note that this routine assumes that the proper defaults
 |	 are set for the GC attributes not used by PEX (attributes
 |	 such as stipple mask, etc...)
 |
 --*/

ddpex3rtn
miDDC_to_GC_edges(pRend, pddc, pgc)
/* in */
ddRendererPtr	pRend;	/* renderer handle */
miDDContext	*pddc;	/* dd Context handle */
GCPtr		pgc;	/* X GC handle */
{
      ddLONG		gcmask = 0;
      ddUSHORT		status;
      ddColourSpecifier	edgecolour;
      miColourEntry	*pedgecolour;
      ddULONG		colourindex;
      ddSHORT		edgewidth;


      /*
       * Set edge colour. 
       * The colour must be processed according
       * to the contents of the current colour approximation table
       * entry to compute the proper direct colour.
       */

      /*
       * Calculate final color index.
       */
      if (pddc->Static.attrs->echoMode == PEXEcho) 
	  edgecolour = pddc->Static.attrs->echoColour;
      else 
	  edgecolour = pddc->Static.attrs->edgeColour;

      miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
			&edgecolour, &colourindex);
      

      /* Only set GC value if necessary */
      if (colourindex != pgc->fgPixel) {
	gcmask |= GCForeground;
	pgc->fgPixel = colourindex;
      }

      /*
       * Set edge width. PEX edge width is defined as
       * the product of the edge width scale factor (lineWidth in
       * the PC) and the workstation nominal line width (1 for X).
       * therefore the edge is simply equal to the edge width
       * scale factor. Note that this code sets edgeWidth of 1
       * to ddx linewidth 0. Although this is technically wrong,
       * it sure does speed things up!
       */
      if (pddc->Static.attrs->edgeWidth <= 1.0) edgewidth = 0;
      else edgewidth = (ddSHORT)pddc->Static.attrs->edgeWidth;

      /* Only set GC value if necessary */
      if (edgewidth != pgc->lineWidth) {
	gcmask |= GCLineWidth;
	pgc->lineWidth = edgewidth;
      }

      /*
       * Next, set the edge dash. PEX defines three line dash
       * types: solid (no dashing), dashed (equally spaced dashes)
       * dotted (equally spaced dots) and dash dot (alternating
       * dots and dashes). Note that the default dashing is
       * defined in miLineDash.h.
       */
      switch (pddc->Static.attrs->edgeType) {

	case PEXLineTypeSolid:
	   if (pgc->lineStyle != LineSolid) {
	     gcmask |= GCLineStyle;
	     pgc->lineStyle = LineSolid;
	   }
	   break;

	case PEXLineTypeDashed:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dashed_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashed, 
		    sizeof(mi_line_dashed));

	   } else if (pgc->dash != mi_line_dashed) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dashed_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashed, 
		    sizeof(mi_line_dashed));
	   }
	   break;

	case PEXLineTypeDotted:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dotted_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dotted, 
		    sizeof(mi_line_dotted));

	   } else if (pgc->dash != mi_line_dotted) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dotted_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dotted, 
		    sizeof(mi_line_dotted));
	   }
	   break;

	case PEXLineTypeDashDot:
	   if (pgc->lineStyle != LineOnOffDash) {
	     gcmask |= (GCLineStyle | GCDashList);
	     pgc->lineStyle = LineOnOffDash;
	     pgc->numInDashList = mi_line_dashdot_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashdot, 
		    sizeof(mi_line_dashdot));

	   } else if (pgc->dash != mi_line_dashdot) {
	     gcmask |= GCDashList;
	     pgc->numInDashList = mi_line_dashdot_length;
	     if (pddc->Static.misc.flags & NOLINEDASHFLAG) {
	       pgc->dash = (unsigned char *)xalloc(MAX_LINE_DASH_LENGTH_SIZE);
	       pddc->Static.misc.flags &= ~NOLINEDASHFLAG;
	     }
	     memcpy( (char *)(pgc->dash), (char *)mi_line_dashdot, 
		    sizeof(mi_line_dashdot));
	   }
	   break;
      }

      /* Register changes with ddx */
      if (gcmask) {
	pgc->serialNumber |= GC_CHANGE_SERIAL_BIT;
	pgc->stateChanges |= gcmask;
	(*pgc->funcs->ChangeGC)(pgc, gcmask);
      }

      /* Clear polyline GC change flag */
      pddc->Static.misc.flags &= ~EDGEGCFLAG;

      return (Success);
}

/*++
 |
 |  Function Name:	miDDC_to_GC_fill_area
 |
 |  Function Description:
 |	 Initializes the attributes in a GC in order to correctly
 |	 render a fill area. Currently, therefore, 
 |	 the following attributes are set:
 |
 |		interior colour
 |
 |  Note(s):
 |	 Note that this routine assumes that the proper defaults
 |	 are set for the GC attributes not used by PEX (attributes
 |	 such as stipple mask, etc...)
 |
 --*/

ddpex3rtn
miDDC_to_GC_fill_area(pRend, pddc, pgc)
/* in */
ddRendererPtr	pRend;	/* renderer handle */
miDDContext	*pddc;	/* dd Context handle */
GCPtr		pgc;	/* X GC handle */
{
      ddLONG		gcmask = 0;


}

/*++
 |
 |  Function Name:	miDDC_to_GC_marker
 |
 |  Function Description:
 |	 Initializes the attributes in a GC in order to correctly
 |	 render a marker. Currently, therefore, 
 |	 the following attributes are set:
 |
 |		colour
 |
 |  Note(s):
 |	 Note that this routine assumes that the proper defaults
 |	 are set for the GC attributes not used by PEX (attributes
 |	 such as stipple mask, etc...)
 |
 --*/

ddpex3rtn
miDDC_to_GC_marker(pRend, pddc, pgc)
/* in */
ddRendererPtr	pRend;	/* renderer handle */
miDDContext	*pddc;	/* dd Context handle */
GCPtr		pgc;	/* X GC handle */
{
      ddLONG			gcmask = 0;
      ddUSHORT			status;
      ddColourSpecifier		markercolour;
      miColourEntry		*pmarkercolour;
      ddULONG			colourindex;


      /*
       * Set the marker colour. 
       * The colour must be processed according
       * to the contents of the current colour approximation table
       * entry to compute the proper direct colour.
       */

      /*
       * Calculate final color index.
       */
      if (pddc->Static.attrs->echoMode == PEXEcho) 
	  markercolour = pddc->Static.attrs->echoColour;
      else 
	  markercolour = pddc->Static.attrs->markerColour;

      miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
			&markercolour, &colourindex);


      /* Only set GC value if necessary */
      if (colourindex != pgc->fgPixel) {
	gcmask |= GCForeground;
	pgc->fgPixel = colourindex;
      }

      /* Register changes with ddx */
      if (gcmask) {
	pgc->serialNumber |= GC_CHANGE_SERIAL_BIT;
	pgc->stateChanges |= gcmask;
	(*pgc->funcs->ChangeGC)(pgc, gcmask);
      }

      /* Clear polyline GC change flag */
      pddc->Static.misc.flags &= ~MARKERGCFLAG;

      return (Success);
}

/*++
 |
 |  Function Name:	miDDC_to_GC_text
 |
 |  Function Description:
 |	 Initializes the attributes in a GC in order to correctly
 |	 render a text. Currently, therefore, 
 |	 the following attributes are set:
 |
 |		colour
 |
 |  Note(s):
 |	 Note that this routine assumes that the proper defaults
 |	 are set for the GC attributes not used by PEX (attributes
 |	 such as stipple mask, etc...)
 |
 --*/

ddpex3rtn
miDDC_to_GC_text(pRend, pddc, pgc)
/* in */
ddRendererPtr	pRend;	/* renderer handle */
miDDContext	*pddc;	/* dd Context handle */
GCPtr		pgc;	/* X GC handle */
{
      ddLONG		gcmask = 0;
      ddUSHORT		status;
      ddColourSpecifier	textcolour;
      miColourEntry	*ptextcolour;
      ddULONG		colourindex;

      /*
       * Set text colour. 
       * The colour must be processed according
       * to the contents of the current colour approximation table
       * entry to compute the proper direct colour.
       */

      /*
       * Calculate final color index.
       */
      if (pddc->Static.attrs->echoMode == PEXEcho) 
	  textcolour = pddc->Static.attrs->echoColour;
      else 
	  textcolour = pddc->Static.attrs->textColour;

      miColourtoIndex(pRend, pddc->Dynamic->pPCAttr->colourApproxIndex,
			&textcolour, &colourindex);


      /* Only set GC value if necessary */
      if (colourindex != pgc->fgPixel) {
	gcmask |= GCForeground;
	pgc->fgPixel = colourindex;
      }

      /* Register changes with ddx */
      if (gcmask) {
	pgc->serialNumber |= GC_CHANGE_SERIAL_BIT;
	pgc->stateChanges |= gcmask;
	(*pgc->funcs->ChangeGC)(pgc, gcmask);
      }

      /* Clear polyline GC change flag */
      pddc->Static.misc.flags &= ~TEXTGCFLAG;

      return (Success);
}

