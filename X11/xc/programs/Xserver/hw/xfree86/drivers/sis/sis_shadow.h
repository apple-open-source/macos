/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_shadow.h,v 1.2 2001/04/19 14:11:37 alanh Exp $ */

void SISPointerMoved(int index, int x, int y);
void SISRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SISRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
