/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/v1krisc.c,v 1.7 2002/01/25 21:56:07 tsi Exp $ */
/*
 *
 */

/*
 * includes
 */

#include "rendition.h"
#include "v1krisc.h"
#include "vos.h"



/*
 * defines
 */

/* RISC registers */
#define RISC_FLAG 37     /* RISC flags register */
#define RISC_SP   252    /* RISC stack pointer/scratch register */
#define RISC_IRA  253    /* RISC register */
#define RISC_RA   254    /* RISC program link/scratch register */
#define RISC_FP   255    /* RISC frame pointer/scratch register */

/* RISC opcodes used */
#define NOP_INSTR       0x00000000      /* really add immed with D=zero */
#define ADDI_OP         0x00
#define SPRI_OP         0x4f
#define ADD_OP          0x10
#define ANDN_OP         0x12
#define OR_OP           0x15
#define ADDIFI_OP       0x40
#define ADDSL8_OP       0x4b
#define LB_OP           0x70
#define LH_OP           0x71
#define LW_OP           0x72
#define LI_OP           0x76
#define LUI_OP          0x77
#define SB_OP           0x78
#define SH_OP           0x79
#define SW_OP           0x7a
#define JMP_OP          0x6c

/* defines for instruction composition */
#define INT_INSTR(op, d, s2, s1) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | ((vu32)(s2)<<8) | ((vu32)(s1)&0xff))
#define STR_INSTR(op, off8, s2, s1) \
 (((vu32)(op)<<24) | (((vu32)(off8)&0xff)<<16) | ((vu32)(s2)<<8) | ((vu32)(s1)))
#define LD_INSTR(op, d, off8, s1) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | (((vu32)(off8)&0xff)<<8) | ((vu32)(s1)))
#define LI_INSTR(op, d, immed16) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | ((vu32)(immed16)&0xffff))
#define BR_INSTR(op, off16, s1) \
 (((vu32)(op)<<24) | (((vu32)(off16)&0xffff)<<8) | ((vu32)(s1)))
#define JMP_INSTR(op, addr24) \
 (((vu32)(op)<<24) | ((vu32)(addr24)))

/* some masks */
#define TYPEINSTR	0xf0000000
#define TYPEBRANCH	0x60000000
#define HALTBIT		0x00000008

#define VBIOS_DTNR	0x2000

#define READ_BYTE	0
#define READ_SHORT	1
#define READ_WORD	2

#define WRITE_BYTE	0
#define WRITE_SHORT	1
#define WRITE_WORD	2

#define VERITE_MAX_POLLS	100



/*
 * local function prototypes
 */

static void verite_iopoll(IOADDRESS port, vu32 data, vu32 mask);
static void verite_iopoll8(IOADDRESS port, vu8 data, vu8 mask);

static vu32 readRF(IOADDRESS io_base, vu8 index);
static void writeRF(IOADDRESS io_base, vu8 index, vu32 data);

static vu32 risc_readmem(IOADDRESS io_base, vu32 addr, vu8 read_type);
static void risc_writemem(IOADDRESS io_base, vu32 addr, vu32 data, vu8 write_type);

#if 0
static void risc_step(IOADDRESS io_base, vu32 count);
#endif
static void risc_forcestep(IOADDRESS io_base, vu32 instruction);
static void risc_continue(IOADDRESS io_base);



/*
 * functions
 */

/*
 * void v1k_start(ScrnInfoPtr pScreenInfo, vu32 pc)
 *
 * Start the RISC with its PC set to |pc|.
 */
void
v1k_start(ScrnInfoPtr pScreenInfo, vu32 pc)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  IOADDRESS io_base=pRendition->board.io_base;

  v1k_stop(pScreenInfo);
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, JMP_INSTR(JMP_OP, pc>>2));
  risc_forcestep(io_base, NOP_INSTR);

  v1k_continue(pScreenInfo);
}



/*
 * void v1k_continue(ScrnInfoPtr pScreenInfo)
 *
 * Let the RISC do its work.
 */
void
v1k_continue(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

  risc_continue(pRendition->board.io_base);
}



/*
 * void v1k_stop(ScrnInfoPtr pScreenInfo)
 *
 * Stop the RISC.
 */
void
v1k_stop(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  vu8	debugreg, statusreg;
  IOADDRESS io_base=pRendition->board.io_base;
  vu16 STATUS = 0x4A;   /* v2x00 io register offset */
  int c;

  debugreg=verite_in8(io_base+DEBUGREG);

  if (pRendition->board.chip == V2000_DEVICE){
    c=0;
    do {
/*      if(!(c%10000))ErrorF("#S1# !0x%x! -- ",verite_in8(io_base+STATUS)); */
      statusreg = verite_in8(io_base+STATUS);
      if ((statusreg & 0x8C) == 0x8C)
	break;
    } while (c++<0xfffff);
    if (c >= 0xfffff)
      xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		 ("Status timeout (1)\n"));

    verite_out8(io_base+DEBUGREG, debugreg|HOLDRISC);

    if (pRendition->board.chip == V2000_DEVICE){
      c=0;
      do {
/*	if(!(c%10000))ErrorF("#S2# !0x%x! -- ",verite_in8(io_base+STATUS)); */
	statusreg = verite_in8(io_base+STATUS);
	if (statusreg & HOLDRISC) break;
      } while (c++<0xfffff);
      if (c >= 0xfffff)
	xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		   ("Status timeout (2)\n"));
    }
  }   
  else {
    /* V1000 stop */
    verite_out8(io_base+DEBUGREG, debugreg|HOLDRISC);

    verite_iopoll(io_base+STATEDATA, 0, 0); /* short pause */
    verite_iopoll(io_base+STATEDATA, 0, 0); /* short pause */
    verite_iopoll(io_base+STATEDATA, 0, 0); /* short pause */
  }
}



/* 
 * void v1k_flushicache(ScrnInfoPtr pScreenInfo)
 *
 * Returns with Icache on, also flushes Pixel engine line buffers 
 * in the Dcache.
 */
void
v1k_flushicache(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  vu32 c, p1, p2;
  IOADDRESS io_base=pRendition->board.io_base;

  /* first flush store accumulation buffers so data is all in memory */
  p1=risc_readmem(io_base, 0, READ_WORD);
  p2=risc_readmem(io_base, 8, READ_WORD);
  risc_writemem(io_base, 0, p1, WRITE_WORD);
  risc_writemem(io_base, 8, p2, WRITE_WORD);
  (void)risc_readmem(io_base, 0, READ_WORD);
  (void)risc_readmem(io_base, 8, READ_WORD);

  /* now force a spr Sync,zero to cause the pixel engine line buffers
   * to be flushed. */
  risc_forcestep(io_base, INT_INSTR(SPRI_OP, 0, 0, 31)); /* spri Sync,zero */
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, NOP_INSTR);


  writeRF(io_base, RISC_RA, ICACHE_ONOFF_MASK);        /* load mask */
  /* set bits */
  risc_forcestep(io_base, INT_INSTR(OR_OP, RISC_FLAG, RISC_FLAG, RISC_RA));
  risc_forcestep(io_base, NOP_INSTR);                  /* clear hazard */
  risc_forcestep(io_base, NOP_INSTR);
  risc_forcestep(io_base, NOP_INSTR);
    
  /* flush ICache */
  for (c=0; c<ICACHESIZE*2; c+=ICACHELINESIZE)
    risc_forcestep(io_base, JMP_INSTR(JMP_OP, c>>2)); /* jmp NextCacheLine. */

  writeRF(io_base, RISC_RA, ICACHE_ONOFF_MASK);      /* load mask */
  /* clear bits */
  risc_forcestep(io_base, INT_INSTR(ANDN_OP, RISC_FLAG, RISC_FLAG, RISC_RA));
  risc_forcestep(io_base, NOP_INSTR);                /* jump back to PC=0 */
  risc_forcestep(io_base, JMP_INSTR(JMP_OP, 0));
  risc_forcestep(io_base, NOP_INSTR);
}



/*
 * void v1k_softreset(ScrnInfoPtr pScreenInfo)
 *
 * Soft Reset RISC.
 */
void
v1k_softreset(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  IOADDRESS io_base=pRendition->board.io_base;

  verite_out8(io_base+DEBUGREG, SOFTRESET|HOLDRISC);
  verite_out8(io_base+STATEINDEX, STATEINDEX_PC);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);

  verite_out8(io_base+DEBUGREG, HOLDRISC);
  verite_iopoll(io_base+STATEDATA, 0, 0);
  verite_iopoll(io_base+STATEDATA, 0, 0);
  verite_iopoll(io_base+STATEDATA, 0, 0);

  /* turn icache on */
  risc_forcestep(io_base, LI_INSTR(LI_OP, RISC_RA, ICACHE_ONOFF_MASK&0xffff));
  risc_forcestep(io_base, INT_INSTR(ADDIFI_OP, RISC_FLAG, RISC_RA, 
                                    ICACHE_ONOFF_MASK>>16));
  /* clear any interrupts */
  verite_out8(io_base+INTR, 0xff);
  /* byte swap mode=word */
  verite_out8(io_base+MEMENDIAN, MEMENDIAN_NO);	
}



/*
void
v1k_getriscprocs(verite_board_desc *boardDesc)
{
    boardDesc->risc_procs.risc_softreset = v1krisc_softreset;
    boardDesc->risc_procs.risc_flushicache = v1krisc_flushicache;
    boardDesc->risc_procs.risc_start = v1krisc_start;
    boardDesc->risc_procs.risc_stop = v1krisc_stop;
    boardDesc->risc_procs.risc_continue = v1krisc_continue;
    return;
}
*/



/*
 * local functions
 */

/* 
 * static void verite_iopoll(IOADDRESS port, vu32 data, vu32 mask)
 *
 * Loop on IO read until expected data is read or VERITE_MAX_POLLS is reached.
 */
static void
verite_iopoll(IOADDRESS port, vu32 data, vu32 mask)
{
  vu32 c;

  c=0;
  do {
    c++;
    if ((verite_in32(port)&mask) == (data&mask))
      break;
  } while (c <= VERITE_MAX_POLLS);
}



/* 
 * static void verite_iopoll8(IOADDRESS port, vu8 data, vu8 mask)
 *
 * Loop on IO read until expected data is read or VERITE_MAX_POLLS is reached.
 */
static void
verite_iopoll8(IOADDRESS port, vu8 data, vu8 mask)
{
  vu32 c;

  c=0;
  do {
	c++;
	if ((verite_in8(port)&mask) == (data&mask))
	  break;
  } while (c <= VERITE_MAX_POLLS);
}



/*
 * static vu32 readRF(IOADDRESS io_base, vu8 index)
 *
 * Reads data from register file.
 */
static vu32
readRF(IOADDRESS io_base, vu8 index)
{
  vu32 data, instr;
  vu8 debug, stateindex;
    
  debug=verite_in8(io_base+DEBUGREG);
  stateindex=verite_in8(io_base+STATEINDEX);

  /* force RISC instruction: add zero,zero,index
   * S1 reg address = reg index to read
   * write to the DEC_IR, but no need to step it! */
  verite_out8(io_base+DEBUGREG, debug|HOLDRISC);

  instr=INT_INSTR(ADD_OP, 0, 0, index);
  verite_out32(io_base+STATEDATA, instr);

  /* wait for instruction to get to RISC IR. */
  verite_out8(io_base+STATEINDEX, STATEINDEX_IR);  /* point at DEC_IR */
  verite_iopoll(io_base+STATEDATA, instr, 0xffffffff);

  verite_out8(io_base+STATEINDEX, STATEINDEX_S1);  /* point at RISCS1 */
  verite_iopoll(io_base+STATEINDEX, 0, 0);         /* short pause */
  data=verite_in32(io_base+STATEDATA);             /* read RF */
    
  verite_out8(io_base+STATEINDEX, stateindex);     /* restore state_index */
  verite_out8(io_base+DEBUGREG, debug);            /* restore debug */
    
  return data;
}



/*
 * static void writeRF(IOADDRESS io_base, vu8 index, vu32 data)
 *
 * Set RF register, being careful on how to set regs below 64.
 */
static void
writeRF(IOADDRESS io_base, vu8 index, vu32 data)
{
  vu8 special=0;

  if (index < 64) {     /* constants or HW regs */
	special=index;      /* keep track of special register */
	index=RISC_SP;      /* use |sp| as tmp, since it gets restored */
  }

  if (!(data & 0xff000000)) {    /* only 16 or 24 LSBs */
    /* Set 16 LS bits. */
	risc_forcestep(io_base, LI_INSTR(LI_OP,index,data&0xffff));
	if (data & 0x00ff0000)       /* need all 24 LS bits? */
	  risc_forcestep(io_base, INT_INSTR(ADDIFI_OP,index,index,data>>16) );
  }
  else {                         /* else, do all 32 bits */
	risc_forcestep(io_base, LI_INSTR(LUI_OP, index, data>>16));
	risc_forcestep(io_base, INT_INSTR(ADDSL8_OP, index, index, (data>>8)&0xff));
	risc_forcestep(io_base, INT_INSTR(ADDI_OP, index, index, data&0xff));
  }

  if (special) {
	/* move data to special register */
	risc_forcestep(io_base, INT_INSTR(ADD_OP, special, 0, RISC_SP));
	/* clear hazard */
    risc_forcestep(io_base, NOP_INSTR);	
	risc_forcestep(io_base, NOP_INSTR);
	risc_forcestep(io_base, NOP_INSTR);
  }
}



/*
 * static vu32 risc_readmem(IOADDRESS io_base, vu32 addr, vu8 read_type)
 *
 * NOTE: Assumes RISC is in hold mode.
 */
static vu32
risc_readmem(IOADDRESS io_base, vu32 addr, vu8 read_type)
{
  vu32 data;

  writeRF(io_base, RISC_RA, addr);          /* point to memory */
  if (READ_BYTE == read_type)               /* read memory */
	risc_forcestep(io_base, LD_INSTR(LB_OP, RISC_SP, 0, RISC_RA));
  else 
    if (READ_SHORT == read_type)
	  risc_forcestep(io_base, LD_INSTR(LH_OP, RISC_SP, 0, RISC_RA));
    else
	  risc_forcestep(io_base, LD_INSTR(LW_OP, RISC_SP, 0, RISC_RA));

  risc_forcestep(io_base, NOP_INSTR);      /* need nop's */
  risc_forcestep(io_base, NOP_INSTR);      /* need nop's */
  data=readRF(io_base, RISC_SP);           /* get data */

  return data; 
}



/*
 * static vu32 risc_writemem(IOADDRESS io_base, vu32 addr, vu32 data, vu8 write_type)
 *
 * NOTE: Assumes RISC is in hold mode.
 */
static void
risc_writemem(IOADDRESS io_base, vu32 addr, vu32 data, vu8 write_type)
{
  writeRF(io_base, RISC_RA, addr);          /* point to memory */
  writeRF(io_base, RISC_FP, data);          /* set data */
  if (WRITE_BYTE == write_type)             /* write memory */
    risc_forcestep(io_base, STR_INSTR(SB_OP, 0, RISC_FP, RISC_RA));
  else 
    if (WRITE_SHORT == write_type)
      risc_forcestep(io_base, STR_INSTR(SH_OP, 0, RISC_FP, RISC_RA));
    else
      risc_forcestep(io_base, STR_INSTR(SW_OP, 0, RISC_FP, RISC_RA));
}


#if 0
/*
 * static void risc_step(IOADDRESS io_base, vu32 count)
 *
 * Single step the RISC. NOTE: Do not force instruction into RISCIR!
 */
static void
risc_step(IOADDRESS io_base, vu32 count)
{
  vu32 c, d;
  vu8 debugreg;

  /* RISC is already held; just single step it */
    
  for (c=0; c<count; c++) {
    debugreg=verite_in8(io_base+DEBUGREG);
    verite_out8(io_base+DEBUGREG, debugreg|STEPRISC); 
    
    for (d=0; d<1000; d++)
      if(0 == (verite_in8(io_base+DEBUGREG)&STEPRISC))
		break;

    if (1000 == d)
	  return;   /* stall occurred, we're done */
  }
}
#endif


/*
 * static void risc_forcestep(IOADDRESS io_base, vu32 instruction)
 *
 * Single step RISC; force instruction; assumes RISC held.
 */
static void
risc_forcestep(IOADDRESS io_base, vu32 instruction)
{
  vu32 c;
  vu8 debugreg, stateindex;
    
    
  debugreg=verite_in8(io_base+DEBUGREG);
  stateindex=verite_in8(io_base+STATEINDEX);
  verite_out8(io_base+STATEINDEX, STATEINDEX_IR);
  verite_iopoll8(io_base+STATEINDEX, STATEINDEX_IR, 0xff);    /* wait */
  verite_out32(io_base+STATEDATA, instruction);               /* load instruction */
  verite_iopoll(io_base+STATEDATA, instruction, 0xffffffff);  /* wait */
  verite_out8(io_base+DEBUGREG, debugreg|HOLDRISC|STEPRISC);  /* step */
  verite_iopoll(io_base+STATEDATA, 0, 0);                     /* short pause */
    
  for (c=0; c<VERITE_MAX_POLLS; c++)
    if (HOLDRISC == (verite_in8(io_base+DEBUGREG) & (HOLDRISC|STEPRISC)))
      break;

  /* restore */
  verite_out8(io_base+STATEINDEX, stateindex);
}



/*
 * static void risc_continue(IOADDRESS io_base)
 *
 * Turn off hold bit.    
 */
static void
risc_continue(IOADDRESS io_base)
{
  vu8 debugreg;

  debugreg=verite_in8(io_base+DEBUGREG);
  verite_out8(io_base+DEBUGREG, debugreg&(~HOLDRISC));
  verite_iopoll(io_base+STATEDATA, 0, 0);    /* short pause */
}



/*
 * end of file v1krisc.c
 */
