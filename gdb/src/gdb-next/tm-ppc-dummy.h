#define CALL_DUMMY { \
  \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  \
  0x7c0802a6, /* mflr   r0             */ \
  0xd8010000, /* stfd   r?, num(r1)    */ \
  0xbc010000, /* stm    r0, num(r1)    */ \
  0x94210000, /* stwu   r1, num(r1)    */ \
  0xfeedfeed, \
  0xfeedfeed, \
  /* save toc pointer */ \
  0x3c400000, /* addis  r2, 0, 0x0     */ \
  0x60420000, /* ori    r2, r2, 0x0    */ \
  /* save function pointer */ \
  0x3d800000, /* lis    r12, 0x0       */ \
  0x618c0000, /* ori    r12, r12, 0x0  */ \
  /* call function */ \
  0x7d8903a6, /* mtctr r12              */ \
  0x4e800421, /* bctrl                 */ \
  /* breakpoint for function return */ \
  0x7fe00008, /* trap                  */ \
  0x60000000, /* nop                   */ \
  0xfeedfeed, \
  0xfeedfeed, \
  \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed, \
  0xfeedfeed, 0xfeedfeed, 0xfeedfeed, 0xfeedfeed \
}

/* keep this as multiple of 16 ($sp requires 16 byte alignment) */

#define INSTRUCTION_SIZE 4

#define CALL_DUMMY_START_OFFSET      ((32 + 6) * INSTRUCTION_SIZE)
#define	TOC_ADDR_OFFSET              CALL_DUMMY_START_OFFSET
#define	TARGET_ADDR_OFFSET           (CALL_DUMMY_START_OFFSET + (2 * INSTRUCTION_SIZE))
#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + (6 * INSTRUCTION_SIZE))
#define CALL_DUMMY_LENGTH            ((32 + 16 + 32) * INSTRUCTION_SIZE)
