#include "defs.h"
#include "top.h"
#include "command.h"
#include "gdbcmd.h"

#include <stdio.h>
#include <string.h>

#include <mach-o/dyld.h>

struct plugin_state {
  char **names;
  size_t num;
};

static struct plugin_state pstate;

static int debug_plugins_flag = 0;

void load_plugin (char *arg, int from_tty)
{
  char *path = arg;
  void (*fptr) () = NULL;
  char *init_func_name = "_init_from_gdb";

  if (path == NULL) {
    error ("Usage: load-plugin <plugin>");
    return;
  }

  if (debug_plugins_flag) {
    printf_unfiltered ("Loading GDB module from \"%s\"\n", path);
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
  (* fptr) ();

  pstate.names = xrealloc (pstate.names, (pstate.num + 1) * sizeof (char *));
  pstate.names[pstate.num] = xstrdup (path);
  pstate.num++;
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

  add_info ("dyld", info_plugins_command, "Show current plug-ins state.");
}
