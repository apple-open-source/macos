/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx1_shadow.c,v 1.2 2003/01/14 09:34:32 alanh Exp $ */
/*
 * $Workfile: nsc_gx1_shadow.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * File Contents: Direct graphics display routines are implemented and 
 *                graphics rendering are all done in memory.
 *
 * Project:       Geode Xfree Frame buffer device driver.
 *
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

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "nsc.h"
#include "shadowfb.h"
#include "servermd.h"

void GX1RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void GX1PointerMoved(int index, int x, int y);
void GX1RefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void GX1RefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void GX1RefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void GX1RefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/*----------------------------------------------------------------------------
 * GX1RefreshArea.
 *
 * Description	:This function  copies the memory to be displayed from the
 *               shadow pointer.
 * Parameters.
 *    pScrn		:Pointer to screen structure.
 *    num		:Specifies the num of squarebox area to be displayed.
 *    pbox		:Points to square of memory to be displayed.
 * Returns		:none
 *
 * Comments		: none
 *
*----------------------------------------------------------------------------
*/
void
GX1RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   int width, height, Bpp, FBPitch;
   unsigned char *src, *dst;

   Bpp = pScrn->bitsPerPixel >> 3;
   FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);
   while (num--) {
      width = (pbox->x2 - pbox->x1) * Bpp;
      height = pbox->y2 - pbox->y1;
      src = pGeode->ShadowPtr + (pbox->y1 * pGeode->ShadowPitch) +
	    (pbox->x1 * Bpp);
      dst = pGeode->FBBase + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);
      while (height--) {
	 memcpy(dst, src, width);
	 dst += FBPitch;
	 src += pGeode->ShadowPitch;
      }

      pbox++;
   }
}

/*----------------------------------------------------------------------------
 * GX1PointerMoved.
 *
 * Description	:This function moves one screen memory from one area to other.
 *
 * Parameters.
 *    index		:Pointer to screen index.
 *     x		:Specifies the new x co-ordinates of new area.
 *     y		:Specifies the new y co-ordinates of new area.
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX1PointerMoved(int index, int x, int y)
{
   ScrnInfoPtr pScrn = xf86Screens[index];
   GeodePtr pGeode = GEODEPTR(pScrn);
   int newX, newY;

   if (pGeode->Rotate == 1) {
      newX = pScrn->pScreen->height - y - 1;
      newY = x;
   } else {
      newX = y;
      newY = pScrn->pScreen->width - x - 1;
   }
   (*pGeode->PointerMoved) (index, newX, newY);
}

/*----------------------------------------------------------------------------
 * GX1RefreshArea8.
 *
 * Description	:This function  copies the memory to be displayed from the
 *                 shadow pointer by 8bpp.
 * Parameters.
 *    pScrn		:Pointer to screen structure.
 *    num		:Specifies the num of squarebox area to be displayed.
 *    pbox		:Points to square of memory to be displayed.
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX1RefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   int count, width, height, y1, y2, dstPitch, srcPitch, srcPitch2,
	 srcPitch3, srcPitch4;
   CARD8 *dstPtr, *srcPtr, *src;
   CARD32 *dst;

   dstPitch = pScrn->displayWidth;
   srcPitch = -pGeode->Rotate * pGeode->ShadowPitch;
   srcPitch2 = srcPitch * 2;
   srcPitch3 = srcPitch * 3;
   srcPitch4 = srcPitch * 4;
   while (num--) {
      width = pbox->x2 - pbox->x1;
      y1 = pbox->y1 & ~3;
      y2 = (pbox->y2 + 3) & ~3;
      height = (y2 - y1) >> 2;		/* in dwords */

      if (pGeode->Rotate == 1) {
	 dstPtr = pGeode->FBBase +
	       (pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	 srcPtr = pGeode->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
      } else {
	 dstPtr = pGeode->FBBase +
	       ((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	 srcPtr = pGeode->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
      }
      while (width--) {
	 src = srcPtr;
	 dst = (CARD32 *) dstPtr;
	 count = height;
	 while (count--) {
	    *(dst++) = src[0] | (src[srcPitch] << 8) |
		  (src[srcPitch2] << 16) | (src[srcPitch3] << 24);
	    src += srcPitch4;
	 }
	 srcPtr += pGeode->Rotate;
	 dstPtr += dstPitch;
      }
      pbox++;
   }
}

/*----------------------------------------------------------------------------
 * GX1RefreshArea16.
 *
 * Description	:This function  copies the memory to be displayed from the
 *               shadow pointer by 16bpp.
 * Parameters:
 *    pScrn		:Pointer to screen structure.
 *    num       :Specifies the num of squarebox area to be displayed.
 *    pbox      :Points to square of memory to be displayed.
 * Returns		:none
 *
 * Comments     :none
 *
*----------------------------------------------------------------------------
*/
void
GX1RefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   int count, width, height, y1, y2, dstPitch, srcPitch, srcPitch2;
   CARD16 *dstPtr, *srcPtr, *src;
   CARD32 *dst;

   dstPitch = pScrn->displayWidth;
   srcPitch = -pGeode->Rotate * pGeode->ShadowPitch >> 1;
   srcPitch2 = srcPitch * 2;
   while (num--) {
      width = pbox->x2 - pbox->x1;
      y1 = pbox->y1 & ~1;
      y2 = (pbox->y2 + 1) & ~1;
      height = (y2 - y1) >> 1;		/* in dwords */
      if (pGeode->Rotate == 1) {
	 dstPtr = (CARD16 *) pGeode->FBBase +
	       (pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	 srcPtr = (CARD16 *) pGeode->ShadowPtr +
	       ((1 - y2) * srcPitch) + pbox->x1;
      } else {
	 dstPtr = (CARD16 *) pGeode->FBBase +
	       ((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	 srcPtr = (CARD16 *) pGeode->ShadowPtr +
	       (y1 * srcPitch) + pbox->x2 - 1;
      }

      while (width--) {
	 src = srcPtr;
	 dst = (CARD32 *) dstPtr;
	 count = height;
	 while (count--) {
	    *(dst++) = src[0] | (src[srcPitch] << 16);
	    src += srcPitch2;
	 }
	 srcPtr += pGeode->Rotate;
	 dstPtr += dstPitch;
      }

      pbox++;
   }
}

/*----------------------------------------------------------------------------
 * GX1RefreshArea24.
 *
 * Description	:This function  copies the memory to be displayed from the
 *               shadow pointer by 24bpp.
 * Parameters.
 *    pScrn		:Pointer to screen structure.
 *    num		:Specifies the num of squarebox area to be displayed.
 *    pbox        :Points to square of memory to be displayed.
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX1RefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   int count, width, height, y1, y2, dstPitch, srcPitch, srcPitch2, srcPitch3;
   CARD8 *dstPtr, *srcPtr, *src;
   CARD32 *dst;

   dstPitch = BitmapBytePad(pScrn->displayWidth * 24);
   srcPitch = -pGeode->Rotate * pGeode->ShadowPitch;
   srcPitch2 = srcPitch * 2;
   srcPitch3 = srcPitch * 3;
   while (num--) {
      width = pbox->x2 - pbox->x1;
      y1 = pbox->y1 & ~3;
      y2 = (pbox->y2 + 3) & ~3;
      height = (y2 - y1) >> 2;		/* blocks of 3 dwords */
      if (pGeode->Rotate == 1) {
	 dstPtr = pGeode->FBBase +
	       (pbox->x1 * dstPitch) + ((pScrn->virtualX - y2) * 3);
	 srcPtr = pGeode->ShadowPtr + ((1 - y2) * srcPitch) + (pbox->x1 * 3);
      } else {
	 dstPtr = pGeode->FBBase +
	       ((pScrn->virtualY - pbox->x2) * dstPitch) + (y1 * 3);
	 srcPtr = pGeode->ShadowPtr + (y1 * srcPitch) + (pbox->x2 * 3) - 3;
      }
      while (width--) {
	 src = srcPtr;
	 dst = (CARD32 *) dstPtr;
	 count = height;
	 while (count--) {
	    dst[0] = src[0] | (src[1] << 8) | (src[2] << 16) |
		  (src[srcPitch] << 24);
	    dst[1] = src[srcPitch + 1] | (src[srcPitch + 2] << 8) |
		  (src[srcPitch2] << 16) | (src[srcPitch2 + 1] << 24);
	    dst[2] = src[srcPitch2 + 2] | (src[srcPitch3] << 8) |
		  (src[srcPitch3 + 1] << 16) | (src[srcPitch3 + 2] << 24);
	    dst += 3;
	    src += srcPitch << 2;
	 }
	 srcPtr += pGeode->Rotate * 3;
	 dstPtr += dstPitch;
      }
      pbox++;
   }
}

/*----------------------------------------------------------------------------
 * GX1RefreshArea32.
 *
 * Description	:This function  copies the memory to be displayed from the
 *                    shadow pointer by 32bpp.
 * Parameters:
 *    pScrn		:Pointer to screen structure.
 *    num		:Specifies the num of squarebox area to be displayed.
 *    pbox		:Points to square of memory to be displayed.
 * Returns		: none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX1RefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   int count, width, height, dstPitch, srcPitch;
   CARD32 *dstPtr, *srcPtr, *src, *dst;

   dstPitch = pScrn->displayWidth;
   srcPitch = -pGeode->Rotate * pGeode->ShadowPitch >> 2;
   while (num--) {
      width = pbox->x2 - pbox->x1;
      height = pbox->y2 - pbox->y1;

      if (pGeode->Rotate == 1) {
	 dstPtr = (CARD32 *) pGeode->FBBase +
	       (pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
	 srcPtr = (CARD32 *) pGeode->ShadowPtr +
	       ((1 - pbox->y2) * srcPitch) + pbox->x1;
      } else {
	 dstPtr = (CARD32 *) pGeode->FBBase +
	       ((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	 srcPtr = (CARD32 *) pGeode->ShadowPtr +
	       (pbox->y1 * srcPitch) + pbox->x2 - 1;
      }
      while (width--) {
	 src = srcPtr;
	 dst = dstPtr;
	 count = height;
	 while (count--) {
	    *(dst++) = *src;
	    src += srcPitch;
	 }
	 srcPtr += pGeode->Rotate;
	 dstPtr += dstPitch;
      }
      pbox++;
   }
}

/* End of file */
