#import "GdbManager.h"
#import "GuiGdbCmd.h"
#import "GuiGdbManagerExecLock_Protocol.h"
#import "GuiGdbManagerInternal_Protocol.h"
#import "DebuggerControllerInternal_Protocol.h"


//
// GuiGdbManager object coordinates the main Gdb thread
// and the DebuggerController thread.  It manages the queue
// of commands between the two; commands are created in
// the Controller thread and executed in the Gdb thread.
//

@interface GuiGdbManager : GdbManager
		<GuiGdbManagerExecLock, GuiGdbManagerInternal>
{
    id<DebuggerControllerInternal>	controller;	// in other thread

    // how we connect to the controller
    NSString 				*controllerConnectionName;

    // synchs gdb thread w/ controller thread
    // gdb blocks until provider is avail.
    NSConditionLock  			*displayProviderLock;

    // command queue between threads
    GdbCmd				*cmdQHead;
    NSConditionLock  			*cmdQLock;

    // held when executing a cmd -- not used 
    NSLock				*execLock;

    // true if we've just entered the command loop
    int					startedCommandLoop;

    // currently executing command
    GdbCmd		*executingCommand;
    NSMutableString    	*cmdOutput;

    // used when debuggercontroller wants to know gdb is done with all commands
    NSConditionLock			*synchLock;
}

//
// Removes a command from the queue;
// can block while waiting for a lock or empty queue.
// Called by Gdb thread
//
- (GdbCmd*) dequeueCmd;

// fork controller thread, wait util we get a display
// provider, return to gdb.
- (int) establishConnection;

// called from within Gdb thread after getting cmd
- (void) executeCommand:(GdbCmd*)cmd;

// for synching
- (void) synch;
@end
