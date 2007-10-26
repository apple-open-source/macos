#ifndef __GDB_PPC_MACOSX_REGS_H__
#define __GDB_PPC_MACOSX_REGS_H__

#include "ppc-macosx-thread-status.h"

void ppc_macosx_fetch_gp_registers (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_store_gp_registers (gdb_ppc_thread_state_t *gp_regs);
void ppc_macosx_fetch_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_store_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs);
void ppc_macosx_fetch_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_store_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs);
void ppc_macosx_fetch_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs);
void ppc_macosx_store_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs);

int ppc_macosx_stab_reg_to_regnum (int num);

#endif /* __GDB_PPC_MACOSX_REGS_H__ */
