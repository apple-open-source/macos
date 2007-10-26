#ifndef __GDB_PPC_MACOSX_THREAD_STATUS_H__
#define __GDB_PPC_MACOSX_THREAD_STATUS_H__

#define GDB_PPC_THREAD_STATE 1
#define GDB_PPC_THREAD_FPSTATE 2
#define GDB_PPC_THREAD_VPSTATE 4
#define GDB_PPC_THREAD_STATE_64 5

struct gdb_ppc_thread_state
{

  unsigned int srr0;            /* program counter */
  unsigned int srr1;            /* machine state register */

  unsigned int gpregs[32];

  unsigned int cr;              /* condition register */
  unsigned int xer;             /* integer exception register */
  unsigned int lr;              /* link register */
  unsigned int ctr;
  unsigned int mq;

  unsigned int vrsave;          /* vector save register */
};

typedef struct gdb_ppc_thread_state gdb_ppc_thread_state_t;

struct gdb_ppc_thread_state_64
{

  unsigned long long srr0;      /* program counter */
  unsigned long long srr1;      /* machine state register */

  unsigned long long gpregs[32];

  unsigned int cr;              /* condition register */
  unsigned long long xer;       /* integer exception register */
  unsigned long long lr;        /* link register */
  unsigned long long ctr;

  unsigned int vrsave;          /* vector save register */
};

typedef struct gdb_ppc_thread_state_64 gdb_ppc_thread_state_64_t;

struct gdb_ppc_thread_fpstate
{

  double fpregs[32];

  unsigned int fpscr_pad;       /* fpscr is 64 bits; first 32 are unused */
  unsigned int fpscr;           /* floating point status register */
};

typedef struct gdb_ppc_thread_fpstate gdb_ppc_thread_fpstate_t;

struct gdb_ppc_thread_vpstate
{
  unsigned long save_vr[32][4];
  unsigned long save_vscr[4];
  unsigned int save_pad5[4];
  unsigned int save_vrvalid;    /* vrs that have been saved */
  unsigned int save_pad6[7];
};

typedef struct gdb_ppc_thread_vpstate gdb_ppc_thread_vpstate_t;

#define GDB_PPC_THREAD_STATE_COUNT \
  (sizeof (gdb_ppc_thread_state_t) / sizeof (unsigned int))

#define GDB_PPC_THREAD_STATE_64_COUNT \
  (sizeof (gdb_ppc_thread_state_64_t) / sizeof (unsigned int))

#define GDB_PPC_THREAD_FPSTATE_COUNT \
  (sizeof (gdb_ppc_thread_fpstate_t) / sizeof (unsigned int))

#define GDB_PPC_THREAD_VPSTATE_COUNT \
  (sizeof (gdb_ppc_thread_vpstate_t) / sizeof (unsigned int))

#endif /* __GDB_PPC_MACOSX_THREAD_STATUS_H__ */
