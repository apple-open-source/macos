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
#include "filenames.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <dlfcn.h>

#include <mach-o/dyld.h>

struct plugin_state
{
  char **names;
  size_t num;
  void **plugin_data;
};

void *_plugin_global_data;

static struct plugin_state pstate;

static int debug_plugins_flag = 0;

void
load_plugin (char *arg, int from_tty)
{
  void (*fptr) () = NULL;
  char *init_func_name = "init_from_gdb";
  char *p, path[PATH_MAX + 1];
  struct stat sb;

  if (arg == NULL)
    {
      error ("Usage: load-plugin <plugin>");
      return;
    }

  strcpy (path, p = tilde_expand (arg));
  xfree (p);

  if (stat (path, &sb) != 0)
    error ("GDB plugin \"%s\" not found.", path);

  /* If gdb is running as setgid, check that the plugin is also setgid
     (to the same gid) to avoid a privilege escalation.  */

  if (getgid () != getegid ())
    {
      /* Same setgid as gdb itself?  */
      if (getegid () != sb.st_gid || (sb.st_mode & S_ISGID) == 0)
        {
          struct group *gr;
          char *grpname = "";
          gr = getgrgid (getegid ());
          if (gr && gr->gr_name != NULL)
            grpname = gr->gr_name;
          error ("GDB plugin \"%s\" must be setgid %s to be loaded.", path,
                  grpname);
        }
    }

  /* dyld won't let a setgid program like gdb load a plugin by relative
     path.  */
  if (!IS_ABSOLUTE_PATH (path))
    error ("Usage: load-plugin FULL-PATHNAME\n"
           "Relative pathnames ('%s') are not permitted.", path);

  if (debug_plugins_flag)
    {
      printf_unfiltered ("Loading GDB module from \"%s\"\n", arg);
    }

  {
    if (debug_plugins_flag)
      {
        printf_unfiltered ("Linking GDB module from \"%s\"\n", path);
      }

    void *ret = dlopen (path, RTLD_LOCAL | RTLD_NOW);
    if (ret == NULL)
      {
        error ("Unable to dlopen plugin \"%s\", reason: %s",
               path, dlerror ());
      }
    fptr = dlsym (ret, init_func_name);
    if (fptr == NULL)
      {
        dlclose (ret);
        error ("Unable to locate symbol '%s' in module.", init_func_name);
      }
  }

  if (debug_plugins_flag)
    {
      printf_unfiltered ("Calling '%s' in \"%s\"\n", init_func_name, path);
    }

  CHECK_FATAL (fptr != NULL);

  /* Make sure the names and data arrays are updated BEFORE calling 
     init_func_name() so that the plugin can use _plugin_private_data()  */

  pstate.plugin_data =
    xrealloc (pstate.plugin_data, (pstate.num + 1) * sizeof (void *));
  pstate.plugin_data[pstate.num] = NULL;

  pstate.names = xrealloc (pstate.names, (pstate.num + 1) * sizeof (char *));
  pstate.names[pstate.num] = xstrdup (path);
  pstate.num++;

  (*fptr) ();
}

void
info_plugins_command (char *arg, int from_tty)
{
  size_t i;
  for (i = 0; i < pstate.num; i++)
    {
      printf_unfiltered ("%s\n", pstate.names[i]);
    }
}

void
_initialize_load_plugin (void)
{
  struct cmd_list_element *cmd;

  pstate.names = NULL;
  pstate.num = 0;
  pstate.plugin_data = NULL;

  cmd = add_cmd ("load-plugin", class_obscure, load_plugin,
                 "Usage: load-plugin <plugin>\n"
                 "Load a plugin from the specified path.", &cmdlist);
  set_cmd_completer (cmd, filename_completer);
  /* cmd->completer_word_break_characters = gdb_completer_filename_word_break_characters; *//* FIXME */

  add_setshow_boolean_cmd ("plugins", class_obscure,
			   &debug_plugins_flag, _("\
Set if tracing of plugin loading is enabled"), _("\
Show if tracing of plugin loading is enabled"), NULL,
			   NULL, NULL,
			   &setdebuglist, &showdebuglist);

  add_info ("plugins", info_plugins_command, "Show current plug-ins state.");
}

/* Search for a loaded plugin by name and return a pointer to it's private data
   slot allocated for plugin use.  Return NULL if plugin is not loaded.
   If NULL is passed for a plugin name then a pointer to a global data pointer
   is returned.  */
void **
_plugin_private_data (char *plugin_name)
{
  size_t i;
  char *p;

  if (plugin_name == NULL)
    return &_plugin_global_data;

  for (i = 0; i < pstate.num; i++)
    {
      p = strrchr (pstate.names[i], '/');
      if (p)
        {
          if (strcmp (plugin_name, p + 1) != 0)
            continue;
        }
      else if (strcmp (plugin_name, pstate.names[i]) != 0)
        continue;
      return &pstate.plugin_data[i];
    }

  return NULL;
}
