/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dac.h,v 1.20 2004/02/25 17:45:13 twini Exp $ */
/*
 * DAC helper functions (Save/Restore, MemClk, etc)
 * Definitions and prototypes
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria.
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
 */

int  SiS_compute_vclk(int Clock, int *out_n, int *out_dn, int *out_div,
	     		int *out_sbit, int *out_scale);
void SISDACPreInit(ScrnInfoPtr pScrn);
void SISLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
		        LOCO *colors, VisualPtr pVisual);
void SiSCalcClock(ScrnInfoPtr pScrn, int clock, int max_VLD,
                        unsigned int *vclk);
void SiSIODump(ScrnInfoPtr pScrn);
int  SiSMemBandWidth(ScrnInfoPtr pScrn, BOOLEAN IsForCRT2);
int  SiSMclk(SISPtr pSiS);
void SiSRestoreBridge(ScrnInfoPtr pScrn, SISRegPtr sisReg);

extern void     SiS6326SetTVReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data);
extern unsigned char SiS6326GetTVReg(ScrnInfoPtr pScrn, CARD8 index);
extern void     SiS6326SetXXReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data);
extern unsigned char SiS6326GetXXReg(ScrnInfoPtr pScrn, CARD8 index);

extern int      SiSCalcVRate(DisplayModePtr mode);

/* Functions from init.c and init301.c */
extern void     SiS_UnLockCRT2(SiS_Private *SiS_Pr, PSIS_HW_INFO);
extern void     SiS_LockCRT2(SiS_Private *SiS_Pr, PSIS_HW_INFO);
extern void     SiS_DisableBridge(SiS_Private *SiS_Pr, PSIS_HW_INFO);
extern void     SiS_EnableBridge(SiS_Private *SiS_Pr, PSIS_HW_INFO);
extern USHORT 	SiS_GetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT 	SiS_GetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT 	SiS_GetCH70xx(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH70xx(SiS_Private *SiS_Pr, USHORT tempbx);
extern void     SiS_SetCH70xxANDOR(SiS_Private *SiS_Pr, USHORT tempax,USHORT tempbh);
extern void     SiS_SetTrumpReg(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT   SiS_GetTrumpReg(SiS_Private *SiS_Pr, USHORT tempbx);
extern void     SiS_DDC2Delay(SiS_Private *SiS_Pr, USHORT delaytime);
extern USHORT   SiS_ReadDDC1Bit(SiS_Private *SiS_Pr);
extern USHORT   SiS_HandleDDC(SiS_Private *SiS_Pr, unsigned long VBFlags, int VGAEngine,
                              USHORT adaptnum, USHORT DDCdatatype, unsigned char *buffer);
extern void     SiS_SetChrontelGPIO(SiS_Private *SiS_Pr, USHORT myvbinfo);
extern void     SiS_DisplayOn(SiS_Private *SiS_Pr);
extern unsigned char SiS_GetSetModeID(ScrnInfoPtr pScrn, unsigned char id);
extern void     SiS_SetEnableDstn(SiS_Private *SiS_Pr, int enable);
extern void     SiS_SetEnableFstn(SiS_Private *SiS_Pr, int enable);
