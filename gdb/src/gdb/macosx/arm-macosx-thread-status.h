#ifndef __GDB_ARM_MACOSX_THREAD_STATUS_H__
#define __GDB_ARM_MACOSX_THREAD_STATUS_H__

#define GDB_ARM_THREAD_STATE 1
#define GDB_ARM_THREAD_FPSTATE     2 /* Equivalent to ARM_VFP_STATE */

/* This structure comes from /usr/include/mach/arm/_types.h */
#include <stdint.h>

struct gdb_arm_thread_state
{
  uint32_t        r[16];          /* General purpose register r0-r15 */
  uint32_t        cpsr;           /* Current program status register */
};

typedef struct gdb_arm_thread_state gdb_arm_thread_state_t;

#define GDB_ARM_THREAD_STATE_COUNT \
    (sizeof (struct gdb_arm_thread_state) / sizeof (uint32_t))

struct gdb_arm_thread_fpstate
{
  uint32_t        r[32];
  uint32_t        fpscr;
};

typedef struct gdb_arm_thread_fpstate gdb_arm_thread_fpstate_t;

#define GDB_ARM_THREAD_FPSTATE_COUNT \
    (sizeof (struct gdb_arm_thread_fpstate) / sizeof (uint32_t))


#endif /* __GDB_ARM_MACOSX_THREAD_STATUS_H__ */

