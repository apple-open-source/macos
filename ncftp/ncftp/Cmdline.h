/* 	Cmdline.h */

#ifndef _cmdline_h_
#define _cmdline_h_

/* If a command (like a transfer) took longer than this many seconds, beep
 * at the user to notify them that it completed.
 */
#define kBeepAfterCmdTime 15

typedef int (*CmdProc)(int argc, char **argv);

/* These are used in the command table, to specify that a command
 * doesn't require an exact number of parameters.
 */
#define kNoMax (-1)
#define kNoMin (-1)

/* Maximum depth allowed for ExecCommandLine, which can run itself
 * recursively.  This limit is in place to prevent infinite recursion.
 */
#define kRecursionLimit 8

/* Name of the script we run right before the interactive shell. */
#define kStartupScript "init"

/* Structure of the command table.  We keep some extra stuff in the
 * table, so each command doesn't have to check the number of
 * arguments and print it's own usage messages if it doesn't want to.
 */
typedef struct Command {
	char *name;
	int flags;
	CmdProc proc;
	int minargs, maxargs;
	char *usage, *help;
	int complete;
} Command, *CommandPtr;

/* Parameter to GetCommandOrMacro(). */
#define kAbbreviatedMatchAllowed 0
#define kExactMatchRequired 1

/* These can be returned by the GetCommand() routine. */
#define kAmbiguousCommand ((CommandPtr) -1)
#define kNoCommand ((CommandPtr) 0)

/* Command flag bits. */
#define kCmdHidden				001
#define kCmdMustBeConnected		002
#define kCmdDelayPipe			004
#define kCmdNoRedirect			010
#define kCmdWaitMsg				020
#define kCmdMustBeDisconnected	040

#ifndef _macro_h_
#include "Macro.h"
#endif

/* Structure of the name list.  Each name is either a CommandPtr or
 * a MacroNodePtr.
 */
typedef struct CMName {
	char *name;
	union cm {
		CommandPtr		cmd;
		MacroNodePtr 	mac;
	} u;
	int isCmd;
} CMName, *CMNamePtr;

/* These can be returned by the GetCommandOrMacro() routine. */
#define kAmbiguousName ((CMNamePtr) -1)
#define kNoName ((CMNamePtr) 0)

int InitCommandAndMacroNameList(void);
CMNamePtr GetCommandOrMacro(char *, int);
CommandPtr GetCommand(char *name, int wantExactMatch);
void PrintCmdHelp(CommandPtr);
void PrintCmdUsage(CommandPtr);
int ExecCommandLine(char *);
void RunScript(FILE *);
void RunStartupScript(void);
void CommandShell(void);

#endif	/* _cmdline_h_ */

/* eof Cmdline.h */
