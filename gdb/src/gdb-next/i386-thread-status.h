#if 0
#ifndef _GDB_MACH_I386_THREAD_STATUS_H_
#define _GDB_MACH_I386_THREAD_STATUS_H_

#ifndef i386_THREAD_STATE
#include <mach/i386/thread_status.h>
#endif

#define GDB_i386_THREAD_STATE i386_THREAD_STATE
#define GDB_i386_THREAD_STATE_COUNT i386_THREAD_STATE_COUNT
#define GDB_i386_THREAD_FPSTATE i386_THREAD_FPSTATE
#define GDB_i386_THREAD_FPSTATE_COUNT i386_THREAD_FPSTATE_COUNT

typedef i386_thread_state_t gdb_i386_thread_state_t;
typedef i386_thread_fpstate_t gdb_i386_thread_fpstate_t;

#endif /* _GDB_MACH_I386_THREAD_STATUS_H_ */
#endif /* 1 */

#ifndef _GDB_MACH_I386_THREAD_STATUS_H_
#define _GDB_MACH_I386_THREAD_STATUS_H_

#define GDB_i386_THREAD_STATE -1
#define GDB_i386_THREAD_FPSTATE	-2

typedef struct gdb_sel {
  unsigned short rpl : 2;
#define KERN_PRIV 0
#define USER_PRIV 3
  unsigned short ti : 1;
#define SEL_GDT 0
#define SEL_LDT 1
  unsigned short index : 13;
} gdb_sel_t;

#define GDB_NULL_SEL ((gdb_sel_t) { 0, 0, 0 } )

typedef struct gdb_fp_data_reg {
  unsigned short mant;
  unsigned short mant1 : 16;
  unsigned short mant2 : 16;
  unsigned short mant3 : 16;
  unsigned short exp : 15;
  unsigned short sign : 1;
} gdb_fp_data_reg_t;

typedef struct gdb_fp_stack {
  gdb_fp_data_reg_t ST[8];
} gdb_fp_stack_t;

typedef struct gdb_fp_tag {
  unsigned short tag0 : 2;
  unsigned short tag1 : 2;
  unsigned short tag2 : 2;
  unsigned short tag3 : 2;
  unsigned short tag4 : 2;
  unsigned short tag5 : 2;
  unsigned short tag6 : 2;
  unsigned short tag7 : 2;
} gdb_fp_tag_t;

#define FP_TAG_VALID 0
#define FP_TAG_ZERO 1
#define FP_TAG_SPEC 2
#define FP_TAG_EMPTY 3

typedef struct gdb_fp_status {
  unsigned short invalid : 1;
  unsigned short denorm : 1;
  unsigned short zdiv : 1;
  unsigned short ovrfl: 1;
  unsigned short undfl : 1;
  unsigned short precis : 1;
  unsigned short stkflt : 1;
  unsigned short errsumm : 1;
  unsigned short c0 : 1;
  unsigned short c1 : 1;
  unsigned short c2 : 1;
  unsigned short tos : 3;
  unsigned short c3 : 1;
  unsigned short busy : 1;
} gdb_fp_status_t;

typedef struct gdb_fp_control {
  unsigned short invalid : 1;
  unsigned short denorm : 1;
  unsigned short zdiv : 1;
  unsigned short ovrfl : 1;
  unsigned short undfl : 1;
  unsigned short precis : 1;
  unsigned short : 2;
  unsigned short pc : 2;
  unsigned short rc : 2;
  unsigned short : 1;
  unsigned short : 3;
} gdb_fp_control_t;

#define FP_PREC_24B 0
#define FP_PREC_53B 2
#define FP_PREC_64B 3

#define FP_RND_NEAR 0
#define FP_RND_DOWN 1
#define FP_RND_UP 2
#define FP_CHOP 3

typedef struct gdb_fp_env {
  gdb_fp_control_t control;
  unsigned short : 16;
  gdb_fp_status_t status;
  unsigned short : 16;
  gdb_fp_tag_t tag;
  unsigned short : 16;
  unsigned int ip;
  gdb_sel_t cs;
  unsigned short opcode;
  unsigned int dp;
  gdb_sel_t ds;
  unsigned short : 16;
} gdb_fp_env_t;

typedef struct gdb_i386_thread_state {
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
  unsigned int edi;
  unsigned int esi;
  unsigned int ebp;
  unsigned int esp;
  unsigned int ss;
  unsigned int eflags;
  unsigned int eip;
  unsigned int cs;
  unsigned int ds;
  unsigned int es;
  unsigned int fs;
  unsigned int gs;
} gdb_i386_thread_state_t;

typedef struct gdb_i386_thread_fpstate {
  gdb_fp_env_t environ;
  gdb_fp_stack_t stack;
} gdb_i386_thread_fpstate_t;

#define GDB_i386_THREAD_STATE_COUNT \
    (sizeof (gdb_i386_thread_state_t) / sizeof (unsigned int))

#define GDB_i386_THREAD_FPSTATE_COUNT \
    (sizeof (gdb_i386_thread_fpstate_t) / sizeof (unsigned int))

#endif /* _GDB_MACH_I386_THREAD_STATUS_H_ */
