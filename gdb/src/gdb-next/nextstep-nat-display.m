#include "nextstep-nat-display.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-inferior.h"

#include "defs.h"
#include "symtab.h"
#include "symfile.h"
#include "target.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "breakpoint.h"

#include "DisplaySetup.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

static char *view_program = NULL;
static char *view_host = NULL;
static char *view_protocol = NULL;

static int have_connection = 0;

/* This must not use error() or warning(), as it may be called before
   the rest of GDB has initialized. */

void connect_to (char *arg)
{
  const char *name = arg;
  const char *host = NULL;
  char *s;

  if (arg == NULL) {
    fprintf (stderr, "Requires a connection name as arguement.\n");
  }

  for (s = arg; *s != '\0'; s++) {
    if (*s == ':') {
      *s = '\0';
      host = name;
      name = s + 1;
      break;
    }
  }
    
  if (setup_display_system (display_system_pb, host, name) != 0) {
    fprintf (stderr, "Unable to connect to \"%s\" on host \"%s\".\n",
	     arg, view_host);
  }
  have_connection = 1;
}

static void view_command (char *arg, int from_tty)
{
  if (arg) {
    error ("Command does not take any arguments.\n");
  }

  if (have_connection) {
    error ("Viewing disabled when gdb is started by ProjectBuilder.\n");
  }
    
  {
    char name[80];
    snprintf (name, 80, "%s:%s", view_program, view_protocol);
    
    if (setup_display_system (display_system_view, view_host, name) != 0) {
      error ("Unable to connect to \"%s\" on host \"%s\" via \"%s\".\n",
	     view_program, view_host, view_protocol);
    }
  }
  
  have_connection = 1;

  if (target_has_stack) {
    print_sel_frame (1);
  }
}

static void unview_command (char *arg, int from_tty)
{
  shut_down_display_system ();
  have_connection = 0;

  if (target_has_stack) {
    print_sel_frame (1);
  }
}

void
_initialize_display ()
{
  struct cmd_list_element *cmd = NULL;

  add_com ("view", class_files, view_command,
	   "Displays the current line in an editor program;\n"
	   "(default editor is ProjectBuilder).");
  add_com_alias ("v", "view", class_files, 1);
  
  add_com ("unview", class_files, unview_command,
	   "Stops viewing of source files.");

  if (view_program == NULL) {
    view_program = getenv ("ViewProgram");
  }
  if (view_program == NULL) {
    view_program = "ProjectBuilder";
  }
  view_program = savestring (view_program, strlen (view_program));

  if (view_host == NULL) {
    view_host = "";
  }
  view_host = savestring (view_host, strlen (view_host));

  view_protocol = "ViewForGdb";
  view_protocol = savestring (view_protocol, strlen (view_protocol));

  cmd = add_set_cmd ("view-host", class_files, var_string_noescape,
		     (char *) &view_host,
		     "Set host to connect to when viewing.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("view-program", class_files, var_string_noescape, 
		     (char *) &view_program,
		     "Set name of program to connect to when viewing.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
	
  cmd = add_set_cmd ("view-protocol", class_files, var_string_noescape,
		     (char *) &view_protocol,
		     "Set protocol to use when connecting to viewer program.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
}
