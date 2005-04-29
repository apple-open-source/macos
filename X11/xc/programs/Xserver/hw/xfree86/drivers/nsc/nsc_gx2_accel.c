/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx2_accel.c,v 1.5 2003/11/03 05:11:20 tsi Exp $ */
/*
 * $Workfile: nsc_gx2_accel.c $
 * $Revision: 1.1.1.2 $
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

#define DASHED_SUPPORT 0
#define IMGWRITE_SUPPORT 0
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
static int GeodeCounter;

#if !defined(STB_X)
static unsigned int GeodeROP = 0;
static unsigned short Geode_blt_mode = 0;
static unsigned short Geode_vector_mode = 0;
#endif
static unsigned int gu2_xshift = 1;
static unsigned int gu2_yshift = 1;
static unsigned int gu2_bpp = 1;
static unsigned int SetCPUToScreen = 0;
#if IMGWRITE_SUPPORT
static unsigned int SetImageWriteRect = 0;
#endif
static unsigned int ImgBufOffset;

#define GU2_WAIT_PENDING while(READ_GP32(MGP_BLT_STATUS) & MGP_BS_BLT_PENDING)
#define GU2_WAIT_BUSY while(READ_GP32(MGP_BLT_STATUS) & MGP_BS_BLT_BUSY)

#define CALC_FBOFFSET(_SrcX, _SrcY) \
	(((unsigned int) (_SrcY) << gu2_yshift) |\
		(((unsigned int) (_SrcX)) << gu2_xshift))

#define GFX_PATTERN_FILL(_SrcX, _SrcY, _Width, _Height) \
{ \
	GU2_WAIT_PENDING;\
	WRITE_GP32(MGP_DST_OFFSET, CALC_FBOFFSET((_SrcX), (_SrcY)));\
	WRITE_GP32(MGP_WID_HEIGHT, \
		(((unsigned int) (_Width)) << 16) | (_Height));\
	WRITE_GP32(MGP_BLT_MODE, Geode_blt_mode);\
}

static XAAInfoRecPtr localRecPtr;

Bool GX2AccelInit(ScreenPtr pScreen);
void GX2AccelSync(ScrnInfoPtr pScreenInfo);
void GX2SetupForFillRectSolid(ScrnInfoPtr pScreenInfo, int color, int rop,
			      unsigned int planemask);
void GX2SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y,
				int w, int h);
void GX2SetupFor8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				      int patternx, int patterny,
				      int rop, unsigned int planemask,
				      int trans_color);
void GX2Subsequent8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
					int patternx, int patterny, int x,
					int y, int w, int h);
void GX2SetupFor8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo, int patternx,
				     int patterny, int fg, int bg, int rop,
				     unsigned int planemask);
void GX2Subsequent8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo, int patternx,
				       int patterny, int x, int y, int w,
				       int h);
void GX2SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int xdir,
				   int ydir, int rop, unsigned int planemask,
				   int transparency_color);
void GX2SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int x1, int y1,
				     int x2, int y2, int w, int h);
void GX2SetupForSolidLine(ScrnInfoPtr pScreenInfo, int color, int rop,
			  unsigned int planemask);
void GX2SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
			   unsigned int planemask, int length,
			   unsigned char *pattern);
void GX2SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo, int x1, int y1,
				int absmaj, int absmin, int err, int len,
				int octant);
void GX2SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo, int x0, int y0,
				    int x1, int y1, int flags);
void GX2SubsequentHorVertLine(ScrnInfoPtr pScreenInfo, int x, int y, int len,
			      int dir);

void GX2SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				   int rop, unsigned int planemask,
				   int transparency_color, int bpp,
				   int depth);

void GX2SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
					 int x, int y, int w, int h,
					 int skipleft);

void GX2SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno);
void GX2FillCacheBltRects(ScrnInfoPtr pScrn, int rop, unsigned int planemask,
			  int nBox, BoxPtr pBox, int xorg, int yorg,
			  XAACacheInfoPtr pCache);
void GX2SetupForImageWrite(ScrnInfoPtr pScreenInfo,
			   int rop, unsigned int planemask,
			   int transparency_color, int bpp, int depth);
void GX2SubsequentImageWriteRect(ScrnInfoPtr pScreenInfo,
				 int x, int y, int w, int h, int skipleft);
void GX2SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					   int fg, int bg, int rop,
					   unsigned int planemask);
void GX2SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					     int x, int y, int w, int h,
					     int skipleft);
void OPTGX2SetupForFillRectSolid(ScrnInfoPtr pScreenInfo, int color, int rop,
				 unsigned int planemask);
void OPTGX2SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y,
				   int w, int h);
void OPTGX2SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int xdir,
				      int ydir, int rop,
				      unsigned int planemask,
				      int transparency_color);
void OPTGX2SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo, int x1,
					int y1, int x2, int y2, int w, int h);
void OPTGX2SetupForSolidLine(ScrnInfoPtr pScreenInfo, int color, int rop,
			     unsigned int planemask);
void OPTGX2SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
			      unsigned int planemask, int length,
			      unsigned char *pattern);
void OPTGX2SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo, int x1, int y1,
				   int absmaj, int absmin, int err, int len,
				   int octant);
void OPTGX2SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
				       int x0, int y0, int x1, int y1,
				       int flags);
void OPTGX2SubsequentHorVertLine(ScrnInfoPtr pScreenInfo, int x, int y,
				 int len, int dir);

void OPTGX2SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				      int rop, unsigned int planemask,
				      int transparency_color, int bpp,
				      int depth);

void OPTGX2SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
					    int x, int y, int w, int h,
					    int skipleft);

void OPTGX2SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno);
void OPTGX2SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					      int fg, int bg, int rop,
					      unsigned int planemask);
void OPTGX2SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
						int x, int y, int w, int h,
						int skipleft);
void OPTGX2SetupForImageWrite(ScrnInfoPtr pScreenInfo,
			      int rop, unsigned int planemask,
			      int transparency_color, int bpp, int depth);
void OPTGX2SubsequentImageWriteRect(ScrnInfoPtr pScreenInfo,
				    int x, int y, int w, int h, int skipleft);

/*----------------------------------------------------------------------------
 * GX2AccelSync.
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
GX2AccelSync(ScrnInfoPtr pScreenInfo)
{
   if (SetCPUToScreen) {
#if defined(OPT_ACCEL)
      WRITE_GP32(MGP_BLT_MODE, Geode_blt_mode |
		 MGP_BM_SRC_FB | MGP_BM_SRC_MONO);
#else
      GFX(mono_bitmap_to_screen_blt(0, 0, Geodedstx, Geodedsty,
				    Geodewidth, Geodeheight,
				    localRecPtr->ColorExpandBase,
				    ((Geodewidth + 31) >> 5) << 2));
#endif

      SetCPUToScreen = 0;
   }
#if IMGWRITE_SUPPORT
   if (SetImageWriteRect) {
      unsigned long srcpitch;

#if defined(OPT_ACCEL)
      GeodePtr pGeode = GEODEPTR(pScreenInfo);

      srcpitch = ((Geodewidth << gu2_xshift) + 3) & 0xFFFFFFFC;

      GU2_WAIT_PENDING;
      WRITE_GP32(MGP_STRIDE, (srcpitch << 16) | pGeode->Pitch);
      WRITE_GP32(MGP_SRC_OFFSET, ImgBufOffset);
      WRITE_GP32(MGP_DST_OFFSET,
		 (CALC_FBOFFSET(Geodedstx, Geodedsty)) & 0x00FFFFFF);
      WRITE_GP32(MGP_WID_HEIGHT,
		 ((unsigned long)Geodewidth << 16) | (unsigned long)
		 Geodeheight);
/*
		ErrorF("%d %d, %d\n", Geodewidth, Geodeheight, gu2_xshift);
		ErrorF("%X , %X %X %X %X\n", srcpitch, ((srcpitch << 16) | 
                        pGeode->Pitch), ImgBufOffset, 
			(CALC_FBOFFSET(Geodedstx, Geodedsty)) & 0x00FFFFFF,
			((unsigned long)Geodewidth << 16) | 
                        (unsigned long)Geodeheight);
*/
      WRITE_GP32(MGP_BLT_MODE, Geode_blt_mode);
#else
      srcpitch = ((Geodewidth << gu2_xshift) + 3) & 0xFFFFFFFC;
      GFX2(set_source_stride(srcpitch));
      GFX2(screen_to_screen_blt(ImgBufOffset,
				CALC_FBOFFSET(Geodedstx, Geodedsty),
				Geodewidth, Geodeheight, 0));
#endif
      SetImageWriteRect = 0;
   }
#endif /* IMGWRITE_SUPPORT */

   GFX(wait_until_idle());
}

/*----------------------------------------------------------------------------
 * GX2SetupForFillRectSolid.
 *
 * Description	:This routine is called to setup the solid pattern
 *               color for   future  rectangular fills or vectors.
 *
 * Parameters.
 * pScreenInfo
 *    Ptr       :Screen handler pointer having screen information.
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
GX2SetupForFillRectSolid(ScrnInfoPtr pScreenInfo,
			 int color, int rop, unsigned int planemask)
{
   GFX(set_solid_pattern((unsigned int)color));

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
 * GX2SubsequentFillRectSolid.
 *
 * Description	:This routine is used to fill the rectangle of previously
 *               specified  solid pattern.
 *
 * Parameters.
 *  pScreenInfo :Screen handler pointer having screen information.
 *     x and y	:Specifies the x and y co-ordinatesarea.
 *      w and h	:Specifies width and height respectively.
 *
 * Returns      :none
 *
 * Comments     :desired pattern can be set before this function by
 *               gfx_set_solid_pattern.
 * Sample application uses:
 *   - Window backgrounds. 
 *   - x11perf: rectangle tests (-rect500).
 *   - x11perf: fill trapezoid tests (-trap100).
 *   - x11perf: horizontal line segments (-hseg500).
 *----------------------------------------------------------------------------
*/
void
GX2SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo, int x, int y, int w,
			   int h)
{
   DEBUGMSG(0, (0, 0, "FillRect %d %d %dx%d\n", x, y, w, h));

   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */

   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)w, (unsigned short)h));
}

/*----------------------------------------------------------------------------
 * GX2SetupFor8x8PatternColorExpand
 *
 * Description	:This routine is called to fill the color pattern of
 *                 8x8.
 * Parameters.
 *	pScreenInfo :Screen handler pointer having screen information.
 *	patternx    :This is set from on rop data.
 *      patterny    :This is set based on rop data.
 *	planemask   :Specifies the value of masking from rop data
 *	trans_color :to be added. 
 * Returns	:none.
 *
 * Comments     :none.
 *
*----------------------------------------------------------------------------
*/

void
GX2SetupFor8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				 int patternx, int patterny, int rop,
				 unsigned int planemask, int trans_color)
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
 * GX2Subsequent8x8PatternColorExpand
 *
 * Description	:This routine is called to fill  a rectangle with the
 *               color pattern of previously loaded pattern.
 *
 * Parameters.
 *  pScreenInfo	:Screen handler pointer having screen information.
 *  patternx	:This is set from on rop data.
 *  patterny	:This is set based on rop data.
 *      x	:x -coordinates of the destination rectangle
 *      y       :y-co-ordinates of the destination rectangle
 *      w	:Specifies width of the rectangle
 *      h       :Height of the window of the rectangle
 *		
 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses:
 *   - Patterned desktops
 *   - x11perf: stippled rectangle tests (-srect500).
 *   - x11perf: opaque stippled rectangle tests (-osrect500).
*----------------------------------------------------------------------------
*/
void
GX2Subsequent8x8PatternColorExpand(ScrnInfoPtr pScreenInfo,
				   int patternx, int patterny, int x, int y,
				   int w, int h)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   DEBUGMSG(1, (0, 0, "8x8color %d %d %dx%d\n", x, y, w, h));

   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */
   /* Ignores specified pattern. */
   GFX(color_pattern_fill((unsigned short)x, (unsigned short)y,
			  (unsigned short)w, (unsigned short)h,
			  ((unsigned long *)((pGeode->FBBase +
					      (patterny << gu2_yshift)) +
					     patternx))));
}

/*----------------------------------------------------------------------------
 * GX2SetupFor8x8PatternMonoExpand
 *
 * Description	:This routine is called to fill the monochrome pattern of
 *                 8x8.
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      patternx    :This is set from on rop data.
 *      patterny    :This is set based on rop data.
 *       fg	    :Specifies the foreground color
 *       bg         :Specifies the background color
 *	planemask   :Specifies the value of masking from rop data

 * Returns	:none.
 *
 * Comments     :none.
 *
*----------------------------------------------------------------------------
*/
void
GX2SetupFor8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo,
				int patternx, int patterny, int fg,
				int bg, int rop, unsigned int planemask)
{
   int trans = (bg == -1);

   /* LOAD PATTERN COLORS AND DATA */
   GFX(set_mono_pattern((unsigned int)bg, (unsigned int)fg,
			(unsigned int)patternx, (unsigned int)patterny,
			trans));

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
 * GX2Subsequent8x8PatternMonoExpand
 *
 * Description	:This routine is called to fill  a ractanglethe
 *                 monochrome pattern of previusly loaded pattern.
 *
 * Parameters.
 *  pScreenInfo	:Screen handler pointer having screen information.
 *  patternx	:This is set from on rop data.
 *  patterny	:This is set based on rop data.
 *       fg	:Specifies the foreground color
 *       bg	:Specifies the background color
 *  planemask	:Specifies the value of masking from rop data

 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses:
 *   - Patterned desktops
 *   - x11perf: stippled rectangle tests (-srect500).
 *   - x11perf: opaque stippled rectangle tests (-osrect500).
*----------------------------------------------------------------------------
*/
void
GX2Subsequent8x8PatternMonoExpand(ScrnInfoPtr pScreenInfo,
				  int patternx, int patterny, int x, int y,
				  int w, int h)
{
   DEBUGMSG(0, (0, 0, "8x8mono %d %d %dx%d\n", x, y, w, h));

   /* SIMPLY PASS THE PARAMETERS TO THE DURANGO ROUTINE */
   /* Ignores specified pattern. */
   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)w, (unsigned short)h));
}

/*----------------------------------------------------------------------------
 * GX2SetupForScreenToScreenCopy
 *
 * Description	:This function is used to set up the planemask and raster
 *                 for future Bliting functionality.
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      xdir	:This is set based on rop data.
 *      ydir    :This is set based on rop data.
 *      rop	:sets the raster operation
 *	transparency:tobeadded
 *  planemask	:Specifies the value of masking from rop data

 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
*----------------------------------------------------------------------------
*/
void
GX2SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
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
 * GX2SubsquentScreenToScreenCopy
 *
 * Description	:This function is called to perform a screen to screen
 *               BLT  using the previously  specified planemask,raster
 *               operation and  * transparency flag
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *  	x1	:x -coordinates of the source window
 *      y1      :y-co-ordinates of the source window
 *      x2	:x -coordinates of the destination window
 *      y2      :y-co-ordinates of the destination window
 *      w	:Specifies width of the window to be copied
 *      h       :Height of the window to be copied.
 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/
void
GX2SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				int x1, int y1, int x2, int y2, int w, int h)
{
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

void
GX2SetupForImageWrite(ScrnInfoPtr pScreenInfo,
		      int rop, unsigned int planemask,
		      int transparency_color, int bpp, int depth)
{
   GFX(set_solid_pattern((unsigned int)planemask));
   /* SET RASTER OPERATION FOR USING PATTERN AS PLANE MASK */
   GFX(set_raster_operation(XAACopyROP[rop]));
   /* SAVE TRANSPARENCY FLAG */
   GeodeTransparent = (transparency_color == -1) ? 0 : 1;
   GeodeTransColor = transparency_color;
}

void
GX2SubsequentImageWriteRect(ScrnInfoPtr pScreenInfo,
			    int x, int y, int w, int h, int skipleft)
{
   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
#if IMGWRITE_SUPPORT
   SetImageWriteRect = 1;
#endif

}

/*----------------------------------------------------------------------------
 * GX2SetupForScanlineImageWrite
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.
 *
 * Parameters:
 *  pScreenInfo	       :Screen handler pointer having screen information.
 *  rop		       :sets the raster operation
 *  transparency_color :transparency color key.
 *  planemask	       :Specifies the value of masking from rop data
 *  bpp		       :bits per pixel of the source pixmap
 *  depth	       :depth of the source pixmap.
 * Returns	:none
 *
 * Comments	:none
 *  x11perf -putimage10  
 *  x11perf -putimage100 
 *  x11perf -putimage500 
*----------------------------------------------------------------------------
*/
void
GX2SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
			      int rop, unsigned int planemask,
			      int transparency_color, int bpp, int depth)
{
   GFX(set_solid_pattern((unsigned int)planemask));
   /* SET RASTER OPERATION FOR USING PATTERN AS PLANE MASK */
   GFX(set_raster_operation(XAACopyROP[rop & 0x0F]));
   /* SAVE TRANSPARENCY FLAG */
   GeodeTransparent = (transparency_color == -1) ? 0 : 1;
   GeodeTransColor = transparency_color;
}

/*----------------------------------------------------------------------------
 * GX2SubsequentScanlineImageWriteRect
 *
 * Description	:This function is used to set up the x,y corordinates and width
 *               &height for future Bliting functionality.
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      x	    :destination x
 *	y	    :destination y
 *	w	    :Specifies the width of the rectangle to be  copied
 *	h	    :Specifies the height of the rectangle to be  copied
 *	
 * Returns	:none
 *
 * Comments	:none
*----------------------------------------------------------------------------
*/
void
GX2SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
				    int x, int y, int w, int h, int skipleft)
{
   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
   GeodeCounter = 0;
}

/*----------------------------------------------------------------------------
 * GX2SubsquentImageWriteScanline
 *
 * Description	:This function is called to 
 *               BLT  using the previously  specified planemask,raster
 *               operation and   transparency flag
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *
 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/
void
GX2SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno)
{
   GeodePtr pGeode;
   int blt_height = 0;
   char blit = FALSE;

   pGeode = GEODEPTR(pScreenInfo);

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

/*----------------------------------------------------------------------------
 * GX2SetupForCPUToScreenColorExpandFill
 *
 * Description	:This routine is called to setup the background and 
 *		 foreground colors,rop and plane mask for future
 *	         color expansion blits from source patterns stored
 *		 in system memory
 *				
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      fg	    :Specifies the foreground color
 *      bg	    :Specifies the background color
 *      rop	    :Specifies rop values.
 *	planemask   :Specifies the value of masking from rop data
 *
 * Returns	:none.
 *
 * Comments     :
 * All the text gets rendered through this i/f. We have given 
 * the offscreen memory loaction to temporarily put the text 
 * bitmap. Generaly all the text comes as bitmap and then gets 
 * rendered via the HOST_SRC(similar to scratchpad in GX1). Now
 * since we already have the bitmap in offscreen we can do a 
 * src_FB_EXPAND. This is the best possible you can do with GX2
 * CPU-to-screen color expansion 
 *----------------------------------------------------------------------------
*/
void
GX2SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
				      int fg, int bg, int rop,
				      unsigned int planemask)
{
   GFX(set_solid_pattern(planemask));
   GFX(set_mono_source(bg, fg, (bg == -1)));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(XAACopyROP_PM[rop & 0x0F]));

   DEBUGMSG(0, (0, X_NONE, "%x %x %x %x\n", fg, bg, rop, planemask));
}

/*-------------------------------------------------------------------------------
 * GX2SubsequentCPUToScreenColorExpandFill
 *
 * Description	:This routine is used to perform color expansion blits from 
 *               source  patterns stored in system memory using the 
 *               previously set rop and plane mask.
 *
 * Parameters.
 *	pScreenInfo :Screen handler pointer having screen information.
 *	x and y	    :Specifies the x and y co-ordinatesarea.
 *      w and h	    :Specifies width and height respectively.
 *
 * Returns		:none
 *
 * Comments		:none
 *--------------------------------------------------------------------------------
 */
void
GX2SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					int x, int y, int w, int h,
					int skipleft)
{
   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
   SetCPUToScreen = 1;
}

#if SCR2SCREXP
void
GX2SetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
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
GX2SubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
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
 * GX2SetupForSolidLine
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
GX2SetupForSolidLine(ScrnInfoPtr pScreenInfo,
		     int color, int rop, unsigned int planemask)
{
   /* LOAD THE SOLID PATTERN COLOR */
   GFX(set_solid_pattern((unsigned int)color));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(XAAPatternROP[rop & 0x0F]));
}

/*---------------------------------------------------------------------------
 * GX2SubsequentBresenhamLine
 *
 * Description	:This function is used to render a vector using the
 *                 specified bresenham parameters.
 *
 * Parameters:
 * pScreenInfo 	:Screen handler pointer having screen information.
 *      x1  	:Specifies the starting x position
 *      y1      :Specifies starting y possition
 *      absmaj	:Specfies the Bresenman absolute major.
 *	absmin	:Specfies the Bresenman absolute minor.
 *	err     :Specifies the bresenham err term.
 *	len     :Specifies the length of the vector interms of pixels.
 *	octant  :not used in this function,may be added for standard
 *                    interface.
 * Returns	:none
 *
 * Comments     :none
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-line500).
 *   - x11perf: line segments (-seg500).
*----------------------------------------------------------------------------
*/
void
GX2SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo,
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
GX2SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
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
 * GX2SubsequentHorVertLine
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
GX2SubsequentHorVertLine(ScrnInfoPtr pScreenInfo,
			 int x, int y, int len, int dir)
{
   DEBUGMSG(0, (0, 0, "HLine %d, %d, %d, %d\n", x, y, len, dir));
   GFX(pattern_fill((unsigned short)x, (unsigned short)y,
		    (unsigned short)((dir == DEGREES_0) ? len : 1),
		    (unsigned short)((dir == DEGREES_0) ? 1 : len)));
}

#if DASHED_SUPPORT
void
BuildPattern(CARD32 pat, int len, CARD32 * pat8x8)
{
   unsigned long i, count;

   /* find homany can fit comfortably */
   count = 32 / len;
   /* add 1 for the residue */
   count++;
   /* construct the mask and knock off the unwanted data */
   i = ((CARD32) 0xFFFFFFFF) << (31 - len);
   pat &= i;
   /* init before the show */
   pat8x8[0] = 0;
   /* loop and build the pattern aray data */
   for (i = 0; i < count; i++) {
      pat8x8[0] |= (pat >> (len * i));
   }

   /* equate both the array's and then adjust */
   pat8x8[1] = pat8x8[0];

   /* how many carried from last operation */
   i = (len * count) - 32;
   pat8x8[1] >>= i;
   pat8x8[1] |= (pat << (len - i));
}

#define PAT_SHIFT(pat,n) pat >> n

/*----------------------------------------------------------------------------
 * GX2SetupForDashedLine
 *
 * Description	:This function is used to setup for
 *               future line draws.
 *
 *
 * Parameters.
 * 		pScreenInfo :Screen handler pointer having screen information.
 * Returns		:none
 *
 * Comments		:none
 * x11perf -dseg100  
 * x11perf -dline100 
 * x11perf -ddline100
*----------------------------------------------------------------------------
*/
void
GX2SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		      unsigned int planemask, int length,
		      unsigned char *pattern)
{
   int trans = (bg == -1);
   CARD32 *pat = (CARD32 *) pattern;
   CARD32 pat8x8[2];

   pat8x8[0] = pat[0];
   switch (length) {
   case 2:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 2);	/* fall through */
   case 4:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 4);	/* fall through */
   case 8:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 8);	/* fall through */
   case 16:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 16);
   case 32:
      pat8x8[1] = pat8x8[0];
      break;
   case 64:
      pat8x8[1] = pat[1];
      break;
   default:
      BuildPattern(pat[0], length, pat8x8);
   }
/*	
	ErrorF("%X %d, %X %X\n", pat[0], length, pat8x8[0], pat8x8[1]); 
*/
   /* LOAD PATTERN COLORS AND DATA */

   GFX(set_mono_pattern((unsigned int)bg, (unsigned int)fg,
			pat8x8[0], pat8x8[1], (unsigned char)trans));

   /* CHECK IF PLANEMASK IS NOT USED (ALL PLANES ENABLED) */

   if (planemask == 0xFFFFFFFF) {
      /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */

      GFX(set_raster_operation(windowsROPpat[rop & 0x0F]));
   } else {
      /* SELECT ROP THAT USES SOURCE DATA FOR PLANEMASK */

      GFX(set_raster_operation(windowsROPsrcMask[rop & 0x0F]));
   }
}

/*---------------------------------------------------------------------------
 * GX2SubsequentDashedBresenhamLine
 *
 * Description	:This function is used to render a vector using the
 *                 specified bresenham parameters.
 *
 * Parameters:
 * pScreenInfo 	:Screen handler pointer having screen information.
 *      x1  	:Specifies the starting x position
 *      y1      :Specifies starting y possition
 *      absmaj	:Specfies the Bresenman absolute major.
 *	absmin	:Specfies the Bresenman absolute minor.
 *	err     :Specifies the bresenham err term.
 *	len     :Specifies the length of the vector interms of pixels.
 *	octant  :not used in this function,may be added for standard
 *               interface.
 * Returns	:none
 *
 * Comments     :none
 * Sample application uses:
 *   - Window outlines on window move.
 *   - x11perf: line segments (-line500).
 *   - x11perf: line segments (-seg500).
*----------------------------------------------------------------------------
*/
void
GX2SubsequentDashedBresenhamLine(ScrnInfoPtr pScreenInfo,
				 int x1, int y1, int absmaj, int absmin,
				 int err, int len, int octant)
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

   gfx2_set_pattern_origin(x1, y1);
   gfx2_bresenham_line(CALC_FBOFFSET(x1, y1),
		       (unsigned short)len, (unsigned short)init,
		       (unsigned short)axial, (unsigned short)diag,
		       (unsigned short)vector_mode_table[octant]);

}
#endif

#if !defined(STB_X)
/*----------------------------------------------------------------------------
 * OPTGX2SetupForCPUToScreenColorExpandFill
 *
 * Description	:This routine is called to setup the background and 
 *		 foreground colors,rop and plane mask for future
 *		 color expansion blits from source patterns stored
 *		 in system memory(non durango version).
 *				
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *      fg	    :Specifies the foreground color
 *      bg	    :Specifies the background color
 *      rop	    :Specifies rop values.
 *	planemask   :Specifies the value of masking from rop data
 *
 * Returns	:none.
 *
 * Comments     :
 * All the text gets rendered through this i/f. We have given 
 * the offscreen memory loaction to temporarily put the text 
 * bitmap. Generaly all the text comes as bitmap and then gets 
 * rendered via the HOST_SRC(similar to scratchpad in GX1). Now
 * since we already have the bitmap in offscreen we can do a 
 * src_FB_EXPAND. This is the best possible you can do with GX2
 * CPU-to-screen color expansion 
 *  x11perf -ftext (pure indirect):
 *  x11perf -oddsrect10   
 *  x11perf -oddsrect100  
 *  x11perf -bigsrect10   
 *  x11perf -bigsrect100  
 *  x11perf -polytext     
 *  x11perf -polytext16   
 *  x11perf -seg1         
 *  x11perf -copyplane10  
 *  x11perf -copyplane100 
 *  x11perf -putimagexy10 
 *  x11perf -putimagexy100
*----------------------------------------------------------------------------
*/

void
OPTGX2SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					 int fg, int bg, int rop,
					 unsigned int planemask)
{
   int trans = (bg == -1);

   GeodeROP = XAACopyROP_PM[rop];

   if ((GeodeROP & 0x55) ^ ((GeodeROP >> 1) & 0x55)) {
      Geode_blt_mode = MGP_BM_DST_REQ;
   } else {
      Geode_blt_mode = 0;
   }
   if (trans)
      GeodeROP |= MGP_RM_SRC_TRANS;

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_PAT_COLOR_0, (unsigned long)planemask);
   WRITE_GP32(MGP_RASTER_MODE, gu2_bpp | GeodeROP);
   WRITE_GP32(MGP_SRC_COLOR_FG, fg);
   WRITE_GP32(MGP_SRC_COLOR_BG, bg);
}

/*-------------------------------------------------------------------------------
 * OPTGX2SubsequentCPUToScreenColorExpandFill
 *
 * Description	:This routine is used to perform color expansion blits from 
 *               source patterns stored in system memory using the previously 
 *               set rop and plane mask.(non durango version)
 *
 * Parameters.
 *		pScreenInfo :Screen handler pointer having screen information.
 *		x and y	    :Specifies the x and y co-ordinatesarea.
 *              w and h	    :Specifies width and height respectively.
 *
 * Returns		:none
 *
 * Comments		:none
 *--------------------------------------------------------------------------------
 */
void
OPTGX2SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScreenInfo,
					   int x, int y, int w, int h,
					   int skipleft)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_SRC_OFFSET, ((unsigned int)(localRecPtr->ColorExpandBase -
					      pGeode->FBBase)));
   WRITE_GP32(MGP_DST_OFFSET, CALC_FBOFFSET(x, y));
   WRITE_GP32(MGP_WID_HEIGHT, (((unsigned long)w) << 16) | h);
   WRITE_GP32(MGP_STRIDE, (((w + 31) >> 5) << 18) | pGeode->Pitch);
   SetCPUToScreen = 1;
}

/*----------------------------------------------------------------------------
 * OPTGX2SetupForFillRectSolid.
 *
 * Description	:This routine is called to setup the solid pattern
 *               color for   future  rectangular fills or vectors.
 *				 (non durango version)
 *
 * Parameters.
 * pScreenInfo
 *    Ptr	:Screen handler pointer having screen information.
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
OPTGX2SetupForFillRectSolid(ScrnInfoPtr pScreenInfo,
			    int color, int rop, unsigned int planemask)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_PAT_COLOR_0, (unsigned long)color);

   WRITE_GP32(MGP_STRIDE, pGeode->Pitch);

   if (planemask == 0xFFFFFFFF) {
      GeodeROP = XAAPatternROP[rop];
   } else {
      WRITE_GP32(MGP_SRC_COLOR_FG, (unsigned long)planemask);
      GeodeROP = XAAPatternROP_PM[rop];
   }

   WRITE_GP32(MGP_RASTER_MODE, gu2_bpp | GeodeROP);

   Geode_blt_mode = 0;
   if (!((GeodeROP & 0x33) ^ ((GeodeROP >> 2) & 0x33)))
      Geode_blt_mode = MGP_BM_SRC_MONO;

   if ((GeodeROP & 0x55) ^ ((GeodeROP >> 1) & 0x55)) {
      Geode_blt_mode |= MGP_BM_DST_REQ;
      Geode_vector_mode = MGP_VM_DST_REQ;
   } else {
      Geode_vector_mode = 0;
   }
}

 /*----------------------------------------------------------------------------
 * OPTGX2SubsequentFillRectSolid.
 *
 * Description	:This routine is used to fill the rectangle of previously
 *               specified  solid pattern.
 *		(non durango version)
 *
 * Parameters.
 *  pScreenInfo :Screen handler pointer having screen information.
 *     x and y	:Specifies the x and y co-ordinatesarea.
 *     w and h	:Specifies width and height respectively.
 *
 * Returns	:none
 *
 * Comments	:desired pattern can be set before this function by
 *               gfx_set_solid_pattern.
 * Sample application uses:
 *   - Window backgrounds. 
 *   - x11perf: rectangle tests (-rect500).
 *   - x11perf: fill trapezoid tests (-trap100).
 *   - x11perf: horizontal line segments (-hseg500).
 *----------------------------------------------------------------------------
*/
void
OPTGX2SubsequentFillRectSolid(ScrnInfoPtr pScreenInfo,
			      int x, int y, int width, int height)
{
   DEBUGMSG(0, (0, 0, "FillRect %d %d %dx%d\n", x, y, width, height));

   GFX_PATTERN_FILL(x, y, width, height);
}

/*----------------------------------------------------------------------------
 * OPTGX2SetupForScreenToScreenCopy
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.
 *		(non durango version)
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      xdir	:This is set based on rop data.
 *      ydir    :This is set based on rop data.
 *      rop	:sets the raster operation
 *	transparency:tobeadded
 *  planemask	:Specifies the value of masking from rop data

 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
*----------------------------------------------------------------------------
*/
void
OPTGX2SetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				 int xdir, int ydir, int rop,
				 unsigned int planemask,
				 int transparency_color)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   GeodeROP = XAACopyROP_PM[rop];

   Geode_blt_mode = MGP_BM_SRC_FB;

   /* CALCULATE THE DIRECTION OF THE BLT */
   if ((GeodeROP & 0x55) ^ ((GeodeROP >> 1) & 0x55)) {
      Geode_blt_mode |= MGP_BM_DST_REQ;
   }

   GU2_WAIT_PENDING;

   if (transparency_color != -1) {
      WRITE_GP32(MGP_SRC_COLOR_FG, transparency_color);
      WRITE_GP32(MGP_SRC_COLOR_BG, 0xFFFFFFFF);
      GeodeROP = MGP_RM_SRC_TRANS | 0xCC;
   }
   WRITE_GP32(MGP_PAT_COLOR_0, planemask);
   WRITE_GP32(MGP_RASTER_MODE, gu2_bpp | GeodeROP);
   WRITE_GP32(MGP_STRIDE, pGeode->Pitch | (pGeode->Pitch << 16));
}

/*----------------------------------------------------------------------------
 * OPTGX2SubsquentScreenToScreenCopy
 *
 * Description	:This function is called to perform a screen to screen
 *               BLT  using the previously  specified planemask,raster
 *               operation and  * transparency flag
 *		(non durango version)
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *  	srcx	:x -coordinates of the source window
 *      srcy    :y-co-ordinates of the source window
 *      dstx	:x -coordinates of the destination window
 *      dsty    :y-co-ordinates of the destination window
 *      width	:Specifies width of the window to be copied
 *      height  :Height of the window to be copied.
 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/
void
OPTGX2SubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
				   int srcx, int srcy, int dstx, int dsty,
				   int width, int height)
{
   unsigned int srcoffset, dstoffset, blt_mode, size;

   DEBUGMSG(0, (0, 0, "Scr2scr %d %d %d %d %dx%d\n",
		srcx, srcy, dstx, dsty, width, height));

   size = (((unsigned int)width) << 16) | height;

   blt_mode = Geode_blt_mode;

   if (dstx > srcx) {
      blt_mode |= MGP_BM_NEG_XDIR;
      srcx += width - 1;
      dstx += width - 1;
   }
   if (dsty > srcy) {
      blt_mode |= MGP_BM_NEG_YDIR;
      srcy += height - 1;
      dsty += height - 1;
   }

   /* CALCULATE STARTING OFFSETS */

   srcoffset = CALC_FBOFFSET(srcx, srcy);
   dstoffset = CALC_FBOFFSET(dstx, dsty) & 0xFFFFFF;

   /* TURN INTO BYTE ADDRESS IF NEGATIVE X DIRECTION */
   /* This is a quirk of the hardware. */

   if (Geode_blt_mode & MGP_BM_NEG_XDIR) {
      srcoffset += (1 << gu2_xshift) - 1;
      dstoffset += (1 << gu2_xshift) - 1;
   }
   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Put off poll for as long as possible (do most calculations first). */

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_SRC_OFFSET, srcoffset);
   WRITE_GP32(MGP_DST_OFFSET, dstoffset);
   WRITE_GP32(MGP_WID_HEIGHT, size);
   WRITE_GP32(MGP_BLT_MODE, blt_mode);
}

/*----------------------------------------------------------------------------
 * OPTGX2SetupForImageWrite
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.(non durango version)
 *
 * Parameters:
 *	pScreenInfo	:Screen handler pointer having screen information.
 *      rop		:sets the raster operation
 *	transparency_color	:tobeadded
 *	planemask		:Specifies the value of masking from rop data
 *      bpp			:bits per pixel of the source pixmap
 *	depth			:depth of the source pixmap.
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
OPTGX2SetupForImageWrite(ScrnInfoPtr pScreenInfo,
			 int rop, unsigned int planemask,
			 int transparency_color, int bpp, int depth)
{
   OPTGX2SetupForScreenToScreenCopy(pScreenInfo,
				    0, 0, rop, planemask, transparency_color);
}

void
OPTGX2SubsequentImageWriteRect(ScrnInfoPtr pScreenInfo,
			       int x, int y, int w, int h, int skipleft)
{
   Geodedstx = x;
   Geodedsty = y;
   Geodewidth = w;
   Geodeheight = h;
#if IMGWRITE_SUPPORT
   SetImageWriteRect = 1;
#endif
}

/*----------------------------------------------------------------------------
 * OPTGX2SetupForScanlineImageWrite
 *
 * Description	:This function is used to set up the planemask and raster
 *               for future Bliting functionality.(non durango version)
 *
 * Parameters:
 *	pScreenInfo	  :Screen handler pointer having screen information.
 *      rop		  :sets the raster operation
 *	transparency_color:tobeadded
 *	planemask	  :Specifies the value of masking from rop data
 *      bpp		  :bits per pixel of the source pixmap
 *	depth		  :depth of the source pixmap.
 * Returns		:none
 *
 * Comments		:none
*----------------------------------------------------------------------------
*/
void
OPTGX2SetupForScanlineImageWrite(ScrnInfoPtr pScreenInfo,
				 int rop, unsigned int planemask,
				 int transparency_color, int bpp, int depth)
{
   OPTGX2SetupForScreenToScreenCopy(pScreenInfo,
				    0, 0, rop, planemask, transparency_color);
}

/*----------------------------------------------------------------------------
 * OPTGX2SubsequentScanlineImageWriteRect
 *
 * Description	:This function is used to set up the x,y corordinates and width
 *                &height for future Bliting functionality.(non durango version)
 *
 * Parameters:
 *	pScreenInfo :Screen handler pointer having screen information.
 *      x		:destination x
 *	y		:destination y
 *	w		:Specifies the width of the rectangle to be  copied
 *	h		:Specifies the height of the rectangle to be  copied
 * Returns		:none
 *
 * Comments		:none
 *----------------------------------------------------------------------------
*/
void
OPTGX2SubsequentScanlineImageWriteRect(ScrnInfoPtr pScreenInfo,
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
 * OPTGX2SubsquentImageWriteScanline
 *
 * Description	:This function is called to 
 *               BLT  using the previously  specified planemask,raster
 *               operation and transparency flag(non durango version)
 *
 * Parameters.
 * 	pScreenInfo :Screen handler pointer having screen information.
 *
 * Returns	:none
 *
 * Comments	:The patterns specified is ignored inside the function
 * Sample application uses (non-transparent):
 *   - Moving windows.
 *   - x11perf: scroll tests (-scroll500).
 *   - x11perf: copy from window to window (-copywinwin500).
 *
 * No application found using transparency.
*----------------------------------------------------------------------------
*/

void
OPTGX2SubsequentImageWriteScanline(ScrnInfoPtr pScreenInfo, int bufno)
{
   GeodePtr pGeode;

   int blt_height = 0;
   char blit = FALSE;

   pGeode = GEODEPTR(pScreenInfo);
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
      OPTGX2SubsequentScreenToScreenCopy(pScreenInfo,
					 Geodesrcx, Geodesrcy, Geodedstx,
					 Geodedsty, Geodewidth, blt_height);
      Geodedsty += blt_height;
      GU2_WAIT_BUSY;
   }
}

/*----------------------------------------------------------------------------
 * OPTGX2SetupForSolidLine
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
OPTGX2SetupForSolidLine(ScrnInfoPtr pScreenInfo,
			int color, int rop, unsigned int planemask)
{
   OPTGX2SetupForFillRectSolid(pScreenInfo, color, rop, planemask);
}

/*---------------------------------------------------------------------------
 * OPTGX2SubsequentBresenhamLine
 *
 * Description	:This function is used to render a vector using the
 *                 specified bresenham parameters.
 *
 * Parameters:
 * pScreenInfo 	:Screen handler pointer having screen information.
 *      x1  	:Specifies the starting x position
 *      y1      :Specifies starting y possition
 *      absmaj	:Specfies the Bresenman absolute major.
 *	absmin	:Specfies the Bresenman absolute minor.
 *	err     :Specifies the bresenham err term.
 *	len     :Specifies the length of the vector interms of pixels.
 *	octant  :not used in this function,may be added for standard
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
OPTGX2SubsequentBresenhamLine(ScrnInfoPtr pScreenInfo,
			      int x1, int y1, int absmaj, int absmin,
			      int err, int len, int octant)
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

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_DST_OFFSET, CALC_FBOFFSET(x1, y1));
   WRITE_GP32(MGP_VEC_ERR,
	      (((unsigned long)axial) << 16) | (unsigned short)diag);
   WRITE_GP32(MGP_VEC_LEN,
	      (((unsigned long)len) << 16) | (unsigned short)init);
   WRITE_GP32(MGP_VECTOR_MODE,
	      (Geode_vector_mode | vector_mode_table[octant]));
}

void
OPTGX2SubsequentSolidTwoPointLine(ScrnInfoPtr pScreenInfo,
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

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_DST_OFFSET, CALC_FBOFFSET(x0, y0));
   WRITE_GP32(MGP_VEC_ERR,
	      (((unsigned long)axialerr) << 16) | (unsigned short)diagerr);
   WRITE_GP32(MGP_VEC_LEN,
	      (((unsigned long)dmaj) << 16) | (unsigned short)initerr);
   WRITE_GP32(MGP_VECTOR_MODE, (Geode_vector_mode | vec_flags));
}

/*---------------------------------------------------------------------------
 * OPTGX2SubsequentHorVertLine
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
OPTGX2SubsequentHorVertLine(ScrnInfoPtr pScreenInfo,
			    int x, int y, int len, int dir)
{
   DEBUGMSG(0, (0, 0, "HLine %d, %d, %d, %d\n", x, y, len, dir));
#if 1
   GFX_PATTERN_FILL(x, y,
		    (unsigned short)((dir == DEGREES_0) ? len : 1),
		    (unsigned short)((dir == DEGREES_0) ? 1 : len));
#else
   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_DST_OFFSET, CALC_FBOFFSET(x, y));
   WRITE_GP32(MGP_VEC_ERR, 0);
   WRITE_GP32(MGP_VEC_LEN,
	      (((unsigned long)len) << 16) | (unsigned short)-len);
   WRITE_GP32(MGP_VECTOR_MODE,
	      (Geode_vector_mode |
	       vector_mode_table[(dir == DEGREES_0) ? 2 : 5]));
#endif
}

#if DASHED_SUPPORT
/* Setup for XAA dashed lines.

   Tests: xtest CH05/stdshs, XFree86/drwln

   x11perf -dseg100  
   x11perf -dline100 
   x11perf -ddline100
*/
void
OPTGX2SetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
			 unsigned int planemask, int length,
			 unsigned char *pattern)
{
   GeodePtr pGeode = GEODEPTR(pScrn);
   CARD32 *pat = (CARD32 *) pattern;
   CARD32 pat8x8[2];

   pat8x8[0] = pat[0];
   switch (length) {
   case 2:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 2);	/* fall through */
   case 4:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 4);	/* fall through */
   case 8:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 8);	/* fall through */
   case 16:
      pat8x8[0] |= PAT_SHIFT(pat8x8[0], 16);
   case 32:
      pat8x8[1] = pat8x8[0];
      break;
   case 64:
      pat8x8[1] = pat[1];
      break;
   default:
      BuildPattern(pat[0], length, pat8x8);
   }
   /* LOAD PATTERN COLORS AND DATA */

   /* SET PATTERN FLAGS */

   if (planemask == 0xFFFFFFFF) {
      GeodeROP = XAAPatternROP[rop & 0x0F];
   } else {
      GeodeROP = XAAPatternROP_PM[rop & 0x0F];
   }
   if (bg == -1)
      GeodeROP |= MGP_RM_PAT_MONO | MGP_RM_PAT_TRANS;
   else
      GeodeROP |= MGP_RM_PAT_MONO;

   if ((GeodeROP & 0x55) ^ ((GeodeROP >> 1) & 0x55)) {
      Geode_blt_mode = MGP_BM_DST_REQ;
      Geode_vector_mode = MGP_VM_DST_REQ;
   } else {
      Geode_blt_mode = MGP_BM_SRC_MONO;
      Geode_vector_mode = 0;
   }

   /* POLL UNTIL ABLE TO WRITE THE PATTERN COLOR */

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_RASTER_MODE, gu2_bpp | GeodeROP);
   WRITE_GP32(MGP_PAT_COLOR_0, bg);
   WRITE_GP32(MGP_PAT_COLOR_1, fg);
   WRITE_GP32(MGP_PAT_DATA_0, pat8x8[0]);
   WRITE_GP32(MGP_PAT_DATA_1, pat8x8[1]);
   WRITE_GP32(MGP_STRIDE, pGeode->Pitch);

}

void
OPTGX2SubsequentDashedBresenhamLine(ScrnInfoPtr pScreenInfo,
				    int x1, int y1, int absmaj, int absmin,
				    int err, int len, int octant)
{
   int axial, init, diag;
   unsigned long gu2_pattern_origin;

   DEBUGMSG(0, (0, 0, "BLine %d, %d, %d, %d, %d, %d, %d\n",
		x1, y1, absmaj, absmin, err, len, octant));

   /* CHECK NULL LENGTH */

   if (!len)
      return;

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

   gu2_pattern_origin = (((unsigned long)(x1 & 7)) << 26) |
	 (((unsigned long)(y1 & 7)) << 29);

   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Put off poll for as long as possible (do most calculations first). */

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_DST_OFFSET, (CALC_FBOFFSET(x1, y1) & 0x00FFFFFF) |
	      gu2_pattern_origin);
   WRITE_GP32(MGP_VEC_ERR,
	      (((unsigned long)axial) << 16) | (unsigned short)diag);
   WRITE_GP32(MGP_VEC_LEN,
	      (((unsigned long)len) << 16) | (unsigned short)init);
   WRITE_GP32(MGP_VECTOR_MODE, (Geode_vector_mode |
				vector_mode_table[octant]));
}

void
OPTGX2SubsequentDashedTwoPointLine(ScrnInfoPtr pScreenInfo,
				   int x0, int y0, int x1, int y1, int flags)
{
   long dx, dy, dmaj, dmin;
   long axialerr, diagerr, initerr;
   unsigned long gu2_pattern_origin;
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

   /* CALL ROUTINE TO DRAW VECTOR */

   gu2_pattern_origin = (((unsigned long)(x0 & 7)) << 26) |
	 (((unsigned long)(y0 & 7)) << 29);

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_DST_OFFSET, (CALC_FBOFFSET(x0, y0) & 0x00FFFFFF) |
	      gu2_pattern_origin);
   WRITE_GP32(MGP_VEC_ERR,
	      (((unsigned long)axialerr) << 16) | (unsigned short)diagerr);
   WRITE_GP32(MGP_VEC_LEN,
	      (((unsigned long)dmaj) << 16) | (unsigned short)initerr);
   WRITE_GP16(MGP_VECTOR_MODE, (Geode_vector_mode | vec_flags));
}
#endif /* DASHED_SUPPORT */
#endif /* STB_X */

#if 0
void
GX2WriteBitmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
	       unsigned char *src, int srcwidth, int skipleft,
	       int fg, int bg, int rop, unsigned int planemask)
{
   GFX(set_solid_pattern(planemask));
   GFX(set_mono_source(bg, fg, (bg == -1)));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(windowsROPpatMask[rop & 0x0F]));
   GFX(mono_bitmap_to_screen_blt_swp(0, 0, x, y,
				     srcwidth << 3, h, src, srcwidth));
}

void
GX2WritePixmap(ScrnInfoPtr pScrn, int x, int y, int w, int h, unsigned char *src, int srcwidth,	/* bytes */
	       int rop, unsigned int planemask, int trans, int bpp, int depth)
{
   /*
    * ErrorF("GX2WritePixmap %d %d %d %d %X %d %d %d, %d\n",
    * x, y, w,  h, src, srcwidth, bpp, depth, trans);
    */
   GFX(set_solid_pattern(planemask));

   /* USE NORMAL PATTERN ROPs IF ALL PLANES ARE ENABLED */
   GFX(set_raster_operation(windowsROPpatMask[rop & 0x0F]));
   if (trans == -1) {
      GFX(color_bitmap_to_screen_blt(0, 0, x, y, w, h, src, srcwidth));
   } else {
      GFX(color_bitmap_to_screen_xblt(0, 0, x, y, w, h, src,
				      srcwidth, trans));
   }
}

void
GX2ReadPixmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
	      unsigned char *dst, int dstwidth, int bpp, int depth)
{
   ErrorF("GX2ReadPixmap %d %d %d %d %X %d %d %d\n",
	  x, y, w, h, dst, dstwidth, bpp, depth);
}
#endif
/**************************************************************************/

/*----------------------------------------------------------------------------
 * GX2AccelInit.
 *
 * Description	:This function sets up the supported acceleration routines and
 *                 appropriate flags.
 *
 * Parameters:
 *      pScreen	:Screeen pointer structure.
 *
 * Returns		:TRUE on success and FALSE on Failure
 *
 * Comments		:This function is called in GX2ScreenInit in
                     geode_driver.c to set  * the acceleration.
*----------------------------------------------------------------------------
*/
Bool
GX2AccelInit(ScreenPtr pScreen)
{
   GeodePtr pGeode;
   ScrnInfoPtr pScreenInfo;

   pScreenInfo = xf86Screens[pScreen->myNum];
   pGeode = GEODEPTR(pScreenInfo);

   switch (pScreenInfo->bitsPerPixel) {
   case 8:
      gu2_bpp = MGP_RM_BPPFMT_332;
      break;
   case 16:
      gu2_bpp = MGP_RM_BPPFMT_565;
      break;
   case 32:
      gu2_bpp = MGP_RM_BPPFMT_8888;
      break;
   }

   gu2_xshift = pScreenInfo->bitsPerPixel >> 4;

   switch (pGeode->Pitch) {
   case 1024:
      gu2_yshift = 10;
      break;
   case 2048:
      gu2_yshift = 11;
      break;
   case 4096:
      gu2_yshift = 12;
      break;
   case 8192:
      gu2_yshift = 13;
      break;
   }

   /* Getting the pointer for acceleration Inforecord */
   pGeode->AccelInfoRec = localRecPtr = XAACreateInfoRec();

   /* SET ACCELERATION FLAGS */
   localRecPtr->Flags = PIXMAP_CACHE | OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER;

   /* HOOK SYNCRONIZARION ROUTINE */
   localRecPtr->Sync = GX2AccelSync;

   /* HOOK FILLED RECTANGLES */
   localRecPtr->SetupForSolidFill = OPTACCEL(GX2SetupForFillRectSolid);
   localRecPtr->SubsequentSolidFillRect =
	 OPTACCEL(GX2SubsequentFillRectSolid);
   localRecPtr->SolidFillFlags = 0;

   /* HOOK 8x8 Mono EXPAND PATTERNS */
   localRecPtr->SetupForMono8x8PatternFill = GX2SetupFor8x8PatternMonoExpand;
   localRecPtr->SubsequentMono8x8PatternFillRect =
	 GX2Subsequent8x8PatternMonoExpand;
   localRecPtr->Mono8x8PatternFillFlags = BIT_ORDER_IN_BYTE_MSBFIRST |
	 HARDWARE_PATTERN_PROGRAMMED_BITS | HARDWARE_PATTERN_SCREEN_ORIGIN;

   localRecPtr->SetupForColor8x8PatternFill =
	 GX2SetupFor8x8PatternColorExpand;
   localRecPtr->SubsequentColor8x8PatternFillRect =
	 GX2Subsequent8x8PatternColorExpand;
   /* Color expansion */
   localRecPtr->Color8x8PatternFillFlags =
	 BIT_ORDER_IN_BYTE_MSBFIRST |
	 SCANLINE_PAD_DWORD | HARDWARE_PATTERN_SCREEN_ORIGIN;

   /* HOOK SCREEN TO SCREEN COPIES
    * * Set flag to only allow copy if transparency is enabled.
    */
   localRecPtr->SetupForScreenToScreenCopy =
	 OPTACCEL(GX2SetupForScreenToScreenCopy);
   localRecPtr->SubsequentScreenToScreenCopy =
	 OPTACCEL(GX2SubsequentScreenToScreenCopy);
   localRecPtr->ScreenToScreenCopyFlags = 0;

   /* HOOK BRESENHAM SOLID LINES */
   /* Do not hook unless flag can be set preventing use of planemask. */
   localRecPtr->SolidLineFlags = NO_PLANEMASK;
   localRecPtr->SetupForSolidLine = OPTACCEL(GX2SetupForSolidLine);
   localRecPtr->SubsequentSolidBresenhamLine =
	 OPTACCEL(GX2SubsequentBresenhamLine);
   localRecPtr->SubsequentSolidHorVertLine =
	 OPTACCEL(GX2SubsequentHorVertLine);
   localRecPtr->SubsequentSolidTwoPointLine =
	 OPTACCEL(GX2SubsequentSolidTwoPointLine);
   localRecPtr->SolidBresenhamLineErrorTermBits = 15;

#if DASHED_SUPPORT
   localRecPtr->SetupForDashedLine = OPTACCEL(GX2SetupForDashedLine);
   localRecPtr->SubsequentDashedBresenhamLine =
	 OPTACCEL(GX2SubsequentDashedBresenhamLine);
   localRecPtr->SubsequentSolidTwoPointLine =
	 OPTACCEL(GX2SubsequentDashedTwoPointLine);
   localRecPtr->DashedBresenhamLineErrorTermBits = 15;
   localRecPtr->DashPatternMaxLength = 64;
   localRecPtr->DashedLineFlags = NO_PLANEMASK |
	 LINE_PATTERN_POWER_OF_2_ONLY | LINE_PATTERN_MSBFIRST_MSBJUSTIFIED;
#endif
#if SCR2SCREXP
   /* Color expansion */
   localRecPtr->ScreenToScreenColorExpandFillFlags =
	 BIT_ORDER_IN_BYTE_MSBFIRST | NO_TRANSPARENCY;

   localRecPtr->SetupForScreenToScreenColorExpandFill =
	 (GX2SetupForScreenToScreenColorExpandFill);
   localRecPtr->SubsequentScreenToScreenColorExpandFill =
	 (GX2SubsequentScreenToScreenColorExpandFill);
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
      /* Color expansion */
      localRecPtr->CPUToScreenColorExpandFillFlags =
	    BIT_ORDER_IN_BYTE_MSBFIRST |
	    NO_PLANEMASK | SYNC_AFTER_COLOR_EXPAND | SCANLINE_PAD_DWORD;
      localRecPtr->ColorExpandBase = pGeode->AccelImageWriteBufferOffsets[0];
      localRecPtr->ColorExpandRange = pGeode->NoOfImgBuffers << gu2_yshift;

      localRecPtr->SetupForCPUToScreenColorExpandFill =
	    OPTACCEL(GX2SetupForCPUToScreenColorExpandFill);
      localRecPtr->SubsequentCPUToScreenColorExpandFill =
	    OPTACCEL(GX2SubsequentCPUToScreenColorExpandFill);

#if IMGWRITE_SUPPORT
      localRecPtr->ImageWriteFlags = NO_PLANEMASK |
	    SCANLINE_PAD_DWORD | SYNC_AFTER_IMAGE_WRITE;
      localRecPtr->ImageWriteBase = pGeode->AccelImageWriteBufferOffsets[0];
      localRecPtr->ImageWriteRange = pGeode->NoOfImgBuffers << gu2_yshift;
      localRecPtr->SetupForImageWrite = OPTACCEL(GX2SetupForImageWrite);
      localRecPtr->SubsequentImageWriteRect =
	    OPTACCEL(GX2SubsequentImageWriteRect);
#endif /* IMGWRITE_SUPPORT */

      localRecPtr->ScanlineImageWriteFlags =
	    localRecPtr->ScreenToScreenCopyFlags;
      localRecPtr->ScanlineImageWriteBuffers =
	    pGeode->AccelImageWriteBufferOffsets;
      localRecPtr->NumScanlineImageWriteBuffers = pGeode->NoOfImgBuffers;
      localRecPtr->ImageWriteRange = pGeode->NoOfImgBuffers << gu2_yshift;
      localRecPtr->SetupForScanlineImageWrite =
	    OPTACCEL(GX2SetupForScanlineImageWrite);
      localRecPtr->SubsequentScanlineImageWriteRect =
	    OPTACCEL(GX2SubsequentScanlineImageWriteRect);
      localRecPtr->SubsequentImageWriteScanline =
	    OPTACCEL(GX2SubsequentImageWriteScanline);

      ImgBufOffset = pGeode->AccelImageWriteBufferOffsets[0] - pGeode->FBBase;
      Geodesrcy = ImgBufOffset >> gu2_yshift;

      Geodesrcx = ImgBufOffset & (pGeode->Pitch - 1);
      Geodesrcx /= (pScreenInfo->bitsPerPixel >> 3);
   } else {
      localRecPtr->PixmapCacheFlags = DO_NOT_BLIT_STIPPLES;
   }
#if 0
#if !defined(STB_X)
   localRecPtr->WriteBitmap = GX2WriteBitmap;
#endif
   localRecPtr->WritePixmap = GX2WritePixmap;
   localRecPtr->ReadPixmap = GX2ReadPixmap;
#endif

   return (XAAInit(pScreen, localRecPtr));
}

/* END OF FILE */
