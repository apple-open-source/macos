/* Variable user interface layer for GDB, the GNU debugger.
   Copyright 1999-2000 Free Software Foundation, Inc.

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
#include "expression.h"
#include "frame.h"
#include "valprint.h"
#include "language.h"
#include "tui/tui-file.h"

#include <tcl.h>
#include <tk.h>
#include "gdbtk.h"
#include "gdbtk-wrapper.h"

#include <math.h>

/* Enumeration for the format types */
enum display_format
{
  FORMAT_NATURAL,          /* What gdb actually calls 'natural' */
  FORMAT_BINARY,           /* Binary display                    */
  FORMAT_DECIMAL,          /* Decimal display                   */
  FORMAT_HEXADECIMAL,      /* Hex display                       */
  FORMAT_OCTAL             /* Octal display                     */
};

/* Languages supported by this variable system. */
enum vlanguage { vlang_c = 0, vlang_cplus, vlang_java, vlang_end };

/* Every variable keeps a linked list of its children, described
   by the following structure. */
struct variable_child {

  /* Pointer to the child's data */
  struct _gdb_variable *child;

  /* Pointer to the next child */
  struct variable_child *next;
};

/* Every root variable has one of these structures saved in its
   gdb_variable. Members which must be free'd are noted. */
struct variable_root {

  /* Alloc'd expression for this parent. */
  struct expression *exp;

  /* Block for which this expression is valid */
  struct block *valid_block;

  /* The frame for this expression */
  CORE_ADDR frame;  

  /* Language info for this variable and its children */
  struct language_specific *lang;

  /* The gdb_variable for this root node. */
  struct _gdb_variable *root;
};

/* Every variable in the system has a structure of this type defined
   for it. This structure holds all information necessary to manipulate
   a particular object variable. Members which must be freed are noted. */
struct _gdb_variable {

  /* Alloc'd name of the variable for this object.. If this variable is a
     child, then this name will be the child's source name.
     (bar, not foo.bar) */
  char *name;

  /* The alloc'd name for this variable's object. This is here for
     convenience when constructing this object's children. */
  char *obj_name;

  /* Index of this variable in its parent or -1 */
  int index;

  /* The type of this variable. This may NEVER be NULL. */
  struct type *type;

  /* The value of this expression or subexpression.  This may be NULL. */
  value_ptr value;

  /* Did an error occur evaluating the expression or getting its value? */
  int error;

  /* The number of (immediate) children this variable has */
  int num_children;

  /* If this object is a child, this points to its immediate parent. */
  struct _gdb_variable *parent;

  /* A list of this object's children */
  struct variable_child *children;

  /* Description of the root variable. Points to root variable for children. */
  struct variable_root *root;

  /* The format of the output for this object */
  enum display_format format;
};

typedef struct _gdb_variable gdb_variable;

struct language_specific {

  /* The language of this variable */
  enum vlanguage language;

  /* The number of children of PARENT. */
  int (*number_of_children) (struct _gdb_variable * parent);

  /* The name of the INDEX'th child of PARENT. */
  char *(*name_of_child) (struct _gdb_variable * parent, int index);

  /* The value_ptr of the root variable ROOT. */
  value_ptr (*value_of_root) (struct _gdb_variable * root);

  /* The value_ptr of the INDEX'th child of PARENT. */
  value_ptr (*value_of_child) (struct _gdb_variable * parent, int index);

  /* The type of the INDEX'th child of PARENT. */
  struct type *(*type_of_child) (struct _gdb_variable * parent, int index);

  /* Is VAR editable? */
  int (*variable_editable) (struct _gdb_variable * var);

  /* The current value of VAR is returned in *OBJ. */
  int (*value_of_variable) (struct _gdb_variable * var, Tcl_Obj ** obj);
};

struct vstack {
  gdb_variable *var;
  struct vstack *next;
};

/* A little convenience enum for dealing with C++/Java */
enum vsections { v_public = 0, v_private, v_protected };

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

static void variable_delete (Tcl_Interp *, gdb_variable *);

static Tcl_Obj *variable_children (Tcl_Interp *, gdb_variable *);

static int variable_format (Tcl_Interp *, int, Tcl_Obj * CONST[],
			    gdb_variable *);

static int variable_type (Tcl_Interp *, int, Tcl_Obj * CONST[],
			  gdb_variable *);

static int variable_value (Tcl_Interp *, int, Tcl_Obj * CONST[],
			   gdb_variable *);

static int variable_editable (gdb_variable *);

static int my_value_of_variable (gdb_variable * var, Tcl_Obj ** obj);

static Tcl_Obj *variable_update (Tcl_Interp * interp, gdb_variable * var);

/* Helper functions for the above subcommands. */

static gdb_variable *create_variable (char *name, CORE_ADDR frame);

static void delete_children (Tcl_Interp *, gdb_variable *, int);

static void install_variable (Tcl_Interp *, char *, gdb_variable *);

static void uninstall_variable (Tcl_Interp *, gdb_variable *);

static gdb_variable *child_exists (gdb_variable *, char *);

static gdb_variable *create_child (Tcl_Interp *, gdb_variable *, int, char *);
static char *name_of_child (gdb_variable *, int);

static int number_of_children (gdb_variable *);

static enum display_format variable_default_display (gdb_variable *);

static void save_child_in_parent (gdb_variable *, gdb_variable *);

static void remove_child_from_parent (gdb_variable *, gdb_variable *);

/* Utility routines */

static struct type *get_type (gdb_variable * var);

static struct type *get_type_deref (gdb_variable * var);

static struct type *get_target_type (struct type *);

static Tcl_Obj *get_call_output (void);

static void clear_gdb_output (void);

static int call_gdb_type_print (value_ptr);

static int call_gdb_val_print (value_ptr, int);

static void variable_fputs (const char *, struct ui_file *);

static void null_fputs (const char *, struct ui_file *);

static int my_value_equal (gdb_variable *, value_ptr);

static void vpush (struct vstack **pstack, gdb_variable * var);

static gdb_variable *vpop (struct vstack **pstack);

/* Language-specific routines. */

static value_ptr value_of_child (gdb_variable * parent, int index);

static value_ptr value_of_root (gdb_variable * var);

static struct type *type_of_child (gdb_variable * var);

static int type_changeable (gdb_variable * var);

static int c_number_of_children (gdb_variable * var);

static char *c_name_of_child (gdb_variable * parent, int index);

static value_ptr c_value_of_root (gdb_variable * var);

static value_ptr c_value_of_child (gdb_variable * parent, int index);

static struct type *c_type_of_child (gdb_variable * parent, int index);

static int c_variable_editable (gdb_variable * var);

static int c_value_of_variable (gdb_variable * var, Tcl_Obj ** obj);

static int cplus_number_of_children (gdb_variable * var);

static void cplus_class_num_children (struct type *type, int children[3]);

static char *cplus_name_of_child (gdb_variable * parent, int index);

static value_ptr cplus_value_of_root (gdb_variable * var);

static value_ptr cplus_value_of_child (gdb_variable * parent, int index);

static struct type *cplus_type_of_child (gdb_variable * parent, int index);

static int cplus_variable_editable (gdb_variable * var);

static int cplus_value_of_variable (gdb_variable * var, Tcl_Obj ** obj);

static int java_number_of_children (gdb_variable * var);

static char *java_name_of_child (gdb_variable * parent, int index);

static value_ptr java_value_of_root (gdb_variable * var);

static value_ptr java_value_of_child (gdb_variable * parent, int index);

static struct type *java_type_of_child (gdb_variable * parent, int index);

static int java_variable_editable (gdb_variable * var);

static int java_value_of_variable (gdb_variable * var, Tcl_Obj ** obj);

static enum vlanguage variable_language (gdb_variable * var);

static gdb_variable *new_variable (void);

static gdb_variable *new_root_variable (void);

static void free_variable (gdb_variable * var);

/* String representations of gdb's format codes */
char *format_string[] = {"natural", "binary", "decimal", "hexadecimal", "octal"};

/* Array of known source language routines. */
static struct language_specific languages[vlang_end][sizeof(struct language_specific)] = {
  { vlang_c, c_number_of_children, c_name_of_child, c_value_of_root,
    c_value_of_child, c_type_of_child, c_variable_editable,
    c_value_of_variable },
  { vlang_cplus, cplus_number_of_children, cplus_name_of_child, cplus_value_of_root,
    cplus_value_of_child, cplus_type_of_child, cplus_variable_editable,
    cplus_value_of_variable },
  { vlang_java, java_number_of_children, java_name_of_child, java_value_of_root,
    java_value_of_child, java_type_of_child, java_variable_editable,
    java_value_of_variable }};

/* Mappings of display_format enums to gdb's format codes */
int format_code[] = {0, 't', 'd', 'x', 'o'};

/* This variable will hold the value of the output from gdb
   for commands executed through call_gdb_* */
static Tcl_Obj *fputs_obj;

#if defined(FREEIF)
#  undef FREEIF
#endif
#define FREEIF(x) if (x != NULL) free((char *) (x))

/* Is the variable X one of our "fake" children? */
#define CPLUS_FAKE_CHILD(x) \
((x) != NULL && (x)->type == NULL && (x)->value == NULL)

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
     ClientData   clientData;
     Tcl_Interp  *interp;
     int          objc;
     Tcl_Obj *CONST objv[];
{
  static char *commands[] = { "create", "list", NULL };
  enum commands_enum { VARIABLE_CREATE, VARIABLE_LIST };
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
  enum commands_enum {
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
  static char *commands[] = {
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
  gdb_variable *var = (gdb_variable *) clientData;
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
              delete_children (interp, var, 1);
              break;
            }
        }
      variable_delete (interp, var);
      break;

    case VARIABLE_NUM_CHILDREN:
      if (var->num_children == -1)
	var->num_children = number_of_children (var);

      Tcl_SetObjResult (interp, Tcl_NewIntObj (var->num_children));
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
	/* If var->name has "-" in it, it's because we
	   needed to escape periods in the name... */
	char *p, *name;
	name = savestring (var->name, strlen (var->name));
	p = name;
	while (*p != '\000')
	  {
	    if (*p == '-')
	      *p = '.';
	    p++;
	  }
	Tcl_SetObjResult (interp, Tcl_NewStringObj (name, -1));
	free (name);
      }
      break;

    case VARIABLE_EDITABLE:
      Tcl_SetObjResult (interp, Tcl_NewIntObj (variable_editable (var)));
      break;

    case VARIABLE_UPDATE:
      /* Only root variables can be updated */
      if (var->parent == NULL)
	{
	  Tcl_Obj *obj = variable_update (interp, var);
	  Tcl_SetObjResult (interp, obj);
	}
      result = TCL_OK;
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
  enum create_opts { CREATE_EXPR, CREATE_FRAME };
  static char *create_options[] = { "-expr", "-frame", NULL };
  gdb_variable *var;
  char *name;
  char obj_name[31];
  int index;
  static int id = 0;
  CORE_ADDR frame = (CORE_ADDR) -1;

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
      id++;
      sprintf (obj_name, "var%d", id);
    }
  else
    {
      /* specified name for object */
      strncpy (obj_name, name, 30);
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
  var = create_variable (name, frame);

  if (var != NULL)
    {
      /* Install a command into the interpreter that represents this
         object */
      install_variable (interp, obj_name, var);
      Tcl_SetObjResult (interp, Tcl_NewStringObj (obj_name, -1));
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;

      return TCL_OK;
    }

  return TCL_ERROR;
}

/* Fill out a gdb_variable structure for the (root) variable being constructed. */
static gdb_variable *
create_variable (name, frame)
     char *name;
     CORE_ADDR frame;
{
  gdb_variable *var;
  struct frame_info *fi, *old_fi;
  struct block *block;
  void (*old_fputs) (const char *, struct ui_file *);
  gdb_result r;

  var = new_root_variable ();
  if (name != NULL)
    {
      char *p;
      enum vlanguage lang;

      /* Several of the GDB_* calls can cause messages to be displayed. We swallow
         those here, because we don't need them (the "value" command will
         show them). */
      old_fputs = fputs_unfiltered_hook;
      fputs_unfiltered_hook = null_fputs;

      /* Parse and evaluate the expression, filling in as much
         of the variable's data as possible */

      /* Allow creator to specify context of variable */
      r = GDB_OK;
      if (frame == (CORE_ADDR) -1)
	fi = selected_frame;
      else
	r = GDB_find_frame_addr_in_frame_chain (frame, &fi);

      block = NULL;
      if (fi != NULL)
	r = GDB_get_frame_block (fi, &block);

      p = name;
      innermost_block  = NULL;
      r = GDB_parse_exp_1 (&p, block, 0, &(var->root->exp));
      if (r != GDB_OK)
        {
	  free_variable (var);

	  /* Restore the output hook to normal */
	  fputs_unfiltered_hook = old_fputs;

          return NULL;
        }

      /* Don't allow variables to be created for types. */
      if (var->root->exp->elts[0].opcode == OP_TYPE)
        {
	  free_variable (var);

	  /* Restore the output hook to normal */
	  fputs_unfiltered_hook = old_fputs;

          printf_unfiltered ("Attempt to use a type name as an expression.");
          return NULL;
        }

      var->format            = variable_default_display (var);
      var->root->valid_block = innermost_block;
      var->name              = savestring (name, strlen (name));
      
      /* When the frame is different from the current frame, 
         we must select the appropriate frame before parsing
         the expression, otherwise the value will not be current.
         Since select_frame is so benign, just call it for all cases. */
      if (fi != NULL)
	{
	  var->root->frame = FRAME_FP (fi);
	  old_fi           = selected_frame;
	  GDB_select_frame (fi, -1);
	}

      if (GDB_evaluate_expression (var->root->exp, &var->value) == GDB_OK)
        {
          release_value (var->value);
          if (VALUE_LAZY (var->value))
	    GDB_value_fetch_lazy (var->value);
        }
      else
        var->value = evaluate_type (var->root->exp);

      var->type = VALUE_TYPE (var->value);

      /* Set language info */
      lang = variable_language (var);
      var->root->lang = languages[lang];

      /* Set ourselves as our root */
      var->root->root = var;

      /* Reset the selected frame */
      if (fi != NULL)
	GDB_select_frame (old_fi, -1);


      /* Restore the output hook to normal */
      fputs_unfiltered_hook = old_fputs;
    }

  return var;
}

/* Install the given variable VAR into the tcl interpreter with
   the object name NAME. */
static void
install_variable (interp, name, var)
     Tcl_Interp *interp;
     char *name;
     gdb_variable *var;
{
  var->obj_name = savestring (name, strlen (name));
  Tcl_CreateObjCommand (interp, name, variable_obj_command, 
                        (ClientData) var, NULL);
}

/* Unistall the object VAR in the tcl interpreter. */
static void
uninstall_variable (interp, var)
     Tcl_Interp *interp;
     gdb_variable *var;
{
  Tcl_DeleteCommand (interp, var->obj_name);
}

/* Delete the variable object VAR and its children */
static void
variable_delete (interp, var)
     Tcl_Interp *interp;
     gdb_variable *var;
{
  /* Delete any children of this variable, too. */
  delete_children (interp, var, 0);

  /* If this variable has a parent, remove it from its parent's list */
  if (var->parent != NULL)
    {
      remove_child_from_parent (var->parent, var);
    }

  uninstall_variable (interp, var);

  /* Free memory associated with this variable */
  free_variable (var);
}

/* Free any allocated memory associated with VAR. */
static void free_variable (var)
     gdb_variable *var;
{
  /* Free the expression if this is a root variable. */
  if (var->root->root == var)
    {
      free_current_contents ((char **) &var->root->exp);
      FREEIF (var->root);
    }

  FREEIF (var->name);
  FREEIF (var->obj_name);
  FREEIF (var);
}

/*
 * Child construction/destruction
 */

/* Delete the children associated with the object VAR. If NOTIFY is set,
   notify the parent object that this child was deleted. This is used as
   a small optimization when deleting variables and their children. If the
   parent is also being deleted, don't bother notifying it that its children
   are being deleted. */
static void
delete_children (interp, var, notify)
     Tcl_Interp *interp;
     gdb_variable *var;
     int notify;
{
  struct variable_child *vc;
  struct variable_child *next;

  for (vc = var->children; vc != NULL; vc = next)
    {
      if (!notify)
        vc->child->parent = NULL;
      variable_delete (interp, vc->child);
      next = vc->next;
      free (vc);
    }
}

/* Return the number of children for a given variable.
   The result of this function is defined by the language
   implementation. The number of children returned by this function
   is the number of children that the user will see in the variable
   display. */
static int
number_of_children (var)
     gdb_variable *var;
{
  return (*var->root->lang->number_of_children) (var);;
}

/* Return a list of all the children of VAR, creating them if necessary. */
static Tcl_Obj *
variable_children (interp, var)
     Tcl_Interp *interp;
     gdb_variable *var;
{
  Tcl_Obj *list;
  gdb_variable *child;
  char *name;
  int i;

  list = Tcl_NewListObj (0, NULL);
  if (var->num_children == -1)
    var->num_children = number_of_children (var);

  for (i = 0; i < var->num_children; i++)
    {
      /* check if child exists */
      name = name_of_child (var, i);
      child = child_exists (var, name);
      if (child == NULL)
	child = create_child (interp, var, i, name);

      if (child != NULL)
	Tcl_ListObjAppendElement (NULL, list, Tcl_NewStringObj (child->obj_name, -1));
    }

  return list;
}

/* Does a child with the name NAME exist in VAR? If so, return its data.
   If not, return NULL. */
static gdb_variable *
child_exists (var, name)
     gdb_variable *var; /* Parent */
     char *name;        /* name of child */
{
  struct variable_child *vc;

  for (vc = var->children; vc != NULL; vc = vc->next)
    {
      if (STREQ (vc->child->name, name))
        return vc->child;
    }

  return NULL;
}

/* Create and install a child of the parent of the given name */
static gdb_variable *
create_child (interp, parent, index, name)
     Tcl_Interp *interp;
     gdb_variable *parent;
     int index;
     char *name;
{
  gdb_variable *child;
  char *childs_name;

  child = new_variable ();

  /* name is allocated by name_of_child */
  child->name   = name;
  child->index  = index;
  child->value  = value_of_child (parent, index);
  if (child->value == NULL || parent->error)
    child->error = 1;
  child->parent = parent;
  child->root = parent->root;
  childs_name = (char *) xmalloc ((strlen (parent->obj_name) + strlen (name) + 2)
				    * sizeof (char));
  sprintf (childs_name, "%s.%s", parent->obj_name, name);
  install_variable (interp, childs_name, child);
  free (childs_name);

  /* Save a pointer to this child in the parent */
  save_child_in_parent (parent, child);

  /* Note the type of this child */
  child->type = type_of_child (child);

  return child;
}

/* Save CHILD in the PARENT's data. */
static void
save_child_in_parent (parent, child)
     gdb_variable *parent;
     gdb_variable *child;
{
  struct variable_child *vc;

  /* Insert the child at the top */
  vc = parent->children;
  parent->children =
    (struct variable_child *) xmalloc (sizeof (struct variable_child));

  parent->children->next = vc;
  parent->children->child  = child;
}

/* Remove the CHILD from the PARENT's list of children. */
static void
remove_child_from_parent (parent, child)
     gdb_variable *parent;
     gdb_variable *child;
{
  struct variable_child *vc, *prev;

  /* Find the child in the parent's list */
  prev = NULL;
  for (vc = parent->children; vc != NULL; )
    {
      if (vc->child == child)
        break;
      prev = vc;
      vc = vc->next;
    }

  if (prev == NULL)
    parent->children = vc->next;
  else
    prev->next = vc->next;
  
}

/* What is the name of the INDEX'th child of VAR? Returns a malloc'd string. */
static char *
name_of_child (var, index)
     gdb_variable *var;
     int index;
{
  return (*var->root->lang->name_of_child) (var, index);
}

/* Update the values for a variable and its children.  This is a
   two-pronged attack.  First, re-parse the value for the root's
   expression to see if it's changed.  Then go all the way
   through its children, reconstructing them and noting if they've
   changed.

   Only root variables can be updated... */
static Tcl_Obj *
variable_update (interp, var)
     Tcl_Interp *interp;
     gdb_variable *var;
{
  void (*old_hook) (const char *, struct ui_file *);
  Tcl_Obj *changed;
  gdb_variable *v;
  value_ptr new;
  struct vstack *stack = NULL;
  struct frame_info *old_fi;

  /* Initialize a stack */
  vpush (&stack, NULL);

  /* Save the selected stack frame, since we will need to change it
     in order to evaluate expressions. */
  old_fi = selected_frame;

  /* evaluate_expression can output errors to the screen,
     so swallow them here. */
  old_hook = fputs_unfiltered_hook;
  fputs_unfiltered_hook = null_fputs;

  changed = Tcl_NewListObj (0, NULL);

  /* Update the root variable. value_of_root can return NULL
     if the variable is no longer around, i.e. we stepped out of
     the frame in which a local existed. */
  new = value_of_root (var);
  if (new == NULL)
    return changed;

  if (!my_value_equal (var, new))
    {
      /* Note that it's changed   There a couple of exceptions here,
	 though. We don't want some types to be reported as "changed". */
      if (type_changeable (var))
	Tcl_ListObjAppendElement (interp, changed, Tcl_NewStringObj (var->obj_name, -1));
    }

  /* We must always keep around the new value for this root
     variable expression, or we lose the updated children! */
  value_free (var->value);
  var->value = new;

  /* Push the root's children */
  if (var->children != NULL)
    {
      struct variable_child *c;
      for (c = var->children; c != NULL; c = c->next)
	vpush (&stack, c->child);
    }

  /* Walk through the children, reconstructing them all. */
  v = vpop (&stack);
  while (v != NULL)
    {
      /* Push any children */
      if (v->children != NULL)
	{
	  struct variable_child *c;
	  for (c = v->children; c != NULL; c = c->next)
	    vpush (&stack, c->child);
	}

      /* Update this variable */
      new = value_of_child (v->parent, v->index);
      if (type_changeable (v) && !my_value_equal (v, new))
	{
	  /* Note that it's changed */
	  Tcl_ListObjAppendElement (interp, changed,
				    Tcl_NewStringObj (v->obj_name, -1));
	}

      /* We must always keep new values, since children depend on it. */
      if (v->value != NULL)
	value_free (v->value);
      v->value = new;

      /* Get next child */
      v = vpop (&stack);
    }

  /* Restore the original fputs_hook. */
  fputs_unfiltered_hook = old_hook;

  /* Restore selected frame */
  GDB_select_frame (old_fi, -1);

  return changed;
}

/* What is the type of VAR? */
static struct type *
type_of_child (var)
     gdb_variable *var;
{

  /* If the child had no evaluation errors, var->value
     will be non-NULL and contain a valid type. */
  if (var->value != NULL)
    return VALUE_TYPE (var->value);

  /* Otherwise, we must compute the type. */
  return (*var->root->lang->type_of_child) (var->parent, var->index);
}

/* What is the value_ptr for the INDEX'th child of PARENT? */
static value_ptr
value_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  value_ptr value;
  void (*old_hook) (const char *, struct ui_file *);

  /* Same deal here as before. GDB can output error messages to the
     screen while it attempts to work its way through the tree. */
  old_hook = fputs_unfiltered_hook;
  fputs_unfiltered_hook = null_fputs;

  value = (*parent->root->lang->value_of_child) (parent, index);

  /* If we're being lazy, fetch the real value of the variable. */
  if (value != NULL && VALUE_LAZY (value))
    GDB_value_fetch_lazy (value);

  /* Restore output hook */
  fputs_unfiltered_hook = old_hook;

  return value;
}

/* What is the value_ptr of the root variable VAR? */
static value_ptr
value_of_root (var)
     gdb_variable *var;
{
  return (*var->root->lang->value_of_root) (var);
}

/* This implements the format object command allowing
   the querying or setting of the object's display format. */
static int
variable_format (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     gdb_variable *var;
{

  if (objc > 2)
    {
      /* Set the format of VAR to given format */
      int len;
      char *fmt = Tcl_GetStringFromObj (objv[2], &len);
      if (STREQN (fmt, "natural", len))
        var->format = FORMAT_NATURAL;
      else if (STREQN (fmt, "binary", len))
        var->format = FORMAT_BINARY;
      else if (STREQN (fmt, "decimal", len))
        var->format = FORMAT_DECIMAL;
      else if (STREQN (fmt, "hexadecimal", len))
        var->format = FORMAT_HEXADECIMAL;
      else if (STREQN (fmt, "octal", len))
        var->format = FORMAT_OCTAL;
      else
        {
          Tcl_Obj *obj = Tcl_NewStringObj (NULL, 0);
          Tcl_AppendStringsToObj (obj, "unknown display format \"",
                                  fmt, "\": must be: \"natural\", \"binary\""
                                  ", \"decimal\", \"hexadecimal\", or \"octal\"",
                                  NULL);
          Tcl_SetObjResult (interp, obj);
          return TCL_ERROR;
        }
    }
  else
    {
      /* Report the current format */
      Tcl_Obj *fmt;

      fmt = Tcl_NewStringObj (format_string [(int) var->format], -1);
      Tcl_SetObjResult (interp, fmt);
    }

  return TCL_OK;
}

/* What is the default display for this variable? We assume that
   everything is "natural". Any exceptions? */
static enum display_format
variable_default_display (var)
     gdb_variable *var;
{
  return FORMAT_NATURAL;
}

/* This function implements the type object command, which returns the type of a
   variable in the interpreter (or an error). */
static int
variable_type (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     gdb_variable *var;
{
  int result;
  value_ptr val;
  char *first, *last, *string;
  Tcl_RegExp regexp;
  gdb_result r;

  /* For the "fake" variables, do not return a type. (It's type is
     NULL, too.) */
  if (CPLUS_FAKE_CHILD (var))
    {
      Tcl_ResetResult (interp); 
      return TCL_OK;
    }

  /* To print the type, we simply create a zero value_ptr and
     cast it to our type. We then typeprint this variable. */
  val = value_zero (var->type, not_lval);
  result = call_gdb_type_print (val);
  if (result == TCL_OK)
    {
      string = xstrdup (Tcl_GetStringFromObj (get_call_output (), NULL));
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

  Tcl_SetObjResult (interp, get_call_output ());
  return result;
}

/* This function implements the value object command, which allows an object's
   value to be queried or set. */
static int
variable_value (interp, objc, objv, var)
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
     gdb_variable *var;
{
  int result;
  struct type *type;
  value_ptr val;
  Tcl_Obj *str;
  gdb_result r;
  int real_addressprint;
  int offset = 0;

  /* If we're setting the value of the variable, objv[2] will contain the
     variable's new value. We need to first construct a legal expression
     for this -- ugh! */
  if (objc > 2)
    {
      /* Does this cover all the bases? */
      struct expression *exp;
      value_ptr value;
      int saved_input_radix = input_radix;

      if (variable_editable (var) && !var->error)
        {
          char *s;
	  int i;
	  value_ptr temp;

          input_radix = 10;   /* ALWAYS reset to decimal temporarily */
          s = Tcl_GetStringFromObj (objv[2], NULL);
          r = GDB_parse_exp_1 (&s, 0, 0, &exp);
          if (r != GDB_OK)
            return TCL_ERROR;
          if (GDB_evaluate_expression (exp, &value) != GDB_OK)
            return TCL_ERROR;

	  /* If our parent is "public", "private", or "protected", we could
	     be asking to modify the value of a baseclass. If so, we need to
	     adjust our address by the offset of our baseclass in the subclass,
	     since VALUE_ADDRESS (var->value) points at the start of the subclass.
	     For some reason, value_cast doesn't take care of this properly. */
	  temp = var->value;
	  if (var->parent != NULL && CPLUS_FAKE_CHILD (var->parent))
	    {
	      gdb_variable *super, *sub;
	      struct type *type;
	      super = var->parent->parent;
	      sub   = super->parent;
	      if (sub != NULL)
		{
		  /* Yes, it is a baseclass */
	          type  = get_type_deref (sub);

	          if (super->index < TYPE_N_BASECLASSES (type))
		    {
		      temp = value_copy (var->value);
		      for (i = 0; i < super->index; i++)
		        offset += TYPE_LENGTH (TYPE_FIELD_TYPE (type, i));
		    }
		}
	    }

	  VALUE_ADDRESS (temp) += offset;
          val = value_assign (temp, value);
	  VALUE_ADDRESS (val) -= offset;
          value_free (var->value);
          release_value (val);
          var->value = val;
          input_radix = saved_input_radix;
        }

      Tcl_ResetResult (interp);
      return TCL_OK;
    }

  result = my_value_of_variable (var, &str);
  Tcl_SetObjResult (interp, str);

  return result;
}

/* GDB already has a command called "value_of_variable". Sigh. */
static int
my_value_of_variable (var, obj)
     gdb_variable *var;
     Tcl_Obj **obj;
{
  return (*var->root->lang->value_of_variable) (var, obj);
}

/* Is this variable editable? Use the variable's type to make
   this determination. */
static int
variable_editable (var)
     gdb_variable *var;
{
  return (*var->root->lang->variable_editable) (var);
}

/*
 * Call stuff. These functions are used to capture the output of gdb commands
 * without going through the tcl interpreter.
 */

/* Retrieve gdb output in the buffer since last call. */
static Tcl_Obj *
get_call_output ()
{
  /* Clear the error flags, in case we errored. */
  if (result_ptr != NULL)
    result_ptr->flags &= ~GDBTK_ERROR_ONLY;
  return fputs_obj;
}

/* Clear the output of the buffer. */
static void
clear_gdb_output ()
{
  if (fputs_obj != NULL)
    Tcl_DecrRefCount (fputs_obj);

  fputs_obj = Tcl_NewStringObj (NULL, -1);
  Tcl_IncrRefCount (fputs_obj);
}

/* Call the gdb command "type_print", retaining its output in the buffer. */
static int
call_gdb_type_print (val)
     value_ptr val;
{
  void (*old_hook) (const char *, struct ui_file *);
  int result;

  /* Save the old hook and install new hook */
  old_hook = fputs_unfiltered_hook;
  fputs_unfiltered_hook = variable_fputs;

  /* Call our command with our args */
  clear_gdb_output ();


  if (GDB_type_print (val, "", gdb_stdout, -1) == GDB_OK)
    result = TCL_OK;
  else
    result = TCL_ERROR;

  /* Restore fputs hook */
  fputs_unfiltered_hook = old_hook;

  return result;
}

/* Call the gdb command "val_print", retaining its output in the buffer. */
static int
call_gdb_val_print (val, format)
     value_ptr val;
     int format;
{
  void (*old_hook) (const char *, struct ui_file *);
  gdb_result r;
  int result;

  /* Save the old hook and install new hook */
  old_hook = fputs_unfiltered_hook;
  fputs_unfiltered_hook = variable_fputs;

  /* Call our command with our args */
  clear_gdb_output ();

  if (VALUE_LAZY (val))
    {
      r = GDB_value_fetch_lazy (val);
      if (r != GDB_OK)
        {
          fputs_unfiltered_hook = old_hook;
          return TCL_ERROR;
        }
    }
  r = GDB_val_print (VALUE_TYPE (val), VALUE_CONTENTS_RAW (val), VALUE_ADDRESS (val),
                     gdb_stdout, format, 1, 0, 0);
  if (r == GDB_OK)
    result = TCL_OK;
  else
    result = TCL_ERROR;

  /* Restore fputs hook */
  fputs_unfiltered_hook = old_hook;

  return result;
}

/* The fputs_unfiltered_hook function used to save the output from one of the
   call commands in this file. */
static void
variable_fputs (text, stream)
     const char *text;
     struct ui_file *stream;
{
  /* Just append everything to the fputs_obj... Issues with stderr/stdout? */
  Tcl_AppendToObj (fputs_obj, (char *) text, -1);
}

/* Empty handler for the fputs_unfiltered_hook. Set the hook to this function
   whenever the output is irrelevent. */
static void
null_fputs (text, stream)
     const char *text;
     struct ui_file *stream;
{
  return;
}

/*
 * Miscellaneous utility functions.
 */

/* This returns the type of the variable. This skips past typedefs
   and returns the real type of the variable. It also dereferences
   pointers and references. */
static struct type *
get_type (var)
     gdb_variable *var;
{
  struct type *type = NULL;
  type = var->type;

  while (type != NULL && TYPE_CODE (type) == TYPE_CODE_TYPEDEF)
    type = TYPE_TARGET_TYPE (type);

  return type;
}

/* This returns the type of the variable, dereferencing pointers, too. */
static struct type *
get_type_deref (var)
     gdb_variable *var;
{
  struct type *type = NULL;

  type = get_type (var);

  if (type != NULL && (TYPE_CODE (type) == TYPE_CODE_PTR
		       || TYPE_CODE (type) == TYPE_CODE_REF))
    type = get_target_type (type);

  return type;
}

/* This returns the target type (or NULL) of TYPE, also skipping
   past typedefs, just like get_type (). */
static struct type *
get_target_type (type)
     struct type *type;
{
  if (type != NULL)
    {
      type = TYPE_TARGET_TYPE (type);
      while (type != NULL && TYPE_CODE (type) == TYPE_CODE_TYPEDEF)
        type = TYPE_TARGET_TYPE (type);
    }

  return type;
}

/* Get the language of variable VAR. */
static enum vlanguage
variable_language (var)
     gdb_variable *var;
{
  enum vlanguage lang;

  switch (var->root->exp->language_defn->la_language)
    {
    default:
    case language_c:
      lang = vlang_c;
      break;
    case language_cplus:
      lang = vlang_cplus;
      break;
    case language_java:
      lang = vlang_java;
      break;
    }

  return lang;
}

/* This function is similar to gdb's value_equal, except that this
   one is "safe" -- it NEVER longjmps. It determines if the VAR's
   value is the same as VAL2. */
static int
my_value_equal (var, val2)
     gdb_variable *var;
     value_ptr val2;
{
  int r, err1, err2;
  gdb_result result;

  /* Special case: NULL values. If both are null, say
     they're equal. */
  if (var->value == NULL && val2 == NULL)
    return 1;
  else if (var->value == NULL || val2 == NULL)
    return 0;

  /* This is bogus, but unfortunately necessary. We must know
   exactly what caused an error -- reading var->val or val2 --  so
   that we can really determine if we think that something has changed. */
  err1 = 0;
  err2 = 0;
  result = GDB_value_equal (var->value, var->value, &r);
  if (result != GDB_OK)
    err1 = 1;

  result = GDB_value_equal (val2, val2, &r);
  if (result != GDB_OK)
    err2 = 1;

  if (err1 != err2)
    return 0;

  if (GDB_value_equal (var->value, val2, &r) != GDB_OK)
    {
      /* An error occurred, this could have happened if
         either val1 or val2 errored. ERR1 and ERR2 tell
         us which of these it is. If both errored, then
         we assume nothing has changed. If one of them is
         valid, though, then something has changed. */
      if (err1 == err2)
        {
          /* both the old and new values caused errors, so
             we say the value did not change */
          /* This is indeterminate, though. Perhaps we should
             be safe and say, yes, it changed anyway?? */
          return 1;
        }
      else
        {
          /* err2 replaces var->error since this new value
             WILL replace the old one. */
	  var->error = err2;
          return 0;
        }
    }

  return r;
}

static void
vpush (pstack, var)
     struct vstack **pstack;
     gdb_variable *var;
{
  struct vstack *s;

  s = (struct vstack *) xmalloc (sizeof (struct vstack));
  s->var = var;
  s->next = *pstack;
  *pstack = s;
}

static gdb_variable *
vpop (pstack)
     struct vstack **pstack;
{
  struct vstack *s;
  gdb_variable *v;

  if ((*pstack)->var == NULL && (*pstack)->next == NULL)
    return NULL;

  s = *pstack;
  v = s->var;
  *pstack = (*pstack)->next;
  free (s);

  return v;
}

/* Is VAR something that can change? Depending on language,
   some variable's values never change. For example,
   struct and unions never change values. */
static int
type_changeable (var)
     gdb_variable *var;
{
  int r;
  struct type *type;
 
  r = 0;
  if (!CPLUS_FAKE_CHILD (var))
    {
      type = get_type (var);
      switch (TYPE_CODE (type))
	{
	case TYPE_CODE_STRUCT:
	case TYPE_CODE_UNION:
	  r = 0;
	  break;

	default:
	  r = 1;
	}
    }

  return r;
}

/* Allocate memory and initialize a new variable */
static gdb_variable *
new_variable ()
{
  gdb_variable *var;

  var = (gdb_variable *) xmalloc (sizeof (gdb_variable));
  var->name         = NULL;
  var->obj_name     = NULL;
  var->index        = -1;
  var->type         = NULL;
  var->value        = NULL;
  var->error        = 0;
  var->num_children = -1;
  var->parent       = NULL;
  var->children     = NULL;
  var->format       = 0;
  var->root         = NULL;

  return var;
}

/* Allocate memory and initialize a new root variable */
static gdb_variable *
new_root_variable (void)
{
  gdb_variable *var = new_variable ();
  var->root = (struct variable_root *) xmalloc (sizeof (struct variable_root));;
  var->root->lang   = NULL;
  var->root->exp    = NULL;
  var->root->valid_block  = NULL;
  var->root->frame  = (CORE_ADDR) -1;
  var->root->root   = NULL;

  return var;
}

/*
 * Language-dependencies
 */

/* C */
static int
c_number_of_children (var)
     gdb_variable *var;
{
  struct type *type;
  struct type *target;
  int children;

  type     = get_type (var);
  target   = get_target_type (type);
  children = 0;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (target) > 0
	  && TYPE_ARRAY_UPPER_BOUND_TYPE (type) != BOUND_CANNOT_BE_DETERMINED)
	children = TYPE_LENGTH (type) / TYPE_LENGTH (target);
      else
	children = -1;
      break;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      children = TYPE_NFIELDS (type);
      break;

    case TYPE_CODE_PTR:
      /* This is where things get compilcated. All pointers have one child.
	 Except, of course, for struct and union ptr, which we automagically
	 dereference for the user and function ptrs, which have no children. */
      switch (TYPE_CODE (target))
	{
	case TYPE_CODE_STRUCT:
	case TYPE_CODE_UNION:
	  children = TYPE_NFIELDS (target);
	  break;

	case TYPE_CODE_FUNC:
	  children = 0;
	  break;

	default: 
	  /* Don't dereference char* or void*. */
	  if (TYPE_NAME (target) != NULL
	      && (STREQ (TYPE_NAME (target), "char")
		  || STREQ (TYPE_NAME (target), "void")))
	    children = 0;
	  else
	    children = 1;
	}
      break;

    default:
      break;
    }

  return children;
}

static char *
c_name_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  struct type *type;
  struct type *target;
  char *name;
  char *string;

  type = get_type (parent);
  target = get_target_type (type);

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      {
        /* We never get here unless parent->num_children is greater than 0... */
        int len = 1;
        while ((int) pow ((double) 10, (double) len) < index)
          len++;
        name = (char *) xmalloc (1 + len * sizeof (char));
        sprintf (name, "%d", index);
      }
      break;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      string = TYPE_FIELD_NAME (type, index);
      name   = savestring (string, strlen (string));
      break;

    case TYPE_CODE_PTR:
      switch (TYPE_CODE (target))
        {
        case TYPE_CODE_STRUCT:
        case TYPE_CODE_UNION:
          string = TYPE_FIELD_NAME (target, index);
          name   = savestring (string, strlen (string));
          break;

        default:
          name = (char *) xmalloc ((strlen (parent->name) + 2) * sizeof (char));
          sprintf (name, "*%s", parent->name);
          break;
        }
    }

  return name;
}

static value_ptr
c_value_of_root (var)
     gdb_variable *var;
{
  value_ptr value, new_val;
  struct frame_info *fi, *old_fi;
  int within_scope;
  gdb_result r;

  /* Determine whether the variable is still around. */
  if (var->root->valid_block == NULL)
    within_scope = 1;
  else
    {
      GDB_reinit_frame_cache ();
      r = GDB_find_frame_addr_in_frame_chain (var->root->frame, &fi);
      if (r != GDB_OK)
        fi = NULL;
      within_scope = fi != NULL;
      /* FIXME: GDB_select_frame could fail */
      if (within_scope)
        GDB_select_frame (fi, -1);
    }

  if (within_scope)
    {
      struct type *type = get_type (var);
      if (GDB_evaluate_expression (var->root->exp, &new_val) == GDB_OK)
	{
	  if (VALUE_LAZY (new_val))
	      {
		if (GDB_value_fetch_lazy (new_val) != GDB_OK)
		  var->error = 1;
		else
		  var->error = 0;
	      }
	}
      else
	var->error = 1;

      release_value (new_val);
      return new_val;
    }

  return NULL;
}

static value_ptr
c_value_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  value_ptr value, temp;
  struct type *type, *target;
  gdb_result r;
  char *name;

  type  = get_type (parent);
  target = get_target_type (type);
  name  = name_of_child (parent, index);
  temp  = parent->value;
  value = NULL;

  if (temp != NULL)
    {
      switch (TYPE_CODE (type))
	{
	case TYPE_CODE_ARRAY:
	  r = GDB_value_slice (temp, index, 1, &value);
	  r = GDB_value_coerce_array (value, &temp);
	  r = GDB_value_ind (temp, &value);
	  break;

	case TYPE_CODE_STRUCT:
	case TYPE_CODE_UNION:
	  r = GDB_value_struct_elt (&temp, NULL, name, NULL, "vstructure", &value);
	  break;

	case TYPE_CODE_PTR:
	  switch (TYPE_CODE (target))
	    {
	    case TYPE_CODE_STRUCT:
	    case TYPE_CODE_UNION:
	      r = GDB_value_struct_elt (&temp, NULL, name, NULL, "vstructure", &value);
	      break;

	    default:
	      r = GDB_value_ind (temp, &value);
	      break;
	    }
	  break;

	default:
	  break;
	}
    }
     
  if (value != NULL)
    release_value (value);

  return value;
}

static struct type *
c_type_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  struct type *type;
  gdb_result r;
  char *name = name_of_child (parent, index);

  switch (TYPE_CODE (parent->type))
    {
    case TYPE_CODE_ARRAY:
      type = TYPE_TARGET_TYPE (parent->type);
      break;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      type = lookup_struct_elt_type (parent->type, name, 0);
      break;

    case TYPE_CODE_PTR:
      switch (TYPE_CODE (TYPE_TARGET_TYPE (parent->type)))
        {
        case TYPE_CODE_STRUCT:
        case TYPE_CODE_UNION:
	  type = lookup_struct_elt_type (parent->type, name, 0);
          break;

        default:
	  type = TYPE_TARGET_TYPE (parent->type);
          break;
        }

    default:
      break;
    }

  return type;
}

static int
c_variable_editable (var)
     gdb_variable *var;
{
  switch (TYPE_CODE (get_type (var)))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_MEMBER:
    case TYPE_CODE_METHOD:
      return 0;
      break;

    default:
      return 1;
      break;
    }
} 

static int
c_value_of_variable (var, obj)
     gdb_variable *var;
     Tcl_Obj **obj;
{
  struct type *type;
  value_ptr val;
  int result;

  if (var->value != NULL)
    val = var->value;
  else
    {
      /* This can happen if we attempt to get the value of a struct
         member when the parent is an invalid pointer.

         GDB reports the error as the error derived from accessing the
         parent, but we don't have access to that here... */
      *obj = Tcl_NewStringObj ("???", -1);
      return TCL_ERROR;
    }

  /* BOGUS: if val_print sees a struct/class, it will print out its
     children instead of "{...}" */
  type = get_type (var);
  result = TCL_OK;
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      *obj = Tcl_NewStringObj ("{...}", -1);
      break;

    case TYPE_CODE_ARRAY:
      {
        char number[16];
        *obj = Tcl_NewStringObj (NULL, 0);
        sprintf (number, "%d", var->num_children);
        Tcl_AppendStringsToObj (*obj, "[", number, "]", NULL);
      }
      break;

    default:
      result = call_gdb_val_print (val, format_code[(int) var->format]);
      *obj   = get_call_output ();
      break;
    }

  return result;
}


/* C++ */

static int
cplus_number_of_children (var)
     gdb_variable *var;
{
  struct type *type;
  int children, dont_know;

  dont_know = 1;
  children  = 0;

  if (!CPLUS_FAKE_CHILD (var))
    {
      type = get_type_deref (var);

      switch (TYPE_CODE (type))
	{
	case TYPE_CODE_STRUCT:
	case TYPE_CODE_UNION:
	  {
	    int kids[3];
  
	    cplus_class_num_children (type, kids);
	    if (kids[v_public] != 0)
	      children++;
	    if (kids[v_private] != 0)
	      children++;
	    if (kids[v_protected] != 0)
	      children++;

	    /* Add any baseclasses */
	    children += TYPE_N_BASECLASSES (type);
	    dont_know = 0;

	    /* FIXME: save children in var */
	  }
	  break;
	}
    }
  else
    {
      int kids[3];

      type = get_type_deref (var->parent);

      cplus_class_num_children (type, kids);
      if (STREQ (var->name, "public"))
	children = kids[v_public];
      else if (STREQ (var->name, "private"))
	children = kids[v_private];
      else
	children = kids[v_protected];
      dont_know = 0;
    }

  if (dont_know)
    children = c_number_of_children (var);

  return children;
}

/* Compute # of public, private, and protected variables in this class.
   That means we need to descend into all baseclasses and find out
   how many are there, too. */
static void
cplus_class_num_children (type, children)
     struct type *type;
     int children[3];
{
  int i;

  children[v_public]    = 0;
  children[v_private]   = 0;
  children[v_protected] = 0;

  for (i =  TYPE_N_BASECLASSES (type); i < TYPE_NFIELDS (type); i++)
    {
      /* If we have a virtual table pointer, omit it. */
      if (TYPE_VPTR_BASETYPE (type) == type
	  && TYPE_VPTR_FIELDNO (type) == i)
	continue;

      if (TYPE_FIELD_PROTECTED (type, i))
	children[v_protected]++;
      else if (TYPE_FIELD_PRIVATE (type, i))
	children[v_private]++;
      else
	children[v_public]++;
    }
}

static char *
cplus_name_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  char *name;
  struct type *type;
  int children[3];

  if (CPLUS_FAKE_CHILD (parent))
    {
      /* Looking for children of public, private, or protected. */
      type = get_type_deref (parent->parent);
    }
  else
    type = get_type_deref (parent);

  name = NULL;
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      cplus_class_num_children (type, children);

      if (CPLUS_FAKE_CHILD (parent))
	{
	  /* FIXME: This assumes that type orders
	     inherited, public, private, protected */
	  int i = index + TYPE_N_BASECLASSES (type);
	  if (STREQ (parent->name, "private") || STREQ (parent->name, "protected"))
	    i += children[v_public];
	  if (STREQ (parent->name, "protected"))
	    i += children[v_private];

	  name = TYPE_FIELD_NAME (type, i);
	}
      else if (index < TYPE_N_BASECLASSES (type))
	name = TYPE_FIELD_NAME (type, index);
      else
	{
	  /* Everything beyond the baseclasses can
	     only be "public", "private", or "protected" */
	  index -= TYPE_N_BASECLASSES (type);
	  switch (index)
	    {
	    case 0:
	      if (children[v_public] != 0)
		{
		  name = "public";
		  break;
		}
	    case 1:
	      if (children[v_private] != 0)
		{
		  name = "private";
		  break;
		}
	    case 2:
	      if (children[v_protected] != 0)
		{
		  name = "protected";
		  break;
		}
	    default:
	      /* error! */
	      break;
	    }
	}
      break;

    default:
      break;
    }

  if (name == NULL)
    return c_name_of_child (parent, index);
  else
    {
      if (name != NULL)
	name = savestring (name, strlen (name));
    }

  return name;
}

static value_ptr
cplus_value_of_root (var)
     gdb_variable *var;
{
  return c_value_of_root (var);
}

static value_ptr
cplus_value_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  struct type *type;
  value_ptr value;
  char *name;
  gdb_result r;

  if (CPLUS_FAKE_CHILD (parent))
    type = get_type_deref (parent->parent);
  else
    type = get_type_deref (parent);

  value   = NULL;
  name    = name_of_child (parent, index);

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      if (CPLUS_FAKE_CHILD (parent))
	{
	  value_ptr temp = parent->parent->value;
	  r = GDB_value_struct_elt (&temp, NULL, name,
				    NULL, "cplus_structure", &value);
	  if (r == GDB_OK)
	    release_value (value);
	}
      else if (index >= TYPE_N_BASECLASSES (type))
	{
	  /* public, private, or protected */
	  return NULL;
	}
      else
	{
	  /* Baseclass */
	  if (parent->value != NULL)
	    {
	      value_ptr temp;
	      int i;

	      if (TYPE_CODE (VALUE_TYPE (parent->value)) == TYPE_CODE_PTR
		  || TYPE_CODE (VALUE_TYPE (parent->value)) == TYPE_CODE_REF)
		GDB_value_ind (parent->value, &temp);
	      else
		temp = parent->value;

	      r = GDB_value_cast (TYPE_FIELD_TYPE (type, index), temp, &value);
	      if (r == GDB_OK)
		release_value (value);
	    }
	}
      break;
    }

  if (value == NULL)
    return c_value_of_child (parent, index);

  return value;
}

static struct type *
cplus_type_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  struct type *type, *t;
  gdb_result r;

  t = get_type_deref (parent);
  type = NULL;
  switch (TYPE_CODE (t))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      if (index >= TYPE_N_BASECLASSES (t))
	{
	  /* special */
	  return NULL;
	}
      else
	{
	  /* Baseclass */
	  type = TYPE_FIELD_TYPE (t, index);
	}
      break;

    default:
      break;
    }

  if (type == NULL)
    return c_type_of_child (parent, index);

  return type;
}

static int
cplus_variable_editable (var)
     gdb_variable *var;
{
  if (CPLUS_FAKE_CHILD (var))
    return 0;

  return c_variable_editable (var);
}

static int
cplus_value_of_variable (var, obj)
     gdb_variable *var;
     Tcl_Obj **obj;
{

  /* If we have one of our special types, don't print out
     any value. */
  if (CPLUS_FAKE_CHILD (var))
    {
      *obj = Tcl_NewStringObj ("", -1);
      return TCL_OK;
    }

  return c_value_of_variable (var, obj);
}

/* Java */

static int
java_number_of_children (var)
     gdb_variable *var;
{
  return cplus_number_of_children (var);
}

static char *
java_name_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
    char *name, *p;

    name = cplus_name_of_child (parent, index);
    p = name;

    while (*p != '\000')
	{
	    if (*p == '.')
		*p = '-';
	    p++;
	}

    return name;
}

static value_ptr
java_value_of_root (var)
     gdb_variable *var;
{
  return cplus_value_of_root (var);
}

static value_ptr
java_value_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  return cplus_value_of_child (parent, index);
}

static struct type *
java_type_of_child (parent, index)
     gdb_variable *parent;
     int index;
{
  return cplus_type_of_child (parent, index);
}

static int
java_variable_editable (var)
     gdb_variable *var;
{
  return cplus_variable_editable (var);
}

static int
java_value_of_variable (var, obj)
     gdb_variable *var;
     Tcl_Obj **obj;
{
  return cplus_value_of_variable (var, obj);
}

