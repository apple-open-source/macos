// For simple providers which highlight the current line
// These providers should advertise an object named
// 	<Program>:ViewForGdb
// where <Program> is the program name used in GDB's set view-program
// command. The default is "ProjectBuilder:ViewForGdb"
//

#import "DisplayTypes.h"

@protocol ViewDisplayProvider <NSObject>

// using -1 for the thread values means ignore it. 
- (oneway void) lineChangedForThread:(int) t
                              inFile:(NSString *)f
                         atStartLine:(int)sl
                           toEndLine:(int)el;


- (oneway void) inferiorStateChanged:(DebuggerState) newState;
@end
