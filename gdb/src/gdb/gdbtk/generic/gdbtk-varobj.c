/* Variable user interface layer for GDB, the GNU debugger.
   Copyright 1999 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "value.h"

#include "varobj.h"

#include <tcl.h>
#include "gdbtk.h"


/*
 * Public functions defined in this file
 */

int gdb_variable_init (Tcl_Interp *);

/*
 * Private functions defined in this file
 */

/* Entries into this file */

static int gdb_variable_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST[]);

static int variable_obj_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST[]);

/* Variable object subcommands */

static int variable_create (Tcl_Interp *, int, Tcl_Obj * CONST[]);

static void variable_delete (Tcl_Interp *, struct varobj *, int);

static Tcl_Obj *variable_children (Tcl_Interp *, struct varobj *);

static int variable_format (Tcl_Interp *, int, Tcl_Obj * CONST[],
			    struct varobj *);

static int variable_type (Tcl_Interp *, int, Tcl_Obj * CONST[],
			  struct varobj *);

static int variable_value (Tcl_Interp *, int, Tcl_Obj * CONST[],
			   struct varobj *);

static Tcl_Obj *variable_update (Tcl_Interp * interp, struct varobj *var);

/* Helper functions for the above subcommands. */

static void install_variable (Tcl_Interp *, char *, struct varobj *);

static void uninstall_variable (Tcl_Interp *, char *);

/* String representations of gdb's format codes */
char *format_string[] =
{"natural", "binary", "decimal", "hexadecimal", "octal"};

#if defined(FREEIF)
#undef FREEIF
#endif
#define FREEIF(x) if (x != NULL) free((char *) (x))

/* Initialize the variable code. This function should be called once
   to install and initialize the variable code into the interpreter. */
int
gdb_variable_init (interp)
     Tcl_Interp *interp;
{
  Tcl_Command result;
  static int initialized = 0;

  if (!initialized)
    {
      result = Tcl_CreateObjCommand (interp, "gdb_variable", call_wrapper,
				   (ClientData) gdb_variable_command, NULL);
      if (result == NULL)
	return TCL_ERROR;

      initialized = 1;
    }

  return TCL_OK;
}

/* This function defines the "gdb_variable" command which is used to
   create variable objects. Its syntax includes:

   gdb_variable create
   gdb_variable create NAME
   gdb_variable create -expr EXPR
   gdb_variable create -frame FRAME
   (it will also include permutations of the above options)

   NAME  = name of object to create. If no NAME, then automatically create
   a name
   EXPR  = the gdb expression for which to create a variable. This will
   be the most common usage.
   FRAME = the frame defining the scope of the variable.
 */
static int
gdb_variable_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  static char *commands[] =
  {"create", "list", NULL};
  enum commands_enum
    {
      VARIABLE_CREATE, VARIABLE_LIST
    };
  int index, result;

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "option ?arg...?");
      return TCL_ERROR;
    }

  if (Tcl_GetIndexFromObj (interp, objv[1], commands, "options", 0,
			   &index) != TCL_OK)
    {
      return TCL_ERROR;
    }

  switch ((enum commands_enum) index)
    {
    case VARIABLE_CREATE:
      result = variable_create (interp, objc - 2, objv + 2);
      break;

    default:
      return TCL_ERROR;
    }

  return result;
}

/* This function implements the actual object command for each
   variable object that is created (and each of its children).

   Currently the following commands are implemented:
   - delete        delete this object and its children
   - update        update the variable and its children (root vars only)
   - numChildren   how many children does this object have
   - children      create the children and return a list of their objects
   - name          print out the name of this variable
   - format        query/set the display format of this variable
   - type          get the type of this variable
   - value         get/set the value of this variable
   - editable      is this variable editable?
 */
static int
variable_obj_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  enum commands_enum
    {
      VARIABLE_DELETE,
      VARIABLE_NUM_CHILDREN,
      VARIABLE_CHILDREN,
      VARIABLE_FORMAT,
      VARIABLE_TYPE,
      VARIABLE_VALUE,
      VARIABLE_NAME,
      VARIABLE_EDITABLE,
      VARIABLE_UPDATE
    };
  static char *commands[] =
  {
    "delete",
    "numChildren",
    "children",
    "format",
    "type",
    "value",
    "name",
    "editable",
    "update",
    NULL
  };
  struct varobj *var = (struct varobj *) clientData;
  int index, result;

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "option ?arg...?");
      return TCL_ERROR;
    }

  if (Tcl_GetIndexFromObj (interp, objv[1], commands, "options", 0,
			   &index) != TCL_OK)
    return TCL_ERROR;

  result = TCL_OK;
  switch ((enum commands_enum) index)
    {
    case VARIABLE_DELETE:
      if (objc > 2)
	{
	  int len;
	  char *s = Tcl_GetStringFromObj (objv[2], &len);
	  if (*s == 'c' && strncmp (s, "children", len) == 0)
	    {
	      variable_delete (interp, var, 1 /* only children */ );
	      break;
	    }
	}
      variable_delete (interp, var, 0 /* var and children */ );
      break;

    case VARIABLE_NUM_CHILDREN:
      Tcl_SetObjResult (interp, Tcl_NewIntObj (varobj_get_num_children (var)));
      break;

    case VARIABLE_CHILDREN:
      {
	Tcl_Obj *children = variable_children (interp, var);
	Tcl_SetObjResult (interp, children);
      }
      break;

    case VARIABLE_FORMAT:
      result = variable_format (interp, objc, objv, var);
      break;

    case VARIABLE_TYPE:
      result = variable_type (interp, objc, objv, var);
      break;

    case VARIABLE_VALUE:
      result = variable_value (interp, objc, objv, var);
      break;

    case VARIABLE_NAME:
      {
	char *name = varobj_get_expression (var);
	Tcl_SetObjResult (interp, Tcl_NewStringObj (name, -1));
	FREEIF (name);
      }
      break;

    case VARIABLE_EDITABLE:
      Tcl_SetObjResult (interp, Tcl_NewIntObj (
		varobj_get_attributes (var) & 0x00000001 /* Editable? */ ));
      break;

    case VARIABLE_UPDATE:
      /* Only root variables can be updated */
      {
	Tcl_Obj *obj = variable_update (interp, var);
	Tcl_SetObjResult (interp, obj);
      }
      break;

    default:
      return TCL_ERROR;
    }

  return result;
}

/*
 * Variable object construction/destruction
 */

/* This function is responsible for processing the user's specifications
   and constructing a variable object. */
static int
variable_create (interp, objc, objv)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  enum create_opts
    {
      CREATE_EXPR, CREATE_FRAME
    };
  static char *create_options[] =
  {"-expr", "-frame", NULL};
  struct varobj *var;
  char *name;
  char *obj_name;
  int index;
  CORE_ADDR frame = (CORE_ADDR) -1;
  int how_specified = USE_SELECTED_FRAME;

  /* REMINDER: This command may be invoked in the following ways:
     gdb_variable create [NAME] [-expr EXPR] [-frame FRAME]

     NAME  = name of object to create. If no NAME, then automatically create
     a name
     EXPR  = the gdb expression for which to create a variable. This will
     be the most common usage.
     FRAME = the address of the frame defining the variable's scope
   */
  name = NULL;
  if (objc)
    name = Tcl_GetStringFromObj (objv[0], NULL);
  if (name == NULL || *name == '-')
    {
      /* generate a name for this object */
      obj_name = varobj_gen_name ();
    }
  else
    {
      /* specified name for object */
      obj_name = strdup (name);
      objv++;
      objc--;
    }

  /* Run through all the possible options for this command */
  name = NULL;
  while (objc > 0)
    {
      if (Tcl_GetIndexFromObj (interp, objv[0], create_options, "options",
			       0, &index) != TCL_OK)
	{
	  free (obj_name);
	  result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}

      switch ((enum create_opts) index)
	{
	case CREATE_EXPR:
	  name = Tcl_GetStringFromObj (objv[1], NULL);
	  objc--;
	  objv++;
	  break;

	case CREATE_FRAME:
	  {
	    char *str;
	    str = Tcl_GetStringFromObj (objv[1], NULL);
	    frame = parse_and_eval_address (str);
	    how_specified = USE_SPECIFIED_FRAME;
	    objc--;
	    objv++;
	  }
	  break;

	default:
	  break;
	}

      objc--;
      objv++;
    }

  /* Create the variable */
  var = varobj_create (obj_name, name, frame, how_specified);

  if (var != NULL)
    {
      /* Install a command into the interpreter that represents this
         object */
      install_variable (interp, obj_name, var);
      Tcl_SetObjResult (interp, Tcl_NewStringObj (obj_name, -1));
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;

      free (obj_name);
      return TCL_OK;
    }

  free (obj_name);
  return TCL_ERROR;
}

/* Delete the variable object VAR and its children */
/* If only_children_p, Delete only the children associated with the object. */
static void
variable_delete (interp, var, only_children_p)
     Tcl_Interp *interp;
     struct varobj *var;
     int only_children_p;
{
  char **dellist;
  char **vc;

  varobj_delete (var, &dellist, only_children_p);

  vc = dellist;
  while (*vc != NULL)
    {
      uninstall_variable (interp, *vc);
      free (*vc);
      vc++;
    }

  FREEIF (dellist);
}

/* Return a list of all the children of VAR, creating them if necessary. */
static Tcl_Obj *
variable_children (interp, var)
     Tcl_Interp *interp;
     struct varobj *var;
{
  Tcl_Obj *list;
  struct varobj **childlist;
  struct varobj **vc;
  char *childname;

  list = Tcl_NewListObj (0, NULL);

  varobj_list_children (var, &childlist);

  vc = childlist;
  while (*vc != NULL)
    {
      childname = varobj_get_objname (*vc);
      /* Add child to result list and install the Tcl command for it. */
      Tcl_ListObjAppendElement (NULL, list,
				Tcl_NewStringObj (childname, -1));
      install_variable (interp, childname, *vc);
      vc++;
    }

  FREEIF (childlist);
  return list;
}

/* Update the values for a variable and its children. */
/* NOTE:   Only root variables can be updated... */

static Tcl_Obj *
variable_update (interp, var)
     Tcl_Interp *interp;
     struct varobj *var;
{
  Tcl_Obj *changed;
  struct varobj **changelist;
  struct varobj **vc;

  changed = Tcl_NewListObj (0, NULL);

  /* varobj_update() can return -1 if the variable is no longer around,
     i.e. we stepped out of the frame in which a local existed. */
  if (varobj_update (var, &changelist) == -1)
    return changed;

  vc = changelist;
  while (*vc != NULL)
    {
      /* Add changed variable object to result list */
      Tcl_ListObjAppendElement (NULL, changed,
			   Tcl_NewStringObj (varobj_get_objname (*vc), -1));
      vc++;
    }

  FREEIF (changelist);
  return changed;
}

/* This implements the format object command allowing
   the querying or setting of the object's display format. */
static int
variable_format (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     struct varobj *var;
{
  if (objc > 2)
    {
      /* Set the format of VAR to given format */
      int len;
      char *fmt = Tcl_GetStringFromObj (objv[2], &len);
      if (STREQN (fmt, "natural", len))
	varobj_set_display_format (var, FORMAT_NATURAL);
      else if (STREQN (fmt, "binary", len))
	varobj_set_display_format (var, FORMAT_BINARY);
      else if (STREQN (fmt, "decimal", len))
	varobj_set_display_format (var, FORMAT_DECIMAL);
      else if (STREQN (fmt, "hexadecimal", len))
	varobj_set_display_format (var, FORMAT_HEXADECIMAL);
      else if (STREQN (fmt, "octal", len))
	varobj_set_display_format (var, FORMAT_OCTAL);
      else
	{
	  Tcl_Obj *obj = Tcl_NewStringObj (NULL, 0);
	  Tcl_AppendStringsToObj (obj, "unknown display format \"",
				  fmt, "\": must be: \"natural\", \"binary\""
		      ", \"decimal\", \"hexadecimal\", or \"octal\"", NULL);
	  Tcl_SetObjResult (interp, obj);
	  return TCL_ERROR;
	}
    }
  else
    {
      /* Report the current format */
      Tcl_Obj *fmt;

      /* FIXME: Use varobj_format_string[] instead */
      fmt = Tcl_NewStringObj (
		  format_string[(int) varobj_get_display_format (var)], -1);
      Tcl_SetObjResult (interp, fmt);
    }

  return TCL_OK;
}

/* This function implements the type object command, which returns the type of a
   variable in the interpreter (or an error). */
static int
variable_type (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     struct varobj *var;
{
  char *first, *last, *string;
  Tcl_RegExp regexp;

  /* For the "fake" variables, do not return a type.
     Their type is NULL anyway */
  /* FIXME: varobj_get_type() calls type_print(), so we may have to wrap
     its call here and return TCL_ERROR in the case it errors out */
  if ((string = varobj_get_type (var)) == NULL)
    {
      Tcl_ResetResult (interp);
      return TCL_OK;
    }

  first = string;

  /* gdb will print things out like "struct {...}" for anonymous structs.
     In gui-land, we don't want the {...}, so we strip it here. */
  regexp = Tcl_RegExpCompile (interp, "{...}");
  if (Tcl_RegExpExec (interp, regexp, string, first))
    {
      /* We have an anonymous struct/union/class/enum */
      Tcl_RegExpRange (regexp, 0, &first, &last);
      if (*(first - 1) == ' ')
	first--;
      *first = '\0';
    }

  Tcl_SetObjResult (interp, Tcl_NewStringObj (string, -1));
  FREEIF (string);
  return TCL_OK;
}

/* This function implements the value object command, which allows an object's
   value to be queried or set. */
static int
variable_value (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     struct varobj *var;
{
  char *r;

  /* If we're setting the value of the variable, objv[2] will contain the
     variable's new value. */
  if (objc > 2)
    {
      /* FIXME: Do we need to test if val->error is set here?
         If so, make it an attribute. */
      if (varobj_get_attributes (var) & 0x00000001 /* Editable? */ )
	{
	  char *s;

	  s = Tcl_GetStringFromObj (objv[2], NULL);
	  if (!varobj_set_value (var, s))
            {
              r = error_last_message ();
              Tcl_SetObjResult (interp, Tcl_NewStringObj (r, -1));
              FREEIF (r);
	      return TCL_ERROR;
            }
	}

      Tcl_ResetResult (interp);
      return TCL_OK;
    }

  r = varobj_get_value (var);

  if (r == NULL)
    return TCL_ERROR;
  else
    {
      Tcl_SetObjResult (interp, Tcl_NewStringObj (r, -1));
      FREEIF (r);
      return TCL_OK;
    }
}

/* Helper functions for the above */

/* Install the given variable VAR into the tcl interpreter with
   the object name NAME. */
static void
install_variable (interp, name, var)
     Tcl_Interp *interp;
     char *name;
     struct varobj *var;
{
  Tcl_CreateObjCommand (interp, name, variable_obj_command,
			(ClientData) var, NULL);
}

/* Unistall the object VAR in the tcl interpreter. */
static void
uninstall_variable (interp, varname)
     Tcl_Interp *interp;
     char *varname;
{
  Tcl_DeleteCommand (interp, varname);
}

