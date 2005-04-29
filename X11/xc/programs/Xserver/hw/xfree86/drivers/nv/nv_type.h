/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_type.h,v 1.45 2004/01/06 22:47:07 mvojkovi Exp $ */

#ifndef __NV_STRUCT_H__
#define __NV_STRUCT_H__

#include "colormapst.h"
#include "vgaHW.h"
#include "xaa.h"
#include "xf86Cursor.h"
#include "xf86int10.h"

#define NV_ARCH_04  0x04
#define NV_ARCH_10  0x10
#define NV_ARCH_20  0x20
#define NV_ARCH_30  0x30


#define BITMASK(t,b) (((unsigned)(1U << (((t)-(b)+1)))-1)  << (b))
#define MASKEXPAND(mask) BITMASK(1?mask,0?mask)
#define SetBF(mask,value) ((value) << (0?mask))
#define GetBF(var,mask) (((unsigned)((var) & MASKEXPAND(mask))) >> (0?mask) )
#define SetBitField(value,from,to) SetBF(to, GetBF(value,from))
#define SetBit(n) (1<<(n))
#define Set8Bits(value) ((value)&0xff)

typedef struct {
    int bitsPerPixel;
    int depth;
    int displayWidth;
    rgb weight;
    DisplayModePtr mode;
} NVFBLayout;

typedef struct _riva_hw_state
{
    U032 bpp;
    U032 width;
    U032 height;
    U032 interlace;
    U032 repaint0;
    U032 repaint1;
    U032 screen;
    U032 scale;
    U032 dither;
    U032 extra;
    U032 pixel;
    U032 horiz;
    U032 arbitration0;
    U032 arbitration1;
    U032 pll;
    U032 pllB;
    U032 vpll;
    U032 vpll2;
    U032 vpllB;
    U032 vpll2B;
    U032 pllsel;
    U032 general;
    U032 crtcOwner;
    U032 head;
    U032 head2;
    U032 config;
    U032 cursorConfig;
    U032 cursor0;
    U032 cursor1;
    U032 cursor2;
    U032 timingH;
    U032 timingV;
} RIVA_HW_STATE, *NVRegPtr;


typedef struct {
    RIVA_HW_STATE       SavedReg;
    RIVA_HW_STATE       ModeReg;
    RIVA_HW_STATE       *CurrentState;
    CARD32              Architecture;
    CARD32              CursorStart;
    EntityInfoPtr       pEnt;
    pciVideoPtr         PciInfo;
    PCITAG              PciTag;
    int                 Chipset;
    int                 ChipRev;
    Bool                Primary;
    CARD32              IOAddress;
    unsigned long       FbAddress;
    unsigned char *     FbBase;
    unsigned char *     FbStart;
    CARD32              FbMapSize;
    CARD32              FbUsableSize;
    CARD32              ScratchBufferSize;
    CARD32              ScratchBufferStart;
    Bool                NoAccel;
    Bool                HWCursor;
    Bool                ShadowFB;
    unsigned char *     ShadowPtr;
    int                 ShadowPitch;
    CARD32              MinVClockFreqKHz;
    CARD32              MaxVClockFreqKHz;
    CARD32              CrystalFreqKHz;
    CARD32              RamAmountKBytes;

    volatile U032 *REGS;
    volatile U032 *PCRTC0;
    volatile U032 *PCRTC;
    volatile U032 *PRAMDAC0;
    volatile U032 *PFB;
    volatile U032 *PFIFO;
    volatile U032 *PGRAPH;
    volatile U032 *PEXTDEV;
    volatile U032 *PTIMER;
    volatile U032 *PMC;
    volatile U032 *PRAMIN;
    volatile U032 *FIFO;
    volatile U032 *CURSOR;
    volatile U008 *PCIO0;
    volatile U008 *PCIO;
    volatile U008 *PVIO;
    volatile U008 *PDIO0;
    volatile U008 *PDIO;
    volatile U032 *PRAMDAC;

    XAAInfoRecPtr       AccelInfoRec;
    xf86CursorInfoPtr   CursorInfoRec;
    DGAModePtr          DGAModes;
    int                 numDGAModes;
    Bool                DGAactive;
    int                 DGAViewportStatus;
    void		(*PointerMoved)(int index, int x, int y);
    ScreenBlockHandlerProcPtr BlockHandler;
    CloseScreenProcPtr  CloseScreen;
    Bool                FBDev;
    int			Rotate;
    NVFBLayout		CurrentLayout;
    /* Cursor */
    CARD32              curFg, curBg;
    CARD32              curImage[256];
    /* I2C / DDC */
    I2CBusPtr           I2C;
    xf86Int10InfoPtr    pInt;
    void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    void		(*DMAKickoffCallback)(ScrnInfoPtr);
    XF86VideoAdaptorPtr	overlayAdaptor;
    XF86VideoAdaptorPtr	blitAdaptor;
    int			videoKey;
    int			FlatPanel;
    Bool                FPDither;
    Bool                Television;
    int			CRTCnumber;
    OptionInfoPtr	Options;
    Bool                alphaCursor;
    unsigned char       DDCBase;
    Bool                twoHeads;
    Bool                twoStagePLL;
    Bool                fpScaler;
    int                 fpWidth;
    int                 fpHeight;

    CARD32              dmaPut;
    CARD32              dmaCurrent;
    CARD32              dmaFree;
    CARD32              dmaMax;
    CARD32              *dmaBase;

    CARD32              currentRop;
} NVRec, *NVPtr;

#define NVPTR(p) ((NVPtr)((p)->driverPrivate))

#endif /* __NV_STRUCT_H__ */
