#import "GuiGdbManager.h"
#import "DisplayHooks.h"
#import "GuiDisplayProvider_Protocol.h"
#import "DisplayMisc.h"
#import <Foundation/Foundation.h>
#include "inferior.h"

#define DEBUG_MAIN
#import "debug.h"

#ifdef DEBUG
FILE	*debug_stream;
#endif


@implementation GdbCmd
- initWithCmd:(NSString*)c
       ofType:(Gdb_cmd_type)t
{
    return [self initWithCmd:c
                      ofType:t
                     withTag:0
                      useTty:NO
               useAnnotation:NO];
}

- initWithCmd:(NSString*)c
       ofType:(Gdb_cmd_type)t
      withTag:(int)tg
       useTty:(BOOL)tty
useAnnotation:(BOOL)anno
{
    [super init];
    cmd = c;
    if (cmd != nil)
        [cmd retain];
    type = t;
    useTty = tty;
    useAnnotation = anno;
    tag = tg;
    next = nil;
    return self;
}

- (void) dealloc
{
    [cmd release];
}


- (Gdb_cmd_type)getType
{
    return type;
}

- (NSString *)getCmd
{
    return cmd;
}
- (BOOL) useTty
{
    return useTty;
}

- (BOOL) useAnnotation
{
    return useAnnotation;
}

- (int) tag
{
    return tag;
}
@end

//
// To get started, the main thread sticks info into GuiGdbManager
// which is then read by the Debugger Controller thead. GuiGdbManager,
// in the main thread, waits until it is given a display provider
// before it proceeds;
// then it goes into command processing.
//

@implementation GuiGdbManager 

- init
{
    [super init];
    
    controller = nil;

    cmdQLock = [[NSConditionLock alloc] init];
    cmdQHead = nil;

    execLock = [[NSConditionLock alloc] init];
    displayProviderLock = [[NSConditionLock alloc] init];

    DEBUG_INIT;
#ifdef DEBUG
    debug_stream = stderr;
#endif
    startedCommandLoop = 0;
    executingCommand = nil;
    cmdOutput = [[NSMutableString stringWithString:@""] retain];

    synchLock = [[NSConditionLock alloc] init];
    
    return self;
}


- (void) setDebuggerControllerConnectionName:(NSString *)n
{
    controllerConnectionName = n;
    [controllerConnectionName retain];
}


#define PROVIDER_NOT_AVAILABLE 0
#define PROVIDER_AVAILABLE 1

// called from controller thread; tells gdb thread a provider is available

- (void) setDisplayProvider:(id)dP
{
    [displayProviderLock lock];
    [super setDisplayProvider: dP];
    [displayProviderLock unlockWithCondition: PROVIDER_AVAILABLE];
    DEBUG_PRINT("gdbManager: just set the provider  and unlocked\n");
}

//
// called from gdb thread.
//
    
- (int) establishConnection
{
    NSDistantObject		*c;
    int				i;
    
    fork_and_start_debugger_controller(self);

    
    DEBUG_PRINT("gdbManager: waiting for the PROVIDER to become available\n");
    
    [displayProviderLock lockWhenCondition: PROVIDER_AVAILABLE];
    [displayProviderLock unlock];

#if 0
    // Dec 1995: this workaround no longer needed
    // workaround: the DO info for the display provider is valid
    // for the other (controller) thread; we have to make correct
    // for our thread.
    displayProvider = [(NSDistantObject *)displayProvider
        		  proxyWithProperConnection];
#endif
	
    /* Now, we, the gdb thread, and the debugger controller will only
       communicate via DO or the command queue. Most of our data
       is now Read-Only so we don't use locks to access it.
     */
	
    for (i = 0; i < 10; i++) {
        c = [NSConnection
             rootProxyForConnectionWithRegisteredName: controllerConnectionName
                                                 host: nil];
        if (c)
	    break;
    }
    if (c == nil) {
	NSLog(@"GuiGdbManager: could not get connection to debugger controller");
	return 0;
    }

    [c setProtocolForProxy:@protocol(DebuggerControllerInternal)];
    controller = (id<DebuggerControllerInternal>) c;
    [controller retain];
    
#if 1
    displayProvider = [[controller displayProvider] retain];
#endif

    DEBUG_PRINT("gdbManager: waitForClient: got client & controller DO connection\n");
    return 1;
}

- (void) engageHookFunctions
{
    print_source_lines_hook = tell_displayer_display_lines;
    command_loop_hook = displayer_command_loop;
    fputs_unfiltered_hook = tell_displayer_fputs_output;

    state_change_hook = tell_displayer_state_changed;
    frame_changed_hook = tell_displayer_frame_changed;
    stack_changed_hook = tell_displayer_stack_changed;

    create_breakpoint_hook = displayer_create_breakpoint_hook;
    delete_breakpoint_hook = displayer_delete_breakpoint_hook;
    modify_breakpoint_hook = displayer_modify_breakpoint_hook;
}

- (void) engageQueryHookFunction
{
    query_hook = tell_displayer_do_query;    
    command_line_input_hook = tell_displayer_get_input;
}
    
- (void) disengageHookFunctions
{
    print_frame_info_listing_hook = NULL;

    command_loop_hook = NULL;
    query_hook = NULL;
    fputs_unfiltered_hook = NULL;

    state_change_hook = NULL;
    frame_changed_hook = NULL;
    stack_changed_hook = NULL;

    create_breakpoint_hook = NULL;
    delete_breakpoint_hook = NULL;
    modify_breakpoint_hook = NULL;
}


//
// Implementation of the GdbManagerExecLock protocol
//  
- (void) lockExecLock
{
    [execLock lock];
}

- (void) unlockExecLock
{
    [execLock unlock];
}



#define NO_CMD		0
#define CMD_AVAILABLE	1


- (void)enqueueCmd:(GdbCmd*)newCmd
{
    GdbCmd	*cmd;

    [cmdQLock lock];
    cmd = cmdQHead;
    if (cmd == nil) {
        cmdQHead = newCmd;
    }
    else {
        // we expect the cmd queue will be short;
        // so just find the end each time
        while (cmd->next != nil) {
            cmd = cmd->next;
        }
        cmd->next = newCmd;
    }
    [cmdQLock unlockWithCondition: CMD_AVAILABLE];
}

- (GdbCmd*) dequeueCmd
{
    GdbCmd	*cmd;

    DEBUG_PRINT("gdbManager dequeCmd: waiting for command\n");
    [cmdQLock lockWhenCondition: CMD_AVAILABLE];
    cmd = cmdQHead;
    cmdQHead = cmd->next;
    if (cmdQHead)
        [cmdQLock unlockWithCondition: CMD_AVAILABLE];
    else
        [cmdQLock unlockWithCondition: NO_CMD];
    DEBUG_PRINT("gdbManager dequeCmd: got command\n");
    return cmd;
}

#define SYNC_AVAILABLE 0
#define SYNC_COMPLETED 1

// called by DebuggerController to setup for sync
- (void) setupForSynch
{
   [synchLock lock];
}

// called by DebuggerController to wait until sync command is processed
- (void) waitForSynch
{
    [synchLock lockWhenCondition:SYNC_COMPLETED];
    [synchLock unlockWithCondition:SYNC_AVAILABLE];
}

// called by us (self) when the sync command is processed- (void) synch
- (void) synch
{
    [synchLock unlockWithCondition:SYNC_COMPLETED];
}

// execute a gdb command; assumes we have the exec lock

static int useTty = 0;

// used to tell output hook if the data is annotated. 
static int annotatedOutput = 0;

static int
execute_command_for_PB (char *command)
{
  execute_command (command, useTty);
  bpstat_do_actions (&stop_bpstat);
  return 0;
}

-(void)executeCommand: (GdbCmd *)command
{
    int 	wasError;
    const char	*cmd;
    int		old_verbose;
    extern int	info_verbose;
    extern	annotation_level;

    cmd = [[command getCmd] cString];

    if ([command useAnnotation]) {
        annotation_level = 2;
        printf_unfiltered("\n\032\032annotation-begin\n");
    }

    useTty = (int)[command useTty];
    old_verbose = info_verbose;
    info_verbose = 0;
    
    wasError = catch_errors(execute_command_for_PB, (char *)cmd,
                            NULL, RETURN_MASK_ALL);
    //execute_command_for_PB((char *)cmd);
    
    info_verbose = old_verbose;
    
    if ([command useAnnotation]) {
        annotation_level = 0;
        printf_unfiltered("\n\032\032annotation-end\n");
        annotatedOutput =  0;
    }
    
    if (wasError) {
       // fprintf(stderr, "Error occured processing DO command.\n");
    }
    fflush(stdout);
    fflush(stderr);
}

- (void) beginCommandWithTag:(int) t
{
    id<GuiDisplayProvider3>	dp;

    NS_DURING {
        dp = [self
                displayProviderForProtocol:@protocol(GuiDisplayProvider3)];

        if (dp) {
            [dp willExecuteCommandWithTag:t];
        }
    }
    NS_HANDLER {
        shut_down_display_system();
    }
    NS_ENDHANDLER;
}

- (void) endCommandWithTag:(int) t
{
    id<GuiDisplayProvider3>	dp;

    NS_DURING {
        dp = [self
                displayProviderForProtocol:@protocol(GuiDisplayProvider3)];

        if (dp) {
            [dp didExecuteCommandWithTag:t];
        }
    }
    NS_HANDLER {
        shut_down_display_system();
    }
    NS_ENDHANDLER;
}

- (void) initCommandState:(GdbCmd *)c
{
    executingCommand = c;
}


- (void) sendOutputToDisplayProvider:(NSString *)str
                                type:(GdbOutputType)t
{
    id<GuiDisplayProvider2>	dp;

    if ([str isEqual:@""])
        return;
    
    NS_DURING {
        dp = [self
                displayProviderForProtocol:@protocol(GuiDisplayProvider2)];

        if (dp) {
            [dp outputFromGDB:str
                         type:t];
        }
    }
    NS_HANDLER {
        //EXCEPTION_MSG(@"outputFromGDB");
        shut_down_display_system();
    }
    NS_ENDHANDLER;
}

- (void) processOutput:(NSString *)outputStr
            outputType:(GdbOutputType)t
{
    BOOL	annotated = [executingCommand useAnnotation];

    if ((t == GDB_OUTPUT_STDOUT) && !annotated) {
        [cmdOutput appendString:outputStr];
        if ([outputStr rangeOfString:@"\n"].location != NSNotFound){
            [self sendOutputToDisplayProvider:cmdOutput
                                         type:GDB_OUTPUT_STDOUT];
            [cmdOutput setString:@""];
        }
    }
    else if (annotated) {
        // treat errors just like normal output
        [cmdOutput appendString:outputStr];
    }
    else {
        // flush pending output
        [self sendOutputToDisplayProvider:cmdOutput
                                     type:GDB_OUTPUT_STDOUT];
        [cmdOutput setString:@""];

        // send errors
        [self sendOutputToDisplayProvider:outputStr
                                     type:GDB_OUTPUT_STDERR];          
    }
}

- (void) cleanupCommandState
{

    if (executingCommand && [executingCommand useAnnotation]) {
        [self sendOutputToDisplayProvider:cmdOutput
                                     type:GDB_OUTPUT_ANNOTATION];
    }
    else {
        [self sendOutputToDisplayProvider:cmdOutput
                                     type:GDB_OUTPUT_STDOUT];
    }

    if (executingCommand) {
        [executingCommand release];
        executingCommand = nil;
    }

    [cmdOutput setString:@""];
}

/*
  Called when gdb sends output and wants input which is not
  part of the normal comamnd-prompt loop.
 */

- (const char  *) waitForLineOfInput
{
    GdbCmd	*command;
    const char	*cmdString;
    
    /* send any buffered output to UI display */
    [self sendOutputToDisplayProvider:cmdOutput
                                 type:GDB_OUTPUT_STDOUT];

    /* reset cmdOutput buffer to null string */
    [cmdOutput setString:@""];

    /* wait for command */
    command = [self dequeueCmd];

    /* get cString from it */
    cmdString =  [[command getCmd] cString];

    /* release command object */
    [command release];
    
    return cmdString;
}

- (void) doCommandLoop
{
    GdbCmd *cmd = NULL;
    char *prompt = NULL;
    int do_prompt = 0;

    prompt = get_prompt ();
	
    DEBUG_PRINT ("in Command Loop\n");

    if (!startedCommandLoop) {
	startedCommandLoop = 1;
	[self engageQueryHookFunction];
    }
    

    // send out prompt
    fputs_unfiltered (prompt, gdb_stdout);
    gdb_flush (gdb_stdout);
    do_prompt = 0;

    [self cleanupCommandState];
    
    // process cmds from server; will block waiting for cmds
    while (cmd = [self dequeueCmd]) {

        if ((++pool_num_times) >= POOL_RELEASE_MAX_TIMES) {
            [pool release];
            pool_num_times = 0;
            pool = [[NSAutoreleasePool alloc] init];
        }
        switch ([cmd getType]) {
          case GDB_CMD_EXEC:
              if ([cmd useTty]) {
                  do_prompt = 1;
              }
              [self beginCommandWithTag:[cmd tag]];
              [self initCommandState:cmd];
              [self executeCommand:cmd];
              [self cleanupCommandState];
              [self endCommandWithTag:[cmd tag]];
              break;
              
          case GDB_CMD_SYNC:
              [self synch];
              break;
          default:
            //FIXME: report an error here
            break;
        } // end switch
        
        if (do_prompt) {
            // should endup calling our fputs_unfiltered_hook and
            // get sent to PB
            fputs_unfiltered (prompt, gdb_stdout);
            gdb_flush (gdb_stdout);
            do_prompt = 0;
            [self cleanupCommandState];
        }
        

    } // end while
}

@end


static GuiGdbManager	*guiGdbManager;

GdbManager * make_gui_gdb_manager()
{
     return (guiGdbManager = [[GuiGdbManager alloc] init]);       
}

