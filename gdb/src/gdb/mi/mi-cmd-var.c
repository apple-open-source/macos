/* MI Command Set - varobj commands.

   Copyright 2000, 2002, 2004 Free Software Foundation, Inc.

   Contributed by Cygnus Solutions (a Red Hat company).

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
#include "mi-cmds.h"
#include "ui-out.h"
#include "mi-out.h"
#include "varobj.h"
#include "value.h"
#include <ctype.h>
#include "gdb_string.h"
#include "frame.h"

extern int varobjdebug;		/* defined in varobj.c */

/* This is a useful function for reporting the creation of a varobj in
   a standard way.  */
void mi_report_var_creation (struct ui_out *uiout, struct varobj *var, int is_root);

static int varobj_update_one (struct varobj *var);
static char *typecode_as_string (struct varobj* var);

static struct ui_out *tmp_miout = NULL;

static void prepare_tmp_mi_out (void);

/* VAROBJ operations */

enum mi_cmd_result
mi_cmd_var_create (char *command, char **argv, int argc)
{
  CORE_ADDR frameaddr = 0;
  struct varobj *var;
  char *name;
  char *frame;
  char *expr;
  struct block *block = NULL;
  struct cleanup *old_cleanups;
  struct cleanup *mi_out_cleanup;
  enum varobj_type var_type;

  if (argc != 3)
    {
      /*      xasprintf (&mi_error_message,
         "mi_cmd_var_create: Usage: .");
         return MI_CMD_ERROR; */
      error ("mi_cmd_var_create: Usage: NAME FRAME EXPRESSION.");
    }

  name = xstrdup (argv[0]);
  /* Add cleanup for name. Must be free_current_contents as
     name can be reallocated */
  old_cleanups = make_cleanup (free_current_contents, &name);

  frame = xstrdup (argv[1]);
  old_cleanups = make_cleanup (xfree, frame);

  expr = xstrdup (argv[2]);

  if (strcmp (name, "-") == 0)
    {
      xfree (name);
      name = varobj_gen_name ();
    }
  else if (!isalpha (*name))
    error ("mi_cmd_var_create: name of object must begin with a letter");

  if (strcmp (frame, "*") == 0)
    var_type = USE_CURRENT_FRAME;
  else if (strcmp (frame, "@") == 0)
    var_type = USE_SELECTED_FRAME;
  else if (frame[0] == '+')
    {
      var_type = USE_BLOCK_IN_FRAME;
      frameaddr = string_to_core_addr (frame + 1);
      block = block_for_pc (frameaddr);
    }
  else
    {
      var_type = USE_SPECIFIED_FRAME;
      frameaddr = string_to_core_addr (frame);
    }
  
  if (varobjdebug)
    fprintf_unfiltered (gdb_stdlog,
			"Name=\"%s\", Frame=\"%s\" (0x%s), Expression=\"%s\"\n",
			name, frame, paddr (frameaddr), expr);
  
  prepare_tmp_mi_out ();
  mi_out_cleanup = make_cleanup_restore_uiout (uiout);
  uiout = tmp_miout;
  var = varobj_create (name, expr, frameaddr, block, var_type);
  do_cleanups (mi_out_cleanup);

  if (var == NULL)
    error ("mi_cmd_var_create: unable to create variable object");
  
  mi_report_var_creation (uiout, var, 1);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

/* mi_report_var_creation: Writes to ui_out UIOUT the info for the
   new variable object VAR that is common to parent & child varobj's */

void
mi_report_var_creation (struct ui_out *uiout, struct varobj *var, int is_root)
{  
  char *type;

  if (var == NULL)
    {
      ui_out_field_skip (uiout, "name");
      
      /* For child variables, we print out the expression.  Put it here because
	 that's how it is in the testsuite! */
      if (!is_root)
	ui_out_field_skip (uiout, "exp");

      ui_out_field_skip (uiout, "numchild");
      ui_out_field_skip (uiout, "type");
      ui_out_field_skip (uiout, "dynamic_type");
      ui_out_field_skip (uiout, "typecode");
      if (is_root)
      ui_out_field_skip (uiout, "in_scope");
      return;
    }

  ui_out_field_string (uiout, "name", varobj_get_objname (var));

  /* For child variables, we print out the expression.  Put it here because
     that's how it is in the testsuite! */
  if (!is_root)
    ui_out_field_string (uiout, "exp", varobj_get_expression (var));

  ui_out_field_int (uiout, "numchild", varobj_get_num_children (var));
  type = varobj_get_type (var);

  if (type == NULL)
    {
      if (varobj_is_fake_child (var))
	{
	  ui_out_field_string (uiout, "type", "");
	  ui_out_field_string (uiout, "typecode", "FAKE_CHILD");
	}
      else
	{
      ui_out_field_string (uiout, "type", "");
	  ui_out_field_string (uiout, "typecode", "UNDEF");
	}
    }
  else
    {
      char *typecode;
      ui_out_field_string (uiout, "type", type);
      typecode = typecode_as_string (var);
      ui_out_field_string (uiout, "typecode", typecode);
      xfree (type);
    }

  type = varobj_get_dynamic_type (var);
  if (type == NULL)
    {
      ui_out_field_string (uiout, "dynamic_type", "");
    }
  else
    {
      ui_out_field_string (uiout, "dynamic_type", type);
      xfree (type);
    }

  /* How could a newly created variable be out of scope, you ask?
     we want to be able to create varobj's for all the variables in
     a function, including those in sub-blocks in the function.  However,
     many of these may not yet be in scope... 
     Note, this is only reported for root variables.  */
  
  if (is_root)
    {
      CORE_ADDR block_start, block_end;

      if (varobj_in_scope_p (var))
	ui_out_field_string (uiout, "in_scope", "true");
      else
	ui_out_field_string (uiout, "in_scope", "false");

      varobj_get_valid_block (var, &block_start, &block_end);
      if (block_start == (CORE_ADDR) -1)
	{
	  ui_out_field_string (uiout, "block_start_addr", "no block");
	  ui_out_field_string (uiout, "block_end_addr", "no block");
	}
      else
	{
	  ui_out_field_core_addr (uiout, "block_start_addr", block_start);
	  ui_out_field_core_addr (uiout, "block_end_addr", block_end);
	}
    }

}

static char *
typecode_as_string (struct varobj *var)
{
  enum type_code type_code;
  char *type_code_as_str;
  struct type* type;

  type = varobj_get_type_struct (var);
  if (type == NULL)
    type_code = TYPE_CODE_UNDEF;
  else
    type_code = TYPE_CODE (type);

  switch (type_code)
    {
    case TYPE_CODE_UNDEF:
      type_code_as_str = "UNDEF";
      break;
    case TYPE_CODE_PTR:
      type_code_as_str = "PTR";
      break;
    case TYPE_CODE_ARRAY:
      type_code_as_str = "ARRAY";
      break;
    case TYPE_CODE_STRUCT:
      type_code_as_str = "STRUCT";
      break;
    case TYPE_CODE_UNION:
      type_code_as_str = "UNION";
      break;
    case TYPE_CODE_ENUM:
      type_code_as_str = "ENUM";
      break;
    case TYPE_CODE_FUNC:
      type_code_as_str = "FUNC";
      break;
    case TYPE_CODE_INT:
      type_code_as_str = "INT";
      break;
    case TYPE_CODE_FLT:
      type_code_as_str = "FLT";
      break;
    case TYPE_CODE_VOID:
      type_code_as_str = "VOID";
      break;
    case TYPE_CODE_SET:
      type_code_as_str = "SET";
      break;
    case TYPE_CODE_RANGE:
      type_code_as_str = "RANGE";
      break;
    case TYPE_CODE_BITSTRING:
      type_code_as_str = "BITSTRING";
      break;
    case TYPE_CODE_ERROR:
      type_code_as_str = "ERROR";
      break;
    case TYPE_CODE_MEMBER:
      type_code_as_str = "MEMBER";
      break;
    case TYPE_CODE_METHOD:
      type_code_as_str = "METHOD";
      break;
    case TYPE_CODE_REF:
      type_code_as_str = "REF";
      break;
    case TYPE_CODE_CHAR:
      type_code_as_str = "CHAR";
      break;
    case TYPE_CODE_BOOL:
      type_code_as_str = "BOOL";
      break;
    case TYPE_CODE_COMPLEX:
      type_code_as_str = "COMPLEX";
      break;
    case TYPE_CODE_TYPEDEF:
      type_code_as_str = "TYPEDEF";
      break;
    case TYPE_CODE_TEMPLATE:
      type_code_as_str = "TEMPLATE";
      break;
    case TYPE_CODE_TEMPLATE_ARG:
      type_code_as_str = "TEMPLATE_ARG";
      break;      
    default:
      type_code_as_str = "UNKNOWN";
    }
  return type_code_as_str;
}

enum mi_cmd_result
mi_cmd_var_delete (char *command, char **argv, int argc)
{
  char *name;
  char *expr;
  struct varobj *var;
  int numdel;
  int children_only_p = 0;
  struct cleanup *old_cleanups;

  if (argc < 1 || argc > 2)
    error ("mi_cmd_var_delete: Usage: [-c] EXPRESSION.");

  name = xstrdup (argv[0]);
  /* Add cleanup for name. Must be free_current_contents as
     name can be reallocated */
  old_cleanups = make_cleanup (free_current_contents, &name);

  /* If we have one single argument it cannot be '-c' or any string
     starting with '-'. */
  if (argc == 1)
    {
      if (strcmp (name, "-c") == 0)
	error ("mi_cmd_var_delete: Missing required argument after %s", 
	       "'-c': variable object name");
      if (*name == '-')
	error ("mi_cmd_var_delete: Illegal variable object name");
    }

  /* If we have 2 arguments they must be '-c' followed by a string
     which would be the variable name. */
  if (argc == 2)
    {
      expr = xstrdup (argv[1]);
      if (strcmp (name, "-c") != 0)
	error ("mi_cmd_var_delete: Invalid option.");
      children_only_p = 1;
      xfree (name);
      name = xstrdup (expr);
      xfree (expr);
    }

  /* If we didn't error out, now NAME contains the name of the
     variable. */

  var = varobj_get_handle (name);

  if (var == NULL)
    error ("mi_cmd_var_delete: Variable object not found.");

  numdel = varobj_delete (var, NULL, children_only_p);

  ui_out_field_int (uiout, "ndeleted", numdel);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_set_format (char *command, char **argv, int argc)
{
  enum varobj_display_formats format;
  int len;
  struct varobj *var;
  char *formspec;

  if (argc != 2)
    error ("mi_cmd_var_set_format: Usage: NAME FORMAT.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);

  if (var == NULL)
    error ("mi_cmd_var_set_format: Variable object not found");

  formspec = xstrdup (argv[1]);
  if (formspec == NULL)
    error ("mi_cmd_var_set_format: Must specify the format as: \"natural\", \"binary\", \"decimal\", \"hexadecimal\", \"unsigned\", or \"octal\"");

  len = strlen (formspec);

  if (strncmp (formspec, "natural", len) == 0)
    format = FORMAT_NATURAL;
  else if (strncmp (formspec, "binary", len) == 0)
    format = FORMAT_BINARY;
  else if (strncmp (formspec, "decimal", len) == 0)
    format = FORMAT_DECIMAL;
  else if (strncmp (formspec, "hexadecimal", len) == 0)
    format = FORMAT_HEXADECIMAL;
  else if (strncmp (formspec, "octal", len) == 0)
    format = FORMAT_OCTAL;
  else if (strncmp (formspec, "unsigned", len) == 0)
        format = FORMAT_UNSIGNED;
  else
    error ("mi_cmd_var_set_format: Unknown display format: must be: \"natural\", \"binary\", \"decimal\", \"hexadecimal\",  \"unsigned\", or \"octal\"");

  /* Set the format of VAR to given format */
  varobj_set_display_format (var, format);

  /* Report the new current format */
  ui_out_field_string (uiout, "format", varobj_format_string[(int) format]);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_show_format (char *command, char **argv, int argc)
{
  enum varobj_display_formats format;
  struct varobj *var;

  if (argc != 1)
    error ("mi_cmd_var_show_format: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_show_format: Variable object not found");

  format = varobj_get_display_format (var);

  /* Report the current format */
  ui_out_field_string (uiout, "format", varobj_format_string[(int) format]);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_num_children (char *command, char **argv, int argc)
{
  struct varobj *var;

  if (argc != 1)
    error ("mi_cmd_var_info_num_children: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_num_children: Variable object not found");

  ui_out_field_int (uiout, "numchild", varobj_get_num_children (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_list_children (char *command, char **argv, int argc)
{
  struct varobj *var = NULL; /* APPLE LOCAL: init to NULL for err detection */ 
  struct varobj **childlist;
  struct varobj **cc;
  int numchild;
  enum print_values print_values = PRINT_NO_VALUES;
  struct cleanup *cleanup_children = NULL;
  int argv0_is_flag = 0;
  int argv1_is_flag = 0;
  const char *usage = "mi_cmd_var_list_children: Usage: [--no-values|--all-values] NAME [PRINT_VALUE]";

  /* APPLE LOCAL: In our impl, arguments are reversed.  We use
     'varobj-handle show-value', at the FSF they use 
     'show-value varobj-handle'.  */

  if (argc == 0)
    error ("%s", usage);

  if (strcmp (argv[0], "0") == 0 || strcmp (argv[0], "--no-values") == 0)
    {
      print_values = PRINT_NO_VALUES;
      argv0_is_flag = 1;
    }
  else if (strcmp (argv[0], "1") == 0 || strcmp (argv[0], "--all-values") == 0)
    {
      print_values = PRINT_ALL_VALUES;
      argv0_is_flag = 1;
    }

  if (argc >= 2)
    {
      if (strcmp (argv[1], "0") == 0)
        {
          print_values = PRINT_NO_VALUES;
          argv1_is_flag = 1;
        }
      else if (strcmp (argv[1], "2") == 0)
        {
          print_values = PRINT_ALL_VALUES;
          argv1_is_flag = 1;
        }
     }

  /* APPLE LOCAL: This is dumb, but we can signal the type of
     printing by anyone of one of these methods:
       A command line option-type thing, 
       a numerial at the start, or
       a numerial at the end.  
     e.g. these are all valid:
      var-list-children --print-values var1
      var-list-children 1 var1
      var-list-children var1 2
     Notably I am not supporting
      var-list-children --print-values var1 2
     Because now we're just being silly.  */

  if (argc == 1 && argv0_is_flag)
    error ("%s", usage);

  if (argc == 1 && !argv0_is_flag)
    var = varobj_get_handle (argv[0]);
  else if (argc == 2 && argv0_is_flag)
    var = varobj_get_handle (argv[1]);
  else if (argc == 2 && argv1_is_flag)
    var = varobj_get_handle (argv[0]);

  if (var == NULL)
    error ("Variable object not found");

  numchild = varobj_list_children (var, &childlist);
  ui_out_field_int (uiout, "numchild", numchild);

  if (numchild <= 0)
    return MI_CMD_DONE;

  /* APPLE LOCAL: CHILDREN is a list, not a tuple. */
  cleanup_children = make_cleanup_ui_out_list_begin_end (uiout, "children");

#if 0
  if (mi_version (uiout) == 1)
    cleanup_children = make_cleanup_ui_out_tuple_begin_end (uiout, "children");
  else
    cleanup_children = make_cleanup_ui_out_list_begin_end (uiout, "children");
#endif

  cc = childlist;
  while (*cc != NULL)
    {
      struct cleanup *cleanup_child;
      cleanup_child = make_cleanup_ui_out_tuple_begin_end (uiout, "child");

      mi_report_var_creation (uiout, *cc, 0);

      if (print_values)
	ui_out_field_string (uiout, "value", varobj_get_value (*cc));
      do_cleanups (cleanup_child);
      cc++;
    }
  do_cleanups (cleanup_children);
  xfree (childlist);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_type (char *command, char **argv, int argc)
{
  struct varobj *var;
  char *type;

  if (argc != 1)
    error ("mi_cmd_var_info_type: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_type: Variable object not found");

  type = varobj_get_type (var);
  if (type)
    {
      ui_out_field_string (uiout, "type", type);
      xfree (type);
    }
  else
    ui_out_field_skip (uiout, type);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_path_expression (char *command, char **argv, int argc)
{
  struct varobj *var;
  char *path_expr;

  if (argc != 1)
    error ("mi_cmd_var_info_path_expression: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_path_expression: Variable object not found");
  
  path_expr = varobj_get_path_expr (var);

  ui_out_field_string (uiout, "path_expr", path_expr);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_block (char *command, char **argv, int argc)
{
  struct varobj *var;
  CORE_ADDR block_start, block_end;
  struct symtab_and_line sal;
  
  if (argc != 1)
    error ("mi_cmd_var_info_type: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_type: Variable object not found");
  
  varobj_get_valid_block (var, &block_start, &block_end);

  sal = find_pc_line (block_start, 0);
  if (sal.line > 0)
    ui_out_field_int (uiout, "block_start_line", sal.line);

  ui_out_field_core_addr (uiout, "block_start_addr", block_start);

  sal = find_pc_line (block_end, 1);
  if (sal.line > 0)
    ui_out_field_int (uiout, "block_end_line", sal.line);

  ui_out_field_core_addr (uiout, "block_end_addr", block_end);
  
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_expression (char *command, char **argv, int argc)
{
  enum varobj_languages lang;
  struct varobj *var;

  if (argc != 1)
    error ("mi_cmd_var_info_expression: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_expression: Variable object not found");

  lang = varobj_get_language (var);

  ui_out_field_string (uiout, "lang", varobj_language_string[(int) lang]);
  ui_out_field_string (uiout, "exp", varobj_get_expression (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_show_attributes (char *command, char **argv, int argc)
{
  int attr;
  char *attstr;
  struct varobj *var;

  if (argc != 1)
    error ("mi_cmd_var_show_attributes: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_show_attributes: Variable object not found");

  attr = varobj_get_attributes (var);
  /* FIXME: define masks for attributes */
  if (attr & 0x00000001)
    attstr = "editable";
  else
    attstr = "noneditable";

  ui_out_field_string (uiout, "attr", attstr);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_evaluate_expression (char *command, char **argv, int argc)
{
  struct varobj *var;
  int unwinding_was_requested = 0;
  char *expr;
  struct cleanup *old_chain = NULL;

  if (argc == 1)
    {
      expr = argv[0];
    }
  else if (argc == 2)
    {
      if (strcmp (argv[0], "-u") != 0)
	error ("mi_cmd_var_evaluate_expression: Usage: [-u] NAME.");
      else
	{
	  unwinding_was_requested = 1;
	  expr = argv[1];
	}
    }
  else
    error ("mi_cmd_var_evaluate_expression: Usage: [-u] NAME.");


  /* Get varobj handle, if a valid var obj name was specified */
  
  old_chain = make_cleanup (null_cleanup, NULL);
  if (unwinding_was_requested)
    make_cleanup (set_unwind_on_signal, set_unwind_on_signal (1));

  var = varobj_get_handle (expr);
  if (var == NULL)
    error ("mi_cmd_var_evaluate_expression: Variable object not found");

  ui_out_field_string (uiout, "value", varobj_get_value (var));

  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_assign (char *command, char **argv, int argc)
{
  struct varobj *var;
  char *expression;

  if (argc != 2)
    error ("mi_cmd_var_assign: Usage: NAME EXPRESSION.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_assign: Variable object not found");

  /* FIXME: define masks for attributes */
  if (!(varobj_get_attributes (var) & 0x00000001))
    error ("mi_cmd_var_assign: Variable object is not editable");

  expression = xstrdup (argv[1]);

  if (!varobj_set_value (var, expression))
    error ("mi_cmd_var_assign: Could not assign expression to varible object");

  ui_out_field_string (uiout, "value", varobj_get_value (var));
  return MI_CMD_DONE;
}

void
prepare_tmp_mi_out ()
{
  /* Make sure the tmp_mi_out that we use for suppressing error
     output from varobj_update is up to date. */

  if (tmp_miout == NULL)
    tmp_miout = mi_out_new (mi_version (uiout));
  else if (mi_version (tmp_miout) != mi_version (uiout))
    {
      ui_out_delete (tmp_miout);
      tmp_miout = mi_out_new (mi_version (uiout));
    }
}

enum mi_cmd_result
mi_cmd_var_update (char *command, char **argv, int argc)
{
  struct varobj *var;
  struct varobj **rootlist;
  struct varobj **cr;
  struct cleanup *cleanup;
  int nv;

  if (argc == 0)
    error ("mi_cmd_var_update: Usage: NAME [NAME...].");

 
  prepare_tmp_mi_out ();

 /* Check if the parameter is a "*" which means that we want
     to update all variables */

  if ((argc == 1) && (*argv[0] == '*') && (*(argv[0] + 1) == '\0'))
    {
      nv = varobj_list (&rootlist);
/* APPLE LOCAL: changelist is a list, not a tuple, in our mi1 */
      cleanup = make_cleanup_ui_out_list_begin_end (uiout, "changelist");
      if (nv <= 0)
	{
	  do_cleanups (cleanup);
	  return MI_CMD_DONE;
	}
      cr = rootlist;
      while (*cr != NULL)
	{
	  varobj_update_one (*cr);
	  cr++;
	}
      xfree (rootlist);
      do_cleanups (cleanup);
    }
  else
    {
/* APPLE LOCAL: -var-update accepts multiple varobj names, not just one. */
      int i;
      cleanup = make_cleanup_ui_out_list_begin_end (uiout, "changelist");

      for (i = 0; i < argc; i++)
	{
	  /* Get varobj handle, if a valid var obj name was specified */
	  var = varobj_get_handle (argv[i]);
	  if (var == NULL)
	    error ("mi_cmd_var_update: Variable object \"%s\" not found.",
		   argv[i]);
	  
	  varobj_update_one (var);
	}
      do_cleanups (cleanup);
    }
    return MI_CMD_DONE;
}

/* Helper for mi_cmd_var_update() Returns 0 if the update for
   the variable fails (usually because the variable is out of
   scope), and 1 if it succeeds. */

static int
varobj_update_one (struct varobj *var)
{
  struct varobj_changelist *changelist;
  struct cleanup *cleanup;
  int nc;

  cleanup = make_cleanup_restore_uiout (uiout);
  uiout = tmp_miout;
  nc = varobj_update (&var, &changelist);
  do_cleanups (cleanup);

  /* nc == 0 means that nothing has changed.
     nc == -1 means that an error occured in updating the variable.
     nc == -2 means the variable has changed type. 
     nc == -3 means that the variable has gone out of scope. */
     
  if (nc == 0)
    return 1;
  else if (nc == -1 || nc == -3)
    {
      /* APPLE LOCAL: each varobj tuple is named with VAROBJ; not anonymous */
      cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "varobj");
      ui_out_field_string (uiout, "name", varobj_get_objname(var));
      ui_out_field_string (uiout, "in_scope", "false");
      do_cleanups (cleanup);
      return -1;
    }
  else if (nc == -2)
    {
      char *typecode;
      char *type;

      /* APPLE LOCAL: each varobj tuple is named with VAROBJ; not anonymous */
      cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "varobj");
      ui_out_field_string (uiout, "name", varobj_get_objname (var));
      ui_out_field_string (uiout, "in_scope", "true");
      ui_out_field_string (uiout, "type_changed", "true");
      type = varobj_get_type (var);
      if (type)
	{
	  ui_out_field_string (uiout, "new_type", type);
	  xfree (type);
	}
      else
	ui_out_field_skip (uiout, "new_type");

      type = varobj_get_dynamic_type (var);
      if (type)
	{
	  ui_out_field_string (uiout, "new_dynamic_type", type);
	  xfree (type);
	}
      else
	ui_out_field_skip (uiout, "new_dynamic_type");

      typecode = typecode_as_string (var);
      ui_out_field_string (uiout, "new_typecode", typecode);
      ui_out_field_int (uiout, "new_num_children", 
			   varobj_get_num_children(var));
      do_cleanups (cleanup);
    }
  else
    {
      
      struct varobj *var;
      enum varobj_type_change type_changed;
      var = varobj_changelist_pop (changelist, &type_changed);

      while (var != NULL)
	{
        /* APPLE LOCAL: each varobj tuple is named with VAROBJ; not anonymous */
	  cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "varobj");
	  ui_out_field_string (uiout, "name", varobj_get_objname (var));
	  ui_out_field_string (uiout, "in_scope", "true");
	  if (type_changed == VAROBJ_TYPE_UNCHANGED)
	    ui_out_field_string (uiout, "type_changed", "false");
	  else
	    {
	      ui_out_field_string (uiout, "type_changed", "true");
	      ui_out_field_string (uiout, "new_dynamic_type", 
				   varobj_get_dynamic_type (var));
	      ui_out_field_int (uiout, "new_num_children", 
				varobj_get_num_children(var));
	    }
	    
	  do_cleanups (cleanup);
	  var = varobj_changelist_pop (changelist, &type_changed);
	}

      return 1;
    }
  return 1;
}
