/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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

#include "ppc-macosx-regs.h"

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"

#include "ppc-macosx-regs.h"

void ppc_macosx_fetch_gp_registers (unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs)
{
  int i;
  for (i = 0; i < NUM_GP_REGS; i++) {
    store_unsigned_integer (rdata + (REGISTER_BYTE (FIRST_GP_REGNUM + i)), 
			    sizeof (REGISTER_TYPE), 
			    gp_regs->gpregs[i]);
  }
}

void ppc_macosx_store_gp_registers (unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs)
{
  int i;
  for (i = 0; i < NUM_GP_REGS; i++) {
    gp_regs->gpregs[i] = extract_unsigned_integer (rdata + (REGISTER_BYTE (FIRST_GP_REGNUM + i)),
						   sizeof (REGISTER_TYPE));
  }
}

void ppc_macosx_fetch_sp_registers (unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs)
{
  store_unsigned_integer (rdata + (REGISTER_BYTE (PC_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->srr0);
  store_unsigned_integer (rdata + (REGISTER_BYTE (PS_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->srr1);
  store_unsigned_integer (rdata + (REGISTER_BYTE (CR_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->cr);
  store_unsigned_integer (rdata + (REGISTER_BYTE (LR_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->lr);
  store_unsigned_integer (rdata + (REGISTER_BYTE (CTR_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->ctr);
  store_unsigned_integer (rdata + (REGISTER_BYTE (XER_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->xer);
  store_unsigned_integer (rdata + (REGISTER_BYTE (MQ_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->mq);
  /* store_unsigned_integer (rdata + (REGISTER_BYTE (VRSAVE_REGNUM)), sizeof (REGISTER_TYPE), gp_regs->vrsave); */
}

void ppc_macosx_store_sp_registers (unsigned char *rdata, gdb_ppc_thread_state_t *gp_regs)
{
  gp_regs->srr0 = extract_unsigned_integer (rdata + (REGISTER_BYTE (PC_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->srr1 = extract_unsigned_integer (rdata + (REGISTER_BYTE (PS_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->cr = extract_unsigned_integer (rdata + (REGISTER_BYTE (CR_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->lr = extract_unsigned_integer (rdata + (REGISTER_BYTE (LR_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->ctr = extract_unsigned_integer (rdata + (REGISTER_BYTE (CTR_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->xer = extract_unsigned_integer (rdata + (REGISTER_BYTE (XER_REGNUM)), sizeof (REGISTER_TYPE));
  gp_regs->mq = extract_unsigned_integer (rdata + (REGISTER_BYTE (MQ_REGNUM)), sizeof (REGISTER_TYPE));
  /* gp_regs->vrsave = extract_unsigned_integer (rdata + (REGISTER_BYTE (VRSAVE_REGNUM)), sizeof (REGISTER_TYPE)); */
}

void ppc_macosx_fetch_fp_registers (unsigned char *rdata, gdb_ppc_thread_fpstate_t *fp_regs)
{
  int i;
  FP_REGISTER_TYPE *fpr = fp_regs->fpregs;
  for (i = 0; i < NUM_FP_REGS; i++) {
    store_floating (rdata + (REGISTER_BYTE (FIRST_FP_REGNUM + i)),
		    sizeof (FP_REGISTER_TYPE), fpr[i]);
  }
  store_unsigned_integer (rdata + (REGISTER_BYTE (FPSCR_REGNUM)), sizeof (REGISTER_TYPE), fp_regs->fpscr);
}
  
void ppc_macosx_store_fp_registers (unsigned char *rdata, gdb_ppc_thread_fpstate_t *fp_regs)
{
  int i;
  FP_REGISTER_TYPE *fpr = fp_regs->fpregs;
  for (i = 0; i < NUM_FP_REGS; i++) {
    fpr[i] = extract_floating (rdata + (REGISTER_BYTE (FIRST_FP_REGNUM + i)), 
			       sizeof (FP_REGISTER_TYPE));
  }
  fp_regs->fpscr_pad = 0;
  fp_regs->fpscr = extract_unsigned_integer (rdata + (REGISTER_BYTE (FPSCR_REGNUM)), sizeof (REGISTER_TYPE));
}

void ppc_macosx_fetch_vp_registers (unsigned char *rdata, gdb_ppc_thread_vpstate_t *vp_regs)
{
  int i, j;
  for (i = 0; i < NUM_VP_REGS; i++) {
    for (j = 0; j < 4; j++) {
      store_unsigned_integer (rdata + (REGISTER_BYTE (FIRST_VP_REGNUM + i)) + (j * 4), 4, vp_regs->save_vr[i][j]);
    }
  }
  store_unsigned_integer (rdata + (REGISTER_BYTE (VSCR_REGNUM)), sizeof (REGISTER_TYPE), vp_regs->save_vscr[3]);
  store_unsigned_integer (rdata + (REGISTER_BYTE (VRSAVE_REGNUM)), sizeof (REGISTER_TYPE), vp_regs->save_vrvalid);
}
  
void ppc_macosx_store_vp_registers (unsigned char *rdata, gdb_ppc_thread_vpstate_t *vp_regs)
{
  int i, j;
  for (i = 0; i < NUM_VP_REGS; i++) {
    for (j = 0; j < 4; j++) {
      vp_regs->save_vr[i][j] = extract_unsigned_integer (rdata + (REGISTER_BYTE (FIRST_VP_REGNUM + i)) + (j * 4), 4);
    }
  }
  memset (&vp_regs->save_vscr, 0, sizeof (vp_regs->save_vscr));
  vp_regs->save_vscr[3] = extract_unsigned_integer (rdata + (REGISTER_BYTE (VSCR_REGNUM)), sizeof (REGISTER_TYPE));
  memset (&vp_regs->save_pad5, 0, sizeof (vp_regs->save_pad5));
  vp_regs->save_vrvalid = extract_unsigned_integer (rdata + (REGISTER_BYTE (VRSAVE_REGNUM)), sizeof (REGISTER_TYPE));
  memset (&vp_regs->save_pad5, 0, sizeof (vp_regs->save_pad6));
}

/* Convert a dbx stab register number (from `r' declaration) to a gdb
   REGNUM. */

int
ppc_macosx_stab_reg_to_regnum (int num)
{
  int regnum;

  /* These are the ordinary GP & FP registers */

  if (num <= 64)
    {
      regnum = num;
    }
  /* These are the AltiVec registers */
  else if (num >= 77 && num < 109)
    {
      regnum = VP0_REGNUM + num - 77;
    }
  /* These are some of the SP registers */
  else
    {
      switch (num)
	{
	case 64: 
	  regnum = MQ_REGNUM;
	  break;
	case 65: 
	  regnum = LR_REGNUM;
	  break;
	case 66: 
	  regnum = CTR_REGNUM;
	  break;
	case 76: 
	  regnum = XER_REGNUM;
	  break;
	case 109:
	  regnum = VRSAVE_REGNUM;
	  break;
	default: 
	  regnum = num;
	  break;
	}
    }
  return regnum;
}
