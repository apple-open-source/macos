/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dac.h,v 1.7 2003/01/29 15:42:17 eich Exp $ */

int SiS_compute_vclk(int Clock, int *out_n, int *out_dn, int *out_div,
	     			    int *out_sbit, int *out_scale);
void SISDACPreInit(ScrnInfoPtr pScrn);
unsigned int SiSddc1Read(ScrnInfoPtr pScrn);
void SISLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
		                LOCO *colors, VisualPtr pVisual);
void SiSCalcClock(ScrnInfoPtr pScrn, int clock, int max_VLD,
                        unsigned int *vclk);

void SiSIODump(ScrnInfoPtr pScrn);
int  SiSMemBandWidth(ScrnInfoPtr pScrn);
int  SiSMclk(SISPtr pSiS);
void SiSRestoreBridge(ScrnInfoPtr pScrn, SISRegPtr sisReg);

extern void     SiS6326SetTVReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data);
extern unsigned char SiS6326GetTVReg(ScrnInfoPtr pScrn, CARD8 index);
extern void     SiS6326SetXXReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data);
extern unsigned char SiS6326GetXXReg(ScrnInfoPtr pScrn, CARD8 index);

extern int      SiSCalcVRate(DisplayModePtr mode);

/* TW: Functions from init.c & init301.c */
extern void     SiS_UnLockCRT2(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO,USHORT BaseAddr);
extern void     SiS_LockCRT2(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO,USHORT BaseAddr);
extern void     SiS_DisableBridge(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO,USHORT  BaseAddr);
extern void     SiS_EnableBridge(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO,USHORT BaseAddr);
extern USHORT 	SiS_GetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH700x(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT 	SiS_GetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH701x(SiS_Private *SiS_Pr, USHORT tempbx);
extern USHORT 	SiS_GetCH70xx(SiS_Private *SiS_Pr, USHORT tempbx);
extern void 	SiS_SetCH70xx(SiS_Private *SiS_Pr, USHORT tempbx);
extern void     SiS_SetCH70xxANDOR(SiS_Private *SiS_Pr, USHORT tempax,USHORT tempbh);
extern void     SiS_DDC2Delay(SiS_Private *SiS_Pr, USHORT delaytime);
extern USHORT   SiS_HandleDDC(SiS_Private *SiS_Pr, SISPtr pSiS, USHORT adaptnum,
                              USHORT DDCdatatype, unsigned char *buffer);
extern void     SiS_WhatIsThis(SiS_Private *SiS_Pr, USHORT myvbinfo);
extern void     SiS_DisplayOn(SiS_Private *SiS_Pr);
extern unsigned char SiS_GetSetModeID(ScrnInfoPtr pScrn, unsigned char id);
#if 0
extern void     SiS_SetSwitchDDC2(SiS_Private *SiS_Pr);
extern USHORT   SiS_I2C_GetByte(SiS_Private *SiS_Pr);
extern Bool     SiS_I2C_PutByte(SiS_Private *SiS_Pr, USHORT data);
extern Bool     SiS_I2C_Address(SiS_Private *SiS_Pr, USHORT addr);
extern void     SiS_I2C_Stop(SiS_Private *SiS_Pr);
#endif





