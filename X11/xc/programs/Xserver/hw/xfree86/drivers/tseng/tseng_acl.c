
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_acl.c,v 1.25 2000/12/14 16:33:10 eich Exp $ */





#include "tseng.h"
#include "tseng_acl.h"
#include "compiler.h"

void tseng_terminate_acl(TsengPtr pTseng);

/*
 * conversion from X ROPs to Microsoft ROPs.
 */

int W32OpTable[] =
{
    0x00,			       /* Xclear             0 */
    0x88,			       /* Xand               src AND dst */
    0x44,			       /* XandReverse        src AND NOT dst */
    0xcc,			       /* Xcopy              src */
    0x22,			       /* XandInverted       NOT src AND dst */
    0xaa,			       /* Xnoop              dst */
    0x66,			       /* Xxor               src XOR dst */
    0xee,			       /* Xor                src OR dst */
    0x11,			       /* Xnor               NOT src AND NOT dst */
    0x99,			       /* Xequiv             NOT src XOR dst */
    0x55,			       /* Xinvert            NOT dst */
    0xdd,			       /* XorReverse         src OR NOT dst */
    0x33,			       /* XcopyInverted      NOT src */
    0xbb,			       /* XorInverted        NOT src OR dst */
    0x77,			       /* Xnand              NOT src OR NOT dst */
    0xff			       /* Xset               1 */
};

int W32OpTable_planemask[] =
{
    0x0a,			       /* Xclear             0 */
    0x8a,			       /* Xand               src AND dst */
    0x4a,			       /* XandReverse        src AND NOT dst */
    0xca,			       /* Xcopy              src */
    0x2a,			       /* XandInverted       NOT src AND dst */
    0xaa,			       /* Xnoop              dst */
    0x6a,			       /* Xxor               src XOR dst */
    0xea,			       /* Xor                src OR dst */
    0x1a,			       /* Xnor               NOT src AND NOT dst */
    0x9a,			       /* Xequiv             NOT src XOR dst */
    0x5a,			       /* Xinvert            NOT dst */
    0xda,			       /* XorReverse         src OR NOT dst */
    0x3a,			       /* XcopyInverted      NOT src */
    0xba,			       /* XorInverted        NOT src OR dst */
    0x7a,			       /* Xnand              NOT src OR NOT dst */
    0xfa			       /* Xset               1 */
};

int W32PatternOpTable[] =
{
    0x00,			       /* Xclear             0 */
    0xa0,			       /* Xand               pat AND dst */
    0x50,			       /* XandReverse        pat AND NOT dst */
    0xf0,			       /* Xcopy              pat */
    0x0a,			       /* XandInverted       NOT pat AND dst */
    0xaa,			       /* Xnoop              dst */
    0x5a,			       /* Xxor               pat XOR dst */
    0xfa,			       /* Xor                pat OR dst */
    0x05,			       /* Xnor               NOT pat AND NOT dst */
    0xa5,			       /* Xequiv             NOT pat XOR dst */
    0x55,			       /* Xinvert            NOT dst */
    0xf5,			       /* XorReverse         pat OR NOT dst */
    0x0f,			       /* XcopyInverted      NOT pat */
    0xaf,			       /* XorInverted        NOT pat OR dst */
    0x5f,			       /* Xnand              NOT pat OR NOT dst */
    0xff			       /* Xset               1 */
};



/**********************************************************************/

void 
tseng_terminate_acl(TsengPtr pTseng)
{
    /* only terminate when needed */
/*  if (*(volatile unsigned char *)ACL_ACCELERATOR_STATUS & 0x06) */
    {
	ACL_SUSPEND_TERMINATE(0x00);
	/* suspend any running operation */
	ACL_SUSPEND_TERMINATE(0x01);
	WAIT_ACL;
	ACL_SUSPEND_TERMINATE(0x00);
	/* ... and now terminate it */
	ACL_SUSPEND_TERMINATE(0x10);
	WAIT_ACL;
	ACL_SUSPEND_TERMINATE(0x00);
    }
}

void 
tseng_recover_timeout(TsengPtr pTseng)
{
    if (!Is_ET6K) {
	ErrorF("trying to unlock......................................\n");
	MMIO_OUT32(pTseng->tsengCPU2ACLBase,0,0L); /* try unlocking the bus when CPU-to-accel gets stuck */
    }
    if (Is_W32p) {		       /* flush the accelerator pipeline */
	ACL_SUSPEND_TERMINATE(0x00);
	ACL_SUSPEND_TERMINATE(0x02);
	ACL_SUSPEND_TERMINATE(0x00);
    }
}

void 
tseng_init_acl(ScrnInfoPtr pScrn)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	tseng_init_acl\n");
    /*
     * prepare some shortcuts for faster access to memory mapped registers
     */

    if (pTseng->UseLinMem) {
	pTseng->scratchMemBase = pTseng->FbBase + pTseng->AccelColorBufferOffset;
	/* 
	 * we won't be using tsengCPU2ACLBase in linear memory mode anyway, since
	 * using the MMU apertures restricts the amount of useable video memory
	 * to only 2MB, supposing we ONLY redirect MMU aperture 2 to the CPU.
	 * (see data book W32p, page 207)
	 */
	pTseng->tsengCPU2ACLBase = pTseng->FbBase + 0x200000;	/* MMU aperture 2 */
    } else {
	/*
	 * MMU 0 is used for the scratchpad (i.e. FG and BG colors).
	 *
	 * MMU 1 is used for the Imagewrite buffers. This code assumes those
	 * buffers are back-to-back, with AccelImageWriteBufferOffsets[0]
	 * being the first, and don't exceed 8kb (aperture size) in total
	 * length.
	 */
	pTseng->scratchMemBase = pTseng->FbBase + 0x18000L;
	MMIO_OUT32(pTseng->MMioBase, 0x00<<0, pTseng->AccelColorBufferOffset);
	MMIO_OUT32(pTseng->MMioBase, 0x04<<0, pTseng->AccelImageWriteBufferOffsets[0]);
	/*
	 * tsengCPU2ACLBase is used for CPUtoSCreen...() operations on < ET6000 devices
	 */
	pTseng->tsengCPU2ACLBase = pTseng->FbBase + 0x1C000L;	/* MMU aperture 2 */
	/*      MMIO_IN32(pTseng->MMioBase, 0x08<<0) = 200000; *//* TEST */
    }
#ifdef DEBUG    
    ErrorF("MMioBase = 0x%x, scratchMemBase = 0x%x\n", pTseng->MMioBase, pTseng->scratchMemBase);
#endif

    /*
     * prepare the accelerator for some real work
     */

    tseng_terminate_acl(pTseng);

    ACL_INTERRUPT_STATUS(0xe);       /* clear interrupts */
    ACL_INTERRUPT_MASK(0x04);	       /* disable interrupts, but enable deadlock exit */
    ACL_INTERRUPT_STATUS(0x0);
    ACL_ACCELERATOR_STATUS_SET(0x0);

    if (Is_ET6K) {
	ACL_STEPPING_INHIBIT(0x0);   /* Undefined at power-on, let all maps (Src, Dst, Mix, Pat) step */
	ACL_6K_CONFIG(0x00);	       /* maximum performance -- what did you think? */
	ACL_POWER_CONTROL(0x01);     /* conserve power when ACL is idle */
	ACL_MIX_CONTROL(0x33);
	ACL_TRANSFER_DISABLE(0x00);  /* Undefined at power-on, enable all transfers */
    } else {			       /* W32i/W32p */
  	ACL_RELOAD_CONTROL(0x0); 
	ACL_SYNC_ENABLE(0x1);	       /* | 0x2 = 0WS ACL read. Yields up to 10% faster operation for small blits */
	ACL_ROUTING_CONTROL(0x00);
    }

    if (Is_W32p || Is_ET6K) {
	/* Enable the W32p startup bit and set use an eight-bit pixel depth */
	ACL_NQ_X_POSITION(0);
	ACL_NQ_Y_POSITION(0);
	ACL_PIXEL_DEPTH((pScrn->bitsPerPixel - 8) << 1);
	/* writing destination address will start ACL */
	ACL_OPERATION_STATE(0x10);
    } else {
	/* X, Y positions set to zero's for w32 and w32i */
	ACL_X_POSITION(0);
	ACL_Y_POSITION(0);
	ACL_OPERATION_STATE(0x0);
	/* if we ever use CPU-to-screen pixmap uploading on W32I or W32,
	 * ACL_VIRTUAL_BUS_SIZE will need to be made dynamic (i.e. moved to
	 * Setup() functions).
	 *
	 * VBS = 1 byte is faster than VBS = 4 bytes, since the ACL can
	 * start processing as soon as the first byte arrives.
	 */
	ACL_VIRTUAL_BUS_SIZE(0x00);
    }
    ACL_DESTINATION_Y_OFFSET(pScrn->displayWidth * pTseng->Bytesperpixel - 1);
    ACL_XY_DIRECTION(0);

    MMU_CONTROL(0x74);

    if (Is_W32p && pTseng->UseLinMem) {
	/*
	 * Since the w32p revs C and D don't have any memory mapped when the
	 * accelerator registers are used it is necessary to use the MMUs to
	 * provide a semblance of linear memory. Fortunately on these chips
	 * the MMU appertures are 1 megabyte each. So as long as we are
	 * willing to only use 3 megs of video memory we can have some
	 * acceleration. If we ever get the CPU-to-screen-color-expansion
	 * stuff working then we will NOT need to sacrifice the extra 1MB
	 * provided by MBP2, because we could do dynamic switching of the APT
	 * bit in the MMU control register.
	 *
	 * On W32p rev c and d MBP2 is hardwired to 0x200000 when linear
	 * memory mode is enabled. (On rev a it is programmable).
	 *
	 * W32p rev a and b have their first 2M mapped in the normal (non-MMU)
	 * way, and MMU0 and MMU1, each 512 kb wide, can be used to access
	 * another 1MB of memory. This totals to 3MB of mem. available in
	 * linear memory when the accelerator is enabled.
	 */
	if (Is_W32p_ab) {
	    MMIO_OUT32(pTseng->MMioBase, 0x00<<0, 0x200000L);
	    MMIO_OUT32(pTseng->MMioBase, 0x04<<0, 0x280000L);
	} else {		       /* rev C & D */
	    MMIO_OUT32(pTseng->MMioBase, 0x00<<0, 0x0L);
	    MMIO_OUT32 (pTseng->MMioBase, 0x04<<0, 0x100000L);
	}
    }
}
