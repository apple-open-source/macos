/* Tcl/Tk command definitions for Insight - Registers
   Copyright 2001, 2002 Free Software Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "regcache.h"
#include "value.h"

#include <tcl.h>
#include "gdbtk.h"
#include "gdbtk-cmds.h"

/* This contains the previous values of the registers, since the last call to
   gdb_changed_register_list.

   It is an array of (NUM_REGS+NUM_PSEUDO_REGS)*MAX_REGISTER_RAW_SIZE bytes. */

static char *old_regs = NULL;

static int get_pc_register (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_register_info (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static void get_register (int, void *);
static void get_register_name (int, void *);
static void get_register_size (int regnum, void *arg);
static int map_arg_registers (int, Tcl_Obj * CONST[],
			      void (*)(int, void *), void *);
static void register_changed_p (int, void *);
static void setup_architecture_data (void);

int
Gdbtk_Register_Init (Tcl_Interp *interp)
{
  Tcl_CreateObjCommand (interp, "gdb_reginfo", gdbtk_call_wrapper,
                        gdb_register_info, NULL);
  Tcl_CreateObjCommand (interp, "gdb_pc_reg", gdbtk_call_wrapper, get_pc_register,
			NULL);

  /* Register/initialize any architecture specific data */
  setup_architecture_data ();
  register_gdbarch_swap (&old_regs, sizeof (old_regs), NULL);
  register_gdbarch_swap (NULL, 0, setup_architecture_data);

  return TCL_OK;
}

/* This implements the tcl command "gdb_reginfo".
 * It returns the requested information about registers.
 *
 * Tcl Arguments:
 *    OPTION    - "changed", "name", "size", "value" (see below)
 *    REGNUM(S) - the register(s) for which info is requested
 *
 * Tcl Result:
 *    The requested information
 *
 * Options:
 * changed
 *    Returns a list of registers whose values have changed since the
 *    last time the proc was called.
 *
 *    usage: gdb_reginfo changed [regnum0, ..., regnumN]
 *
 * name
 *    Return a list containing the names of the registers whose numbers
 *    are given by REGNUM ... .  If no register numbers are given, return
 *    all the registers' names.
 *
 *    usage: gdb_reginfo name [-numbers] [regnum0, ..., regnumN]
 *
 *    Note that some processors have gaps in the register numberings:
 *    even if there is no register numbered N, there may still be a
 *    register numbered N+1.  So if you call gdb_regnames with no
 *    arguments, you can't assume that the N'th element of the result is
 *    register number N.
 *
 *    Given the -numbers option, gdb_regnames returns, not a list of names,
 *    but a list of pairs {NAME NUMBER}, where NAME is the register name,
 *    and NUMBER is its number.
 *
 * size
 *    Returns the raw size of the register(s) in bytes.
 *
 *    usage: gdb_reginfo size [regnum0, ..., regnumN]
 *
 * value
 *    Returns a list of register values.
 *
 *    usage: gdb_reginfo value format [regnum0, ..., regnumN]
 *       format: The format string for printing the values, "N", "x", "d", etc
 */
static int
gdb_register_info (ClientData clientData, Tcl_Interp *interp, int objc,
                   Tcl_Obj *CONST objv[])
{
  int index;
  void *argp;
  void (*func)(int, void *);
  static char *commands[] = {"changed", "name", "size", "value", NULL};
  enum commands_enum { REGINFO_CHANGED, REGINFO_NAME, REGINFO_SIZE, REGINFO_VALUE };

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "name|size|value [regnum1 ... regnumN]");
      return TCL_ERROR;
    }

  if (Tcl_GetIndexFromObj (interp, objv[1], commands, "options", 0,
			   &index) != TCL_OK)
    {
      return TCL_ERROR;
    }

  /* Skip the option */
  objc -= 2;
  objv += 2;

  switch ((enum commands_enum) index)
    {
    case REGINFO_CHANGED:
      func = register_changed_p;
      argp = NULL;
      break;

    case REGINFO_NAME:
      {
	int len;
	char *s = Tcl_GetStringFromObj (objv[0], &len);
	if (objc != 0 && strncmp (s, "-numbers", len) == 0)
	  {
	    argp = (void *) 1;
	    objc--;
	    objv++;
	  }
	else
	  argp = NULL;

	func = get_register_name;
      }
      break;

    case REGINFO_SIZE:
      func = get_register_size;
      argp = NULL;
      break;

    case REGINFO_VALUE:
      func = get_register;
      argp = (void *) (int) *(Tcl_GetStringFromObj (objv[0], NULL));
      objc--;
      objv++;
      break;

    default:
      return TCL_ERROR;
    }

  return map_arg_registers (objc, objv, func, argp);
}

static void
get_register_size (int regnum, void *arg)
{
  Tcl_ListObjAppendElement (gdbtk_interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (REGISTER_RAW_SIZE (regnum)));
}

/* This implements the tcl command get_pc_reg
 * It returns the value of the PC register
 *
 * Tcl Arguments:
 *    None
 * Tcl Result:
 *    The value of the pc register.
 */
static int
get_pc_register (ClientData clientData, Tcl_Interp *interp,
		 int objc, Tcl_Obj *CONST objv[])
{
  char *buff;
  xasprintf (&buff, "0x%s", paddr_nz (read_register (PC_REGNUM)));
  Tcl_SetStringObj (result_ptr->obj_ptr, buff, -1);
  free(buff);
  return TCL_OK;
}

static void
get_register (int regnum, void *fp)
{
  struct type *reg_vtype;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  int format = (int) fp;
  int optim;

  if (format == 'N')
    format = 0;

  /* read_relative_register_raw_bytes returns a virtual frame pointer
     (FRAME_FP (selected_frame)) if regnum == FP_REGNUM instead
     of the real contents of the register. To get around this,
     use get_saved_register instead. */
  get_saved_register (raw_buffer, &optim, (CORE_ADDR *) NULL, selected_frame,
		      regnum, (enum lval_type *) NULL);
  if (optim)
    {
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewStringObj ("Optimized out", -1));
      return;
    }

  /* Convert raw data to virtual format if necessary.  */

  reg_vtype = REGISTER_VIRTUAL_TYPE (regnum);
  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, reg_vtype,
      				   raw_buffer, virtual_buffer);
    }
  else
    memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));

  if (format == 'r')
    {
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < REGISTER_RAW_SIZE (regnum); j++)
	{
	  register int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	    : REGISTER_RAW_SIZE (regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) raw_buffer[idx]);
	  ptr += 2;
	}
      fputs_filtered (buf, gdb_stdout);
    }
  else
    if ((TYPE_CODE (reg_vtype) == TYPE_CODE_UNION)
        && (strcmp (FIELD_NAME (TYPE_FIELD (reg_vtype, 0)), REGISTER_NAME (regnum)) == 0))
      {
        val_print (FIELD_TYPE (TYPE_FIELD (reg_vtype, 0)), virtual_buffer, 0, 0,
	           gdb_stdout, format, 1, 0, Val_pretty_default);
      }
    else
      val_print (REGISTER_VIRTUAL_TYPE (regnum), virtual_buffer, 0, 0,
	         gdb_stdout, format, 1, 0, Val_pretty_default);

}

static void
get_register_name (int regnum, void *argp)
{
  /* Non-zero if the caller wants the register numbers, too.  */
  int numbers = (int) argp;
  Tcl_Obj *name = Tcl_NewStringObj (REGISTER_NAME (regnum), -1);
  Tcl_Obj *elt;

  if (numbers)
    {
      /* Build a tuple of the form "{REGNAME NUMBER}", and append it to
	 our result.  */
      Tcl_Obj *array[2];

      array[0] = name;
      array[1] = Tcl_NewIntObj (regnum);
      elt = Tcl_NewListObj (2, array);
    }
  else
    elt = name;

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, elt);
}

/* This is a sort of mapcar function for operations on registers */

static int
map_arg_registers (int objc, Tcl_Obj *CONST objv[],
		   void (*func) (int regnum, void *argp),
		   void *argp)
{
  int regnum, numregs;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  if (objc == 0 || objc > 1)
    result_ptr->flags |= GDBTK_MAKES_LIST;

  if (objc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  
	  func (regnum, argp);
	}
      
      return TCL_OK;
    }

  /* Else, list of register #s, just do listed regs */
  for (; objc > 0; objc--, objv++)
    {
      if (Tcl_GetIntFromObj (NULL, *objv, &regnum) != TCL_OK)
	{
	  result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	func (regnum, argp);
      else
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr, "bad register number", -1);
	  return TCL_ERROR;
	}
    }

  return TCL_OK;
}

static void
register_changed_p (int regnum, void *argp)
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  if (read_relative_register_raw_bytes (regnum, raw_buffer))
    return;

  if (memcmp (&old_regs[regnum * MAX_REGISTER_RAW_SIZE], raw_buffer,
	      REGISTER_RAW_SIZE (regnum)) == 0)
    return;

  /* Found a changed register.  Save new value and return its number. */

  memcpy (&old_regs[regnum * MAX_REGISTER_RAW_SIZE], raw_buffer,
	  REGISTER_RAW_SIZE (regnum));

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewIntObj (regnum));
}

static void
setup_architecture_data ()
{
  if (old_regs != NULL)
    xfree (old_regs);

  old_regs = xmalloc ((NUM_REGS + NUM_PSEUDO_REGS) * MAX_REGISTER_RAW_SIZE + 1);
  memset (old_regs, 0, sizeof  (old_regs));
}

