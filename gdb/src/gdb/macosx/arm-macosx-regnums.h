#ifndef __GDB_ARM_MACOSX_REGNUMS_H__
#define __GDB_ARM_MACOSX_REGNUMS_H__

/* We assume that one of the two following header files will already
   have been included prior to including this header file:
   #include "arm-tdep.h"      (for gdb)
   #include "arm-regnums.h"   (for gdbserver)
 */

#define ARM_MACOSX_FIRST_VFP_STABS_REGNUM 63
#define ARM_MACOSX_LAST_VFP_STABS_REGNUM  94
#define ARM_MACOSX_NUM_GP_REGS 16
#define ARM_MACOSX_NUM_GPS_REGS 1
#define ARM_MACOSX_NUM_FP_REGS 8
#define ARM_MACOSX_NUM_FPS_REGS 1
#define ARM_MACOSX_NUM_VFP_REGS 32
#define ARM_MACOSX_NUM_VFPS_REGS 1
#define ARM_MACOSX_NUM_VFP_PSEUDO_REGS 16

#define ARM_MACOSX_NUM_REGS (ARM_MACOSX_NUM_GP_REGS \
                             + ARM_MACOSX_NUM_GPS_REGS \
			     + ARM_MACOSX_NUM_FP_REGS \
			     + ARM_MACOSX_NUM_FPS_REGS)

#define ARM_V6_MACOSX_NUM_REGS (ARM_MACOSX_NUM_REGS \
                             + ARM_MACOSX_NUM_VFP_REGS \
			     + ARM_MACOSX_NUM_VFPS_REGS)

#define ARM_MACOSX_IS_GP_REGNUM(regno) (((regno) >= ARM_R0_REGNUM) \
    && ((regno) <= ARM_PC_REGNUM))
#define ARM_MACOSX_IS_GPS_REGNUM(regno) ((regno) == ARM_PS_REGNUM)
/* Any GP reg: r0-r15, cpsr.  */
#define ARM_MACOSX_IS_GP_RELATED_REGNUM(regno) (ARM_MACOSX_IS_GP_REGNUM(regno) \
    || ARM_MACOSX_IS_GPS_REGNUM(regno))
/* Any FP reg: f0-f7, fps.  */
#define ARM_MACOSX_IS_FP_RELATED_REGNUM(regno)  (((regno) >= ARM_F0_REGNUM) \
    && ((regno) <= ARM_FPS_REGNUM))
/* Any VFP reg: s0-s31, d0-d15, fpscr.  */
#define ARM_MACOSX_IS_VFP_RELATED_REGNUM(regno) (((regno) >= ARM_FIRST_VFP_REGNUM) \
    && ((regno) <= ARM_LAST_VFP_PSEUDO_REGNUM))

#endif /* __GDB_ARM_MACOSX_REGNUMS_H__ */
