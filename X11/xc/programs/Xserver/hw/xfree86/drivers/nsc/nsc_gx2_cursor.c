/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx2_cursor.c,v 1.5 2003/02/21 16:51:09 alanh Exp $ */
/*
 * $Workfile: nsc_gx2_cursor.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * File Contents: Xfree cursor implementation routines
 *                for geode HWcursor init.setting cursor color,image etc
 *                are done here.
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

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "nsc.h"

/* Forward declarations of the functions */
Bool GX2HWCursorInit(ScreenPtr pScreen);
static void GX2SetCursorColors(ScrnInfoPtr pScreenInfo, int bg, int fg);
static void GX2SetCursorPosition(ScrnInfoPtr pScreenInfo, int x, int y);
void GX2LoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char *src);
void GX2HideCursor(ScrnInfoPtr pScreenInfo);
void GX2ShowCursor(ScrnInfoPtr pScreenInfo);
static Bool GX2UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs);
extern void GX2SetVideoPosition(int x, int y, int width, int height,
				short src_w, short src_h, short drw_w,
				short drw_h, int id, int offset,
				ScrnInfoPtr pScrn);

/*----------------------------------------------------------------------------
 * GX2HWCursorInit.
 *
 * Description	:This function sets the cursor information by probing the
 * hardware.
 *
 * Parameters.
 *     pScreen	:Screeen pointer structure.
 *
 * Returns		:TRUE on success and FALSE on Failure
 *
 * Comments		:Geode supports the hardware_cursor,no need to enable SW
 *                    cursor.
*----------------------------------------------------------------------------
*/
Bool
GX2HWCursorInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[pScreen->myNum];
   GeodePtr pGeode = GEODEPTR(pScreenInfo);
   xf86CursorInfoPtr infoPtr;

   infoPtr = xf86CreateCursorInfoRec();
   if (!infoPtr)
      return FALSE;
   /* the geode structure is intiallized with the cursor infoRec */
   pGeode->CursorInfo = infoPtr;
   infoPtr->MaxWidth = 32;
   infoPtr->MaxHeight = 32;
   /* seeting up the cursor flags */
   infoPtr->Flags = HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
	 HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
	 HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED;
   /* cursor info ptr is intiallized with the values obtained from
    * * durnago calls
    */
   infoPtr->SetCursorColors = GX2SetCursorColors;
   infoPtr->SetCursorPosition = GX2SetCursorPosition;
   infoPtr->LoadCursorImage = GX2LoadCursorImage;
   infoPtr->HideCursor = GX2HideCursor;
   infoPtr->ShowCursor = GX2ShowCursor;
   infoPtr->UseHWCursor = GX2UseHWCursor;
   return (xf86InitCursor(pScreen, infoPtr));
}

/*----------------------------------------------------------------------------
 * GX2SetCursorColors.
 *
 * Description	:This function sets the cursor foreground and background
 *                    colors
 * Parameters:
 *    pScreen:	Screeen pointer structure.
 *    	   bg:	Specifies the color value of cursor background color.
 *    	   fg:	Specifies the color value of cursor foreground color.
 *    Returns:	none.
 *
 *   Comments:	The integer color value passed by this function is
 *              converted into  * RGB  value by the gfx_set_color routines.
 *----------------------------------------------------------------------------
 */
static void
GX2SetCursorColors(ScrnInfoPtr pScreenInfo, int bg, int fg)
{
   GFX(set_cursor_colors(bg, fg));
}

/*----------------------------------------------------------------------------
 * GX2SetCursorPosition.
 *
 * Description	:This function sets the cursor co -ordinates and enable the
 *               cursor.
 *
 * Parameters:
 *    pScreen: Screeen pointer structure.
 *    	    x: Specifies the x-cordinates of the cursor.
 *    	    y: Specifies the y co-ordinate of the cursor.
 *    Returns: none.
 *
 *----------------------------------------------------------------------------
 */
static void
GX2SetCursorPosition(ScrnInfoPtr pScreenInfo, int x, int y)
{
   unsigned long offset;
   static int panOffset = 0;
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   unsigned short xhot = 0, yhot = 0;

   if (x < 0) {
      xhot = (unsigned short)(-x);
      x = 0;
   }
   if (y < 0) {
      yhot = (unsigned short)(-y);
      y = 0;
   }

   GFX(set_cursor_position(pGeode->CursorStartOffset, x, y, xhot, yhot));
   GFX(set_cursor_enable(1));

   if ((pGeode->OverlayON) && (pGeode->Panel)) {
#if defined(STB_X)
      Gal_get_display_offset(&offset);
#else
      offset = gfx_get_display_offset();
#endif
      if (offset != panOffset) {
	 GX2SetVideoPosition(pGeode->video_x, pGeode->video_y,
			     pGeode->video_w, pGeode->video_h,
			     pGeode->video_srcw, pGeode->video_srch,
			     pGeode->video_dstw, pGeode->video_dsth,
			     pGeode->video_id, pGeode->video_offset,
			     pGeode->video_scrnptr);
	 panOffset = offset;
      }
   }
}

/*----------------------------------------------------------------------------
 * GX2LoadCursorImage
 *
 * Description	:This function loads the 32x32 cursor pattern.The shape
 *               and color is set by AND and XOR masking of arrays of 32
 *               DWORD.
 * Parameters:
 *    pScreen: Screeen pointer structure.
 *    src    : Specifies cursor data.
 * Returns   : none
 *
 *----------------------------------------------------------------------------
*/
void
GX2LoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char *src)
{
   int i, j;
   unsigned long shape;
   unsigned long mask;
   unsigned long andMask[32] = { 0, };
   unsigned long xorMask[32] = { 0, };
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   j = 0;
   for (i = 0; i < 32; i++) {
      if (src) {
	 shape = ((unsigned long)src[i * 4] << 24) |
	       ((unsigned long)src[i * 4 + 1] << 16) |
	       ((unsigned long)src[i * 4 + 2] << 8) |
	       ((unsigned long)src[i * 4 + 3] << 0);
	 mask = ((unsigned long)src[i * 4 + 128] << 24) |
	       ((unsigned long)src[i * 4 + 1 + 128] << 16) |
	       ((unsigned long)src[i * 4 + 2 + 128] << 8) |
	       ((unsigned long)src[i * 4 + 3 + 128] << 0);
      } else {
	 mask = 0x0;
	 shape = 0xFFFFFFFF;
      }

      andMask[i] = ~(mask);
      xorMask[i] = shape & mask;
   }

   GFX(set_cursor_shape32(pGeode->CursorStartOffset, andMask, xorMask));
}

/*----------------------------------------------------------------------------
 * GX2HideCursor.
 *
 * Description	:This function will disable the cursor.
 *
 * Parameters:
 *    pScreen: Handles to the Screeen pointer structure.
 *
 *    Returns: none.
 *
 *   Comments:	gfx_set_cursor enable function is hardcoded to disable
 *		the cursor.
 *----------------------------------------------------------------------------
 */
void
GX2HideCursor(ScrnInfoPtr pScreenInfo)
{
   GFX(set_cursor_enable(0));
}

/*----------------------------------------------------------------------------
 * GX2ShowCursor
 *
 * Description	:This function will enable  the cursor.
 *
 * Parameters:
 *	pScreen		:Handles to the Screeen pointer structure.
 *
 * Returns      :none
 *
 * Comments		:gfx_set_cursor enable function is hardcoded to enable the
 * 											cursor
 *----------------------------------------------------------------------------
*/
void
GX2ShowCursor(ScrnInfoPtr pScreenInfo)
{
   GFX(set_cursor_enable(1));
}

/*----------------------------------------------------------------------------
 * GX2UseHwCursor.
 *
 * Description	:This function will sets the hardware cursor flag in
 *                 pscreen  structure.
 *
 * Parameters.
 *	pScreen		:Handles to the Screeen pointer structure.
 *
 * Returns		:none
 *
 * Comments		:none
 *
 *----------------------------------------------------------------------------
*/
static Bool
GX2UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
   ScrnInfoPtr pScreenInfo = XF86SCRNINFO(pScreen);

   if (pScreenInfo->currentMode->Flags & V_DBLSCAN)
      return FALSE;
   return TRUE;
}

/* End of File */
