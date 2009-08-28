/* Common target dependent code for GDB on ARM systems.
   Copyright 2002, 2003 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Register numbers of various important registers.  Note that some of
   these values are "real" register numbers, and correspond to the
   general registers of the machine, and some are "phony" register
   numbers which are too large to be actual register numbers as far as
   the user is concerned but do serve to get the desired values when
   passed to read_register.  */
   
/* APPLE LOCAL: Use R7 as FP for ARM for Darwin, use R11 for everything 
   else. */
#ifdef TM_NEXTSTEP
#define __ARM_FP_REG 7
#else
#define __ARM_FP_REG 11
#endif

enum gdb_regnum {
  ARM_R0_REGNUM = 0,
  ARM_A1_REGNUM = 0,		/* first integer-like argument */
  ARM_A4_REGNUM = 3,		/* last integer-like argument */
  ARM_AP_REGNUM = 11,
  ARM_SP_REGNUM = 13,		/* Contains address of top of stack */
  ARM_LR_REGNUM = 14,		/* address to return to from a function call */
  ARM_PC_REGNUM = 15,		/* Contains program counter */
  ARM_F0_REGNUM = 16,		/* first floating point register */
  ARM_F3_REGNUM = 19,		/* last floating point argument register */
  ARM_F7_REGNUM = 23, 		/* last floating point register */
  ARM_FPS_REGNUM = 24,		/* floating point status register */
  ARM_PS_REGNUM = 25,		/* Contains processor status */
  /* APPLE LOCAL: Allow alternate FP register number for ARM. */
  ARM_FP_REGNUM = __ARM_FP_REG,	/* Frame register in ARM code, if used.  */
  THUMB_FP_REGNUM = 7,		/* Frame register in Thumb code, if used.  */
  ARM_NUM_ARG_REGS = 4, 
  ARM_LAST_ARG_REGNUM = ARM_A4_REGNUM,
  ARM_NUM_FP_ARG_REGS = 4,
  ARM_LAST_FP_ARG_REGNUM = ARM_F3_REGNUM,
  /* APPLE LOCAL START: Support for VFP.  */
  ARM_FIRST_VFP_REGNUM = 26,
  ARM_LAST_VFP_REGNUM = 57,
  ARM_FPSCR_REGNUM = 58,
  ARM_FIRST_VFP_PSEUDO_REGNUM = 59,
  ARM_LAST_VFP_PSEUDO_REGNUM = 74,
  ARM_NUM_VFP_ARG_REGS = 4,
  ARM_NUM_VFP_PSEUDO_REGS = 16
  /* APPLE LOCAL END: Support for VFP. */
};

/* Used in target-specific code when we need to know the size of the
   largest type of register we need to handle.  */
#define ARM_MAX_REGISTER_RAW_SIZE	12
#define ARM_MAX_REGISTER_VIRTUAL_SIZE	8

/* Size of integer registers.  */
#define INT_REGISTER_RAW_SIZE		4
#define INT_REGISTER_VIRTUAL_SIZE	4

/* Say how long FP registers are.  Used for documentation purposes and
   code readability in this header.  IEEE extended doubles are 80
   bits.  DWORD aligned they use 96 bits.  */
#define FP_REGISTER_RAW_SIZE	12

/* GCC doesn't support long doubles (extended IEEE values).  The FP
   register virtual size is therefore 64 bits.  Used for documentation
   purposes and code readability in this header.  */
#define FP_REGISTER_VIRTUAL_SIZE	8

/* APPLE LOCAL BEGIN: VFP support.  */
#define VFP_REGISTER_RAW_SIZE 4
#define VFP_REGISTER_VIRTUAL_SIZE 4
/* APPLE LOCAL END: VFP support.  */

/* Status registers are the same size as general purpose registers.
   Used for documentation purposes and code readability in this
   header.  */
#define STATUS_REGISTER_SIZE	4

/* Number of machine registers.  The only define actually required 
   is NUM_REGS.  The other definitions are used for documentation
   purposes and code readability.  */
/* For 26 bit ARM code, a fake copy of the PC is placed in register 25 (PS)
   (and called PS for processor status) so the status bits can be cleared
   from the PC (register 15).  For 32 bit ARM code, a copy of CPSR is placed
   in PS.  */
#define NUM_FREGS	8	/* Number of floating point registers.  */
#define NUM_SREGS	2	/* Number of status registers.  */
#define NUM_GREGS	16	/* Number of general purpose registers.  */
/* APPLE LOCAL: VFP support.  */
#define NUM_VFPREGS     32      /* Number of VFP registers.  */

/* Instruction condition field values.  */
#define INST_EQ		0x0
#define INST_NE		0x1
#define INST_CS		0x2
#define INST_CC		0x3
#define INST_MI		0x4
#define INST_PL		0x5
#define INST_VS		0x6
#define INST_VC		0x7
#define INST_HI		0x8
#define INST_LS		0x9
#define INST_GE		0xa
#define INST_LT		0xb
#define INST_GT		0xc
#define INST_LE		0xd
#define INST_AL		0xe
#define INST_NV		0xf

/* Program Status Register (PSR) definitions.  */
#define FLAG_MODE_MASK	0x0000001f
#define FLAG_T		(1<<5)
#define FLAG_F		(1<<6)
#define FLAG_I		(1<<7)
#define FLAG_A		(1<<8)
#define FLAG_E		(1<<9)
#define FLAG_GE_MASK	0x000f0000
#define FLAG_J		(1<<24)
#define FLAG_Q		(1<<27)
#define FLAG_V		(1<<28)
#define FLAG_C		(1<<29)
#define FLAG_Z		(1<<30)
#define FLAG_N		(1<<31)



