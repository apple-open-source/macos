/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_priv.h,v 1.11 2001/06/05 15:54:15 alanh Exp $ */


#ifndef _TDFX_FIFO_H_
#define _TDFX_FIFO_H_

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;
typedef int   int32;
typedef short int16;
typedef char  int8;

#define CMDFIFO_PAGES 64

#define PROPSAREADATA \
  volatile int fifoPtr; \
  volatile int fifoRead;

#define PROPDATASTD \
  volatile unsigned int *fifoPtr; \
  volatile unsigned int *fifoRead; \
  int fifoSlots; \
  unsigned int *fifoBase; \
  unsigned int *fifoEnd; \
  int fifoOffset; \
  int fifoSize

#define PROPDATADBG \
  unsigned int *fifoMirrorBase; \
  unsigned int *fifoMirrorPtr;

#ifdef FIFO_DEBUG
#define PROPDATA PROPDATASTD; \
	PROPDATADBG
#else
#define PROPDATA PROPDATASTD;
#endif

#ifdef TDFX_DEBUG_CMDS
#define TDFXMakeRoom(p, n) \
  do { \
    if (fifoCnt) \
      ErrorF("Previous TDFXMakeRoom passed incorrect size\n"); \
    cmdCnt=n; \
    TDFXAllocateSlots(p, (n)+1); \
  while (0)
#define TDFXWriteLong(p, a, v) 
  do { \
    if (lastAddr & a<lastAddr) \
      ErrorF("TDFXWriteLong not ordered\n"); \
    cmdCnt--; \
    WRITE_FIFO(p, a, v); \
  while (0)
#else
#define TDFXMakeRoom(p, n) TDFXAllocateSlots(p, (n)+1)
#define TDFXWriteLong(p, a, v) WRITE_FIFO(p, a, v)
#endif
#define DECLARE SET_PKT2_HEADER
#define DECLARE_LAUNCH SET_PK1_HEADER_LAUNCH
#define TDFXSendNOP TDFXSendNOPFifo

/*
** Structures for Banshee AGP/CMD Transfer/MISC registers.
*/
typedef volatile struct _H3CmdFifo
{
	uint32 baseAddrL;
	uint32 baseSize;
	uint32 bump;
	uint32 readPtrL;
	uint32 readPtrH;
	uint32 aMin;
	uint32 unusedA;
	uint32 aMax;
	uint32 unusedB;
	uint32 depth;
	uint32 holeCount;
	uint32 reserved;
} H3CmdFifo;

Bool TDFXInitFifo(ScreenPtr pScreen);
void TDFXShutdownFifo(ScreenPtr pScreen);
void TDFXAllocateSlots(TDFXPtr pTDFX, int slots);
void TDFXSendNOPFifo2D(ScrnInfoPtr pScreen);

#define CHECK_FOR_ROOM(_n) \
	if ((pTDFX->fifoSlots -= (_n)) < 0) \
		cmdfifo_make_room(adapter, fifo_ptr, _n); \

#if X_BYTE_ORDER == X_BIG_ENDIAN
#define BE_BSWAP32(val) ((((val) & 0x000000ff) << 24) | \
                     (((val) & 0x0000ff00) << 8) |  \
                     (((val) & 0x00ff0000) >> 8) |  \
                     (((val) & 0xff000000) >> 24))

#define BE_WSWAP32(val) ((((val) & 0x0000ffff) << 16) | \
                     (((val) & 0xffff0000) >> 16))
 void TDFXWriteFifo_24(TDFXPtr pTDFX, int val);
 void TDFXWriteFifo_16(TDFXPtr pTDFX, int val);
 void TDFXWriteFifo_8(TDFXPtr pTDFX, int val);
#else
/* Don't swap on little-endian platforms */
#define BE_BSWAP32(val) val
#define BE_WSWAP32(val) val
#endif


#ifdef DEBUG_FIFO
#define WRITE_FIFO(ptr, loc, _val) \
  do { \
    *pTDFX->fifoMirrorPtr++ = _val; \
    *pTDFX->fifoPtr++ = _val; \
  } while(0) 
#else
#if X_BYTE_ORDER == X_BIG_ENDIAN
#define WRITE_FIFO(ptr, loc, _val) \
  do { \
    pTDFX->writeFifo(ptr, _val); \
  } while (0)
#else
#define WRITE_FIFO(ptr, loc, _val) \
  do { \
    *pTDFX->fifoPtr++ = _val; \
  } while(0) 
#endif /* X_BYTE_ORDER == X_BIG_ENDIAN */
#endif /* DEBUG_FIFO */

#if X_BYTE_ORDER == X_BIG_ENDIAN
#define FLUSH_WCB()
#else
#define FLUSH_WCB() inb(0x80)
#endif

/*
** Send a packet header type 1 (2D) to the cmdfifo
*/
#define SET_2DPK1_HEADER(_ndwords, _inc, _reg) \
	WRITE_FIFO ( pTDFX, 0, (_ndwords) << SSTCP_PKT1_NWORDS_SHIFT | \
	                ((_inc) ? SSTCP_PKT1_INC : SSTCP_PKT1_NOINC) | \
	                SSTCP_PKT1_2D | \
	                FIELD_OFFSET(H3_2D_REGISTERS, _reg)/4 << SSTCP_REGBASE_SHIFT | \
	                SSTCP_PKT1 \
	              )

/*
** Send a packet header type 1 to begin at launchArea[0] to the cmdfifo
*/
#define SET_PK1_HEADER_LAUNCH(_ndwords, _inc) \
	WRITE_FIFO ( pTDFX, 0, (_ndwords) << SSTCP_PKT1_NWORDS_SHIFT | \
	                ((_inc) ? SSTCP_PKT1_INC : SSTCP_PKT1_NOINC) | \
	                SSTCP_PKT1_LAUNCH | SSTCP_PKT1 \
	              )

/*
** Send a packet header type 1 to begin at colorPattern[0] to the cmdfifo
*/
#define SET_PK1_HEADER_COLORPATTERN(_ndwords, _inc) \
	WRITE_FIFO ( pTDFX, 0, (_ndwords) << SSTCP_PKT1_NWORDS_SHIFT | \
	                ((_inc) ? SSTCP_PKT1_INC : SSTCP_PKT1_NOINC) | \
	                SSTCP_PKT1_COLORPATTERN | SSTCP_PKT1 \
	              )

/*
** Send a packet header type 2 to the cmdfifo
*/
#define SET_PKT2_HEADER(_mask) \
    WRITE_FIFO (pTDFX, 0, ((_mask) << SSTCP_PKT2_MASK_SHIFT) | SSTCP_PKT2)

/*
** These are used to select a register mask for use with
** command fifo packet type 4.  The defines represent an
** offset register from the base register. 
** e.g. R5 = baseReg + 5, R0 = baseReg etc.. 
*/
#define R0  (1 << 0)
#define R1  (1 << 1)
#define R2  (1 << 2)
#define R3  (1 << 3)
#define R4  (1 << 4)
#define R5  (1 << 5)
#define R6  (1 << 6)
#define R7  (1 << 7)
#define R8  (1 << 8)
#define R9  (1 << 9)
#define R10 (1 << 10)
#define R11 (1 << 11)
#define R12 (1 << 12)
#define R13 (1 << 13)

#define SET_2DPK4_HEADER(_mask, _reg0) \
	WRITE_FIFO ( pTDFX, 0, ((_mask) << SSTCP_PKT4_MASK_SHIFT) | \
	                SSTCP_PKT4_2D | \
	                (_reg0) | \
	                SSTCP_PKT4 \
	              )

#define SET_3DPK4_HEADER(_mask, _reg0) \
	WRITE_FIFO ( pTDFX, 0, ((_mask) << SSTCP_PKT4_MASK_SHIFT) | \
	                (_reg0) | \
	                SSTCP_PKT4 \
	              )

/*----------------- SST cmdFifo*.baseSize bits --------------------------- */
#define SST_CMDFIFO_SIZE		0xFF
#define SST_EN_CMDFIFO			BIT(8)
#define SST_CMDFIFO_AGP			BIT(9)
#define SST_CMDFIFO_DISABLE_HOLES	BIT(10)

/* SST COMMAND PACKET defines */
#define SSTCP_PKT_SIZE	3
#define SSTCP_PKT	SST_MASK(SSTCP_PKT_SIZE)
#define SSTCP_PKT0	0
#define SSTCP_PKT1	1
#define SSTCP_PKT2	2
#define SSTCP_PKT3	3
#define SSTCP_PKT4	4
#define SSTCP_PKT5	5
#define SSTCP_PKT6	6
#define SSTCP_PKT7	7

#define SSTCP_BOGUS_WORDS_SHIFT	29
#define SSTCP_BOGUS_WORDS	(7 << SSTCP_BOGUS_WORDS_SHIFT)

/* packet 0 defines */
#define SSTCP_PKT0_FUNC_SHIFT	3
#define SSTCP_PKT0_FUNC		(7 << SSTCP_PKT0_FUNC_SHIFT)
#define SSTCP_PKT0_NOP		((0<<SSTCP_PKT0_FUNC_SHIFT) | SSTCP_PKT0)
#define SSTCP_PKT0_JSR		((1<<SSTCP_PKT0_FUNC_SHIFT) | SSTCP_PKT0)
#define SSTCP_PKT0_RET		((2<<SSTCP_PKT0_FUNC_SHIFT) | SSTCP_PKT0)
#define SSTCP_PKT0_JMP_LOCAL	((3<<SSTCP_PKT0_FUNC_SHIFT) | SSTCP_PKT0)
#define SSTCP_PKT0_JMP_AGP	((4<<SSTCP_PKT0_FUNC_SHIFT) | SSTCP_PKT0)
#define SSTCP_PKT0_ADDR_SHIFT	6
#define SSTCP_PKT0_ADDR		(0x7FFFFF<<SSTCP_PKT0_ADDR_SHIFT)

/* packet 1 defines */
#define SSTCP_REGBASE_SHIFT	SSTCP_PKT_SIZE
#ifdef H4
#define SSTCP_REGBASE		(0x7FF<<SSTCP_REGBASE_SHIFT)
#define SSTCP_REGBASE_FROM_ADDR(x) ( (((x)>>2) & 0x7FF) << SSTCP_REGBASE_SHIFT )
#else
#define SSTCP_REGBASE		((0x3FF)<<SSTCP_REGBASE_SHIFT)
#define SSTCP_REGBASE_FROM_ADDR(x) ( (((x)>>2) & 0x3FF) << SSTCP_REGBASE_SHIFT )
#endif
#define SSTCP_PKT1_2D	BIT(14)
#define SSTCP_INC           BIT(15)
#define SSTCP_PKT1_NWORDS_SHIFT	16
#define SSTCP_PKT1_NWORDS	(0xFFFFUL<<SSTCP_PKT1_NWORDS_SHIFT)

#define SSTCP_PKT1_NOINC           0
#define SSTCP_PKT1_INC             SSTCP_INC
#define SSTCP_PKT1_CLIP0MIN        (SSTCP_PKT1_2D | (0x02 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_CLIP0MAX        (SSTCP_PKT1_2D | (0x03 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTBASEADDR     (SSTCP_PKT1_2D | (0x04 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTFORMAT       (SSTCP_PKT1_2D | (0x05 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCCOLORKEYMIN  (SSTCP_PKT1_2D | (0x06 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCCOLORKEYMAX  (SSTCP_PKT1_2D | (0x07 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTCOLORKEYMIN  (SSTCP_PKT1_2D | (0x08 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTCOLORKEYMAX  (SSTCP_PKT1_2D | (0x09 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_BRESERROR0      (SSTCP_PKT1_2D | (0x0a << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_BRESERROR1      (SSTCP_PKT1_2D | (0x0b << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_ROP             (SSTCP_PKT1_2D | (0x0c << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCBASEADDR     (SSTCP_PKT1_2D | (0x0d << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_COMMANDEXTRA    (SSTCP_PKT1_2D | (0x0e << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_LINESTIPPLE     (SSTCP_PKT1_2D | (0x0f << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_LINESTYLE       (SSTCP_PKT1_2D | (0x10 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_PATTERN0ALIAS   (SSTCP_PKT1_2D | (0x11 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_PATTERN1ALIAS   (SSTCP_PKT1_2D | (0x12 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_CLIP1MIN        (SSTCP_PKT1_2D | (0x13 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_CLIP1MAX        (SSTCP_PKT1_2D | (0x14 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCFORMAT       (SSTCP_PKT1_2D | (0x15 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCSIZE         (SSTCP_PKT1_2D | (0x16 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_SRCXY           (SSTCP_PKT1_2D | (0x17 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_COLORBACK       (SSTCP_PKT1_2D | (0x18 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_COLORFORE       (SSTCP_PKT1_2D | (0x19 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTSIZE         (SSTCP_PKT1_2D | (0x1a << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_DSTXY           (SSTCP_PKT1_2D | (0x1b << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_COMMAND         (SSTCP_PKT1_2D | (0x1c << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_LAUNCH          (SSTCP_PKT1_2D | (0x20 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT1_COLORPATTERN    (SSTCP_PKT1_2D | (0x40 << SSTCP_REGBASE_SHIFT))

/* packet 2 defines */
#define SSTCP_PKT2_MASK_SHIFT	SSTCP_PKT_SIZE
#define SSTCP_PKT2_MASK		(0x1FFFFFFFUL<<SSTCP_PKT2_MASK_SHIFT)

#define SSTCP_CLIP0MIN         0x00000001L
#define SSTCP_CLIP0MAX         0x00000002L
#define SSTCP_DSTBASEADDR      0x00000004L
#define SSTCP_DSTFORMAT        0x00000008L
#define SSTCP_SRCCOLORKEYMIN   0x00000010L
#define SSTCP_SRCCOLORKEYMAX   0x00000020L
#define SSTCP_DSTCOLORKEYMIN   0x00000040L
#define SSTCP_DSTCOLORKEYMAX   0x00000080L
#define SSTCP_BRESERROR0       0x00000100L
#define SSTCP_BRESERROR1       0x00000200L
#define SSTCP_ROP              0x00000400L
#define SSTCP_SRCBASEADDR      0x00000800L
#define SSTCP_COMMANDEXTRA     0x00001000L
#define SSTCP_LINESTIPPLE      0x00002000L
#define SSTCP_LINESTYLE        0x00004000L
#define SSTCP_PATTERN0ALIAS    0x00008000L
#define SSTCP_PATTERN1ALIAS    0x00010000L
#define SSTCP_CLIP1MIN         0x00020000L
#define SSTCP_CLIP1MAX         0x00040000L
#define SSTCP_SRCFORMAT        0x00080000L
#define SSTCP_SRCSIZE          0x00100000L
#define SSTCP_SRCXY            0x00200000L
#define SSTCP_COLORBACK        0x00400000L
#define SSTCP_COLORFORE        0x00800000L
#define SSTCP_DSTSIZE          0x01000000L
#define SSTCP_DSTXY            0x02000000L
#define SSTCP_COMMAND          0x04000000L

/* packet 3 defines */
#define SSTCP_PKT3_CMD_SHIFT	SSTCP_PKT_SIZE
#define SSTCP_PKT3_CMD		(0x7<<SSTCP_PKT3_CMD_SHIFT)
#  define SSTCP_PKT3_BDDBDD	(0<<SSTCP_PKT3_CMD_SHIFT)
#  define SSTCP_PKT3_BDDDDD	(1<<SSTCP_PKT3_CMD_SHIFT)
#  define SSTCP_PKT3_DDDDDD	(2<<SSTCP_PKT3_CMD_SHIFT)
#define SSTCP_PKT3_NUMVERTEX_SHIFT (SSTCP_PKT_SIZE+3)
#define SSTCP_PKT3_NUMVERTEX	(0xF << SSTCP_PKT3_NUMVERTEX_SHIFT)
#define SSTCP_PKT3_PMASK_SHIFT	(SSTCP_PKT_SIZE+3+4)
#define SSTCP_PKT3_PMASK	(0xFFFUL<<SSTCP_PKT3_PMASK_SHIFT)
#define SSTCP_PKT3_SMODE_SHIFT	(SSTCP_PKT3_PMASK_SHIFT+12)
#define SSTCP_PKT3_SMODE	(0x3FUL<<SSTCP_PKT3_SMODE_SHIFT)
#define SSTCP_PKT3_PACKEDCOLOR	BIT(28)

/* packet 4 defines */
#define SSTCP_REGBASE_SHIFT	SSTCP_PKT_SIZE
#ifdef H4
#define SSTCP_REGBASE		((0x7FF)<<SSTCP_REGBASE_SHIFT)
#else
#define SSTCP_REGBASE		((0x3FF)<<SSTCP_REGBASE_SHIFT)
#endif
#define SSTCP_PKT4_2D		BIT(14)
#define SSTCP_PKT4_MASK_SHIFT	15
#define SSTCP_PKT4_MASK		(0x3FFFUL<<SSTCP_PKT4_MASK_SHIFT)

#define SSTCP_PKT4_CLIP0MIN        (SSTCP_PKT4_2D | (0x02 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_CLIP0MAX        (SSTCP_PKT4_2D | (0x03 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTBASEADDR     (SSTCP_PKT4_2D | (0x04 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTFORMAT       (SSTCP_PKT4_2D | (0x05 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCCOLORKEYMIN  (SSTCP_PKT4_2D | (0x06 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCCOLORKEYMAX  (SSTCP_PKT4_2D | (0x07 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTCOLORKEYMIN  (SSTCP_PKT4_2D | (0x08 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTCOLORKEYMAX  (SSTCP_PKT4_2D | (0x09 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_BRESERROR0      (SSTCP_PKT4_2D | (0x0a << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_BRESERROR1      (SSTCP_PKT4_2D | (0x0b << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_ROP             (SSTCP_PKT4_2D | (0x0c << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCBASEADDR     (SSTCP_PKT4_2D | (0x0d << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_COMMANDEXTRA    (SSTCP_PKT4_2D | (0x0e << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_LINESTIPPLE     (SSTCP_PKT4_2D | (0x0f << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_LINESTYLE       (SSTCP_PKT4_2D | (0x10 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_PATTERN0ALIAS   (SSTCP_PKT4_2D | (0x11 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_PATTERN1ALIAS   (SSTCP_PKT4_2D | (0x12 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_CLIP1MIN        (SSTCP_PKT4_2D | (0x13 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_CLIP1MAX        (SSTCP_PKT4_2D | (0x14 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCFORMAT       (SSTCP_PKT4_2D | (0x15 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCSIZE         (SSTCP_PKT4_2D | (0x16 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_SRCXY           (SSTCP_PKT4_2D | (0x17 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_COLORBACK       (SSTCP_PKT4_2D | (0x18 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_COLORFORE       (SSTCP_PKT4_2D | (0x19 << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTSIZE         (SSTCP_PKT4_2D | (0x1a << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_DSTXY           (SSTCP_PKT4_2D | (0x1b << SSTCP_REGBASE_SHIFT))
#define SSTCP_PKT4_COMMAND         (SSTCP_PKT4_2D | (0x1c << SSTCP_REGBASE_SHIFT))

/* packet 5 defines */
#define SSTCP_PKT5_NWORDS_SHIFT	3
#define SSTCP_PKT5_NWORDS	(0x7FFFFUL<<SSTCP_PKT5_NWORDS_SHIFT)
#define SSTCP_PKT5_BYTEN_WN_SHIFT 22
#define SSTCP_PKT5_BYTEN_WN	(0xFUL<<SSTCP_PKT5_BYTEN_WN_SHIFT)
#define SSTCP_PKT5_BYTEN_W2_SHIFT 26
#define SSTCP_PKT5_BYTEN_W2	(0xFUL<<SSTCP_PKT5_BYTEN_W2_SHIFT)
#define SSTCP_PKT5_SPACE_SHIFT	30
#define SSTCP_PKT5_SPACE	(0x3UL<<SSTCP_PKT5_SPACE_SHIFT)
#define SSTCP_PKT5_LFB		(0x0UL<<SSTCP_PKT5_SPACE_SHIFT)
#define SSTCP_PKT5_YUV		(0x1UL<<SSTCP_PKT5_SPACE_SHIFT)
#define SSTCP_PKT5_3DLFB	(0x2UL<<SSTCP_PKT5_SPACE_SHIFT)
#define SSTCP_PKT5_TEXPORT	(0x3UL<<SSTCP_PKT5_SPACE_SHIFT)
#define SSTCP_PKT5_BASEADDR	0x1FFFFFFUL

/* packet 6 defines */
#define SSTCP_PKT6_SPACE_SHIFT  3
#define SSTCP_PKT6_SPACE        (0x3UL<<SSTCP_PKT6_SPACE_SHIFT)
#define SSTCP_PKT6_LFB          (0x0UL<<SSTCP_PKT6_SPACE_SHIFT)
#define SSTCP_PKT6_YUV          (0x1UL<<SSTCP_PKT6_SPACE_SHIFT)
#define SSTCP_PKT6_3DLFB        (0x2UL<<SSTCP_PKT6_SPACE_SHIFT)
#define SSTCP_PKT6_TEXPORT      (0x3UL<<SSTCP_PKT6_SPACE_SHIFT)
#define SSTCP_PKT6_NBYTES_SHIFT 5
#define SSTCP_PKT6_NBYTES       (SST_MASK(20) << SSTCP_PKT6_NBYTES_SHIFT)
#define SSTCP_PKT6_SRC_BASELOW            SST_MASK(32)
#define SSTCP_PKT6_SRC_WIDTH              SST_MASK(14)
#define SSTCP_PKT6_SRC_STRIDE_SHIFT       14
#define SSTCP_PKT6_SRC_STRIDE             (SST_MASK(14) <<SSTCP_PKT6_SRC_STRIDE_SHIFT)
#define SSTCP_PKT6_SRC_BASEHIGH_SHIFT     28
#define SSTCP_PKT6_SRC_BASEHIGH           (SST_MASK(4) <<SSTCP_PKT6_SRC_BASEHIGH_SHIFT)
#define SSTCP_PKT6_FRAME_BUFFER_OFFSET    SST_MASK(26)
#define SSTCP_PKT6_DST_STRIDE             SST_MASK(15)

#define SST_FIFO_OFFSET 0x0080000
#define SST_FIFO_BASEADDR0 SST_FIFO_OFFSET+0x20
#define SST_FIFO_BASESIZE0 SST_FIFO_OFFSET+0x24
#define SST_FIFO_BUMP0 SST_FIFO_OFFSET+0x28
#define SST_FIFO_RDPTRL0 SST_FIFO_OFFSET+0x2c
#define SST_FIFO_RDPTRH0 SST_FIFO_OFFSET+0x30
#define SST_FIFO_AMIN0 SST_FIFO_OFFSET+0x34
#define SST_FIFO_AMAX0 SST_FIFO_OFFSET+0x3c
#define SST_FIFO_DEPTH0 SST_FIFO_OFFSET+0x44
#define SST_FIFO_HOLECNT0 SST_FIFO_OFFSET+0x48

#define SST_FIFO_BASEADDR1 SST_FIFO_OFFSET+0x50
#define SST_FIFO_BASESIZE1 SST_FIFO_OFFSET+0x54
#define SST_FIFO_BUMP1 SST_FIFO_OFFSET+0x58
#define SST_FIFO_RDPTRL1 SST_FIFO_OFFSET+0x5c
#define SST_FIFO_RDPTRH1 SST_FIFO_OFFSET+0x60
#define SST_FIFO_AMIN1 SST_FIFO_OFFSET+0x64
#define SST_FIFO_AMAX1 SST_FIFO_OFFSET+0x6c
#define SST_FIFO_DEPTH1 SST_FIFO_OFFSET+0x74
#define SST_FIFO_HOLECNT1 SST_FIFO_OFFSET+0x78

#define SST_FIFO_FIFOTHRESH SST_FIFO_OFFSET+0x80

#endif
