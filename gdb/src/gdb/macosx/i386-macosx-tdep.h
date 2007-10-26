#ifndef __GDB_I386_MACOSX_TDEP_H__
#define __GDB_I386_MACOSX_TDEP_H__

#include "i386-macosx-thread-status.h"

#define IS_GP_REGNUM(regno) ((regno >= FIRST_GP_REGNUM) && (regno <= LAST_GP_REGNUM))
#define IS_FP_REGNUM(regno) ((regno >= FIRST_FP_REGNUM) && (regno <= LAST_FP_REGNUM))
#define IS_VP_REGNUM(regno) ((regno >= FIRST_VP_REGNUM) && (regno <= LAST_VP_REGNUM))

#define FIRST_GP_REGNUM 0
#define LAST_GP_REGNUM 15
#define NUM_GP_REGS ((LAST_GP_REGNUM + 1) - FIRST_GP_REGNUM)

#define FIRST_FP_REGNUM 16
#define LAST_FP_REGNUM 31
#define NUM_FP_REGS ((LAST_FP_REGNUM + 1) - FIRST_FP_REGNUM)

#define FIRST_VP_REGNUM 32
#define LAST_VP_REGNUM 40
#define NUM_VP_REGS ((LAST_VP_REGNUM + 1) - FIRST_VP_REGNUM)

#define IS_GP_REGNUM_64(regno) ((regno >= FIRST_GP_REGNUM_64) && (regno <= LAST_GP_REGNUM_64))
#define IS_FP_REGNUM_64(regno) ((regno >= FIRST_FP_REGNUM_64) && (regno <= LAST_FP_REGNUM_64))
#define IS_VP_REGNUM_64(regno) ((regno >= FIRST_VP_REGNUM_64) && (regno <= LAST_VP_REGNUM_64))

#define FIRST_GP_REGNUM_64 0
#define LAST_GP_REGNUM_64 23
#define NUM_GP_REGS_64 ((LAST_GP_REGNUM_64 + 1) - FIRST_GP_REGNUM_64)

#define FIRST_FP_REGNUM_64 24
#define LAST_FP_REGNUM_64 39
#define NUM_FP_REGS_64 ((LAST_FP_REGNUM_64 + 1) - FIRST_FP_REGNUM_64)

#define FIRST_VP_REGNUM_64 40
#define LAST_VP_REGNUM_64 55
#define NUM_VP_REGS_64 ((LAST_VP_REGNUM_64 + 1) - FIRST_VP_REGNUM_64)

/* All fetch functions below that don't end with '_raw' assume that the
   register structure argument is in host endian byte order and, if
   needed, endian swap the values into target endian format and store 
   the values into the register cache. The register cache stores 
   register values in target endian byte order.
   
   All fetch functions that do end with '_raw' will trust that the
   register structure argument is already in target endian format and
   will copy the values in without swapping.  */

void i386_macosx_fetch_gp_registers (gdb_i386_thread_state_t *sp_regs);
void i386_macosx_fetch_gp_registers_raw (gdb_i386_thread_state_t *sp_regs);
void i386_macosx_store_gp_registers (gdb_i386_thread_state_t *sp_regs);
void i386_macosx_store_gp_registers_raw (gdb_i386_thread_state_t *sp_regs);
void x86_64_macosx_fetch_gp_registers (gdb_x86_thread_state64_t *sp_regs);
void x86_64_macosx_fetch_gp_registers_raw (gdb_x86_thread_state64_t *sp_regs);
void x86_64_macosx_store_gp_registers (gdb_x86_thread_state64_t *sp_regs);
void x86_64_macosx_store_gp_registers_raw (gdb_x86_thread_state64_t *sp_regs);
void i386_macosx_fetch_fp_registers (gdb_i386_float_state_t *fp_regs);
void i386_macosx_fetch_fp_registers_raw (gdb_i386_float_state_t *fp_regs);
int  i386_macosx_store_fp_registers (gdb_i386_float_state_t *fp_regs);
int  i386_macosx_store_fp_registers_raw (gdb_i386_float_state_t *fp_regs);
void x86_64_macosx_fetch_fp_registers (gdb_x86_float_state64_t *fp_regs);
void x86_64_macosx_fetch_fp_registers_raw (gdb_x86_float_state64_t *fp_regs);
int  x86_64_macosx_store_fp_registers (gdb_x86_float_state64_t *fp_regs);
int  x86_64_macosx_store_fp_registers_raw (gdb_x86_float_state64_t *fp_regs);

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

#endif /* __GDB_I386_MACOSX_TDEP_H__ */
