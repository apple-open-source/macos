#import "DebuggerController_Private.h"

#import <Foundation/Foundation.h>

#if ! defined (NeXT_PDO)
#import <sys/types.h>
#import <sys/signal.h>
#endif

#include "nextstep-nat-mutils.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"

#import "debug.h"

static DebuggerController	*debuggerController;

extern int inferior_pid;
extern next_inferior_status *next_status;

void fork_and_start_debugger_controller (GdbManager *gm)
{
  /* workaround: force RunLoop to go thru +initialize
     and setup support for multithreading */
  (void) [NSRunLoop class];
			    
  debuggerController = [[DebuggerController alloc] init];

  [NSThread detachNewThreadSelector: @selector(startController:)
	    toTarget: debuggerController
	    withObject: gm];
}

@implementation DebuggerController

- init
{
  [super init];
  displayProvider = nil;
  return self;
}

- (id) displayProvider 
{
  return displayProvider;
}

/* This is the first method called in the new thread.
   Advertise ourself using the given name for the connection.
   A Gui system, like ProjectBuilder, will be looking for
   us at that name.
   
   Also, create a connection for our local (i.e. same process)
   connection to the GuiGdbManager.
   
   Then go into the runloop and wait for the Gui system to
   give us the DisplayProvider object. */

- (void) startController: gm
{
    NSString			*localName;
    NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
    NSDistantObject		*dP;
 
    gdbManager = (GdbManager <GuiGdbManagerInternal,
                  	      GuiGdbManagerExecLock> *)gm;

#if 0
    NSLog(@"Debugger Controller: looking for connection named %@ on host %@", 
    	  [gdbManager displayProviderConnectionName],
    	  [gdbManager displayProviderHostName]);
#endif
	
    dP = [NSConnection
           rootProxyForConnectionWithRegisteredName:[gdbManager displayProviderConnectionName]
                                               host:[gdbManager displayProviderHostName]];

    [dP setProtocolForProxy:@protocol(GuiDisplayProvider)];
    if ([dP conformsToProtocol:@protocol(GuiDisplayProvider2)])
        [dP setProtocolForProxy:@protocol(GuiDisplayProvider2)];
    if ([dP conformsToProtocol:@protocol(GuiDisplayProvider3)])
        [dP setProtocolForProxy:@protocol(GuiDisplayProvider3)];

    displayProviderConnection = [dP connectionForProxy];
    displayProvider = (id <GuiDisplayProvider>) [dP retain];
    [displayProvider setDebuggerController: self];
    
    localName = [[NSProcessInfo processInfo] globallyUniqueString];
    [gdbManager setDebuggerControllerConnectionName: localName];

    gdbConnection = [[NSConnection alloc] init]; 
    [gdbConnection setRootObject: self];
    [gdbConnection registerName: localName];
    
    DEBUG_PRINT ("Dbg Controller: start: exported connections\n");

    [[NSNotificationCenter defaultCenter]
       addObserver: self
          selector: @selector(handleConnectionDeath:)
              name: NSConnectionDidDieNotification
            object: displayProviderConnection];

    [[NSNotificationCenter defaultCenter]
       addObserver: self
          selector: @selector(handleConnectionDeath:)
              name: NSConnectionDidDieNotification
            object: gdbConnection];

    /* This has the side-effect of unblocking the gdbManager. */
    [gdbManager setDisplayProvider: dP];
    
    [pool release];

    [[NSRunLoop currentRunLoop] run];
}



// Called from (remote) client of us, e.g. ProjectBuilder

// execute cmd in gdb
- (oneway void) executeCmd:(NSString *)c
                   withTag:(int)tag
                   withTty:(BOOL)t
            withAnnotation:(BOOL)a
{
    GdbCmd	*cmd;

    // send cmd to Gdb 
    /* copy of command c should not be needed; 
       work around for DO problems */
    cmd = [[GdbCmd alloc] initWithCmd:[c copy]
                               ofType:GDB_CMD_EXEC
                              withTag:tag
                               useTty:t
                        useAnnotation:a];
    [gdbManager enqueueCmd:cmd];
}

- (void) synchronize
{
    GdbCmd	*cmd;

    [gdbManager setupForSynch];
    
    cmd = [[GdbCmd alloc] initWithCmd:nil
                               ofType:GDB_CMD_SYNC];
    [gdbManager enqueueCmd:cmd];

    [gdbManager waitForSynch];
}

/* 
   Interrupt; right now, this only interrupts the inferior.
   Might want to extend this to interrupt gdb thread someday.
 */

- (oneway void) interrupt
{
#if defined (NeXT_PDO)
  extern int ctrlc_received_by_debugger;
  // just jam a 1 into this guy, even though we are in a different thread.
  // sooner or later gdb will get it.
  ctrlc_received_by_debugger = 1;
#elif defined (TARGET_NATIVE)
  if (inferior_pid > 0) {
    int pid;
    thread_t thread;
    next_thread_list_lookup_by_id (next_status, inferior_pid, &pid, &thread);
    kill (pid, SIGINT);	
  }
#endif
}

- (void) handleConnectionDeath: (NSNotification *) notification
{
  if ([notification object] == displayProviderConnection) {
    /* client has gone away */
  } else {
    /* gdb connection has died */
  }
  /* FIXME: do a better job of cleanning up */
  exit (22);
}
@end
