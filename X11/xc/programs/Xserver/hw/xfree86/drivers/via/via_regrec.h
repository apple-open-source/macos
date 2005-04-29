/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_regrec.h,v 1.2 2003/08/27 15:16:13 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_REGREC_H_
#define _VIA_REGREC_H_ 1

/*#define   XV_DEBUG      1*/     /* write log msg to /var/log/XFree86.0.log */

#ifdef XV_DEBUG
# define DBG_DD(x) (x)
#else
# define DBG_DD(x)
#endif

#define VIDREGREC_RESET_COUNTER   0
#define VIDREGREC_SAVE_REGISTER   VIDREGREC_RESET_COUNTER +1
#define VIDREGREC_FLUSH_REGISTER  VIDREGREC_RESET_COUNTER +2
#define VIDEO_REG_NUM  100

#define IN_HQV_FIRE     (*((unsigned long volatile *)(pVia->VidMapBase+HQV_CONTROL))&HQV_IDLE)
#define IN_VIDEO_FIRE   (*((unsigned long volatile *)(pVia->VidMapBase+V_COMPOSE_MODE))&V1_COMMAND_FIRE)
#define IN_HQV_FLIP     (*((unsigned long volatile *)(pVia->VidMapBase+HQV_CONTROL))&HQV_FLIP_STATUS) 
#define IN_VIDEO_DISPLAY     (*((unsigned long volatile *)(pVia->VidMapBase+V_FLAGS))&VBI_STATUS) 

/*#define IN_DISPLAY      (VIDInD(V_FLAGS) & 0x200)
//#define IN_VBLANK       (!IN_DISPLAY)
*/
typedef struct
{
  unsigned long dwIndex;
  unsigned long dwData;
}VIDEOREGISTER;

void viaWaitHQVIdle(VIAPtr pVia);
void viaWaitVideoCommandFire(VIAPtr pVia);
void viaWaitHQVFlip(VIAPtr pVia);
void viaWaitHQVFlipClear(VIAPtr pVia, unsigned long dwData);
void viaWaitVBI(VIAPtr pVia);
void viaWaitHQVDone(VIAPtr pVia);
void viaMacro_VidREGFlush(VIAPtr pVia);
void viaMacro_VidREGRec(VIAPtr pVia, unsigned long dwAction, unsigned long dwIndex, unsigned long dwData);
void viaMacro_VidREGFlushVPE(VIAPtr pVia);
void viaMacro_VidREGRecVPE(VIAPtr pVia, unsigned long dwAction, unsigned long dwIndex, unsigned long dwData);

#endif /* _VIA_REGREC_H_ */
