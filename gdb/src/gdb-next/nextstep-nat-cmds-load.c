#include "defs.h"
#include "top.h"
#include "command.h"
#include "gdbcmd.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <mach-o/dyld.h>

struct plugin_state {
  char **names;
  size_t num;
  void **plugin_data;
};

void *_plugin_global_data;

static struct plugin_state pstate;

static int debug_plugins_flag = 0;

void load_plugin (char *arg, int from_tty)
{
  void (*fptr) () = NULL;
  char *init_func_name = "_init_from_gdb";
  char *p, path[PATH_MAX+1];

  if (arg == NULL) {
    error ("Usage: load-plugin <plugin>");
    return;
  }
  
  strcpy ( path, p = tilde_expand (arg));
  xfree (p);

  if (debug_plugins_flag) {
    printf_unfiltered ("Loading GDB module from \"%s\"\n", arg);
  }

  {
    NSObjectFileImage image;
    NSObjectFileImageReturnCode nsret;
    NSSymbol sym;
    NSModule module;

    nsret = NSCreateObjectFileImageFromFile (path, &image);
    if (nsret != NSObjectFileImageSuccess) {
      error ("NSCreateObjectFileImageFromFile failed for \"%s\" (error %d).", path, nsret);
    }
    
    if (debug_plugins_flag) {
      printf_unfiltered ("Linking GDB module from \"%s\"\n", path);
    }

    module = NSLinkModule (image, path, NSLINKMODULE_OPTION_PRIVATE);
    /*module = NSLinkModule (image, path, NSLINKMODULE_OPTION_NONE);*/
    if (module == NULL) {
      error ("NSLinkModule failed for \"%s\" (error %d).", path, nsret);
    }

    sym = NSLookupSymbolInModule (module, init_func_name);
    if (sym == NULL) {
      error("Unable to locate symbol '%s' in module.", init_func_name);
    }

    fptr = NSAddressOfSymbol (sym);
    if (fptr == NULL) {
      error ("Unable to locate address for symbol '%s' in module.", init_func_name);
    }
  }

  if (debug_plugins_flag) {
    printf_unfiltered ("Calling '%s' in \"%s\"\n", init_func_name, path);
  }
  
  CHECK_FATAL (fptr != NULL);

  /* Make sure the names and data arrays are updated BEFORE calling init_func_name()
     so that the plugin can use _plugin_private_data()  */ 
  
  pstate.plugin_data = xrealloc (pstate.plugin_data, (pstate.num + 1) * sizeof (void *));
  pstate.plugin_data[pstate.num] = NULL;

  pstate.names = xrealloc (pstate.names, (pstate.num + 1) * sizeof (char *));
  pstate.names[pstate.num] = xstrdup (path);
  pstate.num++;

  (* fptr) ();
}

void info_plugins_command (char *arg, int from_tty)
{
  size_t i;
  for (i = 0; i < pstate.num; i++) {
    printf_unfiltered ("%s\n", pstate.names[i]);
  }
}

void
_initialize_load_plugin ()
{
  struct cmd_list_element *cmd;

  pstate.names = NULL;
  pstate.num = 0;
  pstate.plugin_data = NULL;

  cmd = add_cmd ("load-plugin", class_obscure, load_plugin,
                 "Usage: load-plugin <plugin>\n"
                 "Load a plugin from the specified path.",
                 &cmdlist);
  cmd->completer = filename_completer;
  cmd->completer_word_break_characters = gdb_completer_filename_word_break_characters;
  
  cmd = add_set_cmd ("debug-plugins", class_obscure,
                     var_boolean, (char *) &debug_plugins_flag,
                     "Set if tracing of plugin loading is enabled",
                     &setlist);
  add_show_from_set (cmd, &showlist);

  add_info ("plugins", info_plugins_command, "Show current plug-ins state.");
}

/* Search for a loaded plugin by name and return a pointer to it's private data
   slot allocated for plugin use.  Return NULL if plugin is not loaded.  
   If NULL is passed for a plugin name then a pointer to a global data pointer
   is returned.  */
void 
**_plugin_private_data (char *plugin_name)
{
  size_t i;
  char   *p;
  
  if (plugin_name == NULL)
      return &_plugin_global_data;
      
  for (i = 0; i < pstate.num; i++) {
    p = strrchr (pstate.names[i], '/');
    if (p) {
      if (strcmp (plugin_name, p+1) != 0)
        continue;
    } else if (strcmp (plugin_name, pstate.names[i]) != 0)
        continue;
    return &pstate.plugin_data[i];
  }
  
  return NULL;
}
