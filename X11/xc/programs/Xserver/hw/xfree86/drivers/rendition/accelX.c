/*
 * file accelX.c
 *
 * accelerator functions for X
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/accelX.c,v 1.12 2002/04/04 14:05:45 eich Exp $ */




/*
 * includes
 */

#include "rendition.h"
#include "accel.h"
#include "vboard.h"
#include "vmodes.h"
#include "vos.h"
#include "v1kregs.h"
#include "v1krisc.h"
#include "v2kregs.h"
#include "cmd2d.h"
#include "vloaduc.h"

#undef DEBUG

/*
 * defines
 */

#define waitfifo(size)  do { int _c=0; \
                          while ((_c++<0xfffff)&&((verite_in8(iob+FIFOINFREE)&0x1f)<size)) /* if(!(_c%0xffff))ErrorF("#1# !0x%x! -- ",verite_in8(iob+FIFOINFREE)) */; \
                          if (_c >= 0xfffff) { \
                              ErrorF("RENDITION: Input fifo full (1) FIFO in == %d\n",verite_in8(iob+FIFOINFREE)&0x1f); \
                              return; \
                          } \
                        } while (0)

#define waitfifo2(size, rv) do { int _c=0; \
                          while ((_c++<0xfffff)&&((verite_in8(iob+FIFOINFREE)&0x1f)<size)) /* if(!(_c%0xffff))ErrorF("#2# !0x%x! -- ",verite_in8(iob+FIFOINFREE)) */; \
                          if (_c >= 0xfffff) { \
                              ErrorF("RENDITION: Input fifo full (2) FIFO in ==%d\n",verite_in8(iob+FIFOINFREE)&0x1f); \
                              RENDITIONAccelNone(pScreenInfo); \
                              pRendition->board.accel=0; \
                              return rv; \
                          } \
                        } while (0)

#define P1(x)           ((vu32)x)
#define P2(x, y)        ((((vu16)x)<<16)+((vu16)y))

/*
 * local function prototypes
 */

void RENDITIONSaveUcode(ScrnInfoPtr pScreenInfo);
void RENDITIONRestoreUcode(ScrnInfoPtr pScreenInfo);

void RENDITIONSyncV1000(ScrnInfoPtr pScreenInfo);

void RENDITIONSetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
					 int xdir, int ydir, int rop,
					 unsigned int planemask,
					 int trans_color);

void RENDITIONSubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
					   int srcX, int srcY,
					   int dstX, int dstY,
					   int w, int h);


void RENDITIONSetupForSolidFill(ScrnInfoPtr pScreenInfo,
				int color, int rop,
				unsigned int planemask);


void RENDITIONSubsequentSolidFillRect(ScrnInfoPtr pScreenInfo,
				      int x, int y, int w, int h);


void RENDITIONSubsequentTwoPointLine(ScrnInfoPtr pScreenInfo,
				     int x1, int y1,
				     int x2, int y2,
				     int bias);


/*
 * global data
 */

static int Rop2Rop[]={ 
    ROP_ALLBITS0, ROP_AND_SD, ROP_AND_SND,  ROP_S,
 /* GXclear,      GXand,      GXandReverse, GXcopy, */
    ROP_AND_NSD,   ROP_D,  ROP_XOR_SD, ROP_OR_SD,
 /* GXandInverted, GXnoop, GXxor,      GXor,  */
    ROP_NOR_SD, ROP_S,   ROP_NOT_D, ROP_NOT_S,   
 /* suppose I have some problems understanding what invert and revers means <ml>
    ROP_NOR_SD, ROP_S,   ROP_NOT_S, ROP_NOT_D,   */
 /* GXnor,      GXequiv, GXinvert,  GXorReverse, */
    ROP_NOT_S,      ROP_XNOR_SD,  ROP_NAND_SD, ROP_ALLBITS1 };
 /* GXcopyInverted, GXorInverted, GXnand,      GXset */



/*
 * functions
 */

void
RENDITIONAccelPreInit(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONAccelPreInit called\n");
    sleep(1);
#endif
#if 1
    if (RENDITIONLoadUcode(pScreenInfo)){
      ErrorF ("RENDITION: AccelPreInit - Warning. Loading of microcode failed!!\n");
    }
#endif
    pRendition->board.fbOffset += MC_SIZE;

#ifdef DEBUG
    ErrorF("RENDITION: Offset is now %d\n",pRendition->board.fbOffset);
    sleep(2);
#endif

}

void
RENDITIONAccelXAAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr  pScreenInfo = xf86Screens[pScreen->myNum];
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    XAAInfoRecPtr pXAAinfo;

    BoxRec AvailFBArea;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONAccelInit called\n");
    sleep(2);
#endif

    pRendition->AccelInfoRec = pXAAinfo = XAACreateInfoRec();

    if(!pXAAinfo){
      xf86DrvMsg(pScreenInfo->scrnIndex,X_ERROR,
		 ("Failed to set up XAA structure!\n"));
      return;
    }

    /* Here we fill in the XAA callback names */
    /* Sync is the only compulsary function   */
    pXAAinfo->Sync = RENDITIONSyncV1000;

    /* Here are the other functions & flags */
    pXAAinfo->Flags=DO_NOT_BLIT_STIPPLES|
                    LINEAR_FRAMEBUFFER|
                    PIXMAP_CACHE|
                    OFFSCREEN_PIXMAPS;

    /* screen to screen copy */
#if 1
    pXAAinfo->CopyAreaFlags=NO_TRANSPARENCY|
                            ONLY_TWO_BITBLT_DIRECTIONS;
    pXAAinfo->SetupForScreenToScreenCopy=
                RENDITIONSetupForScreenToScreenCopy;
    pXAAinfo->SubsequentScreenToScreenCopy=
                RENDITIONSubsequentScreenToScreenCopy;
#endif

#if 0
    /* solid filled rectangles */
    pXAAinfo->SetupForSolidFill=
                RENDITIONSetupForSolidFill;
    pXAAinfo->SubsequentSolidFillRect=
                RENDITIONSubsequentSolidFillRect;

    /* line */
    xf86AccelInfoRec.SubsequentTwoPointLine =  
        RENDITIONSubsequentTwoPointLine;
#endif /* #if 0 */

    verite_check_csucode(pScreenInfo);

    if (RENDITIONLoadUcode(pScreenInfo)) return;
    if (RENDITIONInitUcode(pScreenInfo)) return;

    verite_check_csucode(pScreenInfo);

    /* the remaining code was copied from s3verite_accel.c.
     * we need to check it if it is suitable <ml> */

    /* make sure offscreen pixmaps aren't bigger than our address space */
    pXAAinfo->maxOffPixWidth  = 2048;
    pXAAinfo->maxOffPixHeight = 2048;


    /*
     * Finally, we set up the video memory space available to the pixmap
     * cache. In this case, all memory from the end of the virtual screen to
     * the end of video memory minus 64K (and any other memory which we 
     * already reserved like HW-Cursor), can be used.
     */

    pXAAinfo->PixmapCacheFlags = DO_NOT_BLIT_STIPPLES;
    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScreenInfo->displayWidth;
    AvailFBArea.y2 = (((pScreenInfo->videoRam*1024) -
		       pRendition->board.fbOffset) /
		      ((pScreenInfo->displayWidth * pScreenInfo->bitsPerPixel)
		       / 8));
    xf86InitFBManager(pScreen, &AvailFBArea);

    XAAInit(pScreen, pXAAinfo);

    pRendition->board.accel=1;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONAccelInit End\n");
    sleep(2);
#endif
}



void
RENDITIONAccelNone(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    XAAInfoRecPtr pXAAinfo=pRendition->AccelInfoRec;
    
#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONAccelNone called\n");
#endif
    pXAAinfo->Flags=0;
    pXAAinfo->Sync=NULL;
    pXAAinfo->SetupForScreenToScreenCopy=NULL;
    pXAAinfo->SubsequentScreenToScreenCopy=NULL;
    pXAAinfo->SetupForSolidFill=NULL;
    pXAAinfo->SubsequentSolidFillRect=NULL;

#if 0
    pXAAinfo->SubsequentTwoPointLine=NULL;
#endif
    
    XAADestroyInfoRec(pRendition->AccelInfoRec);
    pRendition->AccelInfoRec=NULL;
}



int
RENDITIONLoadUcode(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

    static int ucode_loaded=0;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONLoadUcode called\n");
#endif

    /* load or restore ucode */
    if (!ucode_loaded) {
      if (0 != verite_initboard(pScreenInfo)) {
            RENDITIONAccelNone(pScreenInfo);
            pRendition->board.accel=0;
            return 1;
        }
        RENDITIONSaveUcode(pScreenInfo);
    }
    else
        RENDITIONRestoreUcode(pScreenInfo);

    ErrorF ("Rendition: Ucode successfully %s\n", 
		(ucode_loaded ? "restored" : "loaded"));

    ucode_loaded=1;
    return 0;
}


int
RENDITIONInitUcode(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;

    if (0 == verite_getstride(pScreenInfo, NULL,
			 &pRendition->board.mode.stride0, 
                         &pRendition->board.mode.stride1)) {
      xf86DrvMsg(pScreenInfo->scrnIndex,X_ERROR,
		   ("Acceleration for this resolution not available\n"));
        RENDITIONAccelNone(pScreenInfo);
        pRendition->board.accel=0;
        return 1;
    }
    else
      ErrorF ("Rendition: Stride 0: %d, stride 1: %d\n",
	      pRendition->board.mode.stride0, 
	      pRendition->board.mode.stride1);

    /* NOTE: for 1152x864 only! 
    stride0=6;
    stride1=1;
    */

#ifdef DEBUG
    ErrorF("#InitUcode(1)# FIFOIN_FREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
#endif

    /* init the ucode */

    /* ... and start accelerator */
    v1k_flushicache(pScreenInfo);
    v1k_start(pScreenInfo, pRendition->board.csucode_base);

#ifdef DEBUG
    ErrorF("#InitUcode(2)# FIFOIN_FREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
#endif

    verite_out32(iob, 0);     /* a0 - ucode init command */
    verite_out32(iob, 0);     /* a1 - 1024 byte context store area */
    verite_out32(iob, 0);     /* a2 */
    verite_out32(iob, pRendition->board.ucode_entry);

#ifdef DEBUG
    ErrorF("#InitUcode(3)# FIFOIN_FREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
#endif

    waitfifo2(6, 1);

#ifdef DEBUG
    ErrorF("#InitUcode(4)# FIFOIN_FREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
#endif

    verite_out32(iob, CMD_SETUP);
    verite_out32(iob, P2(pRendition->board.mode.virtualwidth,
		    pRendition->board.mode.virtualheight));
    verite_out32(iob, P2(pRendition->board.mode.bitsperpixel,
		    pRendition->board.mode.pixelformat));
    verite_out32(iob, MC_SIZE);

    verite_out32(iob, (pRendition->board.mode.virtualwidth)*
	    (pRendition->board.mode.bitsperpixel>>3));
    verite_out32(iob, (pRendition->board.mode.stride1<<12)|
	    (pRendition->board.mode.stride0<<8)); 

#ifdef DEBUG
    ErrorF("#InitUcode(5)# FIFOIN_FREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
#endif

#if 0
    verite_out32(iob+0x60, 129);
    ErrorF("RENDITION: PC at %x\n", verite_in32(iob+0x64));
#endif

    return 0;
}

void
RENDITIONRestoreUcode(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;

    vu8 memend;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONRestoreUcode called\n");
#endif

#ifdef DEBUG
    ErrorF("Restore...1\n");
    sleep(2);
#endif

    v1k_stop(pScreenInfo);
    memend=verite_in8(iob+MEMENDIAN);
    verite_out8(iob+MEMENDIAN, MEMENDIAN_NO);
#if 1
    memcpy(pRendition->board.vmem_base, pRendition->board.ucode_buffer, MC_SIZE);
#else
    /* SlowBcopy has inverted src and dst */
    xf86SlowBcopy(pRendition->board.ucode_buffer,pRendition->board.vmem_base,MC_SIZE);
#endif
    verite_out8(iob+MEMENDIAN, memend);

    v1k_flushicache(pScreenInfo);
    v1k_start(pScreenInfo, pRendition->board.csucode_base);
    verite_out32(iob, 0);     /* a0 - ucode init command */
    verite_out32(iob, 0);     /* a1 - 1024 byte context store area */
    verite_out32(iob, 0);     /* a2 */
    verite_out32(iob, pRendition->board.ucode_entry);

#if 0
    verite_out32(iob+0x60, 129);
    ErrorF("RENDITION: PC at %x\n", verite_in32(iob+0x64));
#endif
}



void
RENDITIONSaveUcode(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;
    vu8 memend;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSaveUcode called\n");
    sleep(2);
#endif

    v1k_stop(pScreenInfo);
    memend=verite_in8(iob+MEMENDIAN);
    verite_out8(iob+MEMENDIAN, MEMENDIAN_NO);

#if 1
    memcpy(pRendition->board.ucode_buffer, pRendition->board.vmem_base, MC_SIZE);
#else
    /* SlowBcopy has inverted src and dst */
    xf86SlowBcopy(pRendition->board.vmem_base,pRendition->board.ucode_buffer,MC_SIZE);
#endif
    verite_out8(iob+MEMENDIAN, memend);
    v1k_continue(pScreenInfo);
}



/*
 * local functions
 */

/*
 * synchronization -- wait for RISC and pixel engine to become idle
 */
void
RENDITIONSyncV1000(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;

    int c;

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSyncV1000 called\n");

    ErrorF("#Sync (1)# FIFO_INFREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
    ErrorF("#Sync (1)# FIFO_OUTVALID 0x%x -- \n",verite_in8(iob+FIFOOUTVALID));
#endif

    c=0;
    /* empty output fifo,
       i.e. if there is any valid data in the output fifo then read it */

    while ((c++<0xfffff) && ((verite_in8(iob+FIFOOUTVALID)&0x7)>0))
      (void)verite_in32(iob);

/*       if(!(c%0xffff))ErrorF("#F1# !0x%x! -- ",verite_in8(iob+FIFOOUTVALID)); */

    if (c >= 0xfffff) { 
        ErrorF("RENDITION: RISC synchronization failed (1) FIFO out == %d!\n",
	       verite_in8(iob+FIFOOUTVALID)&0x1f);
        return; 
    }

    /* sync RISC */
    waitfifo(2);
    verite_out32(iob, CMD_GET_PIXEL);
    verite_out32(iob, 0);

    c=0;
    while ((c++<0xfffff) && ((verite_in8(iob+FIFOOUTVALID)&0x7)>0))
      (void)verite_in32(iob);

/*       if(!(c%0xffff))ErrorF("#F2# !0x%x! -- ",verite_in8(iob+FIFOOUTVALID)); */


    if (c >= 0xfffff) { 
      ErrorF ("Rendition: RISC synchronization failed (2) FIFO out == %d!\n",
	      verite_in8(iob+FIFOOUTVALID)&0x1f);
        return; 
    }

    /* sync pixel engine using csucode -- I suppose this is quite slow <ml> */
    v1k_stop(pScreenInfo);
    v1k_start(pScreenInfo, pRendition->board.csucode_base);
    verite_out32(iob, 2);     /* a0 - sync command */

    c=0;
    while ((c++<0xfffff) && ((verite_in8(iob+FIFOOUTVALID)&0x7)>0))
      (void)verite_in32(iob);

/*      if(!(c%0xffff))ErrorF("#F3# !0x%x! -- ",verite_in8(iob+FIFOOUTVALID)); */

    if (c == 0xfffff) { 
        ErrorF ("Rendition: Pixel engine synchronization failed FIFO out == %d!\n",
		verite_in8(iob+FIFOOUTVALID)&0x1f);
        return;
    }

    /* restart the ucode */
    verite_out32(iob, 0);     /* a0 - ucode init command */
    verite_out32(iob, 0);     /* a1 - 1024 byte context store area */
    verite_out32(iob, 0);     /* a2 */
    verite_out32(iob, pRendition->board.ucode_entry);

    /* init the ucode */
    waitfifo(6);
    verite_out32(iob, CMD_SETUP);
    verite_out32(iob, P2(pRendition->board.mode.virtualwidth,
		    pRendition->board.mode.virtualheight));
    verite_out32(iob, P2(pRendition->board.mode.bitsperpixel,
		    pRendition->board.mode.pixelformat));
    verite_out32(iob, MC_SIZE);

    verite_out32(iob, pRendition->board.mode.virtualwidth  *
	    (pRendition->board.mode.bitsperpixel>>3));
    verite_out32(iob, (pRendition->board.mode.stride1<<12) |
	    (pRendition->board.mode.stride0<<8)); 
}



/*
 * screen to screen copy
 */
void
RENDITIONSetupForScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
					 int xdir, int ydir, int rop,
					 unsigned planemask, int trans_color)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSetupForScreenToScreenCopy("
        "%2d, %2d, %2d, %u, %d) called\n", xdir, ydir, rop, planemask, trans_color);
    ErrorF("RENDITION: rop is %x/%x\n", rop, Rop2Rop[rop]);
#endif

    pRendition->board.Rop=Rop2Rop[rop];
}

void
RENDITIONSubsequentScreenToScreenCopy(ScrnInfoPtr pScreenInfo,
					   int srcX, int srcY,
					   int dstX, int dstY,
					   int w, int h)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;


#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSubsequentScreenToScreenCopy("
        "%d, %d, %d, %d, %d, %d) called\n", srcX, srcY, dstX, dstY, w, h);
#endif


#if 1 /* def DEBUG */
    ErrorF("#ScreentoScreen# FIFO_INFREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
    ErrorF("#ScreentoScreen# FIFO_OUTVALID 0x%x -- \n",verite_in8(iob+FIFOOUTVALID));
#endif

    waitfifo(5);
    verite_out32(iob, CMD_SCREEN_BLT);
    verite_out32(iob, pRendition->board.Rop); 
    verite_out32(iob, P2(srcX, srcY));
    verite_out32(iob, P2(w, h));
    verite_out32(iob, P2(dstX, dstY));
}



/*
 * solid filled rectangles
 */
void
RENDITIONSetupForSolidFill(ScrnInfoPtr pScreenInfo, 
				int color, int rop,
				unsigned planemask)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSetupForSolidFill called\n");
    ErrorF("RENDITION: Rop is %x/%x\n", rop, Rop2Rop[rop]);
#endif

    pRendition->board.Rop=Rop2Rop[rop];
    pRendition->board.Color=color;
    if (pRendition->board.mode.bitsperpixel < 32)
        pRendition->board.Color|=(pRendition->board.Color<<16);
    if (pRendition->board.mode.bitsperpixel < 16)
        pRendition->board.Color|=(pRendition->board.Color<<8);
}

void
RENDITIONSubsequentSolidFillRect(ScrnInfoPtr pScreenInfo,
				      int x, int y, int w, int h)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;


#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSubsequentSolidFillRect called\n");
#endif

    waitfifo(4);
#ifdef DEBUG
    ErrorF("#SubsequentSolidFill# FIFO_INFREE 0x%x -- \n",verite_in8(iob+FIFOINFREE));
    ErrorF("#SubsequentSolidFill# FIFO_OUTVALID 0x%x -- \n",verite_in8(iob+FIFOOUTVALID));
    sleep(1);
#endif
    verite_out32(iob, P2(pRendition->board.Rop, CMD_RECT_SOLID_ROP));
    verite_out32(iob, pRendition->board.Color);
    verite_out32(iob, P2(x, y));
    verite_out32(iob, P2(w, h));
}



/*
 * line
 */

void
RENDITIONSubsequentTwoPointLine(ScrnInfoPtr pScreenInfo,
				     int x1, int y1,
				     int x2, int y2,
				     int bias)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    IOADDRESS iob = pRendition->board.io_base;


#ifdef DEBUG
    ErrorF("RENDITION: RENDITIONSubsequentTwoPointLine("
        "%d, %d, %d, %d, %d) called\n", x1, y1, x2, y2, bias);
#endif

    waitfifo(5);
    verite_out32(iob, P2(1, CMD_LINE_SOLID));
    verite_out32(iob, pRendition->board.Rop);
    verite_out32(iob, pRendition->board.Color);
    verite_out32(iob, P2(x1, y1));
    verite_out32(iob, P2(x2, y2));
}

/*
 * end of file accelX.c
 */
