/* Longjump free calls to gdb internal routines.
   Copyright 1999, 2000 Free Software Foundation, Inc.

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
/* APPLE LOCAL begin */
#include "varobj.h"
#include "objc-lang.h"
/* APPLE LOCAL end */
#include "wrapper.h"

/* Use this struct to pass arguments to wrapper routines. We assume
   (arbitrarily) that no gdb function takes more than ten arguments. */
struct gdb_wrapper_arguments
  {

    /* Pointer to some result from the gdb function call, if any */
    union wrapper_results 
      {
	int   integer;
	void *pointer;
      } result;
	

    /* The list of arguments. */
    union wrapper_args 
      {
	int   integer;
	void *pointer;
      } args[10];
  };

struct captured_value_struct_elt_args
{
  struct value **argp;
  struct value **args;
  char *name;
  int *static_memfuncp;
  char *err;
  struct value **result_ptr;
};

static int wrap_parse_exp_1 (char *);

static int wrap_evaluate_expression (char *);

/* APPLE LOCAL begin */
static int wrap_print_expression (char *a);

static int wrap_evaluate_type (char *);
/* APPLE LOCAL end */

static int wrap_value_fetch_lazy (char *);

static int wrap_value_equal (char *);

static int wrap_value_assign (char *);

static int wrap_value_subscript (char *);

static int wrap_value_ind (char *opaque_arg);

/* APPLE LOCAL */
static int wrap_value_cast (char *);

static int do_captured_value_struct_elt (struct ui_out *uiout, void *data);

static int wrap_parse_and_eval_type (char *);

/* APPLE LOCAL begin */
static int wrap_varobj_get_value (char *a);

static int wrap_value_objc_target_type (char *);
/* APPLE LOCAL end */

int
gdb_parse_exp_1 (char **stringptr, struct block *block, int comma,
		 struct expression **expression)
{
  struct gdb_wrapper_arguments args;
  args.args[0].pointer = stringptr;
  args.args[1].pointer = block;
  args.args[2].integer = comma;

  if (!catch_errors ((catch_errors_ftype *) wrap_parse_exp_1, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *expression = (struct expression *) args.result.pointer;
  return 1;
  
}

static int
wrap_parse_exp_1 (char *argptr)
{
  struct gdb_wrapper_arguments *args 
    = (struct gdb_wrapper_arguments *) argptr;
  args->result.pointer = parse_exp_1((char **) args->args[0].pointer,
				     (struct block *) args->args[1].pointer,
				     args->args[2].integer);
  return 1;
}

int
gdb_evaluate_expression (struct expression *exp, struct value **value)
{
  struct gdb_wrapper_arguments args;
  /* APPLE LOCAL */
  int old_unwind, retval;

  args.args[0].pointer = exp;

  /* APPLE LOCAL begin */
  /* The expression may contain a function, and we certainly don't want
     a crash in the function to leave the stack in a funny state. */

  old_unwind = set_unwind_on_signal (1);
  /* APPLE LOCAL end */

  if (!catch_errors ((catch_errors_ftype *) wrap_evaluate_expression, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      /* APPLE LOCAL begin */
      retval = 0;
    }
  else
    {
      *value = (struct value *) args.result.pointer;
      retval = 1;
      /* APPLE LOCAL end */
    }

  /* APPLE LOCAL begin */
  set_unwind_on_signal (old_unwind);
  return retval;
  /* APPLE LOCAL end */
}

static int
wrap_evaluate_expression (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;

  (args)->result.pointer =
    (char *) evaluate_expression ((struct expression *) args->args[0].pointer);
  return 1;
}

/* APPLE LOCAL begin safe expression printing */
int
gdb_print_expression (struct expression *exp, struct ui_file *stb)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = exp;
  args.args[1].pointer = stb;

  if (!catch_errors ((catch_errors_ftype *) wrap_print_expression, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  return 1;
}

static int
wrap_print_expression (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;
  struct expression *exp;
  struct ui_file *stb;

  exp = (struct expression *) (args)->args[0].pointer;
  stb = (struct ui_file *) (args)->args[1].pointer;

  print_expression (exp, stb);
  return 1;
}

int
gdb_evaluate_type (struct expression *exp, struct value **value)
{
  struct gdb_wrapper_arguments args;
  int old_unwind;
  int retval;
  args.args[0].pointer = exp;

  /* The expression may contain a function, and we certainly don't want
     a crash in the function to leave the stack in a funny state. */

  old_unwind = set_unwind_on_signal (1);

  if (!catch_errors ((catch_errors_ftype *) wrap_evaluate_type, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      retval = 0;
    }
  else
    {
      *value = (struct value *) args.result.pointer;
      retval = 1;
    }

  set_unwind_on_signal (old_unwind);
  return retval;
}

static int
wrap_evaluate_type (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;

  (args)->result.pointer =
    (char *) evaluate_type ((struct expression *) args->args[0].pointer);
  return 1;
}
/* APPLE LOCAL end safe expression printing */

int
gdb_value_fetch_lazy (struct value *value)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = value;
  return catch_errors ((catch_errors_ftype *) wrap_value_fetch_lazy, &args,
		       "", RETURN_MASK_ERROR);
}

static int
wrap_value_fetch_lazy (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;

  value_fetch_lazy ((struct value *) (args)->args[0].pointer);
  return 1;
}

int
gdb_value_equal (struct value *val1, struct value *val2, int *result)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = val1;
  args.args[1].pointer = val2;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_equal, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *result = args.result.integer;
  return 1;
}

static int
wrap_value_equal (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;
  struct value *val1;
  struct value *val2;

  val1 = (struct value *) (args)->args[0].pointer;
  val2 = (struct value *) (args)->args[1].pointer;

  (args)->result.integer = value_equal (val1, val2);
  return 1;
}

int
gdb_value_assign (struct value *val1, struct value *val2, struct value **result)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = val1;
  args.args[1].pointer = val2;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_assign, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *result = (struct value *) args.result.pointer;
  return 1;
}

static int
wrap_value_assign (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;
  struct value *val1;
  struct value *val2;

  val1 = (struct value *) (args)->args[0].pointer;
  val2 = (struct value *) (args)->args[1].pointer;

  (args)->result.pointer = value_assign (val1, val2);
  return 1;
}

int
gdb_value_subscript (struct value *val1, struct value *val2, struct value **rval)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = val1;
  args.args[1].pointer = val2;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_subscript, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *rval = (struct value *) args.result.pointer;
  return 1;
}

static int
wrap_value_subscript (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;
  struct value *val1;
  struct value *val2;

  val1 = (struct value *) (args)->args[0].pointer;
  val2 = (struct value *) (args)->args[1].pointer;

  (args)->result.pointer = value_subscript (val1, val2);
  return 1;
}

int
gdb_value_ind (struct value *val, struct value **rval)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = val;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_ind, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *rval = (struct value *) args.result.pointer;
  return 1;
}

static int
wrap_value_ind (char *opaque_arg)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) opaque_arg;
  struct value *val;

  val = (struct value *) (args)->args[0].pointer;
  (args)->result.pointer = value_ind (val);
  return 1;
}

/* APPLE LOCAL begin */
int
gdb_value_cast (struct type *type, struct value *in_val, struct value **out_val)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = type;
  args.args[1].pointer = in_val;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_cast, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *out_val = (struct value *) args.result.pointer;
  return 1;
}

static int
wrap_value_cast (char *opaque_arg)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) opaque_arg;
  struct type *type;
  struct value *val;

  type = (struct type *) (args)->args[0].pointer;
  val = (struct value *) (args)->args[1].pointer;

  (args)->result.pointer = value_cast (type, val);
  return 1;
}
/* APPLE LOCAL end */

int
gdb_parse_and_eval_type (char *p, int length, struct type **type)
{
  struct gdb_wrapper_arguments args;
  args.args[0].pointer = p;
  args.args[1].integer = length;

  if (!catch_errors ((catch_errors_ftype *) wrap_parse_and_eval_type, &args,
		     "", RETURN_MASK_ALL))
    {
      /* An error occurred */
      return 0;
    }

  *type = (struct type *) args.result.pointer;
  return 1;
}

static int
wrap_parse_and_eval_type (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;

  char *p = (char *) args->args[0].pointer;
  int length = args->args[1].integer;

  args->result.pointer = (char *) parse_and_eval_type (p, length);

  return 1;
}

enum gdb_rc
gdb_value_struct_elt (struct ui_out *uiout, struct value **result, struct value **argp,
		      struct value **args, char *name, int *static_memfuncp,
		      char *err)
{
  struct captured_value_struct_elt_args cargs;
  cargs.argp = argp;
  cargs.args = args;
  cargs.name = name;
  cargs.static_memfuncp = static_memfuncp;
  cargs.err = err;
  cargs.result_ptr = result;
  return catch_exceptions (uiout, do_captured_value_struct_elt, &cargs,
			   NULL, RETURN_MASK_ALL);
}

static int
do_captured_value_struct_elt (struct ui_out *uiout, void *data)
{
  struct captured_value_struct_elt_args *cargs = data;
  *cargs->result_ptr = value_struct_elt (cargs->argp, cargs->args, cargs->name,
			     cargs->static_memfuncp, cargs->err);
  return GDB_RC_OK;
}

/* APPLE LOCAL begin */
int
gdb_varobj_get_value (struct varobj *val1, char **result)
{
  struct gdb_wrapper_arguments args;

  args.args[0].pointer = val1;

  if (!catch_errors ((catch_errors_ftype *) wrap_varobj_get_value, &args,
		     "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return 0;
    }

  *result = args.result.pointer;
  return 1;
}

static int
wrap_varobj_get_value (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;
  struct varobj *val1;

  val1 = (struct varobj *) (args)->args[0].pointer;

  (args)->result.pointer = varobj_get_value (val1);
  return 1;
}

static int 
wrap_value_objc_target_type (char *a)
{
  struct gdb_wrapper_arguments *args = (struct gdb_wrapper_arguments *) a;

  struct value * val = (struct val *) args->args[0].pointer;
  struct block * block = (struct block *) args->args[1].pointer;

  (args)->result.pointer = value_objc_target_type (val, block);

  return 1;
}

int
safe_value_objc_target_type (struct value *val, struct block *block, struct type **dynamic_type)
{
  struct gdb_wrapper_arguments args;
  struct ui_file *saved_gdb_stderr;
  static struct ui_file *null_stderr = NULL;

  /* suppress error messages 
     FIXME: Consolidate this with the use in value_rtti_target_type
     and elsewhere where we want to suppress the messages. */

  if (null_stderr == NULL)
    null_stderr = ui_file_new ();

  saved_gdb_stderr = gdb_stderr;
  gdb_stderr = null_stderr;

  args.args[0].pointer = val;
  args.args[1].pointer = block;

  if (!catch_errors ((catch_errors_ftype *) wrap_value_objc_target_type, &args,
		     "", RETURN_MASK_ALL))
    {
      /* An error occurred */
      return 0;
    }

  gdb_stderr = saved_gdb_stderr;

  *dynamic_type = (struct type *) args.result.pointer;
  
  return 1;
}
/* APPLE LOCAL end */
