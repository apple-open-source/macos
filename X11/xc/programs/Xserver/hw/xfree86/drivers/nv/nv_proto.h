/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_proto.h,v 1.8 2002/11/26 23:41:59 mvojkovi Exp $ */

#ifndef __NV_PROTO_H__
#define __NV_PROTO_H__

/* in nv_driver.c */
Bool    NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
void    NVAdjustFrame(int scrnIndex, int x, int y, int flags);
xf86MonPtr NVdoDDC(ScrnInfoPtr pScrn);


/* in nv_dac.c */
void    NVRamdacInit(ScrnInfoPtr pScrn);
Bool    NVDACInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
void    NVDACSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg,
                  NVRegPtr nvReg, Bool saveFonts);
void    NVDACRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg,
                     NVRegPtr nvReg, Bool restoreFonts);
void    NVDACLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
                         LOCO *colors, VisualPtr pVisual );

/* in nv_video.c */
void NVInitVideo(ScreenPtr);
void NVResetVideo (ScrnInfoPtr pScrnInfo);

/* in nv_setup.c */
void    RivaEnterLeave(ScrnInfoPtr pScrn, Bool enter);
void    NV1Setup(ScrnInfoPtr pScrn);
void    NV3Setup(ScrnInfoPtr pScrn);
void    NV4Setup(ScrnInfoPtr pScrn);
void    NV10Setup(ScrnInfoPtr pScrn);
void    NV20Setup(ScrnInfoPtr pScrn);

/* in nv_cursor.c */
Bool    NVCursorInit(ScreenPtr pScreen);

/* in nv_xaa.c */
Bool    NVAccelInit(ScreenPtr pScreen);
void    NVSync(ScrnInfoPtr pScrn);
void    NVResetGraphics(ScrnInfoPtr pScrn);

/* in nv_dga.c */
Bool    NVDGAInit(ScreenPtr pScreen);

#endif /* __NV_PROTO_H__ */

