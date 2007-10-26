/* Tcl/Tk command definitions for Insight - Stack.
   Copyright 2001, 2002, 2003 Free Software Foundation, Inc.

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
#include "target.h"
#include "breakpoint.h"
#include "linespec.h"
#include "block.h"
#include "dictionary.h"

#include <tcl.h>
#include "gdbtk.h"
#include "gdbtk-cmds.h"
#include "gdbtk-wrapper.h"

static int gdb_block_vars (ClientData clientData,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[]);
static int gdb_get_args_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_get_blocks (ClientData clientData,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[]);
static int gdb_get_locals_command (ClientData, Tcl_Interp *, int,
				   Tcl_Obj * CONST objv[]);
static int gdb_get_vars_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_selected_block (ClientData clientData,
			       Tcl_Interp * interp, int argc,
			       Tcl_Obj * CONST objv[]);
static int gdb_selected_frame (ClientData clientData,
			       Tcl_Interp * interp, int argc,
			       Tcl_Obj * CONST objv[]);
static int gdb_stack (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static void get_frame_name (Tcl_Interp *interp, Tcl_Obj *list,
			    struct frame_info *fi);

int
Gdbtk_Stack_Init (Tcl_Interp *interp)
{
  Tcl_CreateObjCommand (interp, "gdb_block_variables", gdbtk_call_wrapper,
			gdb_block_vars, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_blocks", gdbtk_call_wrapper,
			gdb_get_blocks, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_args", gdbtk_call_wrapper,
			gdb_get_args_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_locals", gdbtk_call_wrapper,
			gdb_get_locals_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_selected_block", gdbtk_call_wrapper,
			gdb_selected_block, NULL);
  Tcl_CreateObjCommand (interp, "gdb_selected_frame", gdbtk_call_wrapper,
			gdb_selected_frame, NULL);
  Tcl_CreateObjCommand (interp, "gdb_stack", gdbtk_call_wrapper, gdb_stack, NULL);

  return TCL_OK;
}

/* This implements the tcl command gdb_block_vars.
 *
 * Returns all variables valid in the specified block.
 *
 * Arguments:
 *    The start and end addresses which identify the block.
 * Tcl Result:
 *    All variables defined in the given block.
 */
static int
gdb_block_vars (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
  struct block *block;
  struct dict_iterator iter;
  struct symbol *sym;
  CORE_ADDR start, end;

  if (objc < 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "startAddr endAddr");
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  if (deprecated_selected_frame == NULL)
    return TCL_OK;

  start = string_to_core_addr (Tcl_GetStringFromObj (objv[1], NULL));
  end   = string_to_core_addr (Tcl_GetStringFromObj (objv[2], NULL));
  
  block = get_frame_block (deprecated_selected_frame, 0);

  while (block != 0)
    {
      /* APPLE LOCAL begin address ranges  */
      if (block_starts_and_ends (block, start, end))
      /* APPLE LOCAL end address ranges  */
	{
	  ALL_BLOCK_SYMBOLS (block, iter, sym)
	    {
	      switch (SYMBOL_CLASS (sym))
		{
		case LOC_ARG:		  /* argument              */
		case LOC_REF_ARG:	  /* reference arg         */
		case LOC_REGPARM:	  /* register arg          */
		case LOC_REGPARM_ADDR:    /* indirect register arg */
		case LOC_LOCAL_ARG:	  /* stack arg             */
		case LOC_BASEREG_ARG:	  /* basereg arg           */
		case LOC_LOCAL:	          /* stack local           */
		case LOC_BASEREG:	  /* basereg local         */
		case LOC_STATIC:	  /* static                */
		case LOC_REGISTER:        /* register              */
		case LOC_COMPUTED:	  /* computed location     */
		case LOC_COMPUTED_ARG:	  /* computed location arg */
		  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
					    Tcl_NewStringObj (DEPRECATED_SYMBOL_NAME (sym),
							      -1));
		  break;

		default:
		  break;
		}
	    }

	  return TCL_OK;
	}
      else if (BLOCK_FUNCTION (block))
	break;
      else
	block = BLOCK_SUPERBLOCK (block);
    }

  return TCL_OK;
}

/* This implements the tcl command gdb_get_blocks
 *
 * Returns the start and end addresses for all blocks in
 * the selected frame.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    A list of all valid blocks in the selected_frame.
 */
static int
gdb_get_blocks (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
  struct block *block;
  struct dict_iterator iter;
  int junk;
  struct symbol *sym;
  CORE_ADDR pc;

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  
  if (deprecated_selected_frame != NULL)
    {
      block = get_frame_block (deprecated_selected_frame, 0);
      pc = get_frame_pc (deprecated_selected_frame);
      while (block != 0)
	{
	  junk = 0;
	  ALL_BLOCK_SYMBOLS (block, iter, sym)
	    {
	      switch (SYMBOL_CLASS (sym))
		{
		default:
		case LOC_UNDEF:		  /* catches errors        */
		case LOC_CONST:           /* constant              */
		case LOC_TYPEDEF:	  /* local typedef         */
		case LOC_LABEL:	          /* local label           */
		case LOC_BLOCK:	          /* local function        */
		case LOC_CONST_BYTES:	  /* loc. byte seq.        */
		case LOC_UNRESOLVED:      /* unresolved static     */
		case LOC_OPTIMIZED_OUT:   /* optimized out         */
		  junk = 1;
		  break;

		case LOC_ARG:		  /* argument              */
		case LOC_REF_ARG:	  /* reference arg         */
		case LOC_REGPARM:	  /* register arg          */
		case LOC_REGPARM_ADDR:    /* indirect register arg */
		case LOC_LOCAL_ARG:	  /* stack arg             */
		case LOC_BASEREG_ARG:	  /* basereg arg           */
		case LOC_COMPUTED_ARG:	  /* computed location arg */

		case LOC_LOCAL:	          /* stack local           */
		case LOC_BASEREG:	  /* basereg local         */
		case LOC_STATIC:	  /* static                */
		case LOC_REGISTER:        /* register              */
		case LOC_COMPUTED:	  /* computed location     */
		  junk = 0;
		  break;
		}
	    }

	  /* If we found a block with locals in it, add it to the list. 
	     Note that the ranges of start and end address for blocks
	     are exclusive, so double-check against the PC */
	  
	  /* APPLE LOCAL begin address ranges  */
	  if (!junk && block_contains_pc (block, pc))
	    {
	      char *addr;

	      if (!BLOCK_RANGES (block))
		{
		  Tcl_Obj *elt = Tcl_NewListObj (0, NULL);
		  xasprintf (&addr, "0x%s", paddr_nz (BLOCK_START (block)));
		  Tcl_ListObjAppendElement (interp, elt,
					    Tcl_NewStringObj (addr, -1));
		  free(addr);
		  xasprintf (&addr, "0x%s", paddr_nz (BLOCK_END (block)));
		  Tcl_ListObjAppendElement (interp, elt,
					    Tcl_NewStringObj (addr, -1));
		  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, elt);
		  free(addr);
		}
	      else
		{
		  int i;
		  Tcl_Obj *elt = Tcl_NewListObj (0, NULL);

		  for (i = 0; i < BLOCK_RANGES (block)->nelts; i++)
		    {
		      xasprintf (&addr, "0x%s", 
				 paddr_nz (BLOCK_RANGE_START (block, i)));
		      Tcl_ListObjAppendElement (interp, elt,
						Tcl_NewStringObj (addr, -1));
		      free(addr);
		      xasprintf (&addr, "0x%s", 
				 paddr_nz (BLOCK_RANGE_END (block, i)));
		      Tcl_ListObjAppendElement (interp, elt,
					    Tcl_NewStringObj (addr, -1));
		      Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, elt);
		      free(addr);
		    }
		}
	      /* APPLE LOCAL end address ranges  */
	    }

	  if (BLOCK_FUNCTION (block))
	    break;
	  else
	    block = BLOCK_SUPERBLOCK (block);
	}
    }

  return TCL_OK;
}

/* gdb_get_args -
 * This and gdb_get_locals just call gdb_get_vars_command with the right
 * value of clientData.  We can't use the client data in the definition
 * of the command, because the call wrapper uses this instead...
 */
static int
gdb_get_args_command (ClientData clientData, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
  return gdb_get_vars_command ((ClientData) 1, interp, objc, objv);
}


static int
gdb_get_locals_command (ClientData clientData, Tcl_Interp *interp,
			int objc, Tcl_Obj *CONST objv[])
{
  return gdb_get_vars_command ((ClientData) 0, interp, objc, objv);
}

/* This implements the tcl commands "gdb_get_locals" and "gdb_get_args"

* This function sets the Tcl interpreter's result to a list of variable names
* depending on clientData. If clientData is one, the result is a list of
* arguments; zero returns a list of locals -- all relative to the block
* specified as an argument to the command. Valid commands include
* anything decode_line_1 can handle (like "main.c:2", "*0x02020202",
* and "main").
*
* Tcl Arguments:
*   linespec - the linespec defining the scope of the lookup. Empty string
*              to use the current block in the innermost frame.
* Tcl Result:
*   A list of the locals or args
*/
static int
gdb_get_vars_command (ClientData clientData, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
  struct symtabs_and_lines sals;
  struct symbol *sym;
  struct block *block;
  char **canonical, *args;
  struct dict_iterator iter;
  int i, arguments;

  if (objc > 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv,
			"[function:line|function|line|*addr]");
      return TCL_ERROR;
    }

  arguments = (int) clientData;

  /* Initialize the result pointer to an empty list. */

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  if (objc == 2)
    {
      args = Tcl_GetStringFromObj (objv[1], NULL);
      sals = decode_line_1 (&args, 1, NULL, 0, &canonical, NULL);
      if (sals.nelts == 0)
	{
	  gdbtk_set_result (interp, "error decoding line");
	  return TCL_ERROR;
	}

      /* Resolve all line numbers to PC's */
      for (i = 0; i < sals.nelts; i++)
	resolve_sal_pc (&sals.sals[i]);

      block = block_for_pc (sals.sals[0].pc);
    }
  else
    {
      /* Specified currently selected frame */
      if (deprecated_selected_frame == NULL)
	return TCL_OK;

      block = get_frame_block (deprecated_selected_frame, 0);
    }

  while (block != 0)
    {
      ALL_BLOCK_SYMBOLS (block, iter, sym)
	{
	  switch (SYMBOL_CLASS (sym))
	    {
	    default:
	    case LOC_UNDEF:	/* catches errors        */
	    case LOC_CONST:	/* constant              */
	    case LOC_TYPEDEF:	/* local typedef         */
	    case LOC_LABEL:	/* local label           */
	    case LOC_BLOCK:	/* local function        */
	    case LOC_CONST_BYTES:	/* loc. byte seq.        */
	    case LOC_UNRESOLVED:	/* unresolved static     */
	    case LOC_OPTIMIZED_OUT:	/* optimized out         */
	      break;
	    case LOC_ARG:	/* argument              */
	    case LOC_REF_ARG:	/* reference arg         */
	    case LOC_REGPARM:	/* register arg          */
	    case LOC_REGPARM_ADDR:	/* indirect register arg */
	    case LOC_LOCAL_ARG:	/* stack arg             */
	    case LOC_BASEREG_ARG:	/* basereg arg           */
	    case LOC_COMPUTED_ARG:	/* computed location arg */
	      if (arguments)
		Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
					  Tcl_NewStringObj (DEPRECATED_SYMBOL_NAME (sym), -1));
	      break;
	    case LOC_LOCAL:	/* stack local           */
	    case LOC_BASEREG:	/* basereg local         */
	    case LOC_STATIC:	/* static                */
	    case LOC_REGISTER:	/* register              */
	    case LOC_COMPUTED:	/* computed location     */
	      if (!arguments)
		Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
					  Tcl_NewStringObj (DEPRECATED_SYMBOL_NAME (sym), -1));
	      break;
	    }
	}
      if (BLOCK_FUNCTION (block))
	break;
      else
	block = BLOCK_SUPERBLOCK (block);
    }

  return TCL_OK;
}

/* This implements the tcl command gdb_selected_block
 *
 * Returns the start and end addresses of the innermost
 * block in the selected frame.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    The currently selected block's start and end addresses
 */
static int
gdb_selected_block (ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *CONST objv[])
{
  char *start = NULL;
  char *end   = NULL;

  if (deprecated_selected_frame == NULL)
    {
      xasprintf (&start, "%s", "");
      xasprintf (&end, "%s", "");
    }
  else
    {
      struct block *block;
      block = get_frame_block (deprecated_selected_frame, 0);
      /* APPLE LOCAL begin address ranges  */
      if (BLOCK_RANGES (block))
	return TCL_ERROR;
      /* APPLE LOCAL end address ranges  */
      xasprintf (&start, "0x%s", paddr_nz (BLOCK_START (block)));
      xasprintf (&end, "0x%s", paddr_nz (BLOCK_END (block)));
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewStringObj (start, -1));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewStringObj (end, -1));

  free(start);
  free(end);
  return TCL_OK;
}

/* This implements the tcl command gdb_selected_frame

* Returns the address of the selected frame
* frame.
*
* Arguments:
*    None
* Tcl Result:
*    The currently selected frame's address
*/
static int
gdb_selected_frame (ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *CONST objv[])
{
  char *frame;

  if (deprecated_selected_frame == NULL)
    xasprintf (&frame, "%s","");
  else
    /* FIXME: cagney/2002-11-19: This should be using get_frame_id()
       to identify the frame and *NOT* get_frame_base().  */
    xasprintf (&frame, "0x%s",
	       paddr_nz (get_frame_base (deprecated_selected_frame)));

  Tcl_SetStringObj (result_ptr->obj_ptr, frame, -1);

  free(frame);
  return TCL_OK;
}

/* This implements the tcl command gdb_stack.
 * It builds up a list of stack frames.
 *
 * Tcl Arguments:
 *    start  - starting stack frame
 *    count - number of frames to inspect
 * Tcl Result:
 *    A list of function names
 */
static int
gdb_stack (ClientData clientData, Tcl_Interp *interp,
	   int objc, Tcl_Obj *CONST objv[])
{
  int start, count;

  if (objc < 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "start count");
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (NULL, objv[1], &start))
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  if (Tcl_GetIntFromObj (NULL, objv[2], &count))
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  if (target_has_stack)
    {
      gdb_result r;
      struct frame_info *top;
      struct frame_info *fi;

      /* Find the outermost frame */
      r  = GDB_get_current_frame (&fi);
      if (r != GDB_OK)
	return TCL_ERROR;

      while (fi != NULL)
        {
          top = fi;
	  r = GDB_get_prev_frame (fi, &fi);
	  if (r != GDB_OK)
	    fi = NULL;
        }

      /* top now points to the top (outermost frame) of the
         stack, so point it to the requested start */
      start = -start;
      r = GDB_find_relative_frame (top, &start, &top);
      
      result_ptr->obj_ptr = Tcl_NewListObj (0, NULL);
      if (r != GDB_OK)
	return TCL_OK;

      /* If start != 0, then we have asked to start outputting
         frames beyond the innermost stack frame */
      if (start == 0)
        {
          fi = top; 
          while (fi && count--)
            {
              get_frame_name (interp, result_ptr->obj_ptr, fi);
              r = GDB_get_next_frame (fi, &fi);
	      if (r != GDB_OK)
		break;
            }
        }
    }

  return TCL_OK;
}

/* A helper function for get_stack which adds information about
 * the stack frame FI to the caller's LIST.
 *
 * This is stolen from print_frame_info in stack.c.
 */
static void
get_frame_name (Tcl_Interp *interp, Tcl_Obj *list, struct frame_info *fi)
{
  struct symtab_and_line sal;
  struct symbol *func = NULL;
  register char *funname = 0;
  enum language funlang = language_unknown;
  Tcl_Obj *objv[1];

  if (get_frame_type (fi) == DUMMY_FRAME)
    {
      objv[0] = Tcl_NewStringObj ("<function called from gdb>\n", -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
      return;
    }
  if ((get_frame_type (fi) == SIGTRAMP_FRAME))
    {
      objv[0] = Tcl_NewStringObj ("<signal handler called>\n", -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
      return;
    }

  sal =
    find_pc_line (get_frame_pc (fi),
		  get_next_frame (fi) != NULL
		  && !(get_frame_type (fi) == SIGTRAMP_FRAME)
		  && !(get_frame_type (fi) == DUMMY_FRAME));

  func = find_pc_function (get_frame_pc (fi));
  if (func)
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (get_frame_pc (fi));
      /* APPLE LOCAL begin address ranges  */
      if (msymbol != NULL
	  && (SYMBOL_VALUE_ADDRESS (msymbol)
	      > BLOCK_LOWEST_PC (SYMBOL_BLOCK_VALUE (func))))
	/* APPLE LOCAL end address ranges  */
	{
	  func = 0;
	  funname = GDBTK_SYMBOL_SOURCE_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
      else
	{
	  funname = GDBTK_SYMBOL_SOURCE_NAME (func);
	  funlang = SYMBOL_LANGUAGE (func);
	}
    }
  else
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (get_frame_pc (fi));
      if (msymbol != NULL)
	{
	  funname = GDBTK_SYMBOL_SOURCE_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
    }

  if (sal.symtab)
    {
      objv[0] = Tcl_NewStringObj (funname, -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
    }
  else
    {
#if 0
      /* we have no convenient way to deal with this yet... */
      if (fi->pc != sal.pc || !sal.symtab)
	{
	  deprecated_print_address_numeric (fi->pc, 1, gdb_stdout);
	  printf_filtered (" in ");
	}
      printf_symbol_filtered (gdb_stdout, funname ? funname : "??", funlang,
			      DMGL_ANSI);
#endif
      objv[0] = Tcl_NewStringObj (funname != NULL ? funname : "??", -1);
#ifdef PC_LOAD_SEGMENT
      /* If we couldn't print out function name but if can figure out what
         load segment this pc value is from, at least print out some info
         about its load segment. */
      if (!funname)
	{
	  Tcl_AppendStringsToObj (objv[0], " from ", PC_LOAD_SEGMENT (fi->pc),
				  (char *) NULL);
	}
#endif
#ifdef PC_SOLIB
      if (!funname)
	{
	  char *lib = PC_SOLIB (get_frame_pc (fi));
	  if (lib)
	    {
	      Tcl_AppendStringsToObj (objv[0], " from ", lib, (char *) NULL);
	    }
	}
#endif
      Tcl_ListObjAppendElement (interp, list, objv[0]);
    }
}
