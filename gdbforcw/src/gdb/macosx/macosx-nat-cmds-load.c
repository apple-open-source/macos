/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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
#include "top.h"
#include "command.h"
#include "gdbcmd.h"
#include "completer.h"
#include "readline/readline.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

static int debug_plugins_flag = 0;

void
load_plugin (char *arg, int from_tty)
{
  error ("plugins not supported with this gdb");
}

void
info_plugins_command (char *arg, int from_tty)
{
  printf_unfiltered ("No plugins.\n");
}

void
_initialize_load_plugin (void)
{
  struct cmd_list_element *cmd;

  cmd = add_cmd ("load-plugin", class_obscure, load_plugin,
                 "Usage: load-plugin <plugin>\n"
                 "Load a plugin from the specified path.", &cmdlist);
  set_cmd_completer (cmd, filename_completer);

  cmd = add_set_cmd ("plugins", class_obscure,
                     var_boolean, (char *) &debug_plugins_flag,
                     "Set if tracing of plugin loading is enabled",
                     &setdebuglist);
  add_show_from_set (cmd, &showdebuglist);

  add_info ("plugins", info_plugins_command, "Show current plug-ins state.");
}
