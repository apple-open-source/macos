/* $Xorg: pcfromi.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module pcfromi.c ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

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


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************
  
	pcfromi.c -- DIXIE routines for managing the ConvertFromIndex element
  
	Dean Verheiden -- AGE Logic, Inc. June 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pcfromi.c,v 3.5 2001/12/14 19:58:04 dawes Exp $ */

#define _XIEC_PCFROMI
#define _XIEC_PCI

/*
 *  Include files
 */
  /*
   *  Core X Includes
   */
#define NEED_EVENTS
#include <X.h>
#include <Xproto.h>
  /*
   *  XIE Includes
   */
#include <dixie_p.h>
  /*
   *  more X server includes.
   */
#include <scrnintstr.h>
#include <colormapst.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <element.h>

/* routines internal to this module
 */
static Bool PrepConvertFromIndex(floDefPtr flo, peDefPtr ped);

/* dixie entry points
 */
static diElemVecRec pCfromIVec = {
    PrepConvertFromIndex
    };


/*------------------------------------------------------------------------
----------------- routine: make an ConvertFromIndex element ----------------
------------------------------------------------------------------------*/
peDefPtr MakeConvertFromIndex(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloConvertFromIndex);
  ELEMENT_AT_LEAST_SIZE(xieFloConvertFromIndex);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped=MakePEDef(1,(CARD32)stuff->elemLength<<2,sizeof(pCfromIDefRec))))
    FloAllocError(flo,tag,xieElemConvertFromIndex, return(NULL));

  ped->diVec	     = &pCfromIVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloConvertFromIndex *)ped->elemRaw;
  /*
   * copy the standard client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    raw->class      = stuff->class;
    raw->precision  = stuff->precision;
    cpswapl(stuff->colormap, raw->colormap);
  }
  else  
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloConvertFromIndex));
  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;

  return(ped);
}                               /* end MakeConvertFromIndex */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepConvertFromIndex(floDefPtr flo, peDefPtr ped)
{
  xieFloConvertFromIndex *raw = (xieFloConvertFromIndex *)ped->elemRaw;
  pCfromIDefPtr pvt = (pCfromIDefPtr) ped->elemPvt;
  inFloPtr      inf = &ped->inFloLst[SRCt1];
  outFloPtr     src = &inf->srcDef->outFlo;
  outFloPtr     dst = &ped->outFlo;
  CARD32        depth, levels, b;

  /* check client parameters
   */
  if((raw->class != xieValSingleBand &&
      raw->class != xieValTripleBand) ||
     raw->precision < 1 || raw->precision > 16)
    ValueError(flo,ped,raw->precision, return(FALSE));

  /* grab attributes from colormap, visual, ...
   */
  if(!(pvt->cmap = (ColormapPtr) LookupIDByType(raw->colormap, RT_COLORMAP)))
    ColormapError(flo,ped,raw->colormap, return(FALSE));
  pvt->precShift = 16 - raw->precision;
  pvt->class     = pvt->cmap->class;
  pvt->visual    = pvt->cmap->pVisual;
  pvt->pixMsk[0] = pvt->visual->redMask;
  pvt->pixMsk[1] = pvt->visual->greenMask;
  pvt->pixMsk[2] = pvt->visual->blueMask;
  pvt->pixPos[0] = pvt->visual->offsetRed;
  pvt->pixPos[1] = pvt->visual->offsetGreen;
  pvt->pixPos[2] = pvt->visual->offsetBlue;
  pvt->cells     = pvt->visual->ColormapEntries;
  levels = (pvt->class <= PseudoColor ? pvt->cells :
           (pvt->pixMsk[0] | pvt->pixMsk[1] | pvt->pixMsk[2]) + 1);
  SetDepthFromLevels(levels,depth);

  if(IsntConstrained(src->format[0].class)
    || src->bands > 1 || src->format[0].levels != 1<<depth)
    MatchError(flo,ped, return(FALSE));

  /* generate output attributes from input attributes and precision arg
   */
  dst->bands = (raw->class == xieValSingleBand) ? 1 : 3;
  inf->bands = src->bands;
  for(b = 0; b < dst->bands; b++) {
    dst->format[b] = inf->format[0] = src->format[0];
    dst->format[b].band = b;
    dst->format[b].levels = (1<<raw->precision);
  }
  if(!UpdateFormatfromLevels(ped))
    MatchError(flo,ped,return(FALSE));

  return(TRUE);
}                               /* end PrepConvertFromIndex */

/* end module pcfromi.c */
