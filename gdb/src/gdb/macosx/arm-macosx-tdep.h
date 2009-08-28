#ifndef __GDB_ARM_MACOSX_TDEP_H__
#define __GDB_ARM_MACOSX_TDEP_H__
#include "arm-macosx-thread-status.h"
#include "arm-macosx-regnums.h"

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

void arm_macosx_fetch_gp_registers (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_fetch_gp_registers_raw (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_store_gp_registers (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_store_gp_registers_raw (struct gdb_arm_thread_state *gp_regs);
void arm_macosx_fetch_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs);
void arm_macosx_fetch_vfp_registers_raw (struct gdb_arm_thread_fpstate *fp_regs);
void arm_macosx_store_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs);
void arm_macosx_store_vfp_registers_raw (struct gdb_arm_thread_fpstate *fp_regs);

#endif /* __GDB_ARM_MACOSX_TDEP_H__ */
