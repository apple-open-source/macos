//
// Protocol used by client app (e.g. PB) to communicate with
// the debugger.
//

@protocol DebuggerController <NSObject>

// execute cmd in gdb
- (oneway void) executeCmd:(NSString *)c	// execute this command string
                          withTag:(int)tag	// command sequence number or tag
                   withTty:(BOOL)t		// does gdb think it is from a tty
            withAnnotation:(BOOL)a;		// turn on annotation before command
						//  turn it off after command and
						//  print end of annotation to stdout.


// interrupt gdb, which in turn, interrupts the inferior
- (oneway void) interrupt;

// blocks until gdb is done processing commands
- (void) synchronize;
@end
