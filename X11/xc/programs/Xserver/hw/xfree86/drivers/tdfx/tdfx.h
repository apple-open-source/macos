/*
   Voodoo Banshee driver version 1.0.2

   Author: Daryll Strauss

   Copyright: 1998,1999
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx.h,v 1.27 2002/10/17 01:02:08 dawes Exp $ */

#ifndef _TDFX_H_
#define _TDFX_H_

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86xv.h"
#include "tdfxdefs.h"

#ifdef XF86DRI
#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "xf86dri.h"
#include "dri.h"
#include "GL/glxint.h"
#include "tdfx_dri.h"
#include "tdfx_dripriv.h"
#endif

#define TDFX_VERSION 4000
#define TDFX_NAME "TDFX"
#define TDFX_DRIVER_NAME "tdfx"
#define TDFX_MAJOR_VERSION 1
#define TDFX_MINOR_VERSION 0
#define TDFX_PATCHLEVEL 0

struct _TDFXRec;
typedef struct _TDFXRec *TDFXPtr;

#include "tdfx_priv.h"
extern void TDFXSwapContextFifo(ScreenPtr pScreen);
extern void TDFXLostContext(ScreenPtr pScreen);
extern Bool TDFXSetupSLI(ScrnInfoPtr pScrn, Bool sliEnable, int aaSamples);
extern Bool TDFXDisableSLI(TDFXPtr pTDFX);

#ifdef XF86DRI
extern void FillPrivateDRI(TDFXPtr pTDFX, TDFXDRIPtr pTDFXDRI);
#endif

#if 0
/* These are not normally turned on. They are only included for debugging. */
#define TRACE 1
#define TRACEACCEL 1
#define TDFX_DEBUG_CMDS
#define TRACECURS 1
#define REGDEBUG 1
#define TRACEREG
#endif

#ifdef TRACE
#define TDFXTRACE ErrorF
#else
#define TDFXTRACE if(0) ErrorF
#endif

#ifdef TRACEACCEL
#define TDFXTRACEACCEL ErrorF
#else
#define TDFXTRACEACCEL if(0) ErrorF
#endif

#ifdef TRACECURS
#define TDFXTRACECURS ErrorF
#else
#define TDFXTRACECURS if(0) ErrorF
#endif

#ifdef TRACEREG
#define TDFXTRACEREG ErrorF
#else
#define TDFXTRACEREG if(0) ErrorF
#endif

#include "xaa.h"
#include "xf86Cursor.h"

typedef void (*TDFXWriteIndexedByteFunc)(TDFXPtr pTDFX, int addr, 
					 char index, char value);
typedef char (*TDFXReadIndexedByteFunc)(TDFXPtr pTDFX, int addr, 
					char index);
typedef void (*TDFXWriteWordFunc)(TDFXPtr pTDFX, int addr, int value);
typedef int (*TDFXReadWordFunc)(TDFXPtr pTDFX, int addr);
typedef void (*TDFXWriteChipIndexedByteFunc)(TDFXPtr pTDFX, int chip, 
					     int addr, char index, char value);
typedef char (*TDFXReadChipIndexedByteFunc)(TDFXPtr pTDFX, int chip, 
					    int addr, char index);
typedef void (*TDFXWriteChipWordFunc)(TDFXPtr pTDFX, int chip, 
				      int addr, int value);
typedef int (*TDFXReadChipWordFunc)(TDFXPtr pTDFX, int chip,
				    int addr);
typedef void (*TDFXSyncFunc)(ScrnInfoPtr pScrn);
typedef void (*TDFXBufferFunc)(TDFXPtr pTDFX, int which);
#if X_BYTE_ORDER == X_BIG_ENDIAN
typedef void (*TDFXWriteFifoFunc)(TDFXPtr pTDFX, int val);
#endif


typedef struct {
  unsigned int vidcfg;
  unsigned int vidpll;
  unsigned int dacmode;
  unsigned int vgainit0;
  unsigned int vgainit1;
  unsigned int miscinit0;
  unsigned int miscinit1;
  unsigned int screensize;
  unsigned int stride;
  unsigned int cursloc;
  unsigned int startaddr;
  unsigned int clip0min;
  unsigned int clip0max;
  unsigned int clip1min;
  unsigned int clip1max;
  unsigned int srcbaseaddr;
  unsigned int dstbaseaddr;
  unsigned char ExtVga[2];
  unsigned int dactable[512];
} TDFXRegRec, *TDFXRegPtr;

typedef struct TextureData_t {
  int contextid;
  void *data;
  struct TextureData_t *next;
} TextureData;

#define MAXCHIPS 4

typedef struct _TDFXRec {
  unsigned char *MMIOBase[MAXCHIPS];
  unsigned char *FbBase;
  unsigned char *myFbBase;
  IOADDRESS PIOBase[MAXCHIPS];
  long FbMapSize;
  int pixelFormat;
  int stride;
  int cpp;
  int maxClip;
  int MaxClock;
  int ChipType;
  pciVideoPtr PciInfo;
  unsigned long LinearAddr[MAXCHIPS];
  unsigned long MMIOAddr[MAXCHIPS];
  EntityInfoPtr pEnt;
  int numChips;
  PCITAG PciTag[MAXCHIPS];
  Bool Primary;
  int HasSGRAM;
  int PciCnt;
  int PrevDrawState;
  int DrawState;
  int Cmd;
  int DashedLineSize;
  BoxRec prevBlitDest;
  TDFXRegRec SavedReg;
  TDFXRegRec ModeReg;
  XAAInfoRecPtr AccelInfoRec;
  xf86CursorInfoPtr CursorInfoRec;
  CloseScreenProcPtr CloseScreen;
  Bool usePIO;
  Bool NoAccel;
  DGAModePtr DGAModes;
  Bool DGAactive;
  Bool initDone;
  int DGAViewportStatus;
  int cursorOffset;
  int fbOffset;
  int backOffset;
  int depthOffset;
  int texOffset;
  int texSize;
  TDFXWriteIndexedByteFunc writeControl;
  TDFXReadIndexedByteFunc readControl;
  TDFXWriteWordFunc writeLong;
  TDFXReadWordFunc readLong;
  TDFXWriteChipWordFunc writeChipLong;
  TDFXReadChipWordFunc readChipLong;
  TDFXSyncFunc sync;
#if X_BYTE_ORDER == X_BIG_ENDIAN
  TDFXWriteFifoFunc writeFifo;
#endif
  int syncDone;
  int scanlineWidth;
  unsigned char *scanlineColorExpandBuffers[2];
  PROPDATA
#ifdef XF86DRI
  Bool directRenderingEnabled;
  DRIInfoPtr pDRIInfo;
  int drmSubFD;
  int numVisualConfigs;
  __GLXvisualConfig* pVisualConfigs;
  TDFXConfigPrivPtr pVisualConfigsPriv;
  TDFXRegRec DRContextRegs;
#endif
  /* State for video */
  FBAreaPtr offscreenYUVBuf;
  int offscreenYUVBufWidth;
  int offscreenYUVBufHeight;
  
  /* This is a small register shadow.  I'm only shadowing 
   *   sst2dDstFmt
   *   sst2dSrcFmt
   * If a real register shadow is ever needed we should probably
   * shadow everything and make it happen automatically for every write. */
  INT32 sst2DSrcFmtShadow;
  INT32 sst2DDstFmtShadow;
  int pixmapCacheLinesMin;
  int pixmapCacheLinesMax;
  FBAreaPtr reservedArea;
  Bool ShowCache;
  int videoKey;
  void (*VideoTimerCallback)(ScrnInfoPtr, Time);
  FBLinearPtr overlayBuffer;
  FBLinearPtr overlayBuffer2;	/* for double-buffering */
  int whichOverlayBuffer;	/* flip-flop */
  FBAreaPtr textureBuffer;
  Bool TextureXvideo;
  XF86VideoAdaptorPtr overlayAdaptor;
  XF86VideoAdaptorPtr textureAdaptor;
  ScreenBlockHandlerProcPtr BlockHandler;
  OptionInfoPtr Options;
} TDFXRec;

typedef struct {
  RegionRec	clip;
  CARD32	colorKey;
  int		filterQuality;
  CARD32	videoStatus;
  Time		offTime;
  Time		freeTime;
} TDFXPortPrivRec, *TDFXPortPrivPtr;

typedef struct {
  PROPSAREADATA
  int fifoOwner;
  int CtxOwner;
  int TexOwner;
} TDFXSAREAPriv;

#define TDFXPTR(p) ((TDFXPtr)((p)->driverPrivate))

#define DRAW_STATE_CLIPPING 0x1
#define DRAW_STATE_TRANSPARENT 0x2
#define DRAW_STATE_CLIP1CHANGED 0x4

#define TDFX_FRONT 0
#define TDFX_BACK 1
#define TDFX_DEPTH 2

#define TDFX2XCUTOFF 135000

extern Bool TDFXAccelInit(ScreenPtr pScreen);
extern Bool TDFXCursorInit(ScreenPtr pScreen);
extern void TDFXSync(ScrnInfoPtr pScrn);
extern Bool TDFXDRIScreenInit(ScreenPtr pScreen);
extern void TDFXDRICloseScreen(ScreenPtr pScreen);
extern Bool TDFXDRIFinishScreenInit(ScreenPtr pScreen);
extern Bool TDFXDGAInit(ScreenPtr pScreen);
extern void TDFXCursorGrabMemory(ScreenPtr pScreen);
extern void TDFXSetLFBConfig(TDFXPtr pTDFX);
extern void TDFXSendNOPFifo(ScrnInfoPtr pScrn);

extern Bool TDFXSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
extern void TDFXAdjustFrame(int scrnIndex, int x, int y, int flags);

extern void TDFXSetPIOAccess(TDFXPtr pTDFX);
extern void TDFXSetMMIOAccess(TDFXPtr pTDFX);
extern void TDFXWriteLongMMIO(TDFXPtr pTDFX, int addr, int val);
extern int TDFXReadLongMMIO(TDFXPtr pTDFX, int addr);
extern void TDFXWriteChipLongMMIO(TDFXPtr pTDFX, int chip, int addr, int val);

extern void TDFXNeedSync(ScrnInfoPtr pScrn);
extern void TDFXCheckSync(ScrnInfoPtr pScrn);
extern void TDFXFirstSync(ScrnInfoPtr pScrn);

extern void TDFXSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, 
					   int ydir, int rop,
					   unsigned int planemask, 
					   int trans_color);
extern void TDFXSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int srcX, 
					     int srcY, int dstX, int dstY, 
					     int w, int h);
extern void TDFXSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, 
				  unsigned int planemask);
extern void TDFXSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, 
					int w, int h);

extern void TDFXSelectBuffer(TDFXPtr pTDFX, int which);

extern void TDFXInitVideo(ScreenPtr pScreen);
extern void TDFXCloseVideo(ScreenPtr pScreen);

#endif
