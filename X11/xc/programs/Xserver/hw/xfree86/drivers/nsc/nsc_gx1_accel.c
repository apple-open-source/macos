/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx1_accel.c,v 1.5 2003/02/11 13:36:41 alanh Exp $ */
/*
 * $Workfile: nsc_gx1_accel.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * File Contents: This file is consists of main Xfree
 *                acceleration supported routines like solid fill used
 *                here.
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

/*
 * Fixes by
 * Alan Hourihane <alanh@fairlite.demon.co.uk>
 */

/* Xfree86 header files */
#include "vgaHW.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "xaalocal.h"
#include "xf86fbman.h"
#include "miline.h"
#include "xf86_libc.h"
#include "xaarop.h"
#include "nsc.h"

#define SCR2SCREXP 0

/* STATIC VARIABLES FOR THIS FILE
 * Used to maintain state between setup and rendering calls.
 */

static int GeodeTransparent;
static int GeodeTransColor;
static int Geodedstx;
static int Geodedsty;
static int Geodesrcx;
static int Geodesrcy;
static int Geodewidth;
static int Geodeheight;
static int Geodebpp;
static int GeodeCounter;

#if !defined(STB_X)
static unsigned int GeodeROP = 0;
static unsigned short Geode_blt_mode = 0;
static unsigned short Geode_vector_mode = 0;
static unsigned short Geode_buffer_width = 0;
#endif
static unsigned int gu1_bpp = 0;
static unsigned int gu1_xshift = 1;
static unsigned int gu1_yshift = 1;
static unsigned short GeodebufferWidthPixels;
static unsigned int ImgBufOffset;
static unsigned short Geodebb0Base;
static unsigned short Geodebb1Base;
static XAAInfoRecPtr localRecPtr;

#define CALC_FBOFFSET(_SrcX, _SrcY) \
	(((unsigned int) (_SrcY) << gu1_yshift) |\
		(((unsigned int) (_SrcX)) << gu1_xshift))

#define GFX_WAIT_BUSY while(READ_REG16(GP_BLIT_STATUS) & BS_BLIT_BUSY) { ; }
#define GFX_WAIT_PENDING while(READ_REG16(GP_BLIT_STATUS) & BS_BLIT_PENDING) { ; }

#define BB0_BASE_3K		0x400
#define BB1_BASE_3K		0x930

Bool GX1AccelInit(ScreenPtr pScreen);
void GX1AccelSync(ScrnInfoPtr pScreenInfo);
void GX1SetupForFillRectSolid(ScrnInfoPtr pScreenInfo, int color, int rop,
			      unsigned int planemask);
void GX1SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y,
				int w, int h);
void GX1SetupFor8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				      int patternx, int patterny,
				      int rop, unsigned int planemask,
				      int trans_color);
void GX1Subsequent8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
					int patternx, int patterny, int x,
					int y, int w, int h);
void GX1SetupFor8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo, int patternx,
				     int patterny, int fg, int bg, int rop,
				     unsigned int planemask);
void GX1Subsequent8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo, int patternx,
				       int patterny, int x, int y, int w,
				       int h);
void GX1SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int xdir,
				   int ydir, int rop, unsigned int planemask,
				   int transparency_color);
void GX1SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int x1, int y1,
				     int x2, int y2, int w, int h);
void GX1SetupForSolidLine(ScrnInfoPtr pScreenInfo, int color, int rop,
			  unsigned int planemask);
void GX1SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
			   unsigned int planemask, int length,
			   unsigned char *pattern);
void GX1SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo, int x1, int y1,
				int absmaj, int absmin, int err, int len,
				int octant);
void GX1SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo, int x0, int y0,
				    int x1, int y1, int flags);
void GX1SubsequentHorVertLine(ScrnInfoPtr pScreenInfo, int x, int y, int len,
			      int dir);

void GX1SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				   int rop, unsigned int planemask,
				   int transparency_color, int bpp,
				   int depth);

void GX1SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
					 int x, int y, int w, int h,
					 int skipleft);

void GX1SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno);
void GX1FillCacheBltRects(ScrnInfoPtr pScrn, int rop, unsigned int planemask,
			  int nBox, BoxPtr pBox, int xorg, int yorg,
			  XAACacheInfoPtr pCache);
void OPTGX1SetupForFillRectSolid(ScrnInfoPtr pScreenInfo, int color, int rop,
				 unsigned int planemask);
void OPTGX1SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y,
				   int w, int h);
void OPTGX1SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int xdir,
				      int ydir, int rop,
				      unsigned int planemask,
				      int transparency_color);
void OPTGX1SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int x1,
					int y1, int x2, int y2, int w, int h);
void OPTGX1SetupForSolidLine(ScrnInfoPtr pScreenInfo, int color, int rop,
			     unsigned int planemask);
void OPTGX1SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
			      unsigned int planemask, int length,
			      unsigned char *pattern);
void OPTGX1SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo, int x1, int y1,
				   int absmaj, int absmin, int err, int len,
				   int octant);
void OPTGX1SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
				       int x0, int y0, int x1, int y1,
				       int flags);
void OPTGX1SubsequentHorVertLine(ScrnInfoPtr pScreenInfo, int x, int y,
				 int len, int dir);

void OPTGX1SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				      int rop, unsigned int planemask,
				      int transparency_color, int bpp,
				      int depth);

void OPTGX1SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
					    int x, int y, int w, int h,
					    int skipleft);

void OPTGX1SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno);

/*----------------------------------------------------------------------------
 * GX1AccelSync.
 *
 * Description	:This function is called to syncronize with the graphics
 *               engine and it waits the graphic engine is idle.This is
 *               required before allowing   direct access to the
 *               framebuffer.
 * Parameters.
 *   pScreenInfo:Screeen info pointer structure.
 *
 * Returns		:none
 *
 * Comments		:This function is called on geode_video routines.
*----------------------------------------------------------------------------
*/
void
GX1AccelSync(ScrnInfoPtr pScreenInfo)
{
   GFX(wait_until_idle());
}

/*----------------------------------------------------------------------------
 * GX1SetupForFillRectSolid.
 *
 * Description	:This routine is called to setup the solid pattern
 *               color for   future  rectangular fills or vectors.
 *
 * Parameters.
 * pScreenInfo
 *		Ptr		:Screen handler pointer having screen information.
 *    color     :Specifies the color to be filled up in defined area.
 *    rop       :Specifies the raster operation value.
 *   planemask	:Specifies the masking value based rop srcdata.
 *
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
GX1SetupForFillRectSolid(ScrnInfoPtr pScreenInfo,
			 int color, int rop, unsigned int planemask)
{
   GFX(set_solid_pattern((unsigned long)color));

   /* CHECK IF PLANEMASK IS NOT USED (ALL PLANES ENABLED) */
   if (planemask == 0xFFFFFFFF) {
      /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
      GFX(set_raster_operation(XAAPatternROP[rop]));
   } else {
      /* SELECT ROP THAT USES SOURCE DATA FOR PLANEMASK */
      GFX(set_solid_source((unsigned long)planemask));
      GFX(set_raster_operation(XAAPatternROP_PM[rop]));
   }
}

 /*----------------------------------------------------------------------------
 * GX1SubsequentFillRectSolid.
 *
 * Description	:This routine is used to fill the rectangle of previously
 *               specified  solid pattern.
 *
 * Parameters.
 *  pScreenInfo :Screen handler pointer having screen information.
 *     x and y	:Specifies the x and y co-ordinatesarea.
 *      w and h	:Specifies width and height respectively.
 *
 * Returns		:none
 *
 * Comments		:desired pattern can be set before this function by
 *               gfx_set_solid_pattern.
 * Sample application uses:
 *   - Window backgrounds. 
 *   - x11perf: rectangle tests (-rect500).
 *   - x11perf: fill trapezoid tests (-trap100).
 *   - x11perf: horizontal line segments (-hseg500).
 *----------------------------------------------------------------------------
*/
void
GX1SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y, int w,
			   int h)
{
   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */
   GeodePtr pGeode;

   pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      x += pGeode->TVOx;
      y += pGeode->TVOy;
   }

   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)w, (unsigned short)h));
}

/*----------------------------------------------------------------------------
 * GX1SetupFor8x8PatternColorExpand
 *
 * Description	:This routine is called to fill the color pattern of
 *                 8x8.
 * Parameters.
 *		pScreenInfo :Screen handler pointer having screen information.
 *		patternx	:This is set from on rop data.
 *      patterny	:This is set based on rop data.
 *		planemask	:Specifies the value of masking from rop data
 *		trans_color :to be added. 
 * Returns		:none.
 *
 * Comments     :none.
 *
*----------------------------------------------------------------------------
*/

void
GX1SetupFor8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				 int patternx, int patterny,
				 int rop, unsigned int planemask,
				 int trans_color)
{
   /* CHECK IF PLANEMASK IS NOT USED (ALL PLANES ENABLED) */
   if (planemask == 0xFFFFFFFF) {
      /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
      GFX(set_raster_operation(XAAPatternROP[rop]));
   } else {
      /* SELECT ROP THAT USES SOURCE DATA FOR PLANEMASK */
      GFX(set_solid_source((unsigned int)planemask));
      GFX(set_raster_operation(XAAPatternROP_PM[rop]));
   }
}

/*----------------------------------------------------------------------------
 * GX1Subsequent8x8PatternColorExpand
 *
 * Description	:This routine is called to fill  a rectangle with the
 *               color pattern of previously loaded pattern.
 *
 * Parameters.
 *	pScreenInfo	:Screen handler pointer having screen information.
 *  patternx	:This is set from on rop data.
 *  patterny	:This is set based on rop data.
 *      x		:x -coordinates of the destination rectangle
 *      y       :y-co-ordinates of the destination rectangle
 *      w	    :Specifies width of the rectangle
 *      h       :Height of the window of the rectangle
 *		
 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses:
 *   - Patterned desktops
 *   - x11perf: stippled rectangle tests (-srect500).
 *   - x11perf: opaque stippled rectangle tests (-osrect500).
*----------------------------------------------------------------------------
*/
void
GX1Subsequent8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				   int patternx, int patterny, int x, int y,
				   int w, int h)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   DEBUGMSG(1, (0, 0, "8x8color %d %d %dx%d\n", x, y, w, h));
   if (pGeode->TV_Overscan_On) {
      x += pGeode->TVOx;
      y += pGeode->TVOy;
   }
   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */
   /* Ignores specified pattern. */
   GFX(color_pattern_fill((unsigned short)x, (unsigned short)y,
			  (unsigned short)w, (unsigned short)h,
			  ((unsigned long *)((pGeode->FBBase +
					      (patterny << gu1_yshift)) +
					     patternx))));
}

/*----------------------------------------------------------------------------
 * GX1SetupFor8x8PatternMonoExpand
 *
 * Description	:This routine is called to fill the monochrome pattern of
 *                 8x8.
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      patternx:This is set from on rop data.
 *      patterny:This is set based on rop data.
 *       fg		:Specifies the foreground color
 *       bg     :Specifies the background color
 *	planemask	:Specifies the value of masking from rop data

 * Returns		:none.
 *
 * Comments     :none.
 *
*----------------------------------------------------------------------------
*/
void
GX1SetupFor8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo,
				int patternx, int patterny, int fg,
				int bg, int rop, unsigned int planemask)
{
   int trans = (bg == -1);

   /* LOAD PATTERN COLORS AND DATA */
   GFX(set_mono_pattern((unsigned int)bg, (unsigned int)fg,
			(unsigned int)patternx, (unsigned int)patterny,
			trans));

   GFX(set_mono_source((unsigned int)bg, (unsigned int)fg, trans));

   /* CHECK IF PLANEMASK IS NOT USED (ALL PLANES ENABLED) */
   if (planemask == 0xFFFFFFFF) {
      /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
      GFX(set_raster_operation(XAAPatternROP[rop]));
   } else {
      /* SELECT ROP THAT USES SOURCE DATA FOR PLANEMASK */
      GFX(set_solid_source((unsigned int)planemask));
      GFX(set_raster_operation(XAAPatternROP_PM[rop]));
   }
}

/*----------------------------------------------------------------------------
 * GX1Subsequent8x8PatternMonoExpand
 *
 * Description	:This routine is called to fill  a ractanglethe
 *                 monochrome pattern of previusly loaded pattern.
 *
 * Parameters.
 *	pScreenInfo	:Screen handler pointer having screen information.
 *  patternx	:This is set from on rop data.
 *  patterny	:This is set based on rop data.
 *       fg		:Specifies the foreground color
 *       bg		:Specifies the background color
 *  planemask	:Specifies the value of masking from rop data

 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses:
 *   - Patterned desktops
 *   - x11perf: stippled rectangle tests (-srect500).
 *   - x11perf: opaque stippled rectangle tests (-osrect500).
*----------------------------------------------------------------------------
*/
void
GX1Subsequent8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo,
				  int patternx, int patterny, int x, int y,
				  int w, int h)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      x += pGeode->TVOx;
      y += pGeode->TVOy;
   }

   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */
   /* Ignores specified pattern. */
   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)w, (unsigned short)h));
}

/*----------------------------------------------------------------------------
 * GX1SetupForScreenToScreenCopy
 *
 * Description	:This function is used to set up the planemask and raster
 *                 for future Bliting functionality.
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      xdir	:This is set based on rop data.
 *      ydir    :This is set based on rop data.
 *      rop		:sets the raster operation
 *	transparency:tobeadded
 *  planemask	:Specifies the value of masking from rop data

 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
*----------------------------------------------------------------------------
*/
void
GX1SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
			      int xdir, int ydir, int rop,
			      unsigned int planemask, int transparency_color)
{
   GFX(set_solid_pattern(planemask));
   /* SET RASTER OPERATION FOR USING PATTERN AS PLANE MASK */
   GFX(set_raster_operation(XAACopyROP[rop]));
   /* SAVE TRANSPARENCY FLAG */
   GeodeTransparent = (transparency_color == -1) ? 0 : 1;
   GeodeTransColor = transparency_color;

}

/*----------------------------------------------------------------------------
 * GX1SubsquentScreenToScreenCopy
 *
 * Description	:This function is called to perform a screen to screen
 *                 BLT  using the previously  specified planemask,raster
 *                 operation and  * transparency flag
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *  	x1	 	:x -coordinates of the source window
 *      y1      :y-co-ordinates of the source window
 *      x2		:x -coordinates of the destination window
 *      y2      :y-co-ordinates of the destination window
 *      w	    :Specifies width of the window to be copied
 *      h       :Height of the window to be copied.
 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/

void
GX1SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				int x1, int y1, int x2, int y2, int w, int h)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      if ((x1 < pScreenInfo->virtualX) && (y1 < pScreenInfo->virtualY)) {
	 x1 += pGeode->TVOx;
	 y1 += pGeode->TVOy;
      }
      x2 += pGeode->TVOx;
      y2 += pGeode->TVOy;
   }

   if (GeodeTransparent) {
      /* CALL ROUTINE FOR TRANSPARENT SCREEN TO SCREEN BLT
       * * Should only be called for the "copy" raster operation.
       */
      GFX(screen_to_screen_xblt((unsigned short)x1, (unsigned short)y1,
				(unsigned short)x2, (unsigned short)y2,
				(unsigned short)w, (unsigned short)h,
				GeodeTransColor));
   } else {
      /* CALL ROUTINE FOR NORMAL SCREEN TO SCREEN BLT */
      GFX(screen_to_screen_blt((unsigned short)x1, (unsigned short)y1,
			       (unsigned short)x2, (unsigned short)y2,
			       (unsigned short)w, (unsigned short)h));
   }
}

/*----------------------------------------------------------------------------
 * GX1SetupForScanlineImageWrite
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.
 *
 * Parameters:
 *		pScreenInfo			:Screen handler pointer having screen information.
 *		rop					:sets the raster operation
 *		transparency_color	:transparency color key.
 *		planemask			:Specifies the value of masking from rop data
 *      bpp					:bits per pixel of the source pixmap
 *		depth				:depth of the source pixmap.
 * Returns		:none
 *
 * Comments		:none
 *  x11perf -putimage10  
 *  x11perf -putimage100 
 *  x11perf -putimage500 
*----------------------------------------------------------------------------
*/

void
GX1SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
			      int rop, unsigned int planemask,
			      int transparency_color, int bpp, int depth)
{
   GFX(set_solid_pattern((unsigned int)planemask));
   /* SET RASTER OPERATION FOR USING PATTERN AS PLANE MASK */
   GFX(set_raster_operation(XAACopyROP_PM[rop]));
   /* SAVE TRANSPARENCY FLAG */
   GeodeTransparent = (transparency_color == -1) ? 0 : 1;
   GeodeTransColor = transparency_color;
   Geodebpp = bpp;
}

/*----------------------------------------------------------------------------
 * GX1SubsequentScanlineImageWriteRect
 *
 * Description	:This function is used to set up the x,y corordinates and width
 *               &height for future Bliting functionality.
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      x		:destination x
 *	    y		:destination y
 *		w		:Specifies the width of the rectangle to be  copied
 *		h		:Specifies the height of the rectangle to be  copied
 *	
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
GX1SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
				    int x, int y, int w, int h, int skipleft)
{

   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
   GeodeCounter = 0;
}

/*----------------------------------------------------------------------------
 * GX1SubsquentImageWriteScanline
 *
 * Description	:This function is called to 
 *               BLT  using the previously  specified planemask,raster
 *               operation and   transparency flag
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *
 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/

void
GX1SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);
   int blt_height = 0;
   char blit = FALSE;

   GeodeCounter++;

   if ((Geodeheight <= pGeode->NoOfImgBuffers) &&
       (GeodeCounter == Geodeheight)) {
      blit = TRUE;
      blt_height = Geodeheight;
   } else if ((Geodeheight > pGeode->NoOfImgBuffers)
	      && (GeodeCounter == pGeode->NoOfImgBuffers)) {
      blit = TRUE;
      Geodeheight -= pGeode->NoOfImgBuffers;
      blt_height = pGeode->NoOfImgBuffers;
   } else
      return;

   if (blit) {
      blit = FALSE;

      GeodeCounter = 0;

      if (GeodeTransparent) {
	 /* CALL ROUTINE FOR TRANSPARENT SCREEN TO SCREEN BLT
	  * * Should only be called for the "copy" raster operation.
	  */
	 GFX(screen_to_screen_xblt((unsigned short)Geodesrcx,
				   (unsigned short)Geodesrcy,
				   (unsigned short)Geodedstx,
				   (unsigned short)Geodedsty,
				   (unsigned short)Geodewidth,
				   (unsigned short)blt_height,
				   GeodeTransColor));
      } else {
	 /* CALL ROUTINE FOR NORMAL SCREEN TO SCREEN BLT */
	 GFX(screen_to_screen_blt((unsigned short)Geodesrcx,
				  (unsigned short)Geodesrcy,
				  (unsigned short)Geodedstx,
				  (unsigned short)Geodedsty,
				  (unsigned short)Geodewidth,
				  (unsigned short)blt_height));
      }
      Geodedsty += blt_height;
      GFX(wait_until_idle());
   }
}

static unsigned short vector_mode_table[] = {
   VM_MAJOR_INC | VM_MINOR_INC | VM_X_MAJOR,
   VM_MAJOR_INC | VM_MINOR_INC | VM_Y_MAJOR,
   VM_MAJOR_INC | VM_X_MAJOR,
   VM_MINOR_INC | VM_Y_MAJOR,
   VM_MINOR_INC | VM_X_MAJOR,
   VM_MAJOR_INC | VM_Y_MAJOR,
   VM_X_MAJOR,
   VM_Y_MAJOR,
};

/*----------------------------------------------------------------------------
 * GX1SetupForSolidLine
 *
 * Description	:This function is used setup the solid line color for
 *               future line draws.
 *
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      color	:Specifies the color value od line
 *      rop     :Specifies rop values.
 *  Planemask	:Specifies planemask value.
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
GX1SetupForSolidLine(ScrnInfoPtr pScreenInfo,
		     int color, int rop, unsigned int planemask)
{
   /* LOAD THE SOLID PATTERN COLOR */
   GFX(set_solid_pattern((unsigned int)color));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(XAAPatternROP[rop]));
}

/*---------------------------------------------------------------------------
 * GX1SubsequentBresenhamLine
 *
 * Description	:This function is used to render a vector using the
 *                 specified bresenham parameters.
 *
 * Parameters:
 * pScreenInfo 	:Screen handler pointer having screen information.
 *      x1  	:Specifies the starting x position
 *      y1      :Specifies starting y possition
 *      absmaj	:Specfies the Bresenman absolute major.
 *	  absmin	:Specfies the Bresenman absolute minor.
 *	  err       :Specifies the bresenham err term.
 *	  len       :Specifies the length of the vector interms of pixels.
 *	  octant    :not used in this function,may be added for standard
 *                    interface.
 * Returns		:none
 *
 * Comments     :none
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-line500).
 *   - x11perf: line segments (-seg500).
*----------------------------------------------------------------------------
*/
void
GX1SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo,
			   int x1, int y1, int absmaj, int absmin, int err,
			   int len, int octant)
{
   int axial, init, diag;

   DEBUGMSG(0, (0, 0, "BLine %d, %d, %d, %d, %d, %d, %d\n",
		x1, y1, absmaj, absmin, err, len, octant));

   /* DETERMINE BRESENHAM PARAMETERS */

   axial = ((int)absmin << 1);
   init = axial - (int)absmaj;
   diag = init - (int)absmaj;

   /* ADJUST INITIAL ERROR
    * * Adjust by -1 for certain directions so that the vector 
    * * hits the same pixels when drawn in either direction.
    * * The Gamma value is assumed to account for the initial 
    * * error adjustment for clipped lines.
    */

   init += err;

   /* CALL ROUTINE TO DRAW VECTOR */

   GFX(bresenham_line((unsigned short)x1,
		      (unsigned short)y1,
		      (unsigned short)len,
		      (unsigned short)init,
		      (unsigned short)axial,
		      (unsigned short)diag,
		      (unsigned short)vector_mode_table[octant]));

}

#define ABS(_val1, _val2) (((_val1) > (_val2)) ? ((_val1)-(_val2)) : ((_val2) - (_val1)))

void
GX1SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
			       int x0, int y0, int x1, int y1, int flags)
{
   long dx, dy, dmaj, dmin;
   long axialerr, diagerr, initerr;
   unsigned short vec_flags = 0;

   dx = ABS(x1, x0);
   dy = ABS(y1, y0);
   if (dx >= dy) {
      dmaj = dx;
      dmin = dy;
      vec_flags = VM_X_MAJOR;
      if (x1 > x0)
	 vec_flags |= VM_MAJOR_INC;
      if (y1 > y0)
	 vec_flags |= VM_MINOR_INC;
   } else {
      dmaj = dy;
      dmin = dx;
      vec_flags = VM_Y_MAJOR;
      if (x1 > x0)
	 vec_flags |= VM_MINOR_INC;
      if (y1 > y0)
	 vec_flags |= VM_MAJOR_INC;
   }
   axialerr = dmin << 1;
   diagerr = (dmin - dmaj) << 1;
   initerr = (dmin << 1) - dmaj;
   if (!(vec_flags & VM_MINOR_INC))
      initerr--;

   GFX(bresenham_line((unsigned short)x0,
		      (unsigned short)y0,
		      (unsigned short)dmaj,
		      (unsigned short)initerr,
		      (unsigned short)axialerr,
		      (unsigned short)diagerr, vec_flags));
}

/*---------------------------------------------------------------------------
 * GX1SubsequentHorVertLine
 *
 * This routine is called to render a vector using the specified Bresenham
 * parameters.  
 *
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-hseg500).
 *   - x11perf: line segments (-vseg500).
 *---------------------------------------------------------------------------
 */
void
GX1SubsequentHorVertLine(ScrnInfoPtr pScreenInfo,
			 int x, int y, int len, int dir)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      x += pGeode->TVOx;
      y += pGeode->TVOy;
   }
   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)((dir == DEGREES_0) ? len : 1),
		    (unsigned short)((dir == DEGREES_0) ? 1 : len)));
}

void
GX1SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		      unsigned int planemask, int length,
		      unsigned char *pattern)
{
   int trans = (bg == -1);
   unsigned long *pat = (unsigned long *)pattern;

   /* LOAD PATTERN COLORS AND DATA */

   GFX(set_mono_pattern((unsigned long)bg, (unsigned long)fg,
			(unsigned long)pat, (unsigned long)pat,
			(unsigned char)trans));

   /* CHECK IF PLANEMASK IS NOT USED (ALL PLANES ENABLED) */

   if (planemask == (unsigned int)-1) {
      /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */

      GFX(set_raster_operation(XAAPatternROP[rop]));
   } else {
      /* SELECT ROP THAT USES SOURCE DATA FOR PLANEMASK */

      GFX(set_solid_source((unsigned long)planemask));
      GFX(set_raster_operation(XAAPatternROP_PM[rop]));
   }
}

#if SCR2SCREXP
void
GX1SetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
					 int fg, int bg, int rop,
					 unsigned int planemask)
{
   GFX(set_solid_pattern(planemask));
   GFX(set_mono_source(bg, fg, (bg == -1)));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(XAACopyROP_PM[rop & 0x0F]));

   DEBUGMSG(0, (0, X_NONE, "%x %x %x %x\n", fg, bg, rop, planemask));
}

void
GX1SubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
					   int x, int y, int w, int h,
					   int srcx, int srcy, int offset)
{
   GeodePtr pGeode = GEODEPTR(pScrn);

   GFX(mono_bitmap_to_screen_blt(offset, 0, x, y, w, h,
				 (unsigned char *)(pGeode->FBBase +
						   CALC_FBOFFSET(srcx, srcy)),
				 pGeode->Pitch));
}
#endif

#if !defined(STB_X)
/*----------------------------------------------------------------------------
 * OPTGX1SetupForFillRectSolid.
 *
 * Description	:This routine is called to setup the solid pattern
 *               color for   future  rectangular fills or vectors.
 *				 (non durango version)
 *
 * Parameters.
 * pScreenInfo
 *		Ptr		:Screen handler pointer having screen information.
 *    color     :Specifies the color to be filled up in defined area.
 *    rop       :Specifies the raster operation value.
 *   planemask	:Specifies the masking value based rop srcdata.
 *
 * Returns		:none
 *
 * Comments		:none
 *
*----------------------------------------------------------------------------
*/
void
OPTGX1SetupForFillRectSolid(ScrnInfoPtr pScreenInfo,
			    int color, int rop, unsigned int planemask)
{
   unsigned short rop16;

   if (gu1_bpp == 8) {
      color &= 0x00FF;
      color |= (color << 8);
   }

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */

   if (planemask == 0xFFFFFFFF) {
      if (gu1_bpp == 8) {
	 planemask &= 0x00FF;
	 planemask |= (planemask << 8);
      }

      rop16 = XAAPatternROP[rop];

      /* POLL UNTIL ABLE TO WRITE THE SOURCE COLOR */

      GFX_WAIT_PENDING;
      WRITE_REG32(GP_SRC_COLOR_0, (planemask << 16) | planemask);
   } else {
      rop16 = XAAPatternROP_PM[rop];
   }

   Geode_blt_mode = 0;

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */
   /* Only one operation can be pending at a time. */

   GFX_WAIT_PENDING;
   WRITE_REG16(GP_PAT_COLOR_0, (unsigned short)color);
   WRITE_REG16(GP_RASTER_MODE, rop16);
}

 /*----------------------------------------------------------------------------
 * OPTGX1SubsequentFillRectSolid.
 *
 * Description	:This routine is used to fill the rectangle of previously
 *               specified  solid pattern.
 *				 (non durango version)
 *
 * Parameters.
 *  pScreenInfo :Screen handler pointer having screen information.
 *     x and y	:Specifies the x and y co-ordinatesarea.
 *      w and h	:Specifies width and height respectively.
 *
 * Returns		:none
 *
 * Comments		:desired pattern can be set before this function by
 *               gfx_set_solid_pattern.
 * Sample application uses:
 *   - Window backgrounds. 
 *   - x11perf: rectangle tests (-rect500).
 *   - x11perf: fill trapezoid tests (-trap100).
 *   - x11perf: horizontal line segments (-hseg500).
 *----------------------------------------------------------------------------
*/
void
OPTGX1SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo,
			      int x, int y, int width, int height)
{
   unsigned short section;
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      x += pGeode->TVOx;
      y += pGeode->TVOy;
   }

   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Only one operation can be pending at a time. */

   GFX_WAIT_PENDING;

   /* SET REGISTERS TO DRAW RECTANGLE */
   WRITE_REG32(GP_DST_XCOOR, (y << 16) | x);

   WRITE_REG16(GP_HEIGHT, height);

   /* CHECK WIDTH FOR GX BUG WORKAROUND */

   if (width <= 16) {
      /* OK TO DRAW SMALL RECTANGLE IN ONE PASS */

      WRITE_REG16(GP_WIDTH, width);
      WRITE_REG16(GP_BLIT_MODE, Geode_blt_mode);
   } else {
      /* DRAW FIRST PART OF RECTANGLE */
      /* Get to a 16 pixel boundary. */

      section = 0x10 - (x & 0x0F);
      WRITE_REG16(GP_WIDTH, section);
      WRITE_REG16(GP_BLIT_MODE, Geode_blt_mode);

      /* POLL UNTIL ABLE TO LOAD THE SECOND RECTANGLE */

      GFX_WAIT_PENDING;

      WRITE_REG32(GP_DST_XCOOR, (y << 16) | (x + section));
      WRITE_REG16(GP_WIDTH, width - section);
      WRITE_REG16(GP_BLIT_MODE, Geode_blt_mode);
   }
}

/*----------------------------------------------------------------------------
 * OPTGX1SetupForScreenToScreenCopy
 *
 * Description	:This function is used to set up the planemask and raster
 *                 for future Bliting functionality.
 *				 (non durango version)
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      xdir	:This is set based on rop data.
 *      ydir    :This is set based on rop data.
 *      rop		:sets the raster operation
 *	transparency:tobeadded
 *  planemask	:Specifies the value of masking from rop data

 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
*----------------------------------------------------------------------------
*/
void
OPTGX1SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				 int xdir, int ydir, int rop,
				 unsigned int planemask,
				 int transparency_color)
{
   int GFXusesDstData;
   unsigned short rop16 = XAACopyROP[rop];

   /* FORMAT 8 BPP COLOR */
   /* GX requires 8BPP color data be duplicated into bits [15:8]. */

   if (gu1_bpp == 8) {
      planemask &= 0x00FF;
      planemask |= (planemask << 8);
   }

   /* SET FLAG INDICATING ROP REQUIRES DESTINATION DATA */
   /* True if even bits (0:2:4:6) do not equal the correspinding */
   /* even bits (1:3:5:7). */

   GFXusesDstData = ((rop & 0x55) ^ ((rop >> 1) & 0x55));

   Geode_blt_mode = GFXusesDstData ? BM_READ_DST_FB1 | BM_READ_SRC_FB :
	 BM_READ_SRC_FB;

   /* CHECK AVAILABLE BLT BUFFER SIZE */
   /* Can use both BLT buffers if no destination data is required. */

   Geode_buffer_width = GFXusesDstData ? GeodebufferWidthPixels :
	 GeodebufferWidthPixels << 1;

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */
   /* Only one operation can be pending at a time. */

   GFX_WAIT_PENDING;

   WRITE_REG16(GP_PAT_COLOR_0, (unsigned short)planemask);
   WRITE_REG16(GP_RASTER_MODE, rop16);

   GeodeTransparent = (transparency_color == -1) ? 0 : 1;
   GeodeTransColor = transparency_color;
}

/*----------------------------------------------------------------------------
 * OPTGX1SubsquentScreenToScreenCopy
 *
 * Description	:This function is called to perform a screen to screen
 *                 BLT  using the previously  specified planemask,raster
 *                 operation and  * transparency flag
 *				 (non durango version)
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *  	srcx	:x -coordinates of the source window
 *      srcy    :y-co-ordinates of the source window
 *      dstx	:x -coordinates of the destination window
 *      dsty    :y-co-ordinates of the destination window
 *      width	:Specifies width of the window to be copied
 *      height  :Height of the window to be copied.
 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/
void
OPTGX1SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				   int srcx, int srcy, int dstx, int dsty,
				   int width, int height)
{
   unsigned short section;
   unsigned short blit_mode = 0;
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->TV_Overscan_On) {
      if ((srcx < pScreenInfo->virtualX) && (srcy < pScreenInfo->virtualY)) {
	 srcx += pGeode->TVOx;
	 srcy += pGeode->TVOy;
      }
      dstx += pGeode->TVOx;
      dsty += pGeode->TVOy;
   }
   if (GeodeTransparent) {
      if (gu1_bpp == 8) {
	 GeodeTransColor &= 0x00FF;
	 GeodeTransColor |= (GeodeTransColor << 8);
      }
      GeodeTransColor =
	    (GeodeTransColor & 0x0000FFFF) | (GeodeTransColor << 16);

      /* WAIT UNTIL PIPELINE IS NOT BUSY BEFORE LOADING DATA INTO BB1 */
      /* Need to make sure any previous BLT using BB1 is complete. */
      /* Only need to load 32 bits of BB1 for the 1 pixel BLT that follows. */

      GFX_WAIT_BUSY;
      WRITE_SCRATCH32(Geodebb1Base, GeodeTransColor);

      /* DO BOGUS BLT TO LATCH DATA FROM BB1 */
      /* Already know graphics pipeline is idle. */
      /* Only need to latch data into the holding registers for the current */
      /* data from BB1.  A 1 pixel wide BLT will suffice. */

      WRITE_REG32(GP_DST_XCOOR, 0);
      WRITE_REG32(GP_SRC_XCOOR, 0);
      WRITE_REG32(GP_WIDTH, 0x00010001);
      WRITE_REG16(GP_RASTER_MODE, 0x00CC);
      WRITE_REG16(GP_BLIT_MODE, BM_READ_SRC_FB | BM_READ_DST_BB1);

      /* WRITE REGISTERS FOR REAL SCREEN TO SCREEN BLT */

      GFX_WAIT_PENDING;
      WRITE_REG16(GP_HEIGHT, height);
      WRITE_REG16(GP_RASTER_MODE, 0x10C6);
      WRITE_REG32(GP_PAT_COLOR_0, 0xFFFFFFFF);

   }

   /* CHECK Y DIRECTION */
   /* Hardware has support for negative Y direction. */

   if (dsty > srcy) {
      blit_mode = BM_REVERSE_Y;
      srcy += height - 1;
      dsty += height - 1;
   }

   /* CHECK X DIRECTION */
   /* Hardware does not support negative X direction since at the time */
   /* of development all supported resolutions could fit a scanline of */
   /* data at once into the BLT buffers (using both BB0 and BB1).  This */
   /* code is more generic to allow for any size BLT buffer. */

   if (dstx > srcx) {
      srcx += width;
      dstx += width;
   }

   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Write the registers that do not change for each section. */

   GFX_WAIT_PENDING;
   WRITE_REG16(GP_HEIGHT, height);

   /* REPEAT UNTIL FINISHED WITH RECTANGLE */
   /* Perform BLT in vertical sections, as wide as the BLT buffer allows. */
   /* Hardware does not split the operations, so software must do it to */
   /* avoid large scanlines that would overflow the BLT buffers. */

   while (width > 0) {
      /* CHECK WIDTH OF CURRENT SECTION */

      if (width > Geode_buffer_width)
	 section = Geode_buffer_width;
      else
	 section = width;

      /* PROGRAM REGISTERS THAT ARE THE SAME FOR EITHER X DIRECTION */

      GFX_WAIT_PENDING;
      WRITE_REG16(GP_SRC_YCOOR, srcy);
      WRITE_REG16(GP_DST_YCOOR, dsty);
      WRITE_REG16(GP_WIDTH, section);

      /* CHECK X DIRECTION */

      if (dstx > srcx) {
	 /* NEGATIVE X DIRECTION */
	 /* Still positive X direction within the section. */

	 srcx -= section;
	 dstx -= section;
	 WRITE_REG16(GP_SRC_XCOOR, srcx);
	 WRITE_REG16(GP_DST_XCOOR, dstx);
      } else {
	 /* POSITIVE X DIRECTION */

	 WRITE_REG16(GP_SRC_XCOOR, srcx);
	 WRITE_REG16(GP_DST_XCOOR, dstx);
	 dstx += section;
	 srcx += section;
      }
      WRITE_REG16(GP_BLIT_MODE, Geode_blt_mode | blit_mode);
      width -= section;
   }
}

/*----------------------------------------------------------------------------
 * OPTGX1SetupForScanlineImageWrite
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.(non durango version)
 *
 * Parameters:
 *		pScreenInfo			:Screen handler pointer having screen information.
 *      rop					:sets the raster operation
 *		transparency_color	:tobeadded
 *		planemask			:Specifies the value of masking from rop data
 *      bpp					:bits per pixel of the source pixmap
 *		depth				:depth of the source pixmap.
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
OPTGX1SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				 int rop, unsigned int planemask,
				 int transparency_color, int bpp, int depth)
{
   Geodebpp = bpp;
   OPTGX1SetupForScreenToScreenCopy(pScreenInfo,
				    0, 0, rop, planemask, transparency_color);
}

/*----------------------------------------------------------------------------
 * OPTGX1SubsequentScanlineImageWriteRect
 *
 * Description	:This function is used to set up the x,y corordinates and width
 *                &height for future Bliting functionality.(non durango version)
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      x		:destination x
 *	    y		:destination y
 *		w		:Specifies the width of the rectangle to be  copied
 *		h		:Specifies the height of the rectangle to be  copied
 * Returns		:none
 *
 * Comments		:none
 *----------------------------------------------------------------------------
*/
void
OPTGX1SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
				       int x, int y, int w, int h,
				       int skipleft)
{

   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
   GeodeCounter = 0;
}

/*----------------------------------------------------------------------------
 * OPTGX1SubsquentImageWriteScanline
 *
 * Description	:This function is called to 
 *               BLT  using the previously  specified planemask,raster
 *               operation and transparency flag(non durango version)
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *
 * Returns		:none
 *
 * Comments		:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/

void
OPTGX1SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);
   int blt_height = 0;
   char blit = FALSE;

   GeodeCounter++;

   if ((Geodeheight <= pGeode->NoOfImgBuffers) &&
       (GeodeCounter == Geodeheight)) {
      blit = TRUE;
      blt_height = Geodeheight;
   } else if ((Geodeheight > pGeode->NoOfImgBuffers)
	      && (GeodeCounter == pGeode->NoOfImgBuffers)) {
      blit = TRUE;
      Geodeheight -= pGeode->NoOfImgBuffers;
      blt_height = pGeode->NoOfImgBuffers;
   } else
      return;

   if (blit) {
      blit = FALSE;
      GeodeCounter = 0;
      OPTGX1SubsequentScreenToScreenCopy(pScreenInfo,
					 Geodesrcx, Geodesrcy, Geodedstx,
					 Geodedsty, Geodewidth, blt_height);
      Geodedsty += blt_height;
      GFX_WAIT_BUSY;
   }
}

/*----------------------------------------------------------------------------
 * OPTGX1SetupForSolidLine
 *
 * Description	:This function is used setup the solid line color for
 *               future line draws.
 *
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      color	:Specifies the color value od line
 *      rop     :Specifies rop values.
 *  Planemask	:Specifies planemask value.
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
OPTGX1SetupForSolidLine(ScrnInfoPtr pScreenInfo,
			int color, int rop, unsigned int planemask)
{
   if (gu1_bpp == 8) {
      color &= 0x00FF;
      color |= (color << 8);
   }

   GeodeROP = XAAPatternROP[rop];

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */
   GFX_WAIT_PENDING;
   WRITE_REG16(GP_PAT_COLOR_0, (unsigned short)color);
   WRITE_REG16(GP_RASTER_MODE, GeodeROP);

   if ((GeodeROP & 0x55) ^ ((GeodeROP >> 1) & 0x55)) {
      Geode_vector_mode = VM_READ_DST_FB;
      Geode_blt_mode = BM_READ_DST_FB1 | BM_READ_SRC_FB;
   } else {
      Geode_vector_mode = 0;
      Geode_blt_mode = BM_READ_SRC_FB;
   }
}

/*---------------------------------------------------------------------------
 * OPTGX1SubsequentBresenhamLine
 *
 * Description	:This function is used to render a vector using the
 *                 specified bresenham parameters.
 *
 * Parameters:
 * pScreenInfo 	:Screen handler pointer having screen information.
 *      x1  	:Specifies the starting x position
 *      y1      :Specifies starting y possition
 *      absmaj	:Specfies the Bresenman absolute major.
 *	  absmin	:Specfies the Bresenman absolute minor.
 *	  err       :Specifies the bresenham err term.
 *	  len       :Specifies the length of the vector interms of pixels.
 *	  octant    :not used in this function,may be added for standard
 *                    interface.
 * Returns		:none
 *
 * Comments     :none
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-seg500).
*----------------------------------------------------------------------------
*/
void
OPTGX1SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo,
			      int x1, int y1, int absmaj, int absmin, int err,
			      int len, int octant)
{
   int axial, init, diag;

   DEBUGMSG(0, (0, 0, "BLine %d, %d, %d, %d, %d, %d, %d\n",
		x1, y1, absmaj, absmin, err, len, octant));

   /* DETERMINE BRESENHAM PARAMETERS */

   axial = ((int)absmin << 1);
   init = axial - (int)absmaj;
   diag = init - (int)absmaj;

   /* ADJUST INITIAL ERROR
    * * Adjust by -1 for certain directions so that the vector 
    * * hits the same pixels when drawn in either direction.
    * * The Gamma value is assumed to account for the initial 
    * * error adjustment for clipped lines.
    */

   init += err;

   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Put off poll for as long as possible (do most calculations first). */

   GFX_WAIT_PENDING;
   WRITE_REG32(GP_DST_XCOOR, (y1 << 16) | x1);
   WRITE_REG32(GP_VECTOR_LENGTH, (((unsigned long)init) << 16) |
	       ((unsigned short)len));
   WRITE_REG32(GP_AXIAL_ERROR, (((unsigned long)diag) << 16) |
	       ((unsigned short)axial));
   WRITE_REG16(GP_VECTOR_MODE,
	       (Geode_vector_mode | vector_mode_table[octant]));
}

void
OPTGX1SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
				  int x0, int y0, int x1, int y1, int flags)
{
   long dx, dy, dmaj, dmin;
   long axialerr, diagerr, initerr;
   unsigned short vec_flags = 0;

   dx = ABS(x1, x0);
   dy = ABS(y1, y0);
   if (dx >= dy) {
      dmaj = dx;
      dmin = dy;
      vec_flags = VM_X_MAJOR;
      if (x1 > x0)
	 vec_flags |= VM_MAJOR_INC;
      if (y1 > y0)
	 vec_flags |= VM_MINOR_INC;
   } else {
      dmaj = dy;
      dmin = dx;
      vec_flags = VM_Y_MAJOR;
      if (x1 > x0)
	 vec_flags |= VM_MINOR_INC;
      if (y1 > y0)
	 vec_flags |= VM_MAJOR_INC;
   }

   axialerr = dmin << 1;
   diagerr = (dmin - dmaj) << 1;
   initerr = (axialerr - dmaj);

   if (!(vec_flags & VM_MINOR_INC))
      initerr--;

   GFX_WAIT_PENDING;
   WRITE_REG32(GP_DST_XCOOR, (y0 << 16) | x0);
   WRITE_REG32(GP_VECTOR_LENGTH, (((unsigned long)initerr) << 16) |
	       ((unsigned short)dmaj));
   WRITE_REG32(GP_AXIAL_ERROR, (((unsigned long)diagerr) << 16) |
	       ((unsigned short)axialerr));
   WRITE_REG16(GP_VECTOR_MODE, (Geode_vector_mode | vec_flags));
}

/*---------------------------------------------------------------------------
 * OPTGX1SubsequentHorVertLine
 *
 * This routine is called to render a vector using the specified Bresenham
 * parameters.  
 *
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-hseg500).
 *   - x11perf: line segments (-vseg500).
 *---------------------------------------------------------------------------
 */
void
OPTGX1SubsequentHorVertLine(ScrnInfoPtr pScreenInfo,
			    int x, int y, int len, int dir)
{

   DEBUGMSG(0, (0, 0, "HLine %d, %d, %d, %d\n", x, y, len, dir));

   OPTGX1SubsequentFillRectSolid(pScreenInfo,
				 (unsigned short)x, (unsigned short)y,
				 (unsigned short)((dir == DEGREES_0) ? len :
						  1),
				 (unsigned short)((dir == DEGREES_0) ? 1 :
						  len));
}
#endif

/*----------------------------------------------------------------------------
 * GX1AccelInit.
 *
 * Description	:This function sets up the supported acceleration routines and
 *                 appropriate flags.
 *
 * Parameters:
 *      pScreen	:Screeen pointer structure.
 *
 * Returns		:TRUE on success and FALSE on Failure
 *
 * Comments		:This function is called in GX1ScreenInit in
                     geode_driver.c to set  * the acceleration.
*----------------------------------------------------------------------------
*/
Bool
GX1AccelInit(ScreenPtr pScreen)
{
   GeodePtr pGeode;
   ScrnInfoPtr pScreenInfo;

   pScreenInfo = xf86Screens[pScreen->myNum];
   pGeode = GEODEPTR(pScreenInfo);

   switch (pScreenInfo->bitsPerPixel) {
   case 8:
      gu1_bpp = 8;
      break;
   case 16:
      gu1_bpp = 16;
      break;
   }

   gu1_xshift = pScreenInfo->bitsPerPixel >> 4;

   switch (pGeode->Pitch) {
   case 1024:
      gu1_yshift = 10;
      break;
   case 2048:
      gu1_yshift = 11;
      break;
   case 4096:
      gu1_yshift = 12;
      break;
   }

   Geodebb0Base = BB0_BASE_3K;
   Geodebb1Base = BB1_BASE_3K;
   GeodebufferWidthPixels = Geodebb1Base - Geodebb0Base - 16;

   if (gu1_bpp > 8) {
      /* If 16bpp, divide GFXbufferWidthPixels by 2 */
      GeodebufferWidthPixels >>= 1;
   }
   /* Getting the pointer for acceleration Inforecord */
   pGeode->AccelInfoRec = localRecPtr = XAACreateInfoRec();

   /* SET ACCELERATION FLAGS */
   localRecPtr->Flags = PIXMAP_CACHE | OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER;
   localRecPtr->PixmapCacheFlags = DO_NOT_BLIT_STIPPLES;

   /* HOOK SYNCRONIZARION ROUTINE */
   localRecPtr->Sync = GX1AccelSync;

   /* HOOK FILLED RECTANGLES */
   localRecPtr->SetupForSolidFill = (GX1SetupForFillRectSolid);
   localRecPtr->SubsequentSolidFillRect = (GX1SubsequentFillRectSolid);
   localRecPtr->SolidFillFlags = 0;

   /* HOOK 8x8 MonoEXPAND PATTERNS */
   localRecPtr->SetupForMono8x8PatternFill = GX1SetupFor8x8PatternMonoExpand;
   localRecPtr->SubsequentMono8x8PatternFillRect =
	 GX1Subsequent8x8PatternMonoExpand;
   localRecPtr->Mono8x8PatternFillFlags = BIT_ORDER_IN_BYTE_MSBFIRST |
	 HARDWARE_PATTERN_PROGRAMMED_BITS | HARDWARE_PATTERN_SCREEN_ORIGIN;

   localRecPtr->SetupForColor8x8PatternFill =
	 GX1SetupFor8x8PatternColorExpand;
   localRecPtr->SubsequentColor8x8PatternFillRect =
	 GX1Subsequent8x8PatternColorExpand;
   /* Color expansion */
   localRecPtr->Color8x8PatternFillFlags =
	 BIT_ORDER_IN_BYTE_MSBFIRST |
	 SCANLINE_PAD_DWORD | HARDWARE_PATTERN_SCREEN_ORIGIN;

   /* HOOK SCREEN TO SCREEN COPIES
    * * Set flag to only allow copy if transparency is enabled.
    */
   localRecPtr->SetupForScreenToScreenCopy =
	 OPTACCEL(GX1SetupForScreenToScreenCopy);
   localRecPtr->SubsequentScreenToScreenCopy =
	 OPTACCEL(GX1SubsequentScreenToScreenCopy);
   localRecPtr->ScreenToScreenCopyFlags = 0;

   /* HOOK BRESENHAM SOLID LINES */
   /* Do not hook unless flag can be set preventing use of planemask. */
   localRecPtr->SolidLineFlags = NO_PLANEMASK;
   localRecPtr->SetupForSolidLine = OPTACCEL(GX1SetupForSolidLine);
   localRecPtr->SubsequentSolidBresenhamLine =
	 OPTACCEL(GX1SubsequentBresenhamLine);
   localRecPtr->SubsequentSolidHorVertLine =
	 OPTACCEL(GX1SubsequentHorVertLine);
   localRecPtr->SubsequentSolidTwoPointLine =
	 OPTACCEL(GX1SubsequentSolidTwoPointLine);
   localRecPtr->SolidBresenhamLineErrorTermBits = 15;

#if SCR2SCREXP
   /* Color expansion */
   localRecPtr->ScreenToScreenColorExpandFillFlags =
	 BIT_ORDER_IN_BYTE_MSBFIRST | NO_TRANSPARENCY;

   localRecPtr->SetupForScreenToScreenColorExpandFill =
	 (GX1SetupForScreenToScreenColorExpandFill);
   localRecPtr->SubsequentScreenToScreenColorExpandFill =
	 (GX1SubsequentScreenToScreenColorExpandFill);
#endif

   /*
    * ImageWrite.
    *
    * SInce this uses off-screen scanline buffers, it is only of use when
    * complex ROPs are used. But since the current XAA pixmap cache code
    * only works when an ImageWrite is provided, the NO_GXCOPY flag is
    * temporarily disabled.
    */
   if (pGeode->AccelImageWriteBufferOffsets) {

      localRecPtr->ScanlineImageWriteFlags =
	    localRecPtr->ScreenToScreenCopyFlags;
      localRecPtr->ScanlineImageWriteBuffers =
	    pGeode->AccelImageWriteBufferOffsets;
      localRecPtr->NumScanlineImageWriteBuffers = pGeode->NoOfImgBuffers;
      localRecPtr->ImageWriteRange = pGeode->NoOfImgBuffers << gu1_yshift;
      localRecPtr->SetupForScanlineImageWrite =
	    OPTACCEL(GX1SetupForScanlineImageWrite);
      localRecPtr->SubsequentScanlineImageWriteRect =
	    OPTACCEL(GX1SubsequentScanlineImageWriteRect);
      localRecPtr->SubsequentImageWriteScanline =
	    OPTACCEL(GX1SubsequentImageWriteScanline);

      ImgBufOffset = pGeode->AccelImageWriteBufferOffsets[0] - pGeode->FBBase;
      Geodesrcy = ImgBufOffset >> gu1_yshift;

      Geodesrcx = ImgBufOffset & (pGeode->Pitch - 1);
      Geodesrcx /= (pScreenInfo->bitsPerPixel >> 3);
   }

   return (XAAInit(pScreen, localRecPtr));
}

/* END OF FILE */
