/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_priv.c,v 1.16 2001/05/21 21:43:55 dawes Exp $ */


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"
#include "compiler.h"
#include "tdfx.h"

/*
  Memory layout of card is as follows:

  000000-00ffff: VGA memory
  010000-013fff: Cursor 
  011000-xxxxxx: Fifo (Min of CMDFIFO pages)
  xxxxxx- A-1  : Front Buffer (framebuffer)
   A    - B-1  : Pixmap Cache (framebuffer)
   B    - C-1  : Texture Memory
   C    - D-1  : Back Buffer
   D    - E-1  : Depth Buffer

  NB: pixmap cache usually butts right up against texture memory. when
  3d is disabled (via Transition2D) then the pixmap cache is increased
  to overlap the texture memory. maximum pixmap cache of 4095 lines on
  voodoo5 and 2048 on voodoo3/4 applies.
*/

#if X_BYTE_ORDER == X_BIG_ENDIAN
void TDFXWriteFifo_24(TDFXPtr pTDFX, int val) {
  *pTDFX->fifoPtr++ = val;
}
 
void TDFXWriteFifo_16(TDFXPtr pTDFX, int val) {
  *pTDFX->fifoPtr++ = BE_WSWAP32(val);
}

void TDFXWriteFifo_8(TDFXPtr pTDFX, int val) {
  *pTDFX->fifoPtr++ = BE_BSWAP32(val);
}
#endif


static void TDFXSendNOPFifo3D(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  pTDFX=TDFXPTR(pScrn);
  TDFXAllocateSlots(pTDFX, 2);
  SET_3DPK4_HEADER(1, 0x48<<3);
  WRITE_FIFO(pTDFX, 0, 0);
}

void TDFXSendNOPFifo2D(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  pTDFX=TDFXPTR(pScrn);
  if (!pTDFX->syncDone) {
    TDFXFirstSync(pScrn);
    return;
  }
  TDFXAllocateSlots(pTDFX, 2);
  SET_PKT2_HEADER(SSTCP_COMMAND);
  WRITE_FIFO(pTDFX, SST_2D_COMMAND, SST_2D_NOP|SST_2D_GO);
}  

void TDFXSendNOPFifo(ScrnInfoPtr pScrn)
{
  TDFXSendNOPFifo2D(pScrn);
  TDFXSendNOPFifo3D(pScrn);
}

static void InstallFifo(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  pTDFX=TDFXPTR(pScrn);
  /* Install the fifo */
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_BASEADDR0, pTDFX->fifoOffset>>12);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_BUMP0, 0);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_RDPTRL0, pTDFX->fifoOffset);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_RDPTRH0, 0);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_AMIN0, pTDFX->fifoOffset-4);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_AMAX0, pTDFX->fifoOffset-4);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_DEPTH0, 0);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_HOLECNT0, 0);
  if (pTDFX->ChipType == PCI_CHIP_BANSHEE) 
    TDFXWriteLongMMIO(pTDFX, SST_FIFO_FIFOTHRESH, (0x9<<5) | 0x2);
  else 
    TDFXWriteLongMMIO(pTDFX, SST_FIFO_FIFOTHRESH, (0xf<<5) | 0x8);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_BASESIZE0, ((pTDFX->fifoSize>>12)-1) | 
		    SST_EN_CMDFIFO);
  /* Set the internal state */
  pTDFX->fifoRead = pTDFX->fifoBase;
  pTDFX->fifoPtr = pTDFX->fifoBase;
  pTDFX->fifoSlots = (pTDFX->fifoSize>>2) - 1;
  pTDFX->fifoEnd = pTDFX->fifoBase+pTDFX->fifoSlots;
  TDFXSendNOPFifo(pScrn);
}

static void TDFXResetFifo(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  int oldValue;
  long start_sec, end_sec, dummy;

  pTDFX=TDFXPTR(pScrn);
  ErrorF("Resetting FIFO\n");
  /* Shut down the fifo */
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_BASESIZE0, 0);
  /* Toggle the reset bits */
  oldValue=TDFXReadLongMMIO(pTDFX, MISCINIT0);
  TDFXWriteLongMMIO(pTDFX, MISCINIT0, oldValue|0x23);
  getsecs(&start_sec, &dummy);
  do {
    getsecs(&end_sec, &dummy);
  } while (end_sec-start_sec<2);
  TDFXWriteLongMMIO(pTDFX, MISCINIT0, oldValue);
  oldValue=TDFXReadLongMMIO(pTDFX, MISCINIT1);
  TDFXWriteLongMMIO(pTDFX, MISCINIT1, oldValue|BIT(19));
  getsecs(&start_sec, &dummy);
  do {
    getsecs(&end_sec, &dummy);
  } while (end_sec-start_sec<2);
  TDFXWriteLongMMIO(pTDFX, MISCINIT1, oldValue);
  InstallFifo(pScrn);
}

/*
  There are two steps to fully syncing the board:
  
  1) Send a NOP, which waits for any commands in progress
  2) Read the status register for 3 consecutive non-busy

  !!! We should use expedential backoff in our reads !!!
*/
static void TDFXSyncFifo(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  int i, cnt, resets=0;
  int stat;
  long start_sec, end_sec, dummy, readptr;

  TDFXTRACEACCEL("TDFXSyncFifo start\n");
  pTDFX=TDFXPTR(pScrn);
  TDFXSendNOPFifo(pScrn);
  i=0;
  cnt=0;
  start_sec=0;
  readptr=TDFXReadLongMMIO(pTDFX, SST_FIFO_RDPTRL0);
  do {
    readptr=TDFXReadLongMMIO(pTDFX, SST_FIFO_RDPTRL0);
    stat=TDFXReadLongMMIO(pTDFX, 0);
    if (stat&SST_BUSY) i=0; else i++;
    cnt++;
    if (cnt==1000) {
      if (!start_sec) {
	getsecs(&start_sec, &dummy);
      } else {
	getsecs(&end_sec, &dummy);
	if (end_sec-start_sec>3) {
	  dummy=TDFXReadLongMMIO(pTDFX, SST_FIFO_RDPTRL0);
	  if (dummy==readptr) {
	    TDFXResetFifo(pScrn);
	    readptr=dummy;
	    resets++;
	    if (resets==3) {
	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "Board is not responding.\n");
	      return;
	    }
	  }
	  start_sec=0;
        }
      }
      cnt=0;
    }
  } while (i<3);
  pTDFX->PciCnt=stat&0x1F;
  pTDFX->prevBlitDest.x1=pTDFX->prevBlitDest.y1=0;
  pTDFX->prevBlitDest.x2=pTDFX->prevBlitDest.y2=0;
}

Bool TDFXInitFifo(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;

  pScrn = xf86Screens[pScreen->myNum];
  pTDFX=TDFXPTR(pScrn);
#ifdef DEBUG_FIFO
  pTDFX->fifoMirrorBase=0;
#endif
  pTDFX->fifoBase = (uint32*)(pTDFX->FbBase+pTDFX->fifoOffset);
#ifdef DEBUG_FIFO
  pTDFX->fifoMirrorBase = xalloc(pTDFX->fifoSize);
  pTDFX->fifoMirrorPtr = pTDFX->fifoMirrorBase;
#endif
  pTDFX->sync=TDFXSyncFifo;
  InstallFifo(pScrn);
  return TRUE;
}

void TDFXShutdownFifo(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;

  pScrn = xf86Screens[pScreen->myNum];
  pTDFX=TDFXPTR(pScrn);
  TDFXWriteLongMMIO(pTDFX, SST_FIFO_BASESIZE0, 0);
  pTDFX->sync=TDFXSync;
#ifdef DEBUG_FIFO
  if (pTDFX->fifoMirrorBase) xfree(pTDFX->fifoMirrorBase);
  pTDFX->fifoMirrorBase=0;
#endif
}

static uint32 
GetReadPtr(TDFXPtr pTDFX)
{
  uint32 read_ptr, dummy;

  do {
    dummy = TDFXReadLongMMIO(pTDFX, SST_FIFO_RDPTRL0);
    read_ptr = TDFXReadLongMMIO(pTDFX, SST_FIFO_RDPTRL0);
  } while (read_ptr != dummy);
  return read_ptr;
}

#ifdef XF86DRI
void TDFXSwapContextFifo(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;
  int dummy, readPos;
  TDFXSAREAPriv *sPriv;

  pScrn = xf86Screens[pScreen->myNum];
  pTDFX=TDFXPTR(pScrn);
  sPriv=(TDFXSAREAPriv*)DRIGetSAREAPrivate(pScreen);
  /* if (sPriv)
     ErrorF("In FifoPtr=%d FifoRead=%d\n", sPriv->fifoPtr, sPriv->fifoRead); */
#if 1
  do {
    dummy=TDFXReadLongMMIO(pTDFX, SST_FIFO_DEPTH0);
    readPos=TDFXReadLongMMIO(pTDFX, SST_FIFO_DEPTH0);
  } while (dummy || readPos);
  readPos=(GetReadPtr(pTDFX)-pTDFX->fifoOffset)>>2;
  pTDFX->fifoPtr = pTDFX->fifoBase+readPos;
  pTDFX->fifoRead = pTDFX->fifoPtr;
#else
  sPriv=(TDFXSAREAPriv*)DRIGetSAREAPrivate(pScreen);
  if (!sPriv) return;
  if ((sPriv->fifoPtr<pTDFX->fifoOffset) || 
      (sPriv->fifoPtr>(int)pTDFX->fifoOffset+pTDFX->fifoSize) ||
      (sPriv->fifoRead<pTDFX->fifoOffset) ||
      (sPriv->fifoRead>(int)pTDFX->fifoOffset+pTDFX->fifoSize)) {
    ErrorF("Invalid offsets passed between client and X server\n");
    ResetFifo(pScrn);
  } else {
    pTDFX->fifoPtr = (unsigned int *)(pTDFX->FbBase+sPriv->fifoPtr);
    pTDFX->fifoRead = (unsigned int *)(pTDFX->FbBase+sPriv->fifoRead);
  }
#endif
  if (pTDFX->fifoRead>pTDFX->fifoPtr)
    pTDFX->fifoSlots = pTDFX->fifoRead-pTDFX->fifoPtr-1-8;
  else 
    pTDFX->fifoSlots = pTDFX->fifoEnd-pTDFX->fifoPtr-8;
}    

#endif

static void 
TDFXMakeSpace(TDFXPtr pTDFX, uint32 slots)
{
  uint32 slots_available;

  /*
  ** Check to see if we have to wrap to get enough space.
  */
  if (slots > pTDFX->fifoEnd-pTDFX->fifoPtr) {
    /*
    ** Make sure that the read pointer is not ahead of us in the
    ** the command fifo before wrapping.
    ** This insures two things:
    **   1) There is room at fifo_ptr for the JMP packet.
    **   2) There are slots available at the beginning of the fifo up to the read_ptr.
    **
    ** Since we are wrapping, insure that the read pointer is not at the
    ** beginning of the fifo to prevent the ambiguous situation described
    ** below.
    */
    do {
      pTDFX->fifoRead = (uint32*)(pTDFX->FbBase + GetReadPtr(pTDFX));
    }
    while (pTDFX->fifoRead>pTDFX->fifoPtr || 
	   pTDFX->fifoRead == pTDFX->fifoBase);
    /*
    ** Put a jump command in command fifo to wrap to the beginning.
    */
#if X_BYTE_ORDER == X_BIG_ENDIAN
    WRITE_FIFO(pTDFX, 0, (pTDFX->fifoOffset >> 2) << SSTCP_PKT0_ADDR_SHIFT |
      SSTCP_PKT0_JMP_LOCAL);
#else
    *pTDFX->fifoPtr = (pTDFX->fifoOffset >> 2) << SSTCP_PKT0_ADDR_SHIFT |
      SSTCP_PKT0_JMP_LOCAL;
#endif
    FLUSH_WCB();

    /*
    ** Reset the fifo_ptr to the beginning of the command fifo.
    */
    pTDFX->fifoPtr = pTDFX->fifoBase;
#ifdef DEBUG_FIFO
    pTDFX->fifoMirrorPtr = pTDFX->fifoMirrorBase;
#endif
  }

  /*
  ** Wait for enough slots to satisfy the request.
  */
  do {
    pTDFX->fifoRead = (uint32*)(pTDFX->FbBase + GetReadPtr(pTDFX));

    /*
    ** If the HW read_ptr is ahead the SW fifo_ptr, we don't allocate the
    ** fifo slot immediately behind the HW read_ptr.  This is to prevent
    ** the following ambiguous situation...
    **
    ** If (HW read_ptr == SW fifo_ptr) is it because the HW read_ptr has
    ** caught up to the SW fifo_ptr and the fifo is completely empty
    ** OR is it because the SW fifo_ptr has caught up to the HW read_ptr
    ** and the fifo is completely full?
    */
    if ((uint32*)pTDFX->fifoRead > pTDFX->fifoPtr)
      slots_available = pTDFX->fifoRead - pTDFX->fifoPtr - 1;
    else
      slots_available = pTDFX->fifoEnd - pTDFX->fifoPtr;
  } while (slots > slots_available);

  pTDFX->fifoSlots = slots_available-slots;
}

void 
TDFXAllocateSlots(TDFXPtr pTDFX, int slots)
{
#ifdef TDFX_DEBUG_FIFO
  if (pTDFX->fifoEnd-pTDFX->fifoPtr<pTDFX->fifoSlots)
    ErrorF("FIFO overrun\n");
  if (!pTDFX->syncDone) {
    ErrorF("Writing to FIFO without sync\n");
  }
#endif
  pTDFX->fifoSlots-=slots;
  if (pTDFX->fifoSlots<0) TDFXMakeSpace(pTDFX, slots);
}
