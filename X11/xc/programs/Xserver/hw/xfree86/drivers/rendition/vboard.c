/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vboard.c,v 1.19 2003/11/06 18:38:04 tsi Exp $ */
/*
 * includes
 */

#include "rendition.h"
#include "v1krisc.h"
#include "vboard.h"
#include "vloaduc.h"
#include "vos.h"

#if defined(SCO) || defined(SCO325)
#define PATH_MAX 1024
#endif

/* 
 * global data
 */

#include "cscode.h"

/* Global imported during compile-time */
static char MICROCODE_DIR [PATH_MAX] = MODULEDIR;

/*
 * local function prototypes
 */


/*
 * functions
 */
int
verite_initboard(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

    IOADDRESS iob=pRendition->board.io_base;
    vu8 *vmb;
    vu32 offset;
    vu8 memendian;
    int c,pc;

    /* write "monitor" program to memory */
    v1k_stop(pScreenInfo);
    pRendition->board.csucode_base=0x800;
    memendian=verite_in8(iob+MEMENDIAN);
    verite_out8(iob+MEMENDIAN, MEMENDIAN_NO);

    /* Note that CS ucode must wait on address in csucode_base
     * when initialized for later context switch code to work. */

    ErrorF("Loading csucode @ %p + 0x800\n", pRendition->board.vmem_base);
    vmb=pRendition->board.vmem_base;
    offset=pRendition->board.csucode_base;
    for (c=0; c<sizeof(csrisc)/sizeof(vu32); c++, offset+=sizeof(vu32))
	verite_write_memory32(vmb, offset, csrisc[c]);

    /* Initialize the CS flip semaphore */
    verite_write_memory32(vmb, 0x7f8, 0);
    verite_write_memory32(vmb, 0x7fc, 0);

    /* Run the code we just transfered to the boards memory */
    /* ... and start accelerator */
    v1k_flushicache(pScreenInfo);

    verite_out8(iob + STATEINDEX, STATEINDEX_PC);
    pc = verite_in32(iob + STATEDATA);
    v1k_start(pScreenInfo, pRendition->board.csucode_base);

    /* Get on loading the ucode */
    verite_out8(iob + STATEINDEX, STATEINDEX_PC);

    for (c = 0; c < 0xffffffL; c++){
	v1k_stop(pScreenInfo);
	pc = verite_in32(iob + STATEDATA);
	v1k_continue(pScreenInfo);
	if (pc == pRendition->board.csucode_base)
	    break;
    }
    if (pc != pRendition->board.csucode_base){
	xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		   ("VERITE_INITBOARD -- PC != CSUCODEBASE\n"));
	ErrorF ("RENDITION: PC == 0x%x   --  CSU == 0x%lx\n",
		pc,(unsigned long)pRendition->board.csucode_base);
    }

    /* reset memory endian */
    verite_out8(iob+MEMENDIAN, memendian);

    if (V1000_DEVICE == pRendition->board.chip){
	c=verite_load_ucfile(pScreenInfo, strcat ((char *)MICROCODE_DIR,"v10002d.uc"));
    }
    else {
	/* V2x00 chip */
	c=verite_load_ucfile(pScreenInfo, strcat ((char *)MICROCODE_DIR,"v20002d.uc"));
    }

    if (c == -1) {
	xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		   ("Microcode loading failed !!!\n"));
	return 1;
    }

    pRendition->board.ucode_entry=c;

#ifdef DEBUG
    ErrorF("UCode_Entry == 0x%x\n",pRendition->board.ucode_entry); 
#endif

    /* Everything's OK */
    return 0;
}


int
verite_resetboard(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    vu16 iob=pRendition->board.io_base; 
    vu8 memendian=verite_in8(iob+MEMENDIAN);
    vu32 crtcctl = verite_in32(iob+CRTCCTL);

    v1k_softreset(pScreenInfo);
    verite_out8(iob+MEMENDIAN, memendian);
    verite_out32(iob+CRTCCTL, crtcctl);

    return 0;
}

int
verite_getmemorysize(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#define PATTERN  0xf5faaf5f
#define START    0x12345678
#define ONEMEG   (1024L*1024L)
    vu32 offset;
    vu32 pattern;
    vu32 start;
    vu8 memendian;
#ifdef XSERVER
    vu8 modereg;

    modereg=verite_in8(pRendition->board.io_base+MODEREG);
    verite_out8(pRendition->board.io_base+MODEREG, NATIVE_MODE);
#endif

    /* no byte swapping */
    memendian=verite_in8(pRendition->board.io_base+MEMENDIAN);
    verite_out8(pRendition->board.io_base+MEMENDIAN, MEMENDIAN_NO);

    /* it looks like the v1000 wraps the memory; but for I'm not sure,
     * let's test also for non-writable offsets */
    start=verite_read_memory32(pRendition->board.vmem_base, 0);
    verite_write_memory32(pRendition->board.vmem_base, 0, START);
    for (offset=ONEMEG; offset<16*ONEMEG; offset+=ONEMEG) {
#ifdef DEBUG
        ErrorF( "Testing %d MB: ", offset/ONEMEG);
#endif
        pattern=verite_read_memory32(pRendition->board.vmem_base, offset);
        if (START == pattern) {
#ifdef DEBUG
            ErrorF( "Back at the beginning\n");
#endif
            break;    
        }
        
        pattern^=PATTERN;
        verite_write_memory32(pRendition->board.vmem_base, offset, pattern);
        
#ifdef DEBUG
        ErrorF( "%x <-> %x\n", (int)pattern, 
                    (int)verite_read_memory32(pRendition->board.vmem_base, offset));
#endif

        if (pattern != verite_read_memory32(pRendition->board.vmem_base, offset)) {
            offset-=ONEMEG;
            break;    
        }
        verite_write_memory32(pRendition->board.vmem_base, offset, pattern^PATTERN);
    }
    verite_write_memory32(pRendition->board.vmem_base, 0, start);

    if (16*ONEMEG <= offset)
        pRendition->board.mem_size=4*ONEMEG;
    else 
	    pRendition->board.mem_size=offset;

    /* restore default byte swapping */
    verite_out8(pRendition->board.io_base+MEMENDIAN, memendian);

#ifdef XSERVER
    verite_out8(pRendition->board.io_base+MODEREG, modereg);
#endif

    return pRendition->board.mem_size;
#undef PATTERN
#undef ONEMEG
}

void
verite_check_csucode(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  IOADDRESS iob=pRendition->board.io_base;
  vu8 *vmb;
  vu32 offset;
  int c;
  int memend;
  int mismatches=0;

  memend=verite_in8(iob+MEMENDIAN);
  verite_out8(iob+MEMENDIAN, MEMENDIAN_NO);

#ifdef DEBUG
  ErrorF("Checking presence of csucode @ 0x%x + 0x800\n",
	 pRendition->board.vmem_base);

  if (0x800 != pRendition->board.csucode_base)
    ErrorF("pRendition->board.csucode_base == 0x%x\n",
	   pRendition->board.csucode_base);
#endif

  /* compare word by word */
  vmb=pRendition->board.vmem_base;
  offset=pRendition->board.csucode_base;
  for (c=0; c<sizeof(csrisc)/sizeof(vu32); c++, offset+=sizeof(vu32))
    if (csrisc[c] != verite_read_memory32(vmb, offset)) {
      ErrorF("csucode mismatch in word %02d: 0x%08lx should be 0x%08lx\n",
	     c,
	     (unsigned long)verite_read_memory32(vmb, offset),
	     (unsigned long)csrisc[c]);
      mismatches++;
    }
#ifdef DEBUG
  ErrorF("Encountered %d out of %d possible mismatches\n",
	 mismatches,
	 sizeof(csrisc)/sizeof(vu32));
#endif

  verite_out8(iob+MEMENDIAN, memend);
}

/*
 * end of file vboard.c
 */
