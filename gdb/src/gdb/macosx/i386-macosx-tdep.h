#ifndef __GDB_I386_MACOSX_TDEP_H__
#define __GDB_I386_MACOSX_TDEP_H__

#define IS_GP_REGNUM(regno) ((regno >= FIRST_GP_REGNUM) && (regno <= LAST_GP_REGNUM))
#define IS_FP_REGNUM(regno) ((regno >= FIRST_FP_REGNUM) && (regno <= LAST_FP_REGNUM))
#define IS_VP_REGNUM(regno) ((regno >= FIRST_VP_REGNUM) && (regno <= LAST_VP_REGNUM))

#define FIRST_GP_REGNUM 0
#define LAST_GP_REGNUM 15
#define NUM_GP_REGS ((LAST_GP_REGNUM + 1) - FIRST_GP_REGNUM)

#define FIRST_FP_REGNUM 16
#define LAST_FP_REGNUM 31
#define NUM_FP_REGS ((LAST_FP_REGNUM + 1) - FIRST_FP_REGNUM)

#define	FIRST_VP_REGNUM 32
#define LAST_VP_REGNUM 40
#define NUM_VP_REGS ((LAST_VP_REGNUM + 1) - FIRST_VP_REGNUM)

void i386_macosx_fetch_gp_registers (gdb_i386_thread_state_t *sp_regs);
void i386_macosx_store_gp_registers (gdb_i386_thread_state_t *sp_regs);

void i386_macosx_fetch_fp_registers (gdb_i386_thread_fpstate_t *fp_regs);
void i386_macosx_store_fp_registers (gdb_i386_thread_fpstate_t *fp_regs);

#endif /* __GDB_I386_MACOSX_TDEP_H__ */
