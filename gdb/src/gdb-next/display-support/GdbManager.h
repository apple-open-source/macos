#include <Foundation/Foundation.h>

#include "DisplayTypes.h"

/* The GdbManager object holds info which allows gdb to
   communicate with a DisplayProvider object. Subclasses
   do most of the work. */

@interface GdbManager : NSObject
{
  /* When some state changes in the inferior, like the
     current line number of frame, then Gdb needs to inform
     a display provider of the change.  It implements the  
     <ViewDisplayProvider> or <GuiDisplayProvider> protocol. */

  id displayProvider;
  NSString *displayProviderConnectionName;
  NSString *displayProviderHostName;
}

- init;
- (void) dealloc;

- (void) setDisplayProviderConnectionName: (const char *) c
				     host :(const char *) h;

- (NSString *) displayProviderConnectionName;

- (NSString *) displayProviderHostName;

- (void) setDisplayProvider:(id) dP;

/* Get client provider iff it supports the protocol; returns nil otherwise. */
-  displayProviderForProtocol: (Protocol *) p;

/* Subclasses should override these methods */

/* only the GUI uses these */
- (void) doCommandLoop;
- (void) processOutput: (NSString *) outputStr
            outputType: (GdbOutputType) t;

/* called as a hook to get a line of input */
- (const char  *) waitForLineOfInput;

- (void) engageHookFunctions;
- (void) disengageHookFunctions;

/* set up connection to other (remote) object(s) */
- (int) establishConnection;

@end
