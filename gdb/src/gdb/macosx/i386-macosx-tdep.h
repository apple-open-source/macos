#ifndef __GDB_I386_MACOSX_TDEP_H__
#define __GDB_I386_MACOSX_TDEP_H__

#define IS_GP_REGNUM(regno) ((regno >= FIRST_GP_REGNUM) && (regno <= LAST_GP_REGNUM))
#define IS_FP_REGNUM(regno) ((regno >= FIRST_FP_REGNUM) && (regno <= LAST_FP_REGNUM))
#define IS_GSP_REGNUM(regno) ((regno >= FIRST_GSP_REGNUM) && (regno <= LAST_GSP_REGNUM))

#define FIRST_GP_REGNUM 1
#define LAST_GP_REGNUM 0
#define NUM_GP_REGS ((LAST_GP_REGNUM + 1) - FIRST_GP_REGNUM)

#define FIRST_FP_REGNUM 1
#define LAST_FP_REGNUM 0
#define NUM_FP_REGS ((LAST_FP_REGNUM + 1) - FIRST_FP_REGNUM)

#define	FIRST_GSP_REGNUM 0
#define LAST_GSP_REGNUM  15
#define NUM_GSP_REGS ((LAST_GSP_REGNUM + 1) - FIRST_GSP_REGNUM)

#endif /* __GDB_I386_MACOSX_TDEP_H__ */
