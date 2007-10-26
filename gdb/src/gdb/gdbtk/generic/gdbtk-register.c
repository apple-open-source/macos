/* Tcl/Tk command definitions for Insight - Registers
   Copyright 2001, 2002, 2004 Free Software Foundation, Inc.

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
#include "reggroups.h"
#include "value.h"
#include "target.h"
#include "gdb_string.h"

#include <tcl.h>
#include "gdbtk.h"
#include "gdbtk-cmds.h"

/* This contains the previous values of the registers, since the last call to
   gdb_changed_register_list.

   It is an array of (NUM_REGS+NUM_PSEUDO_REGS)*MAX_REGISTER_RAW_SIZE bytes. */

static int gdb_register_info (ClientData, Tcl_Interp *, int, Tcl_Obj **);
static void get_register (int, void *);
static void get_register_name (int, void *);
static void get_register_size (int regnum, void *arg);
static int map_arg_registers (Tcl_Interp *, int, Tcl_Obj **,
			      void (*)(int, void *), void *);
static void register_changed_p (int, void *);
static void setup_architecture_data (void);
static int gdb_regformat (ClientData, Tcl_Interp *, int, Tcl_Obj **);
static int gdb_reggroup (ClientData, Tcl_Interp *, int, Tcl_Obj **);
static int gdb_reggrouplist (ClientData, Tcl_Interp *, int, Tcl_Obj **);

static void get_register_types (int regnum, void *arg);

static char *old_regs = NULL;
static int *regformat = (int *)NULL;
static struct type **regtype = (struct type **)NULL;

int
Gdbtk_Register_Init (Tcl_Interp *interp)
{
  Tcl_CreateObjCommand (interp, "gdb_reginfo", gdbtk_call_wrapper,
                        gdb_register_info, NULL);

  /* Register/initialize any architecture specific data */
  setup_architecture_data ();

  deprecated_register_gdbarch_swap (&old_regs, sizeof (old_regs), NULL);
  deprecated_register_gdbarch_swap (&regformat, sizeof (regformat), NULL);
  deprecated_register_gdbarch_swap (&regtype, sizeof (regtype), NULL);
  deprecated_register_gdbarch_swap (NULL, 0, setup_architecture_data);

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
 *    usage: gdb_reginfo value [regnum0, ..., regnumN]
 */
static int
gdb_register_info (ClientData clientData, Tcl_Interp *interp, int objc,
                   Tcl_Obj **objv)
{
  int index;
  void *argp;
  void (*func)(int, void *);
  static const char *commands[] = {"changed", "name", "size", "value", "type", 
			     "format", "group", "grouplist", NULL};
  enum commands_enum { REGINFO_CHANGED, REGINFO_NAME, REGINFO_SIZE, REGINFO_VALUE, 
		       REGINFO_TYPE, REGINFO_FORMAT, REGINFO_GROUP, REGINFO_GROUPLIST };

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "name|size|value|type|format|groups [regnum1 ... regnumN]");
      return TCL_ERROR;
    }

  if (Tcl_GetIndexFromObj (interp, objv[1], commands, "options", 0,
  			   &index) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
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
      argp = NULL;
      break;

    case REGINFO_TYPE:
      func = get_register_types;
      argp = NULL;
      break;

    case REGINFO_FORMAT:
      return gdb_regformat (clientData, interp, objc, objv);

    case REGINFO_GROUP:
      return gdb_reggroup (clientData, interp, objc, objv);

    case REGINFO_GROUPLIST:
      return gdb_reggrouplist (clientData, interp, objc, objv);

    default:
      return TCL_ERROR;
    }

  return map_arg_registers (interp, objc, objv, func, argp);
}

static void
get_register_size (int regnum, void *arg)
{
  Tcl_ListObjAppendElement (gdbtk_interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (register_size (current_gdbarch, regnum)));
}

/* returns a list of valid types for a register */
/* Normally this will be only one type, except for SIMD and other */
/* special registers. */

static void
get_register_types (int regnum, void *arg)
{ 
  struct type *reg_vtype;
  int i,n;

  reg_vtype = register_type (current_gdbarch, regnum);
  
  if (TYPE_CODE (reg_vtype) == TYPE_CODE_UNION)
    {
      n = TYPE_NFIELDS (reg_vtype);
      /* limit to 16 types */
      if (n > 16) 
	n = 16;
      
      for (i = 0; i < n; i++)
	{
	  Tcl_Obj *ar[3], *list;
	  char *buff;
	  xasprintf (&buff, "%lx", (long)TYPE_FIELD_TYPE (reg_vtype, i));
	  ar[0] = Tcl_NewStringObj (TYPE_FIELD_NAME (reg_vtype, i), -1);
	  ar[1] = Tcl_NewStringObj (buff, -1);
	  if (TYPE_CODE (TYPE_FIELD_TYPE (reg_vtype, i)) == TYPE_CODE_FLT)
	    ar[2] = Tcl_NewStringObj ("float", -1);
	  else
	    ar[2] = Tcl_NewStringObj ("int", -1);	    
	  list = Tcl_NewListObj (3, ar);
	  Tcl_ListObjAppendElement (gdbtk_interp, result_ptr->obj_ptr, list);
	  xfree (buff);
	}
    }
  else
    {
      Tcl_Obj *ar[3], *list;
      char *buff;
      xasprintf (&buff, "%lx", (long)reg_vtype);
      ar[0] = Tcl_NewStringObj (TYPE_NAME(reg_vtype), -1);
      ar[1] = Tcl_NewStringObj (buff, -1);
      if (TYPE_CODE (reg_vtype) == TYPE_CODE_FLT)
	ar[2] = Tcl_NewStringObj ("float", -1);
      else
	ar[2] = Tcl_NewStringObj ("int", -1);	    
      list = Tcl_NewListObj (3, ar);
      xfree (buff);
      Tcl_ListObjAppendElement (gdbtk_interp, result_ptr->obj_ptr, list);
    }
}


static void
get_register (int regnum, void *arg)
{
  int realnum;
  CORE_ADDR addr;
  enum lval_type lval;
  struct type *reg_vtype;
  gdb_byte buffer[MAX_REGISTER_SIZE];
  int optim, format;
  struct cleanup *old_chain = NULL;
  struct ui_file *stb;
  long dummy;
  char *res;
 
  format = regformat[regnum];
  if (format == 0)
    format = 'x';
  
  reg_vtype = regtype[regnum];
  if (reg_vtype == NULL)
    reg_vtype = register_type (current_gdbarch, regnum);

  if (!target_has_registers)
    {
      if (result_ptr->flags & GDBTK_MAKES_LIST)
	Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewStringObj ("", -1));
      else
	Tcl_SetStringObj (result_ptr->obj_ptr, "", -1);
      return;
    }

  frame_register (get_selected_frame (NULL), regnum, &optim, &lval, 
		  &addr, &realnum, buffer);

  if (optim)
    {
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewStringObj ("Optimized out", -1));
      return;
    }

  stb = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (stb);

  if (format == 'r')
    {
      /* shouldn't happen. raw format is deprecated */
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < register_size (current_gdbarch, regnum); j++)
	{
	  int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	    : register_size (current_gdbarch, regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) buffer[idx]);
	  ptr += 2;
	}
      fputs_unfiltered (buf, stb);
    }
  else
    {
      if ((TYPE_CODE (reg_vtype) == TYPE_CODE_UNION)
	  && (strcmp (FIELD_NAME (TYPE_FIELD (reg_vtype, 0)), 
		      REGISTER_NAME (regnum)) == 0))
	{
	  val_print (FIELD_TYPE (TYPE_FIELD (reg_vtype, 0)), buffer, 0, 0,
		     stb, format, 1, 0, Val_pretty_default);
	}
      else
	val_print (reg_vtype, buffer, 0, 0,
		   stb, format, 1, 0, Val_pretty_default);
    }
  
  res = ui_file_xstrdup (stb, &dummy);

  if (result_ptr->flags & GDBTK_MAKES_LIST)
    Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewStringObj (res, -1));
  else
    Tcl_SetStringObj (result_ptr->obj_ptr, res, -1);

  xfree (res);
  do_cleanups (old_chain);
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
map_arg_registers (Tcl_Interp *interp, int objc, Tcl_Obj **objv,
		   void (*func) (int regnum, void *argp), void *argp)
{
  int regnum, numregs;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  if (objc == 0)		/* No args, just do all the regs */
    {
      result_ptr->flags |= GDBTK_MAKES_LIST;
      for (regnum = 0; regnum < numregs; regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  func (regnum, argp);
	}      
      return TCL_OK;
    }

  if (objc == 1)
    if (Tcl_ListObjGetElements (interp, *objv, &objc, &objv ) != TCL_OK)
      return TCL_ERROR;

  if (objc > 1)
    result_ptr->flags |= GDBTK_MAKES_LIST;

  /* Else, list of register #s, just do listed regs */
  for (; objc > 0; objc--, objv++)
    {
      if (Tcl_GetIntFromObj (NULL, *objv, &regnum) != TCL_OK)
	{
	  result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}

      if (regnum >= 0  && regnum < numregs)
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
  char raw_buffer[MAX_REGISTER_SIZE];

  if (deprecated_selected_frame == NULL
      || !frame_register_read (deprecated_selected_frame, regnum, raw_buffer))
    return;

  if (memcmp (&old_regs[regnum * MAX_REGISTER_SIZE], raw_buffer,
	      register_size (current_gdbarch, regnum)) == 0)
    return;

  /* Found a changed register.  Save new value and return its number. */

  memcpy (&old_regs[regnum * MAX_REGISTER_SIZE], raw_buffer,
	  register_size (current_gdbarch, regnum));

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewIntObj (regnum));
}

static void
setup_architecture_data ()
{
  xfree (old_regs);
  xfree (regformat);
  xfree (regtype);

  old_regs = xcalloc (1, (NUM_REGS + NUM_PSEUDO_REGS) * MAX_REGISTER_SIZE + 1);
  regformat = (int *)xcalloc ((NUM_REGS + NUM_PSEUDO_REGS) , sizeof(int));
  regtype = (struct type **)xcalloc ((NUM_REGS + NUM_PSEUDO_REGS), sizeof(struct type **));
}

/* gdb_regformat sets the format for a register */
/* This is necessary to allow "gdb_reginfo value" to return a list */
/* of registers and values. */
/* Usage: gdb_reginfo format regno typeaddr format */

static int
gdb_regformat (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj **objv)
{
  int fm, regno;
  struct type *type;

  if (objc != 3)
    {
      Tcl_WrongNumArgs (interp, 0, objv, "gdb_reginfo regno type format");
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[0], &regno) != TCL_OK)
    return TCL_ERROR;

  type = (struct type *)strtol (Tcl_GetStringFromObj (objv[1], NULL), NULL, 16);  
  fm = (int)*(Tcl_GetStringFromObj (objv[2], NULL));

  if (regno >= NUM_REGS + NUM_PSEUDO_REGS)
    {
      gdbtk_set_result (interp, "Register number %d too large", regno);
      return TCL_ERROR;
    }
  
  regformat[regno] = fm;
  regtype[regno] = type;

  return TCL_OK;
}


/* gdb_reggrouplist returns the names of the register groups */
/* for the current architecture. */
/* Usage: gdb_reginfo groups */

static int
gdb_reggrouplist (ClientData clientData, Tcl_Interp *interp,
		  int objc, Tcl_Obj **objv)
{
  struct reggroup *group;
  int i = 0;

  if (objc != 0)
    {
      Tcl_WrongNumArgs (interp, 0, objv, "gdb_reginfo grouplist");
      return TCL_ERROR;
    }

  for (group = reggroup_next (current_gdbarch, NULL);
       group != NULL;
       group = reggroup_next (current_gdbarch, group))
    {
      if (reggroup_type (group) == USER_REGGROUP)
	Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewStringObj (reggroup_name (group), -1));
    }
  return TCL_OK;
}


/* gdb_reggroup returns the names of the registers in a group. */
/* Usage: gdb_reginfo group groupname */

static int
gdb_reggroup (ClientData clientData, Tcl_Interp *interp,
	      int objc, Tcl_Obj **objv)
{
  struct reggroup *group;
  char *groupname;
  int regnum;

  if (objc != 1)
    {
      Tcl_WrongNumArgs (interp, 0, objv, "gdb_reginfo group groupname");
      return TCL_ERROR;
    }
  
  groupname = Tcl_GetStringFromObj (objv[0], NULL);
  if (groupname == NULL)
    {
      gdbtk_set_result (interp, "could not read groupname");
      return TCL_ERROR;
    }

  for (group = reggroup_next (current_gdbarch, NULL);
       group != NULL;
       group = reggroup_next (current_gdbarch, group))
    {
      if (strcmp (groupname, reggroup_name (group)) == 0)
	break;
    }

  if (group == NULL)
    return TCL_ERROR;

  for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
    {
      if (gdbarch_register_reggroup_p (current_gdbarch, regnum, group))
	Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewIntObj (regnum));
    }
  return TCL_OK;
}

