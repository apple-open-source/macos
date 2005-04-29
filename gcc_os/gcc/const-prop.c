/* APPLE LOCAL BEGIN - single-set constant propagation (ENTIRE FILE) */
/* Constant propagation routines for the GNU compiler.
   Copyright (C) 2000, 2002 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "rtl.h"
#include "tree.h"
#include "basic-block.h"
#include "hard-reg-set.h"
#include "expr.h"
#include "output.h"


/* The functions in this file are designed to perform a very simplified
   version of constant propagation.  The idea is to find all registers
   that are assigned constants, remove any registers that ever receive a
   second assignment, and propagation the remaining constant assignment
   values to uses of the assigned registers.  Because it only propagates
   values for registers that receive a single assignment, I call this
   "single-set constant propagation". */

typedef struct const_prop_info {

  /* The following field contains the rtx code that assigns a constant
     value to the register. */

  rtx assignment;

  /* The following field should contain one of exactly three values:

     0 = no assignment for the regster has been seen yet
     1 = exactly one assignment has been seen, and the value assigned
         is a constant
    -1 = either a non-constant value was assigned to the register, or
         more than one assignment to the register has been seen.
  */

  int is_single_set_const;
} const_prop_record;

const_prop_record *const_prop_data = NULL;

/* Initialize array to hold constant propagation data.  The array contains
   one cell/entry for each register used in the rtx code.  Entries
   are initialized to zero/null, and are filled in when an assignment to
   a register is encountered. */

static void
initialize_const_prop_info ()
{

  int max_regno = max_reg_num ();

  const_prop_data = (const_prop_record *) xmalloc (max_regno * 
						   sizeof (const_prop_record));

  memset (const_prop_data, 0, max_regno * sizeof (const_prop_record));

}


/* This function takes an assignment to a register and fills in the array
   information appropriatly. */

static
void process_assignment (assignment)
     rtx assignment;
{
  rtx src = SET_SRC (assignment);
  rtx dest = SET_DEST (assignment);
  int index;

  /* Check to make sure destination of SET is a register. */

  while ((GET_CODE (dest) == STRICT_LOW_PART)
	 || (GET_CODE (dest) == SUBREG)
	 || (GET_CODE (dest) == SIGN_EXTRACT)
	 || (GET_CODE (dest) == ZERO_EXTRACT))
    dest = XEXP (dest, 0);

  if (GET_CODE (dest) != REG)
    return;

  /* Make sure destination of SET is not a hard register (we don't want to
     perform this optimization on hard registers). */

  if (REGNO (dest) < FIRST_PSEUDO_REGISTER)
    return;
  
  index = REGNO (dest);
	      
  /* Check to see if register is already disqualified for constant propagation. */

  if (const_prop_data[index].is_single_set_const < 0)
    return;


  else if (const_prop_data[index].is_single_set_const > 0)
    /* This is the second assignment we have seen for this register, so 
       disqualify the register for constant propagation */
    {
      const_prop_data[index].assignment = NULL;
      const_prop_data[index].is_single_set_const = -1;
    }
  else 
    {

      /* This is the first assignment we have seen for this register;
	 check to see if it's a constant, then set the register information
	 appropriately. */
      
      if ((GET_CODE (src) == CONST_INT)
	  || (GET_CODE (src) == CONST_DOUBLE))
	{
	  const_prop_data[index].assignment = assignment;
	  const_prop_data[index].is_single_set_const = 1;
	}
      else
	/* Assignment value is not a constant, so disqualify register for
	   constant propagation. */
	
	const_prop_data[index].is_single_set_const = -1;
    }
}


void
find_all_single_set_constants ()
{
  rtx cur_insn;

  /* Set up the register information array. */
  
  initialize_const_prop_info();

  /* Go through all the instructions for the current function, looking for
     assignments.  Whenever an assignment is found, pass it to
     process_assignment to see if it's an assignment of a constant to
     a register, and to set the register information array appropriately. */

  for (cur_insn = get_insns(); cur_insn; cur_insn = NEXT_INSN (cur_insn))
    {
      if (INSN_P (cur_insn))
	{
	  if (GET_CODE (PATTERN (cur_insn)) == PARALLEL)
	    
	    /* Check each element of PARALLEL for assignments. */

	    {
	      int i;
	      rtx vec = PATTERN (cur_insn);

	      for (i = XVECLEN (vec, 0) - 1; i >= 0; i--)
		{
		  rtx vec_elt = XVECEXP (vec, 0, i);

		  if (GET_CODE (vec_elt) == SET)
		    process_assignment (vec_elt);
		}
	    }

	  else if (GET_CODE (PATTERN (cur_insn)) == SET)

	    process_assignment (PATTERN (cur_insn));

	}
    }
}

/* Someone has hijacked a SET that we may have in our table; remove
   the previous table entry, and add the new one.  Either parameter
   may be NULL, but non-NULL parameters should be of the form "(set
   (reg ...".  */
void
ss_replace_assignment (old_assignment, new_assignment)
     rtx old_assignment;
     rtx new_assignment;
{
  if (!const_prop_data)
    return;

  if (old_assignment
      && GET_CODE (old_assignment) == SET
      && GET_CODE (SET_DEST (old_assignment)) == REG)
    {
      int index = REGNO (SET_DEST (old_assignment));
      /* If this register was in our table, forget we saw it.  */
      if (const_prop_data[index].assignment == old_assignment)
	{
	  const_prop_data[index].assignment = NULL;
	  const_prop_data[index].is_single_set_const = 0;
	}
    }

  if (new_assignment
      && GET_CODE (new_assignment) == SET
      && GET_CODE (SET_DEST (new_assignment)) == REG)
    process_assignment (new_assignment);
}

rtx
ss_constant_propagation (op)
     rtx op;
{
  int index;

  /* This function is called from 'simplify_binary_operation' in
     simplify-rtx.c.  It checks to see if the parameter it is passed
     is a register.  If so, it checks to see if the
     register is eligible for constant propagation.  If the register
     is eligible for constant propagation, this function returns the
     constant value rtx, otherwise it returns the rtx that was passed in. */

  if (!const_prop_data)
    return op;

  if (GET_CODE (op) != REG)
    return op;

  index = REGNO (op);

  if (const_prop_data[index].is_single_set_const == 1)
    return (SET_SRC (const_prop_data[index].assignment));
  else
    return op;
}

void
cleanup_ss_constant_propagation ()
{

  /* Free the memory used for the array of register data and set the
     "array" to NULL. */

  free (const_prop_data);
  const_prop_data = NULL;
}

/* APPLE LOCAL END - single-set constant propagation */
