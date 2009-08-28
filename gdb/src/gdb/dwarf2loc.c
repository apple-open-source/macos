/* DWARF 2 location expression support for GDB.

   Copyright 2003, 2005 Free Software Foundation, Inc.

   Contributed by Daniel Jacobowitz, MontaVista Software, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "ui-out.h"
#include "value.h"
#include "frame.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"
#include "ax.h"
#include "ax-gdb.h"
#include "regcache.h"
#include "objfiles.h"
#include "exceptions.h"

#include "elf/dwarf2.h"
#include "dwarf2expr.h"
#include "dwarf2loc.h"
#include "dwarf2read.h"

#include "gdb_string.h"

#ifndef DWARF2_REG_TO_REGNUM
#define DWARF2_REG_TO_REGNUM(REG) (REG)
#endif

/* APPLE LOCAL begin print location lists  */
static void
print_single_dwarf_location (struct ui_file *, gdb_byte **, gdb_byte *,
			     struct dwarf_expr_context *);
/* APPLE LOCAL end print location lists  */

/* A helper function for dealing with location lists.  Given a
   symbol baton (BATON) and a pc value (PC), find the appropriate
   location expression, set *LOCEXPR_LENGTH, and return a pointer
   to the beginning of the expression.  Returns NULL on failure.

   For now, only return the first matching location expression; there
   can be more than one in the list.  */

static gdb_byte *
find_location_expression (struct dwarf2_loclist_baton *baton,
			  size_t *locexpr_length, CORE_ADDR pc)
{
  CORE_ADDR low, high;
  gdb_byte *loc_ptr, *buf_end;
  int length;
  unsigned int addr_size = TARGET_ADDR_BIT / TARGET_CHAR_BIT;
  CORE_ADDR base_mask = ~(~(CORE_ADDR)1 << (addr_size * 8 - 1));
  /* Adjust base_address for relocatable objects.  */
  CORE_ADDR base_offset = objfile_text_section_offset (baton->objfile);
  CORE_ADDR base_address = baton->base_address_untranslated;

  loc_ptr = baton->data;
  buf_end = baton->data + baton->size;

  while (1)
    {
      low = dwarf2_read_address (loc_ptr, buf_end, &length);
      loc_ptr += length;
      high = dwarf2_read_address (loc_ptr, buf_end, &length);
      loc_ptr += length;

      /* An end-of-list entry.  */
      if (low == 0 && high == 0)
	return NULL;

      /* A base-address-selection entry.  */
      if ((low & base_mask) == base_mask)
	{
	  base_address = high;
	  continue;
	}

      /* Otherwise, a location expression entry.  */

      low += base_address;
      high += base_address;

      /* APPLE LOCAL: for the debug-info-in-.o-files case we need to go through
         the address translation table for the addresses in the DWARF (in the
         .o file) to get the final addresses in the executable.  */

      if (baton->addr_map)
        {
          translate_debug_map_address (baton->addr_map, low, &low,  0);
          translate_debug_map_address (baton->addr_map, high, &high, 1);
        }

      low += base_offset;
      high += base_offset;

      length = extract_unsigned_integer (loc_ptr, 2);
      loc_ptr += 2;

      if (pc >= low && pc < high)
	{
	  *locexpr_length = length;
	  return loc_ptr;
	}

      loc_ptr += length;
    }
}

/* This is the baton used when performing dwarf2 expression
   evaluation.  */
struct dwarf_expr_baton
{
  struct frame_info *frame;
  struct objfile *objfile;
};

/* Helper functions for dwarf2_evaluate_loc_desc.  */

/* Using the frame specified in BATON, read register REGNUM.  The lval
   type will be returned in LVALP, and for lval_memory the register
   save address will be returned in ADDRP.  */
static CORE_ADDR
dwarf_expr_read_reg (void *baton, int dwarf_regnum)
{
  struct dwarf_expr_baton *debaton = (struct dwarf_expr_baton *) baton;
  CORE_ADDR result, save_addr;
  enum lval_type lval_type;
  gdb_byte *buf;
  enum opt_state optimized;
  int regnum, realnum, regsize;

  regnum = DWARF2_REG_TO_REGNUM (dwarf_regnum);
  regsize = register_size (current_gdbarch, regnum);
  buf = alloca (regsize);

  frame_register (debaton->frame, regnum, &optimized, &lval_type, &save_addr,
		  &realnum, buf);
  /* NOTE: cagney/2003-05-22: This extract is assuming that a DWARF 2
     address is always unsigned.  That may or may not be true.  */
  result = extract_unsigned_integer (buf, regsize);

  return result;
}

/* Read memory at ADDR (length LEN) into BUF.  */

static void
dwarf_expr_read_mem (void *baton, gdb_byte *buf, CORE_ADDR addr, size_t len)
{
  read_memory (addr, buf, len);
}

/* Using the frame specified in BATON, find the location expression
   describing the frame base.  Return a pointer to it in START and
   its length in LENGTH.  */
static void
dwarf_expr_frame_base (void *baton, gdb_byte **start, size_t * length)
{
  /* FIXME: cagney/2003-03-26: This code should be using
     get_frame_base_address(), and then implement a dwarf2 specific
     this_base method.  */
  struct symbol *framefunc;
  struct dwarf_expr_baton *debaton = (struct dwarf_expr_baton *) baton;

  framefunc = get_frame_function (debaton->frame);

  if (SYMBOL_OPS (framefunc) == &dwarf2_loclist_funcs)
    {
      struct dwarf2_loclist_baton *symbaton;
      symbaton = SYMBOL_LOCATION_BATON (framefunc);
      *start = find_location_expression (symbaton, length,
					 get_frame_pc (debaton->frame));
    }
  else
    {
      struct dwarf2_locexpr_baton *symbaton;
      symbaton = SYMBOL_LOCATION_BATON (framefunc);
      *length = symbaton->size;
      *start = symbaton->data;
    }

  if (*start == NULL)
    error (_("Could not find the frame base for \"%s\"."),
	   SYMBOL_NATURAL_NAME (framefunc));
}

/* Using the objfile specified in BATON, find the address for the
   current thread's thread-local storage with offset OFFSET.  */
static CORE_ADDR
dwarf_expr_tls_address (void *baton, CORE_ADDR offset)
{
  struct dwarf_expr_baton *debaton = (struct dwarf_expr_baton *) baton;
  volatile CORE_ADDR addr = 0;

  if (target_get_thread_local_address_p ()
      && gdbarch_fetch_tls_load_module_address_p (current_gdbarch))
    {
      ptid_t ptid = inferior_ptid;
      struct objfile *objfile = debaton->objfile;
      volatile struct gdb_exception ex;

      TRY_CATCH (ex, RETURN_MASK_ALL)
	{
	  CORE_ADDR lm_addr;
	  
	  /* Fetch the load module address for this objfile.  */
	  lm_addr = gdbarch_fetch_tls_load_module_address (current_gdbarch,
	                                                   objfile);
	  /* If it's 0, throw the appropriate exception.  */
	  if (lm_addr == 0)
	    throw_error (TLS_LOAD_MODULE_NOT_FOUND_ERROR,
			 _("TLS load module not found"));

	  addr = target_get_thread_local_address (ptid, lm_addr, offset);
	}
      /* If an error occurred, print TLS related messages here.  Otherwise,
         throw the error to some higher catcher.  */
      if (ex.reason < 0)
	{
	  int objfile_is_library = (objfile->flags & OBJF_SHARED);

	  switch (ex.error)
	    {
	    case TLS_NO_LIBRARY_SUPPORT_ERROR:
	      error (_("Cannot find thread-local variables in this thread library."));
	      break;
	    case TLS_LOAD_MODULE_NOT_FOUND_ERROR:
	      if (objfile_is_library)
		error (_("Cannot find shared library `%s' in dynamic"
		         " linker's load module list"), objfile->name);
	      else
		error (_("Cannot find executable file `%s' in dynamic"
		         " linker's load module list"), objfile->name);
	      break;
	    case TLS_NOT_ALLOCATED_YET_ERROR:
	      if (objfile_is_library)
		error (_("The inferior has not yet allocated storage for"
		         " thread-local variables in\n"
		         "the shared library `%s'\n"
		         "for %s"),
		       objfile->name, target_pid_to_str (ptid));
	      else
		error (_("The inferior has not yet allocated storage for"
		         " thread-local variables in\n"
		         "the executable `%s'\n"
		         "for %s"),
		       objfile->name, target_pid_to_str (ptid));
	      break;
	    case TLS_GENERIC_ERROR:
	      if (objfile_is_library)
		error (_("Cannot find thread-local storage for %s, "
		         "shared library %s:\n%s"),
		       target_pid_to_str (ptid),
		       objfile->name, ex.message);
	      else
		error (_("Cannot find thread-local storage for %s, "
		         "executable file %s:\n%s"),
		       target_pid_to_str (ptid),
		       objfile->name, ex.message);
	      break;
	    default:
	      throw_exception (ex);
	      break;
	    }
	}
    }
  /* It wouldn't be wrong here to try a gdbarch method, too; finding
     TLS is an ABI-specific thing.  But we don't do that yet.  */
  else
    error (_("Cannot find thread-local variables on this target"));

  return addr;
}

/* Evaluate a location description, starting at DATA and with length
   SIZE, to find the current location of variable VAR in the context
   of FRAME.  */
static struct value *
dwarf2_evaluate_loc_desc (struct symbol *var, struct frame_info *frame,
			  gdb_byte *data, unsigned short size,
			  struct objfile *objfile)
{
  struct value *retval;
  struct dwarf_expr_baton baton;
  struct dwarf_expr_context *ctx;

  if (size == 0)
    {
      retval = allocate_value (SYMBOL_TYPE (var));
      VALUE_LVAL (retval) = not_lval;
      /* APPLE LOCAL variable opt states.  */
      set_value_optimized_out (retval, opt_away);
    }

  baton.frame = frame;
  baton.objfile = objfile;

  ctx = new_dwarf_expr_context ();
  ctx->baton = &baton;
  ctx->read_reg = dwarf_expr_read_reg;
  ctx->read_mem = dwarf_expr_read_mem;
  ctx->get_frame_base = dwarf_expr_frame_base;
  ctx->get_tls_address = dwarf_expr_tls_address;

  dwarf_expr_eval (ctx, data, size, 0);
  /* APPLE LOCAL begin DW_op_pieces for PPC registers */
  if (ctx->num_pieces == 2
      && ctx->pieces[0].in_reg
      && ctx->pieces[1].in_reg
      && (ctx->pieces[0].value + 1) == ctx->pieces[1].value
      && CONVERT_REGISTER_P (ctx->pieces[0].value, SYMBOL_TYPE (var))
      )
    {
      CORE_ADDR dwarf_regnum = ctx->pieces[0].value;
      int gdb_regnum = DWARF2_REG_TO_REGNUM (dwarf_regnum);
      retval = value_from_register (SYMBOL_TYPE (var), gdb_regnum, frame);
    }
  else
    /* APPLE LOCAL end DW_op_pieces for PPC registers */
  if (ctx->num_pieces > 0)
    {
      /* APPLE LOCAL begin mainline */
      int i;
      long offset = 0;
      bfd_byte *contents;

      retval = allocate_value (SYMBOL_TYPE (var));
      contents = value_contents_raw (retval);
      for (i = 0; i < ctx->num_pieces; i++)
	{
	  struct dwarf_expr_piece *p = &ctx->pieces[i];
	  if (p->in_reg)
	    {
	      bfd_byte regval[MAX_REGISTER_SIZE];
	      int gdb_regnum = DWARF2_REG_TO_REGNUM (p->value);
	      get_frame_register (frame, gdb_regnum, regval);
	      memcpy (contents + offset, regval, p->size);
	    }
	  else /* In memory?  */
	    {
	      read_memory (p->value, contents + offset, p->size);
	    }
	  offset += p->size;
	}
      /* APPLE LOCAL end mainline */
    }
  else if (ctx->in_reg)
    {
      CORE_ADDR dwarf_regnum = dwarf_expr_fetch (ctx, 0);
      int gdb_regnum = DWARF2_REG_TO_REGNUM (dwarf_regnum);
      retval = value_from_register (SYMBOL_TYPE (var), gdb_regnum, frame);
    }
  else
    {
      CORE_ADDR address = dwarf_expr_fetch (ctx, 0);

      retval = allocate_value (SYMBOL_TYPE (var));
      VALUE_LVAL (retval) = lval_memory;
      set_value_lazy (retval, 1);
      VALUE_ADDRESS (retval) = address;
    }

  /* APPLE LOCAL variable initialized status  */
  set_var_status (retval, ctx->var_status);

  free_dwarf_expr_context (ctx);

  return retval;
}





/* Helper functions and baton for dwarf2_loc_desc_needs_frame.  */

struct needs_frame_baton
{
  int needs_frame;
};

/* Reads from registers do require a frame.  */
static CORE_ADDR
needs_frame_read_reg (void *baton, int regnum)
{
  struct needs_frame_baton *nf_baton = baton;
  nf_baton->needs_frame = 1;
  return 1;
}

/* Reads from memory do not require a frame.  */
static void
needs_frame_read_mem (void *baton, gdb_byte *buf, CORE_ADDR addr, size_t len)
{
  memset (buf, 0, len);
}

/* Frame-relative accesses do require a frame.  */
static void
needs_frame_frame_base (void *baton, gdb_byte **start, size_t * length)
{
  static gdb_byte lit0 = DW_OP_lit0;
  struct needs_frame_baton *nf_baton = baton;

  *start = &lit0;
  *length = 1;

  nf_baton->needs_frame = 1;
}

/* Thread-local accesses do require a frame.  */
static CORE_ADDR
needs_frame_tls_address (void *baton, CORE_ADDR offset)
{
  struct needs_frame_baton *nf_baton = baton;
  nf_baton->needs_frame = 1;
  return 1;
}

/* Return non-zero iff the location expression at DATA (length SIZE)
   requires a frame to evaluate.  */

static int
dwarf2_loc_desc_needs_frame (gdb_byte *data, unsigned short size)
{
  struct needs_frame_baton baton;
  struct dwarf_expr_context *ctx;
  int in_reg;

  baton.needs_frame = 0;

  ctx = new_dwarf_expr_context ();
  ctx->baton = &baton;
  ctx->read_reg = needs_frame_read_reg;
  ctx->read_mem = needs_frame_read_mem;
  ctx->get_frame_base = needs_frame_frame_base;
  ctx->get_tls_address = needs_frame_tls_address;

  dwarf_expr_eval (ctx, data, size, 0);

  in_reg = ctx->in_reg;

  if (ctx->num_pieces > 0)
    {
      int i;

      /* If the location has several pieces, and any of them are in
         registers, then we will need a frame to fetch them from.  */
      for (i = 0; i < ctx->num_pieces; i++)
        if (ctx->pieces[i].in_reg)
          in_reg = 1;
    }

  free_dwarf_expr_context (ctx);

  return baton.needs_frame || in_reg;
}

static void
dwarf2_tracepoint_var_ref (struct symbol *symbol, struct agent_expr *ax,
			   struct axs_value *value, gdb_byte *data,
			   int size)
{
  if (size == 0)
    error (_("Symbol \"%s\" has been optimized out."),
	   SYMBOL_PRINT_NAME (symbol));

  if (size == 1
      && data[0] >= DW_OP_reg0
      && data[0] <= DW_OP_reg31)
    {
      value->kind = axs_lvalue_register;
      value->u.reg = data[0] - DW_OP_reg0;
    }
  else if (data[0] == DW_OP_regx)
    {
      ULONGEST reg;
      read_uleb128 (data + 1, data + size, &reg);
      value->kind = axs_lvalue_register;
      value->u.reg = reg;
    }
  else if (data[0] == DW_OP_fbreg)
    {
      /* And this is worse than just minimal; we should honor the frame base
	 as above.  */
      int frame_reg;
      LONGEST frame_offset;
      gdb_byte *buf_end;

      buf_end = read_sleb128 (data + 1, data + size, &frame_offset);
      if (buf_end != data + size)
	error (_("Unexpected opcode after DW_OP_fbreg for symbol \"%s\"."),
	       SYMBOL_PRINT_NAME (symbol));

      TARGET_VIRTUAL_FRAME_POINTER (ax->scope, &frame_reg, &frame_offset);
      ax_reg (ax, frame_reg);
      ax_const_l (ax, frame_offset);
      ax_simple (ax, aop_add);

      ax_const_l (ax, frame_offset);
      ax_simple (ax, aop_add);
      value->kind = axs_lvalue_memory;
    }
  else
    error (_("Unsupported DWARF opcode in the location of \"%s\"."),
	   SYMBOL_PRINT_NAME (symbol));
}

/* Return the value of SYMBOL in FRAME using the DWARF-2 expression
   evaluator to calculate the location.  */
static struct value *
locexpr_read_variable (struct symbol *symbol, struct frame_info *frame)
{
  struct dwarf2_locexpr_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  struct value *val;
  val = dwarf2_evaluate_loc_desc (symbol, frame, dlbaton->data, dlbaton->size,
				  dlbaton->objfile);

  return val;
}

/* Return non-zero iff we need a frame to evaluate SYMBOL.  */
static int
locexpr_read_needs_frame (struct symbol *symbol)
{
  struct dwarf2_locexpr_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  return dwarf2_loc_desc_needs_frame (dlbaton->data, dlbaton->size);
}

/* Print a natural-language description of SYMBOL to STREAM.  */
static int
locexpr_describe_location (struct symbol *symbol, struct ui_file *stream)
{
  /* FIXME: be more extensive.  */
  struct dwarf2_locexpr_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  /* APPLE LOCAL begin print better location information.  */
  struct dwarf_expr_context *ctx;
  gdb_byte *loc_ptr;
  gdb_byte *loc_end;
  /* APPLE LOCAL end print better location information.  */

  if (dlbaton->size == 1
      && dlbaton->data[0] >= DW_OP_reg0
      && dlbaton->data[0] <= DW_OP_reg31)
    {
      int regno = DWARF2_REG_TO_REGNUM (dlbaton->data[0] - DW_OP_reg0);
      fprintf_filtered (stream,
			"a variable in register %s", REGISTER_NAME (regno));
      return 1;
    }

  /* The location expression for a TLS variable looks like this (on a
     64-bit LE machine):

     DW_AT_location    : 10 byte block: 3 4 0 0 0 0 0 0 0 e0
                        (DW_OP_addr: 4; DW_OP_GNU_push_tls_address)
     
     0x3 is the encoding for DW_OP_addr, which has an operand as long
     as the size of an address on the target machine (here is 8
     bytes).  0xe0 is the encoding for DW_OP_GNU_push_tls_address.
     The operand represents the offset at which the variable is within
     the thread local storage.  */

  if (dlbaton->size > 1 
      && dlbaton->data[dlbaton->size - 1] == DW_OP_GNU_push_tls_address)
    if (dlbaton->data[0] == DW_OP_addr)
      {
	int bytes_read;
	CORE_ADDR offset = dwarf2_read_address (&dlbaton->data[1],
						&dlbaton->data[dlbaton->size - 1],
						&bytes_read);
	fprintf_filtered (stream, 
			  "a thread-local variable at offset %s in the "
			  "thread-local storage for `%s'",
			  paddr_nz (offset), dlbaton->objfile->name);
	return 1;
      }
  
  /* APPLE LOCAL print better location information.  */

  /* Create a context, to pass to print_single_dwarf_location.  */

  ctx = new_dwarf_expr_context ();
  ctx->baton = dlbaton;
  ctx->read_reg = dwarf_expr_read_reg;
  ctx->read_mem = dwarf_expr_read_mem;
  ctx->get_frame_base = dwarf_expr_frame_base;
  ctx->get_tls_address = dwarf_expr_tls_address;

  /* Set up starting & ending pointers, to pass to 
     print_single_dwarf_location.  */

  loc_ptr = &dlbaton->data[0];
  loc_end = &dlbaton->data[dlbaton->size];

  print_single_dwarf_location (stream, &loc_ptr, loc_end, ctx);

  /* APPLE LOCAL end print better location information.  */

  return 1;
}


/* Describe the location of SYMBOL as an agent value in VALUE, generating
   any necessary bytecode in AX.

   NOTE drow/2003-02-26: This function is extremely minimal, because
   doing it correctly is extremely complicated and there is no
   publicly available stub with tracepoint support for me to test
   against.  When there is one this function should be revisited.  */

static void
locexpr_tracepoint_var_ref (struct symbol * symbol, struct agent_expr * ax,
			    struct axs_value * value)
{
  struct dwarf2_locexpr_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);

  dwarf2_tracepoint_var_ref (symbol, ax, value, dlbaton->data, dlbaton->size);
}

/* The set of location functions used with the DWARF-2 expression
   evaluator.  */
const struct symbol_ops dwarf2_locexpr_funcs = {
  locexpr_read_variable,
  locexpr_read_needs_frame,
  locexpr_describe_location,
  locexpr_tracepoint_var_ref
};


/* Wrapper functions for location lists.  These generally find
   the appropriate location expression and call something above.  */

/* Return the value of SYMBOL in FRAME using the DWARF-2 expression
   evaluator to calculate the location.  */
static struct value *
loclist_read_variable (struct symbol *symbol, struct frame_info *frame)
{
  struct dwarf2_loclist_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  struct value *val;
  gdb_byte *data;
  size_t size;

  data = find_location_expression (dlbaton, &size,
				   frame ? get_frame_pc (frame) : 0);
  if (data == NULL)
    {
      val = allocate_value (SYMBOL_TYPE (symbol));
      VALUE_LVAL (val) = not_lval;
      /* APPLE LOCAL variable opt states.  */
      set_value_optimized_out (val, opt_evicted);
    }
  else
    val = dwarf2_evaluate_loc_desc (symbol, frame, data, size,
				    dlbaton->objfile);

  return val;
}

/* Return non-zero iff we need a frame to evaluate SYMBOL.  */
static int
loclist_read_needs_frame (struct symbol *symbol)
{
  /* If there's a location list, then assume we need to have a frame
     to choose the appropriate location expression.  With tracking of
     global variables this is not necessarily true, but such tracking
     is disabled in GCC at the moment until we figure out how to
     represent it.  */

  return 1;
}

/* APPLE LOCAL begin print location lists */
/* Print a single location from a location list.  */

/* FIXME: This function was copied extensively from the one that
   actually fetches the value; there is a lot of extra code in this
   function that could probably be safely removed since we only want
   the variable's location, not its value. */

static void
print_single_dwarf_location (struct ui_file *stream, gdb_byte **loc_ptr, 
			     gdb_byte *op_end,
			     struct dwarf_expr_context *ctx)
{
  CORE_ADDR result;
  int bytes_read;
  ULONGEST uoffset, reg;
  LONGEST offset;
  gdb_byte *op_ptr = *loc_ptr;

  while (op_ptr < op_end)
    {
      enum dwarf_location_atom op = *op_ptr++;
      switch (op)
	{
	case DW_OP_lit0:
	case DW_OP_lit1:
	case DW_OP_lit2:
	case DW_OP_lit3:
	case DW_OP_lit4:
	case DW_OP_lit5:
	case DW_OP_lit6:
	case DW_OP_lit7:
	case DW_OP_lit8:
	case DW_OP_lit9:
	case DW_OP_lit10:
	case DW_OP_lit11:
	case DW_OP_lit12:
	case DW_OP_lit13:
	case DW_OP_lit14:
	case DW_OP_lit15:
	case DW_OP_lit16:
	case DW_OP_lit17:
	case DW_OP_lit18:
	case DW_OP_lit19:
	case DW_OP_lit20:
	case DW_OP_lit21:
	case DW_OP_lit22:
	case DW_OP_lit23:
	case DW_OP_lit24:
	case DW_OP_lit25:
	case DW_OP_lit26:
	case DW_OP_lit27:
	case DW_OP_lit28:
	case DW_OP_lit29:
	case DW_OP_lit30:
	case DW_OP_lit31:
	  /* result = op - DW_OP_lit0; */
	  fprintf_filtered (stream, "a literal");
	  break;
	      
	case DW_OP_addr:
	  result = dwarf2_read_address (op_ptr, op_end, &bytes_read);
	  fprintf_filtered (stream, "at address 0x%s", paddr_nz (result));
	  op_ptr += bytes_read;
	  break;
	      
	case DW_OP_const1u:
	case DW_OP_const1s:
	  /* result = extract_unsigned_integer (op_ptr, 1); */
	  fprintf_filtered (stream, "a constant");
	  op_ptr += 1;
	  break;
	case DW_OP_const2u:
	case DW_OP_const2s:
	  /* result = extract_unsigned_integer (op_ptr, 2); */
	  fprintf_filtered (stream, "a constant");
	  op_ptr += 2;
	  break;
	case DW_OP_const4u:
	case DW_OP_const4s:
	  /* result = extract_unsigned_integer (op_ptr, 4); */
	  fprintf_filtered (stream, "a constant");
	  op_ptr += 4;
	  break;
	case DW_OP_const8u:
	case DW_OP_const8s:
	  /* result = extract_unsigned_integer (op_ptr, 8); */
	  fprintf_filtered (stream, "a constant");
	  op_ptr += 8;
	  break;
	case DW_OP_constu:
	  op_ptr = read_uleb128 (op_ptr, op_end, &uoffset);
	  /* result = uoffset; */
	  fprintf_filtered (stream, "a constant");
	  break;
	case DW_OP_consts:
	  op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	  /* result = offset; */
	  fprintf_filtered (stream, "a constant");
	  break;
	      
	  /* The DW_OP_reg operations are required to occur alone in
	     location expressions.  */
	case DW_OP_reg0:
	case DW_OP_reg1:
	case DW_OP_reg2:
	case DW_OP_reg3:
	case DW_OP_reg4:
	case DW_OP_reg5:
	case DW_OP_reg6:
	case DW_OP_reg7:
	case DW_OP_reg8:
	case DW_OP_reg9:
	case DW_OP_reg10:
	case DW_OP_reg11:
	case DW_OP_reg12:
	case DW_OP_reg13:
	case DW_OP_reg14:
	case DW_OP_reg15:
	case DW_OP_reg16:
	case DW_OP_reg17:
	case DW_OP_reg18:
	case DW_OP_reg19:
	case DW_OP_reg20:
	case DW_OP_reg21:
	case DW_OP_reg22:
	case DW_OP_reg23:
	case DW_OP_reg24:
	case DW_OP_reg25:
	case DW_OP_reg26:
	case DW_OP_reg27:
	case DW_OP_reg28:
	case DW_OP_reg29:
	case DW_OP_reg30:
	case DW_OP_reg31:
	  if (op_ptr != op_end 
	      && *op_ptr != DW_OP_piece 
	      && *op_ptr != DW_OP_APPLE_uninit)
	    error (_("DWARF-2 expression error: DW_OP_reg operations must be "
		     "used either alone or in conjuction with DW_OP_piece."));
	      
	  result = op - DW_OP_reg0;
	  /* APPLE LOCAL print register name instead of number.  */
	  fprintf_filtered (stream, "in register %s", 
			    REGISTER_NAME (DWARF2_REG_TO_REGNUM (result)));
	      
	  break;
	      
	case DW_OP_regx:
	  op_ptr = read_uleb128 (op_ptr, op_end, &reg);
	  if (op_ptr != op_end 
	      && *op_ptr != DW_OP_piece
	      && *op_ptr != DW_OP_APPLE_uninit)
	    error (_("DWARF-2 expression error: DW_OP_reg operations must be "
		     "used either alone or in conjuction with DW_OP_piece."));
	  /* APPLE LOCAL print register name instead of number.  */
	  fprintf_filtered (stream, "in register %s", 
			    REGISTER_NAME (DWARF2_REG_TO_REGNUM (reg)));
	  break;
	      
	case DW_OP_breg0:
	case DW_OP_breg1:
	case DW_OP_breg2:
	case DW_OP_breg3:
	case DW_OP_breg4:
	case DW_OP_breg5:
	case DW_OP_breg6:
	case DW_OP_breg7:
	case DW_OP_breg8:
	case DW_OP_breg9:
	case DW_OP_breg10:
	case DW_OP_breg11:
	case DW_OP_breg12:
	case DW_OP_breg13:
	case DW_OP_breg14:
	case DW_OP_breg15:
	case DW_OP_breg16:
	case DW_OP_breg17:
	case DW_OP_breg18:
	case DW_OP_breg19:
	case DW_OP_breg20:
	case DW_OP_breg21:
	case DW_OP_breg22:
	case DW_OP_breg23:
	case DW_OP_breg24:
	case DW_OP_breg25:
	case DW_OP_breg26:
	case DW_OP_breg27:
	case DW_OP_breg28:
	case DW_OP_breg29:
	case DW_OP_breg30:
	case DW_OP_breg31:
	  {
	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	    result = op - DW_OP_breg0;

	    fprintf_filtered (stream, "at the address (reg %d + %d)", 
                              (unsigned int) result, (signed int) offset);
	    /*
	      result = (ctx->read_reg) (ctx->baton, op - DW_OP_breg0);
	      result += offset;
	    */
	  }
	  break;
	case DW_OP_bregx:
	  {
	    op_ptr = read_uleb128 (op_ptr, op_end, &reg);
	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	    fprintf_filtered (stream, "at the address (reg %d + %d)",
			      (unsigned int) reg, (signed int)  offset);
	    /*
	      result = (ctx->read_reg) (ctx->baton, reg);
	      result += offset;
	    */
	  }
	  break;
	case DW_OP_fbreg:
	  {
	    unsigned int before_stack_len;
	    
	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	    /* Rather than create a whole new context, we simply
	       record the stack length before execution, then reset it
	       afterwards, effectively erasing whatever the recursive
	       call put there.  */
	    fprintf_filtered (stream, 
                          "at offset %d  from the frame base pointer", 
			      (signed int) offset);
	    before_stack_len = ctx->stack_len;
	    /* FIXME: cagney/2003-03-26: This code should be using
	       get_frame_base_address(), and then implement a dwarf2
	       specific this_base method.  */
	    /*
	      (ctx->get_frame_base) (ctx->baton, &datastart, &datalen);
	      dwarf_expr_eval (ctx, datastart, datalen, 0);
	      result = dwarf_expr_fetch (ctx, 0);
	      if (ctx->in_reg)
	      result = (ctx->read_reg) (ctx->baton, result);
	      result = result + offset;
	      ctx->stack_len = before_stack_len;
	      ctx->in_reg = 0;
	    */
	  }
	  break;
	case DW_OP_dup:
	  result = dwarf_expr_fetch (ctx, 0);
	  break;
	      
	case DW_OP_drop:
	  dwarf_expr_pop (ctx);
	  break;
	      
	case DW_OP_pick:
	  offset = *op_ptr++;
	  result = dwarf_expr_fetch (ctx, offset);
	  break;
	      
	case DW_OP_over:
	  result = dwarf_expr_fetch (ctx, 1);
	  break;
	      
	case DW_OP_rot:
	  {
	    CORE_ADDR t1, t2, t3;
	    

	    if (ctx->stack_len < 3)
	      error (_("Not enough elements for DW_OP_rot. Need 3, have %d."),
		     ctx->stack_len);
	    t1 = ctx->stack[ctx->stack_len - 1];
	    t2 = ctx->stack[ctx->stack_len - 2];
	    t3 = ctx->stack[ctx->stack_len - 3];
	    ctx->stack[ctx->stack_len - 1] = t2;
	    ctx->stack[ctx->stack_len - 2] = t3;
	    ctx->stack[ctx->stack_len - 3] = t1;
	    break;
	  }
	
	case DW_OP_deref:
	case DW_OP_deref_size:
	case DW_OP_abs:
	case DW_OP_neg:
	case DW_OP_not:
	case DW_OP_plus_uconst:
	  /* Unary operations.  */

	  result = dwarf_expr_fetch (ctx, 0);
	  dwarf_expr_pop (ctx);

	  switch (op)
	    {
	    case DW_OP_deref:
	      {
		fprintf_filtered (stream, "at address 0x%s", paddr_nz (result));
		/*
		(ctx->read_mem) (ctx->baton, buf, result,
		                 TARGET_ADDR_BIT / TARGET_CHAR_BIT);
		*/
	      }
	      break;
	      
	    case DW_OP_deref_size:
	      {
		gdb_byte *buf = alloca (TARGET_ADDR_BIT / TARGET_CHAR_BIT);
		
		(ctx->read_mem) (ctx->baton, buf, result, *op_ptr++); 
		fprintf_filtered (stream, "at address 0x%s", paddr_nz (result));
	      }
	      break;
	      
	    case DW_OP_abs:
	      if ((signed int) result < 0)
		result = -result;
	      break;
	    case DW_OP_neg:
	      result = -result;
	      break;
	    case DW_OP_not:
	      result = ~result;
	      break;
	    case DW_OP_plus_uconst:
	      op_ptr = read_uleb128 (op_ptr, op_end, &reg);
	      result += reg;
	      break;
	    default:
	      break;
	    }
	  break;
	      
	case DW_OP_and:
	case DW_OP_div:
	case DW_OP_minus:
	case DW_OP_mod:
	case DW_OP_mul:
	case DW_OP_or:
	case DW_OP_plus:
	case DW_OP_shl:
	case DW_OP_shr:
	case DW_OP_shra:
	case DW_OP_xor:
	case DW_OP_le:
	case DW_OP_ge:
	case DW_OP_eq:
	case DW_OP_lt:
	case DW_OP_gt:
	case DW_OP_ne:
	  {
	    /* Binary operations.  Use the value engine to do computations in
	       the right width.  */
	    CORE_ADDR first, second;
	    enum exp_opcode binop;
	    struct value *val1, *val2;
	    
	    second = dwarf_expr_fetch (ctx, 0);
	    dwarf_expr_pop (ctx);
	    
	    first = dwarf_expr_fetch (ctx, 0);
	    dwarf_expr_pop (ctx);
	    
	    val1 = value_from_longest (unsigned_address_type (), first);
	    val2 = value_from_longest (unsigned_address_type (), second);
	    
	    switch (op)
	      {
	      case DW_OP_and:
		binop = BINOP_BITWISE_AND;
		break;
	      case DW_OP_div:
		binop = BINOP_DIV;
		break;
	      case DW_OP_minus:
		binop = BINOP_SUB;
		break;
	      case DW_OP_mod:
		binop = BINOP_MOD;
		break;
	      case DW_OP_mul:
		binop = BINOP_MUL;
		break;
	      case DW_OP_or:
		binop = BINOP_BITWISE_IOR;
		break;
	      case DW_OP_plus:
		binop = BINOP_ADD;
		break;
	      case DW_OP_shl:
		binop = BINOP_LSH;
		break;
	      case DW_OP_shr:
		binop = BINOP_RSH;
		break;
	      case DW_OP_shra:
		binop = BINOP_RSH;
		val1 = value_from_longest (signed_address_type (), first);
		break;
	      case DW_OP_xor:
		binop = BINOP_BITWISE_XOR;
		break;
	      case DW_OP_le:
		binop = BINOP_LEQ;
		break;
	      case DW_OP_ge:
		binop = BINOP_GEQ;
		break;
	      case DW_OP_eq:
		binop = BINOP_EQUAL;
		break;
	      case DW_OP_lt:
		binop = BINOP_LESS;
		break;
	      case DW_OP_gt:
		binop = BINOP_GTR;
		break;
	      case DW_OP_ne:
		binop = BINOP_NOTEQUAL;
		break;
	      default:
		internal_error (__FILE__, __LINE__,
				_("Can't be reached."));
	      }
	    result = value_as_long (value_binop (val1, val2, binop));
	  }
	  break;
	  
	case DW_OP_GNU_push_tls_address:
	  /* Variable is at a constant offset in the thread-local
	     storage block into the objfile for the current thread and
	     the dynamic linker module containing this expression. Here
	     we return returns the offset from that base.  The top of the
	     stack has the offset from the beginning of the thread
	     control block at which the variable is located.  Nothing
	     should follow this operator, so the top of stack would be
	     returned.  */
	  result = dwarf_expr_fetch (ctx, 0);
	  dwarf_expr_pop (ctx);
	  result = (ctx->get_tls_address) (ctx->baton, result);
	  fprintf_filtered (stream, 
                       "at 0x%s offset from thread-local storage block", 
                       paddr_nz (result));
			    
	  break;
	  
	case DW_OP_skip:
	  offset = extract_signed_integer (op_ptr, 2);
	  op_ptr += 2;
	  op_ptr += offset;
	  break;
	  
	case DW_OP_bra:
	  offset = extract_signed_integer (op_ptr, 2);
	  op_ptr += 2;
	  if (dwarf_expr_fetch (ctx, 0) != 0)
	    op_ptr += offset;
	  dwarf_expr_pop (ctx);
	  break;
	  
	case DW_OP_nop:
	  break;
	  
	case DW_OP_piece:
	  {
	    ULONGEST size;
	    CORE_ADDR addr_or_regnum;
	    
	    /* Record the piece.  */
	    op_ptr = read_uleb128 (op_ptr, op_end, &size);
	    addr_or_regnum = dwarf_expr_fetch (ctx, 0);
	    add_piece (ctx, ctx->in_reg, addr_or_regnum, size);
	    
	    /* Pop off the address/regnum, and clear the in_reg flag.  */
	    dwarf_expr_pop (ctx);
	    ctx->in_reg = 0;
	  }
	  break;
	  
	case DW_OP_APPLE_uninit:
	  fprintf_filtered (stream, " [ uninitialized ]");
	  break;
	  
	default:
	  fprintf_filtered (stream, "unable to determine position");
	}
      
    }

  *loc_ptr = op_ptr;
}

/* Print a natural-language description of SYMBOL to STREAM.  */
static int
loclist_describe_location (struct symbol *symbol, struct ui_file *stream)
{
  /* FIXME: Could print the entire list of locations.  */
  struct dwarf2_loclist_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  struct dwarf_expr_context *ctx;
  gdb_byte *loc_ptr, *buf_end, *loc_end;
  int length;
  CORE_ADDR low, high;
  unsigned int addr_size = TARGET_ADDR_BIT / TARGET_CHAR_BIT;
  CORE_ADDR base_mask = ~(~(CORE_ADDR)1 << (addr_size * 8 - 1));
  /* Adjust base_address for relocatable objects.  */
  CORE_ADDR base_offset = objfile_text_section_offset (dlbaton->objfile);
  CORE_ADDR base_address = dlbaton->base_address_untranslated;

  loc_ptr = dlbaton->data;
  buf_end = dlbaton->data + dlbaton->size;

  fprintf_filtered (stream, "\n");
  while (1)
    {
      low = dwarf2_read_address (loc_ptr, buf_end, &length);
      loc_ptr += length;
      high = dwarf2_read_address (loc_ptr, buf_end, &length);
      loc_ptr += length;

      /* An end-of-list entry.  */
      if (low == 0 && high == 0)
	break;

      /* A base-address-selection entry.  */
      if ((low & base_mask) == base_mask)
	{
	  base_address = high;
	  continue;
	}
      /* Otherwise, a location expression entry.  */

      low += base_address;
      high += base_address;

      /* APPLE LOCAL: for the debug-info-in-.o-files case we need to go through
         the address translation table for the addresses in the DWARF (in the
         .o file) to get the final addresses in the executable.  */

      if (dlbaton->addr_map)
        {
          translate_debug_map_address (dlbaton->addr_map, low, &low,  0);
          translate_debug_map_address (dlbaton->addr_map, high, &high, 1);
        }

      low += base_offset;
      high += base_offset;

      length = extract_unsigned_integer (loc_ptr, 2);
      loc_ptr += 2;

      loc_end = loc_ptr + length;
      
      ctx = new_dwarf_expr_context ();
      ctx->baton = dlbaton;
      ctx->read_reg = dwarf_expr_read_reg;
      ctx->read_mem = dwarf_expr_read_mem;
      ctx->get_frame_base = dwarf_expr_frame_base;
      ctx->get_tls_address = dwarf_expr_tls_address;

      /* APPLE LOCAL begin Don't print extra newline at end of list.  */
      fprintf_filtered (stream, "\n   0x%s - 0x%s: ", paddr_nz (low), 
                        paddr_nz (high));
      
      print_single_dwarf_location (stream, &loc_ptr, loc_end, ctx);
      /* APPLE LOCAL end Don't print extra newline at end of list.  */

      free_dwarf_expr_context (ctx);
    }

  return 1;
}
/* APPLE LOCAL end print location lists  */

/* Describe the location of SYMBOL as an agent value in VALUE, generating
   any necessary bytecode in AX.  */
static void
loclist_tracepoint_var_ref (struct symbol * symbol, struct agent_expr * ax,
			    struct axs_value * value)
{
  struct dwarf2_loclist_baton *dlbaton = SYMBOL_LOCATION_BATON (symbol);
  gdb_byte *data;
  size_t size;

  data = find_location_expression (dlbaton, &size, ax->scope);
  if (data == NULL)
    error (_("Variable \"%s\" is not available."), SYMBOL_NATURAL_NAME (symbol));

  dwarf2_tracepoint_var_ref (symbol, ax, value, data, size);
}

/* The set of location functions used with the DWARF-2 expression
   evaluator and location lists.  */
const struct symbol_ops dwarf2_loclist_funcs = {
  loclist_read_variable,
  loclist_read_needs_frame,
  loclist_describe_location,
  loclist_tracepoint_var_ref
};
