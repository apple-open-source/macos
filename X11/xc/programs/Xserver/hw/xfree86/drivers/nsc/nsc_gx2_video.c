/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx2_video.c,v 1.5 2003/02/21 16:51:09 alanh Exp $ */
/*
 * $Workfile: nsc_gx2_video.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * File Contents: This file consists of main Xfree video supported routines.
 *
 * Project:       Geode Xfree Frame buffer device driver.
 *
 */

/* 
 * NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * National Xfree frame buffer driver
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * National Xfree frame buffer driver
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * National Xfree frame buffer driver
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */

/* 
 * Fixes & Extensions to support Y800 greyscale modes 
 * Alan Hourihane <alanh@fairlite.demon.co.uk>
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "nsc.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"
#include "nsc_fourcc.h"

#define OFF_DELAY 	200		/* milliseconds */
#define FREE_DELAY 	60000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)
#define XV_PROFILE 0
#define REINIT  1
#ifndef XvExtension
void
GX2InitVideo(ScreenPtr pScreen)
{
}

void
GX2ResetVideo(ScrnInfoPtr pScrn)
{
}
#else

#define DBUF 1
void GX2InitVideo(ScreenPtr pScreen);
void GX2ResetVideo(ScrnInfoPtr pScrn);
static XF86VideoAdaptorPtr GX2SetupImageVideo(ScreenPtr);
static void GX2InitOffscreenImages(ScreenPtr);
static void GX2StopVideo(ScrnInfoPtr, pointer, Bool);
static int GX2SetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int GX2GetPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
static void GX2QueryBestSize(ScrnInfoPtr, Bool,
			     short, short, short, short, unsigned int *,
			     unsigned int *, pointer);
static int GX2PutImage(ScrnInfoPtr, short, short, short, short, short, short,
		       short, short, int, unsigned char *, short, short, Bool,
		       RegionPtr, pointer);
static int GX2QueryImageAttributes(ScrnInfoPtr, int, unsigned short *,
				   unsigned short *, int *, int *);

static void GX2BlockHandler(int, pointer, pointer, pointer);
void GX2SetVideoPosition(int x, int y, int width, int height,
			 short src_w, short src_h, short drw_w,
			 short drw_h, int id, int offset, ScrnInfoPtr pScrn);

extern void GX2AccelSync(ScrnInfoPtr pScreenInfo);

#if !defined(STB_X)
extern int DeltaX, DeltaY;
#else
int DeltaX, DeltaY;
#endif

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey, xvColorKeyMode, xvFilter
#if DBUF
  , xvDoubleBuffer
#endif
  ;

/*----------------------------------------------------------------------------
 * GX2InitVideo
 *
 * Description	:This is the initialization routine.It creates a new video adapter
 *				 and calls GX2SetupImageVideo to initialize the adaptor by filling
 *				 XF86VideoAdaptorREc.Then it lists the existing adaptors and adds the 
 *				 new one to it. Finally the list of XF86VideoAdaptorPtr pointers are
 *				 passed to the xf86XVScreenInit().
 *
 * Parameters.
 * ScreenPtr
 *		pScreen	:Screen handler pointer having screen information.
 *
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX2InitVideo(ScreenPtr pScreen)
{
   GeodePtr pGeode;
   ScrnInfoPtr pScreenInfo = xf86Screens[pScreen->myNum];

   pGeode = GEODEPTR(pScreenInfo);

   if (!pGeode->NoAccel) {
      ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
      XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
      XF86VideoAdaptorPtr newAdaptor = NULL;

      int num_adaptors;

      newAdaptor = GX2SetupImageVideo(pScreen);
      GX2InitOffscreenImages(pScreen);

      num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

      if (newAdaptor) {
	 if (!num_adaptors) {
	    num_adaptors = 1;
	    adaptors = &newAdaptor;
	 } else {
	    newAdaptors =		/* need to free this someplace */
		  xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr *));
	    if (newAdaptors) {
	       memcpy(newAdaptors, adaptors, num_adaptors *
		      sizeof(XF86VideoAdaptorPtr));
	       newAdaptors[num_adaptors] = newAdaptor;
	       adaptors = newAdaptors;
	       num_adaptors++;
	    }
	 }
      }

      if (num_adaptors)
	 xf86XVScreenInit(pScreen, adaptors, num_adaptors);

      if (newAdaptors)
	 xfree(newAdaptors);
   }
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] = {
   {
    0,
    "XV_IMAGE",
    1024, 1024,
    {1, 1}
    }
};

#define NUM_FORMATS 4

static XF86VideoFormatRec Formats[NUM_FORMATS] = {
   {8, PseudoColor}, {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#if DBUF
#define NUM_ATTRIBUTES 4
#else
#define NUM_ATTRIBUTES 3
#endif

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] = {
#if DBUF
   {XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"},
#endif
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, 0, 1, "XV_FILTER"},
   {XvSettable | XvGettable, 0, 1, "XV_COLORKEYMODE"}
};

#define NUM_IMAGES 7

static XF86ImageRec Images[NUM_IMAGES] = {
   XVIMAGE_UYVY,
   XVIMAGE_YUY2,
   XVIMAGE_Y2YU,
   XVIMAGE_YVYU,
   XVIMAGE_Y800,
   XVIMAGE_I420,
   XVIMAGE_YV12
};

typedef struct
{
   FBAreaPtr area;
   FBLinearPtr linear;
   RegionRec clip;
   CARD32 colorKey;
   CARD32 colorKeyMode;
   CARD32 filter;
   CARD32 videoStatus;
   Time offTime;
   Time freeTime;
#if DBUF
   Bool doubleBuffer;
   int currentBuffer;
#endif
}
GeodePortPrivRec, *GeodePortPrivPtr;

#define GET_PORT_PRIVATE(pScrn) \
   (GeodePortPrivPtr)((GEODEPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

/*----------------------------------------------------------------------------
 * GX2SetColorKey
 *
 * Description	:This function reads the color key for the pallete and
 *				  sets the video color key register.
 *
 * Parameters.
 * ScreenInfoPtr
 *		pScrn	:Screen  pointer having screen information.
 *		pPriv	:Video port private data
 *
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
static INT32
GX2SetColorkey(ScrnInfoPtr pScrn, GeodePortPrivPtr pPriv)
{
   int red, green, blue;
   unsigned long key;

   switch (pScrn->depth) {
   case 8:
      GFX(get_display_palette_entry(pPriv->colorKey & 0xFF, &key));
      red = ((key >> 16) & 0xFF);
      green = ((key >> 8) & 0xFF);
      blue = (key & 0xFF);
      break;
   case 16:
      red = (pPriv->colorKey & pScrn->mask.red) >>
	    pScrn->offset.red << (8 - pScrn->weight.red);
      green = (pPriv->colorKey & pScrn->mask.green) >>
	    pScrn->offset.green << (8 - pScrn->weight.green);
      blue = (pPriv->colorKey & pScrn->mask.blue) >>
	    pScrn->offset.blue << (8 - pScrn->weight.blue);
      break;
   default:
      /* for > 16 bpp we send in the mask in xf86SetWeight. This
       * function is providing the offset by 1 more. So we take 
       * this as a special case and subtract 1 for > 16
       */
      red = (pPriv->colorKey & pScrn->mask.red) >>
	    (pScrn->offset.red - 1) << (8 - pScrn->weight.red);
      green = (pPriv->colorKey & pScrn->mask.green) >>
	    (pScrn->offset.green - 1) << (8 - pScrn->weight.green);
      blue = (pPriv->colorKey & pScrn->mask.blue) >>
	    (pScrn->offset.blue - 1) << (8 - pScrn->weight.blue);
      break;
   }

   GFX(set_video_color_key((blue | (green << 8) | (red << 16)), 0xFFFFFF,
			   (pPriv->colorKeyMode == 0)));
   REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
   return 0;
}

/*----------------------------------------------------------------------------
 * GX2ResetVideo
 *
 * Description	: This function resets the video
 *
 * Parameters.
 * ScreenInfoPtr
 *		pScrn	:Screen  pointer having screen information.
 *
 * Returns		:None
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/

void
GX2ResetVideo(ScrnInfoPtr pScrn)
{
   GeodePtr pGeode = GEODEPTR(pScrn);

   if (!pGeode->NoAccel) {
      GeodePortPrivPtr pPriv = pGeode->adaptor->pPortPrivates[0].ptr;

      GX2AccelSync(pScrn);
      GFX(set_video_palette(NULL));
      GX2SetColorkey(pScrn, pPriv);
      GFX(set_video_filter(pPriv->filter, pPriv->filter));
   }
}

/*----------------------------------------------------------------------------
 * GX2SetupImageVideo
 *
 * Description	: This function allocates space for a Videoadaptor and initializes
 *				  the XF86VideoAdaptorPtr record.
 *
 * Parameters.
 * ScreenPtr
 *		pScreen	:Screen handler pointer having screen information.
 *
 * Returns		:XF86VideoAdaptorPtr :- pointer to the initialized video adaptor record.
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/

static XF86VideoAdaptorPtr
GX2SetupImageVideo(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   GeodePtr pGeode = GEODEPTR(pScrn);
   XF86VideoAdaptorPtr adapt;
   GeodePortPrivPtr pPriv;

   if (!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			 sizeof(GeodePortPrivRec) + sizeof(DevUnion))))
      return NULL;

   adapt->type = XvWindowMask | XvInputMask | XvImageMask;
   adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
   adapt->name = "National Semiconductor Corporation";
   adapt->nEncodings = 1;
   adapt->pEncodings = DummyEncoding;
   adapt->nFormats = NUM_FORMATS;
   adapt->pFormats = Formats;
   adapt->nPorts = 1;
   adapt->pPortPrivates = (DevUnion *) (&adapt[1]);
   pPriv = (GeodePortPrivPtr) (&adapt->pPortPrivates[1]);
   adapt->pPortPrivates[0].ptr = (pointer) (pPriv);
   adapt->pAttributes = Attributes;
   adapt->nImages = NUM_IMAGES;
   adapt->nAttributes = NUM_ATTRIBUTES;
   adapt->pImages = Images;
   adapt->PutVideo = NULL;
   adapt->PutStill = NULL;
   adapt->GetVideo = NULL;
   adapt->GetStill = NULL;
   adapt->StopVideo = GX2StopVideo;
   adapt->SetPortAttribute = GX2SetPortAttribute;
   adapt->GetPortAttribute = GX2GetPortAttribute;
   adapt->QueryBestSize = GX2QueryBestSize;
   adapt->PutImage = GX2PutImage;
   adapt->QueryImageAttributes = GX2QueryImageAttributes;

   pPriv->colorKey = pGeode->videoKey;
   pPriv->colorKeyMode = 0;
   pPriv->filter = 0;
   pPriv->videoStatus = 0;
#if DBUF
   pPriv->doubleBuffer = TRUE;
   pPriv->currentBuffer = 0;		/* init to first buffer */
#endif

   /* gotta uninit this someplace */
   REGION_INIT(pScreen, &pPriv->clip, NullBox, 0);

   pGeode->adaptor = adapt;

   pGeode->BlockHandler = pScreen->BlockHandler;
   pScreen->BlockHandler = GX2BlockHandler;

   xvColorKey = MAKE_ATOM("XV_COLORKEY");
   xvColorKeyMode = MAKE_ATOM("XV_COLORKEYMODE");
   xvFilter = MAKE_ATOM("XV_FILTER");
#if DBUF
   xvDoubleBuffer = MAKE_ATOM("XV_DOUBLE_BUFFER");
#endif

   GX2ResetVideo(pScrn);

   return adapt;
}

#if REINIT
static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
   int *dataA, *dataB;
   int num;

   num = REGION_NUM_RECTS(A);
   if (num != REGION_NUM_RECTS(B))
      return FALSE;

   if ((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) || (A->extents.y2 != B->extents.y2))
      return FALSE;

   dataA = (int *)REGION_RECTS(A);
   dataB = (int *)REGION_RECTS(B);

   while (num--) {
      if ((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
	 return FALSE;
      dataA += 2;
      dataB += 2;
   }

   return TRUE;
}
#endif

/*----------------------------------------------------------------------------
 * GX2StopVideo
 *
 * Description	:This function is used to stop input and output video
 *
 * Parameters.
 * pScreenInfo
 *		pScrn		:Screen handler pointer having screen information.
 *		data		:Pointer to the video port's private data
 *		exit		:Flag indicating whether the offscreen areas used for video
 *					 to be deallocated or not.
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
static void
GX2StopVideo(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
   GeodePortPrivPtr pPriv = (GeodePortPrivPtr) data;
   GeodePtr pGeode = GEODEPTR(pScrn);

   REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

   GX2AccelSync(pScrn);
   if (exit) {
      if (pPriv->videoStatus & CLIENT_VIDEO_ON) {
	 GFX(set_video_enable(0));
      }
      if (pPriv->area) {
	 xf86FreeOffscreenArea(pPriv->area);
	 pPriv->area = NULL;
      }
      pPriv->videoStatus = 0;
      pGeode->OverlayON = FALSE;
   } else {
      if (pPriv->videoStatus & CLIENT_VIDEO_ON) {
	 pPriv->videoStatus |= OFF_TIMER;
	 pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
      }
   }
}

/*----------------------------------------------------------------------------
 * GX2SetPortAttribute
 *
 * Description	:This function is used to set the attributes of a port like colorkeymode,
 *				  double buffer support and filter.
 *
 * Parameters.
 * pScreenInfo
 *		Ptr			:Screen handler pointer having screen information.
 *		data		:Pointer to the video port's private data
 *		attribute	:The port attribute to be set
 *		value		:Value of the attribute to be set.  
 *					 
 * Returns		:Sucess if the attribute is supported, else BadMatch
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
static int
GX2SetPortAttribute(ScrnInfoPtr pScrn,
		    Atom attribute, INT32 value, pointer data)
{
   GeodePortPrivPtr pPriv = (GeodePortPrivPtr) data;

   GX2AccelSync(pScrn);
   if (attribute == xvColorKey) {
      pPriv->colorKey = value;
      GX2SetColorkey(pScrn, pPriv);
   }
#if DBUF
   else if (attribute == xvDoubleBuffer) {
      if ((value < 0) || (value > 1))
	 return BadValue;
      pPriv->doubleBuffer = value;
   }
#endif
   else if (attribute == xvColorKeyMode) {
      pPriv->colorKeyMode = value;
      GX2SetColorkey(pScrn, pPriv);
   } else if (attribute == xvFilter) {
      pPriv->filter = value;
      GFX(set_video_filter(pPriv->filter, pPriv->filter));
   } else
      return BadMatch;

   return Success;
}

/*----------------------------------------------------------------------------
 * GX2GetPortAttribute
 *
 * Description	:This function is used to get the attributes of a port like hue,
 *				 saturation,brightness or contrast.
 *
 * Parameters.
 * pScreenInfo
 *		Ptr			:Screen handler pointer having screen information.
 *		data		:Pointer to the video port's private data
 *		attribute	:The port attribute to be read
 *		value		:Pointer to the value of the attribute to be read.  
 *					 
 * Returns		:Sucess if the attribute is supported, else BadMatch
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
static int
GX2GetPortAttribute(ScrnInfoPtr pScrn,
		    Atom attribute, INT32 * value, pointer data)
{
   GeodePortPrivPtr pPriv = (GeodePortPrivPtr) data;

   if (attribute == xvColorKey) {
      *value = pPriv->colorKey;
   }
#if DBUF
   else if (attribute == xvDoubleBuffer) {
      *value = (pPriv->doubleBuffer) ? 1 : 0;
   }
#endif
   else if (attribute == xvColorKeyMode) {
      *value = pPriv->colorKeyMode;
   } else if (attribute == xvFilter) {
      *value = pPriv->filter;
   } else
      return BadMatch;

   return Success;
}

/*----------------------------------------------------------------------------
 * GX2QueryBestSize
 *
 * Description	:This function provides a way to query what the destination dimensions
 *				 would end up being if they were to request that an area vid_w by vid_h
 *               from the video stream be scaled to rectangle of drw_w by drw_h on 
 *				 the screen.
 *
 * Parameters.
 * ScreenInfoPtr
 *		pScrn		:Screen handler pointer having screen information.
 *		data		:Pointer to the video port's private data
 *      vid_w,vid_h	:Width and height of the video data.
 *		drw_w,drw_h :Width and height of the scaled rectangle.
 *		p_w,p_h		:Width and height of the destination rectangle. 
 *					 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/
static void
GX2QueryBestSize(ScrnInfoPtr pScrn,
		 Bool motion,
		 short vid_w, short vid_h,
		 short drw_w, short drw_h,
		 unsigned int *p_w, unsigned int *p_h, pointer data)
{
   *p_w = drw_w;
   *p_h = drw_h;

   if (*p_w > 16384)
      *p_w = 16384;
}

static void
GX2CopyGreyscale(unsigned char *src,
		 unsigned char *dst, int srcPitch, int dstPitch, int h, int w)
{
   int i;
   unsigned char *src2 = src;
   unsigned char *dst2 = dst;
   unsigned char *dst3;
   unsigned char *src3;

   dstPitch <<= 1;

   while (h--) {
      dst3 = dst2;
      src3 = src2;
      for (i = 0; i < w; i++) {
	 *dst3++ = *src3++;		/* Copy Y data */
	 *dst3++ = 0x80;		/* Fill UV with 0x80 - greyscale */
      }
      src3 = src2;
      for (i = 0; i < w; i++) {
	 *dst3++ = *src3++;		/* Copy Y data */
	 *dst3++ = 0x80;		/* Fill UV with 0x80 - greyscale */
      }
      dst2 += dstPitch;
      src2 += srcPitch;
   }
}

/*----------------------------------------------------------------------------
 * GX2CopyData420
 *
 * Description	: Copies data from src to destination
 *
 * Parameters.
 *		src			: pointer to the source data
 *		dst			: pointer to destination data
 *		srcPitch	: pitch of the srcdata
 *		dstPitch	: pitch of the destination data 
 *		h & w		: height and width of source data
 *					 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/

static void
GX2CopyData420(unsigned char *src, unsigned char *dst,
	       int srcPitch, int dstPitch, int h, int w)
{
   while (h--) {
      memcpy(dst, src, w);
      src += srcPitch;
      dst += dstPitch;
   }
}

/*----------------------------------------------------------------------------
 * GX2CopyData422
 *
 * Description	: Copies data from src to destination
 *
 * Parameters.
 *		src			: pointer to the source data
 *		dst			: pointer to destination data
 *		srcPitch	: pitch of the srcdata
 *		dstPitch	: pitch of the destination data 
 *		h & w		: height and width of source data
 *					 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/

static void
GX2CopyData422(unsigned char *src, unsigned char *dst,
	       int srcPitch, int dstPitch, int h, int w)
{
   w <<= 1;
   while (h--) {
      memcpy(dst, src, w);
      src += srcPitch;
      dst += dstPitch;
   }
}

static FBAreaPtr
GX2AllocateMemory(ScrnInfoPtr pScrn, FBAreaPtr area, int numlines)
{
   ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
   FBAreaPtr new_area;

   if (area) {
      if ((area->box.y2 - area->box.y1) >= numlines)
	 return area;

      if (xf86ResizeOffscreenArea(area, pScrn->displayWidth, numlines))
	 return area;

      xf86FreeOffscreenArea(area);
   }

   new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
					numlines, 0, NULL, NULL, NULL);

   if (!new_area) {
      int max_w, max_h;

      xf86QueryLargestOffscreenArea(pScreen, &max_w, &max_h, 0,
				    FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);

      if ((max_w < pScrn->displayWidth) || (max_h < numlines))
	 return NULL;

      xf86PurgeUnlockedOffscreenAreas(pScreen);
      new_area = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
					   numlines, 0, NULL, NULL, NULL);
   }

   return new_area;
}

static BoxRec dstBox;
static int srcPitch = 0, srcPitch2 = 0, dstPitch = 0, dstPitch2 = 0;
static INT32 Bx1, Bx2, By1, By2;
static int top, left, npixels, nlines;
static int offset, s1offset = 0, s2offset = 0, s3offset = 0;
static unsigned char *dst_start;
static int d2offset = 0, d3offset = 0;
static Bool
RegionsIntersect(BoxPtr pRcl1, BoxPtr pRcl2, BoxPtr pRclResult)
{
   pRclResult->x1 = max(pRcl1->x1, pRcl2->x1);
   pRclResult->x2 = min(pRcl1->x2, pRcl2->x2);

   if (pRclResult->x1 <= pRclResult->x2) {
      pRclResult->y1 = max(pRcl1->y1, pRcl2->y1);
      pRclResult->y2 = min(pRcl1->y2, pRcl2->y2);

      if (pRclResult->y1 <= pRclResult->y2) {
	 return (TRUE);
      }
   }

   return (FALSE);
}

void
GX2SetVideoPosition(int x, int y, int width, int height,
		    short src_w, short src_h, short drw_w, short drw_h,
		    int id, int offset, ScrnInfoPtr pScrn)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   long xstart, ystart, xend, yend;
   unsigned long lines = 0;
   unsigned long y_extra, uv_extra = 0;
   BoxRec ovly, display, result;

#if defined(STB_X)
   unsigned long startAddress = 0;
#endif

   xend = x + drw_w;
   yend = y + drw_h;

   /* Take care of panning when panel is present */

#if defined(STB_X)
   Gal_get_display_offset(&startAddress);
   DeltaY = startAddress / pGeode->Pitch;
   DeltaX = startAddress & (pGeode->Pitch - 1);
   DeltaX /= (pScrn->bitsPerPixel >> 3);
#endif

   if (pGeode->Panel) {
      ovly.x1 = x;
      ovly.x2 = x + pGeode->video_dstw;
      ovly.y1 = y;
      ovly.y2 = y + pGeode->video_dsth;

      display.x1 = DeltaX;
      display.x2 = DeltaX + pGeode->FPBX;
      display.y1 = DeltaY;
      display.y2 = DeltaY + pGeode->FPBY;
      x = xend = 0;
      if (RegionsIntersect(&display, &ovly, &result)) {
	 x = ovly.x1 - DeltaX;
	 xend = ovly.x2 - DeltaX;
	 y = ovly.y1 - DeltaY;
	 yend = ovly.y2 - DeltaY;
      }
   }

   /*  LEFT CLIPPING */

   if (x < 0) {
      xstart = 0;
   } else {
      xstart = (unsigned long)x;
   }
   drw_w -= (xstart - x);

   /*  TOP CLIPPING */

   if (y < 0) {
      lines = (-y) * src_h / drw_h;
      ystart = 0;
      drw_h += y;
      y_extra = lines * dstPitch;
      uv_extra = (lines >> 1) * (dstPitch2);
   } else {
      ystart = y;
      lines = 0;
      y_extra = 0;
   }
   GFX(set_video_window(xstart, ystart, xend - xstart, yend - ystart));
   if ((id == FOURCC_Y800) || (id == FOURCC_I420) || (id == FOURCC_YV12)) {
      GFX(set_video_yuv_offsets(offset + y_extra,
				offset + d3offset + uv_extra,
				offset + d2offset + uv_extra));
   } else {
      GFX(set_video_offset(offset + y_extra));
   }
   GFX(set_video_left_crop(xstart - x));
}

/*----------------------------------------------------------------------------
 * GX2DisplayVideo
 *
 * Description		: This function sets up the video registers for playing video
 *					  It sets up the video format,width, height & position of the 
 *					  video window ,video offsets( y,u,v) and video pitches(y,u,v)	
 * Parameters.
 *					 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/

static void
GX2DisplayVideo(ScrnInfoPtr pScrn,
		int id,
		int offset,
		short width, short height,
		int pitch,
		int x1, int y1, int x2, int y2,
		BoxPtr dstBox,
		short src_w, short src_h, short drw_w, short drw_h)
{
   GeodePtr pGeode = GEODEPTR(pScrn);

   GX2AccelSync(pScrn);

   GFX(set_video_enable(1));

   switch (id) {
   case FOURCC_UYVY:			/* UYVY */
      GFX(set_video_format(VIDEO_FORMAT_UYVY));
      GFX(set_video_size(width, height));
      break;
   case FOURCC_Y800:			/* Y800 - greyscale - we munge it! */
   case FOURCC_YV12:			/* YV12 */
   case FOURCC_I420:			/* I420 */
      GFX(set_video_format(VIDEO_FORMAT_Y0Y1Y2Y3));
      GFX(set_video_size(width, height));
      GFX(set_video_yuv_pitch(dstPitch, dstPitch2));
      break;
   case FOURCC_YUY2:			/* YUY2 */
      GFX(set_video_format(VIDEO_FORMAT_YUYV));
      GFX(set_video_size(width, height));
      break;
   case FOURCC_Y2YU:			/* Y2YU */
      GFX(set_video_format(VIDEO_FORMAT_Y2YU));
      GFX(set_video_size(width, height));
      break;
   case FOURCC_YVYU:			/* YVYU */
      GFX(set_video_format(VIDEO_FORMAT_YVYU));
      GFX(set_video_size(width, height));
      break;
   }
   if (pGeode->Panel) {
      pGeode->video_x = dstBox->x1;
      pGeode->video_y = dstBox->y1;
      pGeode->video_w = width;
      pGeode->video_h = height;
      pGeode->video_srcw = src_w;
      pGeode->video_srch = src_h;
      pGeode->video_dstw = drw_w;
      pGeode->video_dsth = drw_h;
      pGeode->video_offset = offset;
      pGeode->video_id = id;
      pGeode->video_scrnptr = pScrn;
   }
   GFX(set_video_scale(width, height, drw_w, drw_h));
   GX2SetVideoPosition(dstBox->x1, dstBox->y1, width, height, src_w,
		       src_h, drw_w, drw_h, id, offset, pScrn);

}

/*----------------------------------------------------------------------------
 * GX2PutImage	: This function writes a single frame of video into a drawable.
 *		The position and size of the source rectangle is specified by src_x,src_y,
 *		src_w and src_h. This data is stored in a system memory buffer at buf.  
 *		The position and size of the destination rectangle is specified by drw_x,
 *      drw_y,drw_w,drw_h.The data is in the format indicated by the image descriptor 
 *		and represents a source of size width by height.  If sync is TRUE the driver 
 *		should not return from this function until it is through reading the data from 
 *		buf.  Returning when sync is TRUE indicates that it is safe for the data at buf
 *		to be replaced,freed, or modified.
 *
 *
 * Description		: 
 * Parameters.
 *					 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/

static int
GX2PutImage(ScrnInfoPtr pScrn,
	    short src_x, short src_y,
	    short drw_x, short drw_y,
	    short src_w, short src_h,
	    short drw_w, short drw_h,
	    int id, unsigned char *buf,
	    short width, short height,
	    Bool sync, RegionPtr clipBoxes, pointer data)
{
   GeodePortPrivPtr pPriv = (GeodePortPrivPtr) data;
   GeodePtr pGeode = GEODEPTR(pScrn);
   int new_h;

#if REINIT
   BOOL ReInitVideo = FALSE;
#endif

#if XV_PROFILE
   long oldtime, newtime;

   UpdateCurrentTime();
   oldtime = currentTime.milliseconds;
#endif

#if REINIT
/* update cliplist */
   if (!RegionsEqual(&pPriv->clip, clipBoxes)) {
      ReInitVideo = TRUE;
   }
   if (ReInitVideo) {
      DEBUGMSG(1, (0, X_NONE, "Regional Not Equal - Init\n"));
#endif

      if (drw_w > 16384)
	 drw_w = 16384;

      /* Clip */
      Bx1 = src_x;
      Bx2 = src_x + src_w;
      By1 = src_y;
      By2 = src_y + src_h;

      if ((Bx1 >= Bx2) || (By1 >= By2))
	 return Success;

      dstBox.x1 = drw_x;
      dstBox.x2 = drw_x + drw_w;
      dstBox.y1 = drw_y;
      dstBox.y2 = drw_y + drw_h;

      dstBox.x1 -= pScrn->frameX0;
      dstBox.x2 -= pScrn->frameX0;
      dstBox.y1 -= pScrn->frameY0;
      dstBox.y2 -= pScrn->frameY0;

      switch (id) {
      case FOURCC_YV12:
      case FOURCC_I420:

	 srcPitch = (width + 3) & ~3;	/* of luma */
	 dstPitch = (width + 31) & ~31;

	 s2offset = srcPitch * height;
	 d2offset = dstPitch * height;

	 srcPitch2 = ((width >> 1) + 3) & ~3;
	 dstPitch2 = ((width >> 1) + 15) & ~15;

	 s3offset = (srcPitch2 * (height >> 1)) + s2offset;
	 d3offset = (dstPitch2 * (height >> 1)) + d2offset;

	 new_h = dstPitch * height;	/* Y */
	 new_h += (dstPitch2 * height);	/* U+V */
	 new_h += pGeode->Pitch - 1;
	 new_h /= pGeode->Pitch;
	 break;

      case FOURCC_UYVY:
      case FOURCC_YUY2:
      case FOURCC_Y800:
      default:
	 dstPitch = ((width << 1) + 3) & ~3;
	 srcPitch = (width << 1);
	 new_h = ((dstPitch * height) + pGeode->Pitch - 1) / pGeode->Pitch;
	 break;
      }

#if DBUF
      if (pPriv->doubleBuffer)
	 new_h <<= 1;
#endif

      if (!(pPriv->area = GX2AllocateMemory(pScrn, pPriv->area, new_h)))
	 return BadAlloc;

      /* copy data */
      top = By1;
      left = Bx1 & ~1;
      npixels = ((Bx2 + 1) & ~1) - left;

      switch (id) {
      case FOURCC_YV12:
      case FOURCC_I420:
	 {
	    int tmp;

	    top &= ~1;
	    offset = (pPriv->area->box.y1 * pGeode->Pitch) + (top * dstPitch);

#if DBUF
	    if (pPriv->doubleBuffer && pPriv->currentBuffer)
	       offset += (new_h >> 1) * pGeode->Pitch;
#endif

	    dst_start = pGeode->FBBase + offset + left;
	    tmp = ((top >> 1) * srcPitch2) + (left >> 1);
	    s2offset += tmp;
	    s3offset += tmp;
	    if (id == FOURCC_I420) {
	       tmp = s2offset;
	       s2offset = s3offset;
	       s3offset = tmp;
	    }
	    nlines = ((By2 + 1) & ~1) - top;
	 }
	 break;
      case FOURCC_UYVY:
      case FOURCC_YUY2:
      case FOURCC_Y800:
      default:
	 left <<= 1;
	 buf += (top * srcPitch) + left;
	 nlines = By2 - top;
	 offset = (pPriv->area->box.y1 * pGeode->Pitch) + (top * dstPitch);
#if DBUF
	 if (pPriv->doubleBuffer && pPriv->currentBuffer)
	    offset += (new_h >> 1) * pGeode->Pitch;
#endif

	 dst_start = pGeode->FBBase + offset + left;
	 break;
      }
      s1offset = (top * srcPitch) + left;

#if REINIT
      /* update cliplist */
      REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
      if (pPriv->colorKeyMode == 0) {
	 /* draw these */
	 XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
			   REGION_NUM_RECTS(clipBoxes),
			   REGION_RECTS(clipBoxes));
      }
      GX2DisplayVideo(pScrn, id, offset, width, height, dstPitch,
		      Bx1, By1, Bx2, By2, &dstBox, src_w, src_h, drw_w,
		      drw_h);
   }
#endif

   switch (id) {

   case FOURCC_Y800:
      GX2CopyGreyscale(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
      break;
   case FOURCC_YV12:
   case FOURCC_I420:
      GX2CopyData420(buf + s1offset, dst_start, srcPitch, dstPitch, nlines,
		     npixels);
      GX2CopyData420(buf + s2offset, dst_start + d2offset, srcPitch2,
		     dstPitch2, nlines >> 1, npixels >> 1);
      GX2CopyData420(buf + s3offset, dst_start + d3offset, srcPitch2,
		     dstPitch2, nlines >> 1, npixels >> 1);
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      GX2CopyData422(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
      break;
   }
#if !REINIT
   /* update cliplist */
   REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
   if (pPriv->colorKeyMode == 0) {
      /* draw these */
      XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
			REGION_NUM_RECTS(clipBoxes), REGION_RECTS(clipBoxes));
   }
   GX2DisplayVideo(pScrn, id, offset, width, height, dstPitch,
		   Bx1, By1, Bx2, By2, &dstBox, src_w, src_h, drw_w, drw_h);
#endif

#if XV_PROFILE
   UpdateCurrentTime();
   newtime = currentTime.milliseconds;
   DEBUGMSG(1, (0, X_NONE, "PI %d\n", newtime - oldtime));
#endif

#if DBUF
   pPriv->currentBuffer ^= 1;
#endif

   pPriv->videoStatus = CLIENT_VIDEO_ON;
   pGeode->OverlayON = TRUE;
   return Success;
}

/*----------------------------------------------------------------------------
 * GX2QueryImageAttributes
 *
 * Description	:This function is called to let the driver specify how data
 *				 for a particular image of size width by height should be 
 *				 stored. 		
 *
 * Parameters.
 * pScreenInfo
 *		Ptr			:Screen handler pointer having screen information.
 *		id			:Id for the video format
 *		width		:width  of the image (can be modified by the driver)  
 *		height		:height of the image (can be modified by the driver)  
 * Returns		: Size of the memory required for storing this image
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/
static int
GX2QueryImageAttributes(ScrnInfoPtr pScrn,
			int id,
			unsigned short *w, unsigned short *h,
			int *pitches, int *offsets)
{
   int size;
   int tmp;

   DEBUGMSG(0, (0, X_NONE, "QueryImageAttributes %X\n", id));

   if (*w > 1024)
      *w = 1024;
   if (*h > 1024)
      *h = 1024;

   *w = (*w + 1) & ~1;
   if (offsets)
      offsets[0] = 0;

   switch (id) {
   case FOURCC_YV12:
   case FOURCC_I420:
      *h = (*h + 1) & ~1;
      size = (*w + 3) & ~3;
      if (pitches)
	 pitches[0] = size;
      size *= *h;
      if (offsets)
	 offsets[1] = size;
      tmp = ((*w >> 1) + 3) & ~3;
      if (pitches)
	 pitches[1] = pitches[2] = tmp;
      tmp *= (*h >> 1);
      size += tmp;
      if (offsets)
	 offsets[2] = size;
      size += tmp;
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   case FOURCC_Y800:
   default:
      size = *w << 1;
      if (pitches)
	 pitches[0] = size;
      size *= *h;
      break;
   }
   return size;
}

static void
GX2BlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
   ScreenPtr pScreen = screenInfo.screens[i];
   ScrnInfoPtr pScrn = xf86Screens[i];
   GeodePtr pGeode = GEODEPTR(pScrn);
   GeodePortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

   pScreen->BlockHandler = pGeode->BlockHandler;
   (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
   pScreen->BlockHandler = GX2BlockHandler;

   GX2AccelSync(pScrn);
   if (pPriv->videoStatus & TIMER_MASK) {
      UpdateCurrentTime();
      if (pPriv->videoStatus & OFF_TIMER) {
	 if (pPriv->offTime < currentTime.milliseconds) {
	    GFX(set_video_enable(0));
	    pPriv->videoStatus = FREE_TIMER;
	    pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	 }
      } else {				/* FREE_TIMER */
	 if (pPriv->freeTime < currentTime.milliseconds) {
	    if (pPriv->area) {
	       xf86FreeOffscreenArea(pPriv->area);
	       pPriv->area = NULL;
	    }
	    pPriv->videoStatus = 0;
	 }
      }
   }
}

/****************** Offscreen stuff ***************/

typedef struct
{
   FBAreaPtr area;
   FBLinearPtr linear;
   Bool isOn;
}
OffscreenPrivRec, *OffscreenPrivPtr;

/*----------------------------------------------------------------------------
 * GX2AllocateSurface
 *
 * Description	:This function allocates an area of w by h in the offscreen
 * Parameters.
 * ScreenPtr
 *		pScreen	:Screen handler pointer having screen information.
 * 
 * Returns		:None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/

static int
GX2AllocateSurface(ScrnInfoPtr pScrn,
		   int id,
		   unsigned short w, unsigned short h, XF86SurfacePtr surface)
{
   FBAreaPtr area;
   int pitch, fbpitch, numlines;
   OffscreenPrivPtr pPriv;

   if ((w > 1024) || (h > 1024))
      return BadAlloc;

   w = (w + 1) & ~1;
   pitch = ((w << 1) + 15) & ~15;
   fbpitch = pScrn->bitsPerPixel * pScrn->displayWidth >> 3;
   numlines = ((pitch * h) + fbpitch - 1) / fbpitch;

   if (!(area = GX2AllocateMemory(pScrn, NULL, numlines)))
      return BadAlloc;

   surface->width = w;
   surface->height = h;

   if (!(surface->pitches = xalloc(sizeof(int))))
      return BadAlloc;
   if (!(surface->offsets = xalloc(sizeof(int)))) {
      xfree(surface->pitches);
      return BadAlloc;
   }
   if (!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
      xfree(surface->pitches);
      xfree(surface->offsets);
      return BadAlloc;
   }

   pPriv->area = area;
   pPriv->isOn = FALSE;

   surface->pScrn = pScrn;
   surface->id = id;
   surface->pitches[0] = pitch;
   surface->offsets[0] = area->box.y1 * fbpitch;
   surface->devPrivate.ptr = (pointer) pPriv;

   return Success;
}

static int
GX2StopSurface(XF86SurfacePtr surface)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;

   if (pPriv->isOn) {
      pPriv->isOn = FALSE;
   }

   return Success;
}

static int
GX2FreeSurface(XF86SurfacePtr surface)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;

   if (pPriv->isOn)
      GX2StopSurface(surface);
   xf86FreeOffscreenArea(pPriv->area);
   xfree(surface->pitches);
   xfree(surface->offsets);
   xfree(surface->devPrivate.ptr);

   return Success;
}

static int
GX2GetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 * value)
{
   return GX2GetPortAttribute(pScrn, attribute, value,
			      (pointer) (GET_PORT_PRIVATE(pScrn)));
}

static int
GX2SetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 value)
{
   return GX2SetPortAttribute(pScrn, attribute, value,
			      (pointer) (GET_PORT_PRIVATE(pScrn)));
}

static int
GX2DisplaySurface(XF86SurfacePtr surface,
		  short src_x, short src_y,
		  short drw_x, short drw_y,
		  short src_w, short src_h,
		  short drw_w, short drw_h, RegionPtr clipBoxes)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;
   ScrnInfoPtr pScrn = surface->pScrn;
   GeodePortPrivPtr portPriv = GET_PORT_PRIVATE(pScrn);
   INT32 x1, y1, x2, y2;
   BoxRec dstBox;

   DEBUGMSG(0, (0, X_NONE, "DisplaySuface\n"));
   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;

   if ((x1 >= x2) || (y1 >= y2))
      return Success;

   dstBox.x1 -= pScrn->frameX0;
   dstBox.x2 -= pScrn->frameX0;
   dstBox.y1 -= pScrn->frameY0;
   dstBox.y2 -= pScrn->frameY0;

   xf86XVFillKeyHelper(pScrn->pScreen, portPriv->colorKey, clipBoxes);

   GX2DisplayVideo(pScrn, surface->id, surface->offsets[0],
		   surface->width, surface->height, surface->pitches[0],
		   x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);

   pPriv->isOn = TRUE;
   if (portPriv->videoStatus & CLIENT_VIDEO_ON) {
      REGION_EMPTY(pScrn->pScreen, &portPriv->clip);
      UpdateCurrentTime();
      portPriv->videoStatus = FREE_TIMER;
      portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
   }

   return Success;
}

/*----------------------------------------------------------------------------
 * GX2InitOffscreenImages
 *
 * Description	:This function sets up the offscreen memory management.It fills 
 *				 in the XF86OffscreenImagePtr structure with functions to handle
 *				 offscreen memory operations. 	
 *
 * Parameters.
 * ScreenPtr
 *		pScreen	:Screen handler pointer having screen information.
 * 
 * Returns		: None
 *
 * Comments		:None
 *
*----------------------------------------------------------------------------
*/
static void
GX2InitOffscreenImages(ScreenPtr pScreen)
{
   XF86OffscreenImagePtr offscreenImages;

   /* need to free this someplace */
   if (!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
      return;

   offscreenImages[0].image = &Images[0];
   offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
   offscreenImages[0].alloc_surface = GX2AllocateSurface;
   offscreenImages[0].free_surface = GX2FreeSurface;
   offscreenImages[0].display = GX2DisplaySurface;
   offscreenImages[0].stop = GX2StopSurface;
   offscreenImages[0].setAttribute = GX2SetSurfaceAttribute;
   offscreenImages[0].getAttribute = GX2GetSurfaceAttribute;
   offscreenImages[0].max_width = 1024;
   offscreenImages[0].max_height = 1024;
   offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
   offscreenImages[0].attributes = Attributes;

   xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

#endif /* !XvExtension */
