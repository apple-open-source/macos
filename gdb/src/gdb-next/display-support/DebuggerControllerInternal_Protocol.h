/* Protocol between the Gdb Manager object (and thread)
   the DebuggerController. */
    
@protocol DebuggerControllerInternal <NSObject>

/* return the display provider to gdb */

- (id) displayProvider;

@end


