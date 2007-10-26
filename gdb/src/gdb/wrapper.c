/* Longjump free calls to GDB internal routines.

   Copyright 1999, 2000, 2005 Free Software Foundation, Inc.

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
#include "value.h"
#include "exceptions.h"
#include "wrapper.h"
#include "ui-out.h"

/* APPLE LOCAL: Need some local includes. */
#include "varobj.h"
#include "objc-lang.h"

int
gdb_parse_exp_1 (char **stringptr, struct block *block, int comma,
		 struct expression **expression)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *expression = parse_exp_1 (stringptr, block, comma);
    }

  if (except.reason < 0)
    return 0;
  return 1;
  
}

int
gdb_evaluate_expression (struct expression *exp, struct value **value)
{
  /* APPLE LOCAL: The expression may contain a function, and we
     certainly don't want a crash in the function to leave the stack
     in a funny state. So set unwind-on-signal to clear up after any
     mishaps. */

  volatile struct gdb_exception except;
  int old_unwind;

  old_unwind = set_unwind_on_signal (1);

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *value = evaluate_expression(exp);
    }

  set_unwind_on_signal (old_unwind);

  if (except.reason < 0)
    return 0;
  return 1;
}

/* APPLE LOCAL: New function. */
int
gdb_print_expression (struct expression *exp, struct ui_file *stb)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      print_expression (exp, stb);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

/* APPLE LOCAL: New function. */
int
gdb_evaluate_type (struct expression *exp, struct value **value)
{
  /* APPLE LOCAL: The expression may contain a function, and we
     certainly don't want a crash in the function to leave the stack
     in a funny state. So set unwind-on-signal to clear up after any
     mishaps. */

  volatile struct gdb_exception except;
  int old_unwind;

  old_unwind = set_unwind_on_signal (1);

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *value = evaluate_type (exp);
    }

  set_unwind_on_signal (old_unwind);

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_fetch_lazy (struct value *val)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      value_fetch_lazy (val);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_equal (struct value *val1, struct value *val2, int *result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_equal (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_assign (struct value *val1, struct value *val2,
                  struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_assign (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_subscript (struct value *val1, struct value *val2,
                     struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_subscript (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_ind (struct value *val, struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_ind (val);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

/* APPLE LOCAL: New function. */
int
gdb_value_cast (struct type *type, struct value *val, struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_cast (type, val);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_parse_and_eval_type (char *p, int length, struct type **type)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *type = parse_and_eval_type (p, length);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

enum gdb_rc
gdb_value_struct_elt (struct ui_out *uiout, struct value **result,
                      struct value **argp, struct value **args, char *name,
                      int *static_memfuncp, char *err)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ALL)
    {
      *result = value_struct_elt (argp, args, name, static_memfuncp, err);
    }

  if (except.reason < 0)
    return GDB_RC_FAIL;
  return GDB_RC_OK;
}

/* APPLE LOCAL: New function. */
int
gdb_varobj_get_value (struct varobj *val1, char **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ALL)
    {
      *result = varobj_get_value (val1);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

/* APPLE LOCAL: New function. */
int
safe_value_objc_target_type (struct value *val, struct block *block, 
			     struct type **dynamic_type,
			     char **dynamic_type_handle)
{
  volatile struct gdb_exception except;
  struct ui_file *saved_gdb_stderr;
  static struct ui_file *null_stderr = NULL;

  /* suppress error messages 
     FIXME: Consolidate this with the use in value_rtti_target_type
     and elsewhere where we want to suppress the messages. */

  if (null_stderr == NULL)
    null_stderr = ui_file_new ();

  saved_gdb_stderr = gdb_stderr;
  gdb_stderr = null_stderr;

  TRY_CATCH (except, RETURN_MASK_ALL) 
    {
      *dynamic_type = value_objc_target_type (val, block, dynamic_type_handle);
    }

  gdb_stderr = saved_gdb_stderr;

  if (except.reason < 0)
    return 0;
  return 1;
}
