#import <Foundation/Foundation.h>
#import <stdio.h>

#import "defs.h"
#import "GdbManager.h"
#import "DisplayMisc.h"
#import "DisplaySetup.h"
#import "debug.h"

/* internal functions */

extern char *get_view_connection();
extern char *get_view_host();
extern void turn_off_viewing();

/* gdbManager is the object which manages GDB and the thread it runs in.
   This object communicates with the client (in another process)
   and the DebuggerServer object in our process but in
   another thread. */

GdbManager *gdbManager = nil;

/* The autorelease pool is needed because the DO facilities use it.
   Since the gdb thread isn't in a Run Loop, we have to manage
   the pool ourself and when it gets released. */

NSAutoreleasePool *pool;
int pool_num_times = 0;
static int disp_is_view;

int setup_display_system
(enum display_system t, const char *host, const char *port)
{
  pool = [[NSAutoreleasePool alloc] init];

  switch (t) {
  case display_system_pb:
    gdbManager = make_gui_gdb_manager ();
    disp_is_view = 0;
    break;
    
  case display_system_view:
    gdbManager = make_view_gdb_manager ();
    disp_is_view = 1;
    break;

  default:
    abort ();
  }
   
  [gdbManager setDisplayProviderConnectionName: port
	      host: host];

  if ([gdbManager establishConnection] == 0) {
    [gdbManager release];
    [pool release];
    return -1;
  }

  [gdbManager engageHookFunctions];
  return 0;
}

void
shut_down_display_system ()
{
  [gdbManager disengageHookFunctions];

  if (gdbManager != nil) {
    [gdbManager release];
    gdbManager = nil;
  }

  if (pool != nil) {
    [pool release];
    pool_num_times = 0;
    pool = nil;
  }

  if (disp_is_view) {
    printf ("Connection has gone away.  Viewing is now off.\n");
  }
}    
