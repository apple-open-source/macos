/* Generic code for supporting multiple C++ ABI's
   Copyright 2001 Free Software Foundation, Inc.

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
#include "value.h"
#include "cp-abi.h"
#include "command.h"
#include "ui-out.h"
#include "gdbcmd.h"

static struct cp_abi_ops *find_cp_abi (const char *short_name);

static struct cp_abi_ops current_cp_abi = {"", NULL};
static struct cp_abi_ops auto_cp_abi = {"auto", NULL};

#define CP_ABI_MAX 8

static struct cp_abi_ops *cp_abis[CP_ABI_MAX];

static int num_cp_abis = 0;

enum ctor_kinds
is_constructor_name (const char *name)
{
  if ((current_cp_abi.is_constructor_name) == NULL)
    error ("ABI doesn't define required function is_constructor_name");
  return (*current_cp_abi.is_constructor_name) (name);
}

enum dtor_kinds
is_destructor_name (const char *name)
{
  if ((current_cp_abi.is_destructor_name) == NULL)
    error ("ABI doesn't define required function is_destructor_name");
  return (*current_cp_abi.is_destructor_name) (name);
}

int
is_vtable_name (const char *name)
{
  if ((current_cp_abi.is_vtable_name) == NULL)
    error ("ABI doesn't define required function is_vtable_name");
  return (*current_cp_abi.is_vtable_name) (name);
}

int
is_operator_name (const char *name)
{
  if ((current_cp_abi.is_operator_name) == NULL)
    error ("ABI doesn't define required function is_operator_name");
  return (*current_cp_abi.is_operator_name) (name);
}

int
baseclass_offset (struct type *type, int index, char *valaddr,
		  CORE_ADDR address)
{
  if (current_cp_abi.baseclass_offset == NULL)
    error ("ABI doesn't define required function baseclass_offset");
  return (*current_cp_abi.baseclass_offset) (type, index, valaddr, address);
}

struct value *
value_virtual_fn_field (struct value **arg1p, struct fn_field * f, int j,
			struct type * type, int offset)
{
  if ((current_cp_abi.virtual_fn_field) == NULL)
    return NULL;
  return (*current_cp_abi.virtual_fn_field) (arg1p, f, j, type, offset);
}

struct type *
value_rtti_type (struct value *v, int *full, int *top, int *using_enc)
{
  if ((current_cp_abi.rtti_type) == NULL)
    return NULL;
  return (*current_cp_abi.rtti_type) (v, full, top, using_enc);
}

int
register_cp_abi (struct cp_abi_ops *abi)
{
  if (num_cp_abis == CP_ABI_MAX) 
    {
      internal_error (__FILE__, __LINE__,
		      "Too many CP ABIs, please increase CP_ABI_MAX in cp-abi.c");
    }
  
  cp_abis[num_cp_abis++] = abi;

  return 1;

}

void
set_cp_abi_as_auto_default (const char *short_name)
{
  struct cp_abi_ops *abi = find_cp_abi (short_name);
  if (abi == NULL)
    internal_error (__FILE__, __LINE__,
		    "Cannot find C++ ABI \"%s\" to set it as auto default.",
		    short_name);

  if (auto_cp_abi.longname != NULL)
    xfree (auto_cp_abi.longname);

  if (auto_cp_abi.doc != NULL)
    xfree (auto_cp_abi.doc);

  auto_cp_abi = *abi;

  auto_cp_abi.shortname = "auto";
  auto_cp_abi.longname = (char *) xmalloc (strlen ("currently ")
					   + strlen (abi->shortname));
  sprintf (auto_cp_abi.longname, "currently %s", 
	   abi->shortname);
  
  auto_cp_abi.doc = (char *) xmalloc (strlen ("currently ")
				      + strlen (abi->shortname));
  sprintf (auto_cp_abi.doc, "currently %s", 
	   abi->shortname);
  
  /* Since we copy the current ABI into current_cp_abi instead of using
     a pointer, if auto is currently the default, we need to reset it. */
									  
  if (cp_abi_is_auto_p ())
    switch_to_cp_abi ("auto");
}

int
cp_abi_is_auto_p ()
{
  if (strcmp (current_cp_abi.shortname, "auto") == 0)
    return 1;
  else
    return 0;
}

static struct cp_abi_ops *
find_cp_abi (const char *short_name)
{
  int i;

  for (i = 0; i < num_cp_abis; i++)
    if (strcmp (cp_abis[i]->shortname, short_name) == 0)
      {
	return cp_abis[i];
      }

  return NULL;
}

int
switch_to_cp_abi (const char *short_name)
{
  struct cp_abi_ops *abi;
  
  abi = find_cp_abi (short_name);
  if (abi == NULL)
    return 0;

  current_cp_abi = *abi;
  return 1;
  
}

void
show_cp_abis (int from_tty)
{
  int i;
  ui_out_text (uiout, "The available C++ ABIs are:\n");
 
  ui_out_tuple_begin (uiout, "cp-abi-list");
  for (i = 0; i < num_cp_abis; i++)
    {
      ui_out_field_string (uiout, "cp-abi", cp_abis[i]->shortname);
      ui_out_text_fmt (uiout, " - %s\n", cp_abis[i]->doc); 
    }
  ui_out_tuple_end (uiout);

}

void
set_cp_abi_cmd (char *args, int from_tty)
{

  if (args == NULL)
    {
      show_cp_abis (from_tty);
      return;
    }

  if (!switch_to_cp_abi (args))
    error ("Could not find \"%s\" in ABI list", args);
}

void
show_cp_abi_cmd (char *args, int from_tty)
{
  ui_out_text (uiout, "The currently selected C++ ABI is: ");
 
  ui_out_field_string (uiout, "cp-abi", current_cp_abi.shortname);
  ui_out_text (uiout, " - ");
  ui_out_field_string (uiout, "doc", current_cp_abi.doc);
  ui_out_text (uiout, ".\n");
}

void
_initialize_cp_abi (void)
{
  struct cmd_list_element *cmd;

  register_cp_abi (&auto_cp_abi);
  switch_to_cp_abi ("auto");

  cmd = add_cmd ("cp-abi", class_obscure , set_cp_abi_cmd, 
		 "Set the ABI used for inspecting C++ objects.\n"
		 "\"set cp-abi\" with no arguments will list the "
		 "available ABIs.", 
		 &setlist);

  cmd = add_cmd ("cp-abi", class_obscure, show_cp_abi_cmd, 
		 "Show the ABI used for inspecting C++ objects.", &showlist);

}
