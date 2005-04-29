/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_proto.h,v 1.10 2003/07/31 20:24:29 mvojkovi Exp $ */

#ifndef __NV_PROTO_H__
#define __NV_PROTO_H__

/* in nv_driver.c */
Bool   NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
void   NVAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool   NVI2CInit(ScrnInfoPtr pScrn);


/* in nv_dac.c */
Bool   NVDACInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
void   NVDACSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg,
                 NVRegPtr nvReg, Bool saveFonts);
void   NVDACRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg,
                    NVRegPtr nvReg, Bool restoreFonts);
void   NVDACLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
                        LOCO *colors, VisualPtr pVisual );
Bool   NVDACi2cInit(ScrnInfoPtr pScrn);


/* in nv_video.c */
void NVInitVideo(ScreenPtr);
void NVResetVideo (ScrnInfoPtr pScrnInfo);

/* in nv_setup.c */
void   RivaEnterLeave(ScrnInfoPtr pScrn, Bool enter);
void   NVCommonSetup(ScrnInfoPtr pScrn);

/* in nv_cursor.c */
Bool   NVCursorInit(ScreenPtr pScreen);

/* in nv_xaa.c */
Bool   NVAccelInit(ScreenPtr pScreen);
void   NVSync(ScrnInfoPtr pScrn);
void   NVResetGraphics(ScrnInfoPtr pScrn);
void   NVDmaKickoff(NVPtr pNv);
void   NVDmaWait(NVPtr pNv, int size);

/* in nv_dga.c */
Bool   NVDGAInit(ScreenPtr pScreen);

/* in riva_hw.c */
void NVCalcStateExt(NVPtr,struct _riva_hw_state *,int,int,int,int,int,int);
void NVLoadStateExt(NVPtr,struct _riva_hw_state *);
void NVUnloadStateExt(NVPtr,struct _riva_hw_state *);
void NVSetStartAddress(NVPtr,CARD32);
int  NVShowHideCursor(NVPtr,int);
void NVLockUnlock(NVPtr,int);

/* in nv_shadow.c */
void NVRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void NVRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void NVRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void NVRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void NVPointerMoved(int index, int x, int y);

#endif /* __NV_PROTO_H__ */

