/* $Xorg: miRndText.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level1/miRndText.c,v 3.7 2001/12/14 19:57:18 dawes Exp $ */

#define NEED_EVENTS
#include "miRender.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "extnsionst.h"
#include "gcstruct.h"
#include "ddpex2.h"


/*++
 |
 |  Function Name:	miRenderText
 |
 |  Function Description:
 |	 Renders Text to the screen.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
miRenderText(pRend, pddc, input_list)
/* in */
    ddRendererPtr       pRend;          /* renderer handle */
    miDDContext         *pddc;          /* dd context handle */
    miListHeader        *input_list;    /* fill area data */
{
/* calls */
      ddpex3rtn		miFilterPath();

/* Local variable definitions */
      miListHeader	*temp_list;
      listofddPoint	*sp;
      int		j;
      ddpex3rtn		err = Success;

      /* Remove all data  but vertex coordinates */
      if ((DD_IsVertNormal(input_list->type)) ||
	  (DD_IsVertEdge(input_list->type)) ||
	  (DD_IsVertColour(input_list->type)) ) {
	err = miFilterPath(pddc, input_list, &temp_list, 1);
	if (err) return (err);
	input_list = temp_list;
      }

      /*
       * Update the text GC to reflect the current 
       * text attributes 
       */
      if (pddc->Static.misc.flags & TEXTGCFLAG)
	miDDC_to_GC_text(pRend, pddc, pddc->Static.misc.pTextGC);

      /* Validate GC prior to start of rendering */

      if (pddc->Static.misc.pTextGC->serialNumber != 
	  pRend->pDrawable->serialNumber)
        ValidateGC(pRend->pDrawable, pddc->Static.misc.pTextGC);

      /* We should have DC paths here; Render them */

      for (j = 0, sp = input_list->ddList; 
	   j < input_list->numLists; j++, sp++) {
        if (sp->numPoints > 0) {

	  /* Call ddx to render the polylines */
	  (*GetGCValue(pddc->Static.misc.pTextGC, ops->Polylines)) 
			(pRend->pDrawable,
			 pddc->Static.misc.pTextGC, 
			CoordModeOrigin, 
			sp->numPoints, 
			sp->pts.p2DSpt);
        }
      }
}
