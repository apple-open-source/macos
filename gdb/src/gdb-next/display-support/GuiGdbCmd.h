
//
// GdbCmd objects get created for the DebuggerController object
// in the Controllers's thread.
// The GuiGdbManger mantains a queue of commands. A command can
// be a string for gdb to evaluate and execute or it can be a special
// command telling gdb to go and read stdin and process what it finds there.
//

typedef enum {
   GDB_CMD_EXEC,
   GDB_CMD_SYNC,
   GDB_CMD_INPUT_PENDING
} Gdb_cmd_type;


@interface GdbCmd : NSObject
{
  @private
   NSString		*cmd;
   BOOL			useTty;
   BOOL			useAnnotation;
   int			tag;
   Gdb_cmd_type	type;
  @public
   GdbCmd		*next;
}

- initWithCmd:(NSString*)c
       ofType:(Gdb_cmd_type)t
      withTag:(int)tag
       useTty:(BOOL) tty
useAnnotation:(BOOL)anno;

- initWithCmd:(NSString*)c
       ofType:(Gdb_cmd_type)t;

- (void) dealloc;

- (Gdb_cmd_type)getType;

// note: the returned string is retained by the Cmd object
- (NSString *)getCmd;

- (BOOL) useTty;
- (BOOL) useAnnotation;
- (int) tag;
@end

