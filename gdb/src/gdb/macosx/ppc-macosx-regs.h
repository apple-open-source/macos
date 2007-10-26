#ifndef __GDB_PPC_MACOSX_REGS_H__
#define __GDB_PPC_MACOSX_REGS_H__

#include "ppc-macosx-thread-status.h"

/* All fetch functions that don't end with '_raw' assume that the
   register structure argument is in host endian byte order and, if
   needed, endian swap the values into target endian format and store 
   the values into the register cache. The register cache stores 
   register values in target endian byte order.
   
   All fetch functions that do end with '_raw' will trust that the
   register structure argument is already in target endian format and
   will copy the values in without swapping.  */

void ppc_macosx_fetch_gp_registers (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_fetch_gp_registers_raw (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_store_gp_registers (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_store_gp_registers_raw (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_fetch_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_fetch_gp_registers_64_raw (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_store_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_store_gp_registers_64_raw (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_fetch_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_fetch_fp_registers_raw (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_store_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_store_fp_registers_raw (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_fetch_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs);
void ppc_macosx_fetch_vp_registers_raw (gdb_ppc_thread_vpstate_t *vp_regs);
void ppc_macosx_store_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs);
void ppc_macosx_store_vp_registers_raw (gdb_ppc_thread_vpstate_t *vp_regs);

int ppc_macosx_stab_reg_to_regnum (int num);

#endif /* __GDB_PPC_MACOSX_REGS_H__ */
