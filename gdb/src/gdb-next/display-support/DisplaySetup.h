/* Interface from GDB to the display sub-system.
   Used by gdb's "view" command and for a GUI frontend. */

#include "DisplayTypes.h"

/* Creates objects and connections to the remote display providers. If
 connection_name is null, then it uses the get_view_{connection, host}
 functions to setup a view display provider, otherwise it uses the
 connection_name to create a gui display provider.

 returns 1 on success, 0 on failure */

enum display_system {
  display_system_pb,
  display_system_view
};

int setup_display_system 
PARAMS ((enum display_system t,
	 const char *host, 
	 const char *port));

void shut_down_display_system
PARAMS (());
