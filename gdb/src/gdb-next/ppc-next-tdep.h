#include "tm-ppc.h"

#define IS_GP_REGNUM(regno) ((regno >= FIRST_GP_REGNUM) && (regno <= LAST_GP_REGNUM))
#define IS_FP_REGNUM(regno) ((regno >= FIRST_FP_REGNUM) && (regno <= LAST_FP_REGNUM))
#define IS_VP_REGNUM(regno) ((regno >= FIRST_VP_REGNUM) && (regno <= LAST_VP_REGNUM))

#define IS_GSP_REGNUM(regno) ((regno >= FIRST_GSP_REGNUM) && (regno <= LAST_GSP_REGNUM))
#define IS_FSP_REGNUM(regno) ((regno >= FIRST_FSP_REGNUM) && (regno <= LAST_FSP_REGNUM))
#define IS_VSP_REGNUM(regno) ((regno >= FIRST_VSP_REGNUM) && (regno <= LAST_VSP_REGNUM))

#include "ppc-thread-status.h"

void ppc_next_fetch_sp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs));
void ppc_next_store_sp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs));
void ppc_next_fetch_gp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs));
void ppc_next_store_gp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs));
void ppc_next_fetch_fp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_fpstate_t *fp_regs));
void ppc_next_store_fp_registers PARAMS ((unsigned char *rdata, gdb_ppc_thread_fpstate_t *fp_regs));
CORE_ADDR ppc_next_skip_trampoline_code PARAMS ((CORE_ADDR pc));
int ppc_next_in_solib_return_trampoline PARAMS ((CORE_ADDR pc, char *name));
int ppc_next_in_solib_call_trampoline PARAMS ((CORE_ADDR pc, char *name));
