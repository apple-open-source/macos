/* MI Command Set - varobj commands.
   Copyright (C) 2000, Free Software Foundation, Inc.
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

/* Convenience macro for allocting typesafe memory. */

#undef XMALLOC
#define XMALLOC(TYPE) (TYPE*) xmalloc (sizeof (TYPE))

extern int varobjdebug;		/* defined in varobj.c */

/* This is a useful function for reporting the creation of a varobj in
   a standard way.  */
void mi_report_var_creation (struct ui_out *uiout, struct varobj *var);

static int varobj_update_one (struct varobj *var);
static void print_typecode (struct ui_out *uiout, struct varobj* var);

/* VAROBJ operations */

enum mi_cmd_result
mi_cmd_var_create (char *command, char **argv, int argc)
{
  CORE_ADDR frameaddr = 0;
  struct varobj *var;
  char *name;
  char *frame;
  char *expr;
  struct cleanup *old_cleanups;
  enum varobj_type var_type;

  if (argc != 3)
    {
      /*      asprintf (&mi_error_message,
         "mi_cmd_var_create: Usage: .");
         return MI_CMD_ERROR; */
      error ("mi_cmd_var_create: Usage: NAME FRAME EXPRESSION.");
    }

  name = xstrdup (argv[0]);
  /* Add cleanup for name. Must be free_current_contents as
     name can be reallocated */
  old_cleanups = make_cleanup (free_current_contents, &name);

  frame = xstrdup (argv[1]);
  old_cleanups = make_cleanup (free, frame);

  expr = xstrdup (argv[2]);

  if (strcmp (name, "-") == 0)
    {
      free (name);
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
      frameaddr = atoi (frame + 1);
    }
  else
    {
      var_type = USE_SPECIFIED_FRAME;
      frameaddr = parse_and_eval_address (frame);
    }
  
  if (varobjdebug)
    fprintf_unfiltered (gdb_stdlog,
			"Name=\"%s\", Frame=\"%s\" (0x%s), Expression=\"%s\"\n",
			name, frame, paddr (frameaddr), expr);
  
  var = varobj_create (name, expr, frameaddr, var_type);
  
  if (var == NULL)
    error ("mi_cmd_var_create: unable to create variable object");

  mi_report_var_creation (uiout, var);
  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

void
mi_report_var_creation (struct ui_out *uiout, struct varobj *var)
{  
  char *type;

  if (var == NULL)
    {
      ui_out_field_skip (uiout, "name");
      ui_out_field_skip (uiout, "numchild");
      ui_out_field_skip (uiout, "type");
      ui_out_field_skip (uiout, "typecode");
      ui_out_field_skip (uiout, "in_scope");
      return;
    }

  ui_out_field_string (uiout, "name", varobj_get_objname (var));
  ui_out_field_int (uiout, "numchild", varobj_get_num_children (var));
  type = varobj_get_type (var);

  if (type == NULL)
    {
      ui_out_field_string (uiout, "type", "");
      ui_out_field_int (uiout, "typecode", -1);
    }
  else
    {
      ui_out_field_string (uiout, "type", type);
      print_typecode (uiout, var);
      free (type);
    }

  /* How could a newly created variable be out of scope, you ask?
     we want to be able to create varobj's for all the variables in
     a function, including those in sub-blocks in the function.  However,
     many of these may not yet be in scope... */

  if (varobj_pc_in_valid_block_p (var))
    ui_out_field_string (uiout, "in_scope", "true");
  else
    ui_out_field_string (uiout, "in_scope", "false");
}

static void
print_typecode (struct ui_out *uiout, struct varobj *var)
{
  enum type_code type_code;
  char *type_code_as_str;
  struct type* type;

  type = varobj_get_type_struct (var);
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
  ui_out_field_string (uiout, "typecode", type_code_as_str);
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
      free (name);
      name = xstrdup (expr);
      free (expr);
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
    error ("mi_cmd_var_set_format: Must specify the format as: \"natural\", \"binary\", \"decimal\", \"hexadecimal\", or \"octal\"");

  len = strlen (formspec);

  if (STREQN (formspec, "natural", len))
    format = FORMAT_NATURAL;
  else if (STREQN (formspec, "binary", len))
    format = FORMAT_BINARY;
  else if (STREQN (formspec, "decimal", len))
    format = FORMAT_DECIMAL;
  else if (STREQN (formspec, "hexadecimal", len))
    format = FORMAT_HEXADECIMAL;
  else if (STREQN (formspec, "octal", len))
    format = FORMAT_OCTAL;
  else
    error ("mi_cmd_var_set_format: Unknown display format: must be: \"natural\", \"binary\", \"decimal\", \"hexadecimal\", or \"octal\"");

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
  struct varobj *var;
  struct varobj **childlist;
  struct varobj **cc;
  int numchild;
  int print_value = 0;
  char *type;

  if (argc > 2)
    error ("mi_cmd_var_list_children: Usage: NAME [SHOW_VALUE].");

  if (argc == 2)
      print_value = atoi(argv[1]);
  else
    print_value = 0;

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_list_children: Variable object not found");

  numchild = varobj_list_children (var, &childlist);
  ui_out_field_int (uiout, "numchild", numchild);

  if (numchild <= 0)
    return MI_CMD_DONE;

  ui_out_list_begin (uiout, "children");
  cc = childlist;
  while (*cc != NULL)
    {
      ui_out_list_begin (uiout, "child");
      ui_out_field_string (uiout, "name", varobj_get_objname (*cc));
      ui_out_field_string (uiout, "exp", varobj_get_expression (*cc));
      ui_out_field_int (uiout, "numchild", varobj_get_num_children (*cc));
      type = varobj_get_type (*cc);
      /* C++ pseudo-variables (public, private, protected) do not have a type */
      if (type)
	{
	  ui_out_field_string (uiout, "type", varobj_get_type (*cc));
	  print_typecode (uiout, *cc);
	}
      if (print_value)
	ui_out_field_string (uiout, "value", varobj_get_value (*cc));
      ui_out_list_end (uiout);
      cc++;
    }
  ui_out_list_end (uiout);
  free (childlist);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_type (char *command, char **argv, int argc)
{
  struct varobj *var;

  if (argc != 1)
    error ("mi_cmd_var_info_type: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_type: Variable object not found");

  ui_out_field_string (uiout, "type", varobj_get_type (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_block (char *command, char **argv, int argc)
{
  struct varobj *var;
  CORE_ADDR block_start, block_end;
  struct symtab_and_line sal;
  struct ui_stream *stb = NULL;
  
  if (argc != 1)
    error ("mi_cmd_var_info_type: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_info_type: Variable object not found");
  
  varobj_get_valid_block (var, &block_start, &block_end);

  sal = find_pc_line (block_start, 0);
  if (sal.line > 0)
    ui_out_field_int (uiout, "block_start", sal.line);
  else
    {
      stb = ui_out_stream_new (uiout);
      print_address_numeric (block_start, 1, stb->stream);
      ui_out_field_stream (uiout, "block_start", stb);
      ui_out_stream_delete (stb);
    }

  printf ("Start: 0x%ux, End: 0x%ux.\n", block_start, block_end);

  sal = find_pc_line (block_end, 1);
  if (sal.line > 0)
    ui_out_field_int (uiout, "block_end", sal.line);
  else
    {
      stb = ui_out_stream_new (uiout);
      print_address_numeric (block_end, 1, stb->stream);
      ui_out_field_stream (uiout, "block_end", stb);
      ui_out_stream_delete (stb);
    }
  
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

  if (argc != 1)
    error ("mi_cmd_var_evaluate_expression: Usage: NAME.");

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error ("mi_cmd_var_evaluate_expression: Variable object not found");

  ui_out_field_string (uiout, "value", varobj_get_value (var));
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

enum mi_cmd_result
mi_cmd_var_update (char *command, char **argv, int argc)
{
  struct varobj *var;
  struct varobj **rootlist;
  struct varobj **cr;
  char *name;
  int nv;

  if (argc == 0)
    error ("mi_cmd_var_update: Usage: NAME [NAME...].");

  /* Check if the parameter is a "*" which means that we want
     to update all variables */

  if ((argc == 1) && (*argv[0] == '*') && (*(argv[0] + 1) == '\0'))
    {
      nv = varobj_list (&rootlist);
      ui_out_list_begin (uiout, "changelist");
      if (nv <= 0)
	{
	  ui_out_list_end (uiout);
	  return MI_CMD_DONE;
	}
      cr = rootlist;
      while (*cr != NULL)
	{
	  varobj_update_one (*cr);
	  cr++;
	}
      free (rootlist);
      ui_out_list_end (uiout);
    }
  else
    {
      int i;

      ui_out_list_begin (uiout, "changelist");

      for (i = 0; i < argc; i++)
	{
	  /* Get varobj handle, if a valid var obj name was specified */
	  var = varobj_get_handle (argv[i]);
	  if (var == NULL)
	    error ("mi_cmd_var_update: Variable object not found");
	  
	  varobj_update_one (var);
	}

      ui_out_list_end (uiout);
    }
    return MI_CMD_DONE;
}

/* Helper for mi_cmd_var_update() Returns 0 if the update for
   the variable fails (usually because the variable is out of
   scope), and 1 if it succeeds. */

static int
varobj_update_one (struct varobj *var)
{
  struct varobj **changelist;
  struct varobj **cc;
  int nc;

  nc = varobj_update (var, &changelist);

  /* nc == 0 means that nothing has changed.
     nc == -1 means that an error occured in updating the variable.
     nc == -2 means the variable has changed type. 
     nc == -3 means that the variable has gone out of scope. */
     
  if (nc == 0)
    return 1;
  else if (nc == -1 || nc == -3)
    {
      ui_out_list_begin (uiout, "varobj");
      ui_out_field_string (uiout, "name", varobj_get_objname(var));
      ui_out_field_string (uiout, "in_scope", "false");
      ui_out_list_end (uiout);
      return -1;
    }
  else if (nc == -2)
    {
      ui_out_list_begin (uiout, "varobj");
      ui_out_field_string (uiout, "name", varobj_get_objname (var));
      ui_out_field_string (uiout, "in_scope", "true");
      ui_out_field_string (uiout, "type_changed", "true");
      ui_out_field_string (uiout, "new_type", varobj_get_type(var));
      print_typecode (uiout, var);
      ui_out_field_int (uiout, "new_num_children", 
			   varobj_get_num_children(var));
      ui_out_list_end (uiout);
    }
  else
    {
      
      cc = changelist;
      while (*cc != NULL)
	{
	  ui_out_list_begin (uiout, "varobj");
	  ui_out_field_string (uiout, "name", varobj_get_objname (*cc));
	  ui_out_field_string (uiout, "in_scope", "true");
	  ui_out_field_string (uiout, "type_changed", "false");
	  ui_out_list_end (uiout);
	  cc++;
	}
      free (changelist);
      return 1;
    }
  return 1;
}
