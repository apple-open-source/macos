
#import "ViewDisplayProvider_Protocol.h"

// For providers  which uses a GUI for interacting with the debugger
// Methods invoked by the debugger on the GUI client.

@protocol GuiDisplayProvider <ViewDisplayProvider>

// The debugger controller implements the DebuggerController
// protocol; leave it out of the parameter definition to
// decouple the display provider from the controller.
- (oneway void) setDebuggerController:(id)dC;


- (oneway void) breakpointChanged:(int)bpNum
                         newState:(BreakpointState)state
                           inFile:(NSString *)fileName
                           atLine:(int)lineNumber;

- (oneway void) frameChanged:(int)newFrame;

//
// sent when the stack has changed -- that is when frame 0 is
// in different function; this is not sent then the line changes in
// the current frame.
// If the number of stack frames exceedes a limit, then limitReached is true.
//
- (oneway void) stackChanged:(int)newSize limitReached:(BOOL)maxedOut;
@end

// extention to base GUI protocol for doing all IO via DO.
@protocol GuiDisplayProvider2 <GuiDisplayProvider>

// a query from gdb
- (int) query:(NSString *)qStr;

// output from GDB
- (oneway void) outputFromGDB:(NSString *)oStr
                         type:(GdbOutputType)oType;
@end

// supports sending back the tag of the command
@protocol GuiDisplayProvider3 <GuiDisplayProvider2>

- (oneway void) willExecuteCommandWithTag:(int)tag;
- (oneway void) didExecuteCommandWithTag:(int)tag;

@end

