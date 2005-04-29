/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_vb.h,v 1.17 2004/02/25 17:45:14 twini Exp $ */
/*
 * Video bridge detection and configuration for 300, 315 and 330 series
 * Data and prototypes
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

typedef struct _SiS_LCD_StStruct
{
	ULONG VBLCD_lcdflag;
	USHORT LCDwidth;
	USHORT LCDheight;
	USHORT LCDtype;
} SiS_LCD_StStruct;

void SISCRT1PreInit(ScrnInfoPtr pScrn);
void SISLCDPreInit(ScrnInfoPtr pScrn);
void SISTVPreInit(ScrnInfoPtr pScrn);
void SISCRT2PreInit(ScrnInfoPtr pScrn);

extern BOOLEAN SiS_GetPanelID(SiS_Private *SiS_Pr, PSIS_HW_INFO HwDeviceExtension);
extern USHORT SiS_SenseLCDDDC(SiS_Private *SiS_Pr, SISPtr pSiS);
extern USHORT SiS_SenseVGA2DDC(SiS_Private *SiS_Pr, SISPtr pSiS);
