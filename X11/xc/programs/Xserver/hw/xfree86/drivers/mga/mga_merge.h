/* Merged mode stuff */
/* include file for mga_driver.c  DO NOT try to generate a .o with this file.*/

void		MGADisplayPowerManagementSetMerged(ScrnInfoPtr pScrn, 
					     int PowerManagementMode,
				             int flags);
void	        MGAMergePointerMoved(int scrnIndex, int x, int y);
void            MGAAdjustMergeFrames(int scrnIndex, int x, int y, int flags);
Bool            MGAPreInitMergedFB(ScrnInfoPtr pScrn1, int flags);
Bool		MGACloseScreenMerged(int scrnIndex, ScreenPtr pScreen);
Bool		MGASaveScreenMerged(ScreenPtr pScreen, int mode);

typedef struct _MergedDisplayModeRec {
    DisplayModePtr Monitor1;
    DisplayModePtr Monitor2;
    MgaScrn2Rel    Monitor2Pos; 
} MergedDisplayModeRec, *MergedDisplayModePtr;

#define MDMPTR(x) ((MergedDisplayModePtr)(x->currentMode->Private))
