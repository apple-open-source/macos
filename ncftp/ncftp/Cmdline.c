/* Cmdline.c
 *
 * Purpose: Read and execute user input lines from the program shell.
 */

#include "Sys.h"

#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "Cmdline.h"
#include "Cmds.h"
#include "Main.h"
#include "MakeArgv.h"
#include "Open.h"
#include "Bookmark.h"

static int CMSortCmp(CMNamePtr, CMNamePtr);
static int CMExactSearchCmp(char *, CMNamePtr);
static int CMSubSearchCmp(char *, CMNamePtr);

/* This is the sorted array of commands and macros.  When a user types
 * something on the command line, we look to see if we have a match
 * in this list.
 */
CMNamePtr gNameList = (CMNamePtr)0;

/* Number of commands and macros in the NameList. */
int gNumNames;

/* Upon receipt of a signal during execution of a command, we jump here. */
jmp_buf gCommandJmp;

/* Upon receipt of a signal while waiting for input, we jump here. */
jmp_buf gCmdLoopJmp;

/* We have our commands write to our own "stdout" so we can have our stdout
 * point to a file, and at other times have it go to the screen.
 *
 * So gRealStdout is a copy of the real "stdout" stream, and gStdout is
 * our stream, which may or may not be writing to the screen.
 */
int gRealStdout;
int gStdout;
FILE *gStdoutFile;

/* We keep running the command line interpreter until gDoneApplication
 * is non-zero.
 */
int gDoneApplication = 0;

/* Track how many times they use ^C. */
int gNumInterruptions = 0;

/* Keep a count of the number of commands the user has entered. */
long gEventNumber = 0L;

/* If using visual mode, this controls how to insert extra blank lines
 * before/after commands.
 */
int gBlankLines = 1;

extern int gNumCommands;
extern int gNumGlobalMacros;
extern MacroNodePtr gFirstMacro;
extern int gConnected, gWinInit;
extern string gHost;
extern Command gCommands[];
extern UserInfo gUserInfo;
extern Bookmark gRmtInfo;
extern longstring gRemoteCWD;
extern int gIsFromTTY, gDoingScript, gIsToTTY;


/* This is used as the comparison function when we sort the name list. */
static int CMSortCmp(CMNamePtr a, CMNamePtr b)
{
	return (strcmp((*a).name, (*b).name));
}	/* CMSortCmp */




/* We make a big list of the names of the all the standard commands, plus
 * the names of all the macros read in.  Then when the user
 * types a command name on the command line, we search this list and see
 * what the name matches (either a command, a macro, or nothing).
 */

int InitCommandAndMacroNameList(void)
{
	Command			*c;
	CMNamePtr		canp;
	MacroNodePtr	mnp;
	int i;

	/* Free an old list, in case you want to re-make it. */
	if (gNameList != (CMNamePtr)0)
		free(gNameList);

	gNumNames = gNumGlobalMacros + gNumCommands;	
	gNameList = (CMNamePtr) calloc((size_t)gNumNames, sizeof (CMName));
	if (gNameList == NULL)
		OutOfMemory();

	i = 0;
	for (c = gCommands, canp = gNameList; i < gNumCommands ; c++, canp++) {
		canp->name = c->name;
		canp->isCmd = 1;
		canp->u.cmd = c;
		++i;
	}
	
	for (mnp = gFirstMacro; mnp != NULL; mnp = mnp->next, canp++) {
		canp->name = mnp->name;
		canp->isCmd = 0;
		canp->u.mac = mnp;
		++i;
	}
	
	/* Now sort the list so we can use bsearch later. */
	QSORT(gNameList, gNumNames, sizeof(CMName), CMSortCmp);

	return (0);
}	/* InitCommandAndMacroNameList */




/* This is used as the comparison function when we lookup something
 * in the name list, and when we want an exact match.
 */
static int CMExactSearchCmp(char *key, CMNamePtr b)
{
	return (strcmp(key, (*b).name));
}	/* CMExactSearchCmp */




/* This is used as the comparison function when we lookup something
 * in the name list, and when the key can be just the first few
 * letters of one or more commands.  So a key of "qu" might would match
 * "quit" and "quote" for example.
 */
static int CMSubSearchCmp(char *key, CMNamePtr a)
{
	register char *kcp, *cp;
	int d;

	for (cp = (*a).name, kcp = key; ; ) {
		if (*kcp == 0)
			return 0;
		d = *kcp++ - *cp++;
		if (d)
			return d;
	}
}	/* CMSubSearchCmp */


/* This returns a pointer to a CAName, if the name supplied was long
 * enough to be a unique name.  We return a 0 CANamePtr if we did not
 * find any matches, a -1 CANamePtr if we found more than one match,
 * or the unique CANamePtr.
 */
CMNamePtr GetCommandOrMacro(char *name, int wantExactMatch)
{
	CMNamePtr canp, canp2;

	/* First check for an exact match.  Otherwise if you if asked for
	 * 'cd', it would match both 'cd' and 'cdup' and return an
	 * ambiguous name error, despite having the exact name for 'cd.'
	 */
	canp = (CMNamePtr) BSEARCH(name, gNameList, gNumNames, sizeof(CMName), CMExactSearchCmp);

	if (canp == kNoName && !wantExactMatch) {
		/* Now see if the user typed an abbreviation unique enough
		 * to match only one name in the list.
		 */
		canp = (CMNamePtr) BSEARCH(name, gNameList, gNumNames, sizeof(CMName), CMSubSearchCmp);
		
		if (canp != kNoName) {
			/* Check the entry above us and see if the name we're looking
			 * for would match that, too.
			 */
			if (canp != &gNameList[0]) {
				canp2 = canp - 1;
				if (CMSubSearchCmp(name, canp2) == 0)
					return kAmbiguousName;
			}
			/* Check the entry below us and see if the name we're looking
			 * for would match that one.
			 */
			if (canp != &gNameList[gNumNames - 1]) {
				canp2 = canp + 1;
				if (CMSubSearchCmp(name, canp2) == 0)
					return kAmbiguousName;
			}
		}
	}
	return canp;
}									   /* GetCommandOrMacro */




/* Print the help string for the command specified. */
void PrintCmdHelp(CommandPtr c)
{
	PrintF("%s: %s.\n",
				  c->name,
				  c->help
	);
}									   /* PrintCmdHelp */




/* Print the usage string for the command specified. */
void PrintCmdUsage(CommandPtr c)
{
	if (c->usage != NULL)
		PrintF("Usage: %s %s\n",
					  c->name,
					  c->usage
		);
}									   /* PrintCmdUsage */




/*ARGSUSED*/
static void 
SigPipeExecCmd(/* int sigNumUNUSED */ void)
{
	DebugMsg("\n*Broken Pipe*\n");
	SIGNAL(SIGPIPE, SigPipeExecCmd);
}	/* SigPipeExecCmd */



/*ARGSUSED*/
static void 
SigIntExecCmd(/* int sigNumUNUSED */ void)
{
	EPrintF("\n*Command Interrupted*\n");
	SIGNAL(SIGINT, SigIntExecCmd);
	alarm(0);
	longjmp(gCommandJmp, 1);
}	/* SigIntExecCmd */




/*ARGSUSED*/
static void
SigIntCmdLoop(/* int sigNumUNUSED */ void)
{
	EPrintF("\n*Interrupt*\n");
	SIGNAL(SIGINT, SigIntCmdLoop);
	alarm(0);
	longjmp(gCmdLoopJmp, 1);
}	/* SigIntCmdLoop */




/* Looks for only a command (and not macros) in the name list.
 * If 'wantExactMatch' is zero, then you can 'name' can be
 * a unique abbreviation.
 */
CommandPtr GetCommand(char *name, int wantExactMatch)
{
	CMNamePtr cm;

	cm = GetCommandOrMacro(name, wantExactMatch);
	if ((cm == kAmbiguousName) || (cm == kNoName)) 
		return ((CommandPtr) NULL);
	else if (!cm->isCmd)
		return ((CommandPtr) NULL);
	return (cm->u.cmd);
}	/* GetCommand */




/* Given an entire command line string, parse it up and run it. */
int ExecCommandLine(char *cmdline)
{
	volatile CommandPtr c;
	volatile CMNamePtr cm;
	volatile int err;
	FILE *volatile pipefp;
	volatile CmdLineInfoPtr clp;
	VSig_t si, sp;
	static int depth = 0;
	string str;

	sp = (VSig_t) kNoSignalHandler;
	si = (VSig_t) kNoSignalHandler;
	pipefp = NULL;

	if (++depth > kRecursionLimit) {
		err = -1;
		Error(kDontPerror,
			"Recursion limit reached.   Did you run a recursive macro?\n");
		goto done;
	}

	/*
	 * First alloc a bunch of space we'll need.  We have to do it each time
	 * through unfortunately, because it is possible for ExecCommandLine to
	 * be called recursively.
	 */
	MCHK;
	clp = (volatile CmdLineInfoPtr) NEWCMDLINEINFOPTR;

	if (clp == (volatile CmdLineInfoPtr) 0) {
		Error(kDontPerror, "Not enough memory to parse command line.\n");
		err = -1;
		goto done;
	}

	/* Create the argv[] list and other such stuff. */
	err = (volatile int) MakeArgVector(cmdline, (CmdLineInfoPtr) clp);

	DebugMsg("%s\n", cmdline);
	if (err) {
		/* If err was non-zero, MakeArgVector failed and returned
		 * an error message for us.
		 */
		Error(kDontPerror, "Syntax error: %s.\n", ((CmdLineInfoPtr) clp)->errStr);
		err = -1;
	} else if (((CmdLineInfoPtr) clp)->argCount != 0) {
		err = 0;
		cm = (volatile CMNamePtr)
			GetCommandOrMacro(((CmdLineInfoPtr) clp)->argVector[0], kAbbreviatedMatchAllowed);
		if (cm == (volatile CMNamePtr) kAmbiguousName) {
			Error(kDontPerror, "Ambiguous command or macro name.\n");
			err = -1;
		} else if (cm == (volatile CMNamePtr) kNoName) {
			/* Try implicit cd.  Of course we need to be connected
			 * to do this.
			 */
			if (gRmtInfo.isUnix) {
				/* Some servers have a "feature" that also tries other
				 * than the current directory with CWD.
				 */
				str[0] = '\0';
				if (((CmdLineInfoPtr) clp)->argVector[0][0] != '/') {
					/* Try to use the absolute path if at all possible. */
					STRNCPY(str, gRemoteCWD);
					STRNCAT(str, "/");
				}
				STRNCAT(str, ((CmdLineInfoPtr) clp)->argVector[0]);
			} else {
				STRNCPY(str, ((CmdLineInfoPtr) clp)->argVector[0]);
			}
			if (!gConnected || (TryQuietChdir(str) < 0)) {
				Error(kDontPerror, "Invalid command.\n");
				err = -1;
			}
		} else {
			/* We have something we can run. */
			if (!((CMNamePtr) cm)->isCmd) {
				/* Do a macro. */
				ExecuteMacro(((CMNamePtr) cm)->u.mac, ((CmdLineInfoPtr) clp)->argCount, ((CmdLineInfoPtr) clp)->argVector);
			} else {
				/* Sorry about all these bloody casts, but people complain
				 * to me about compiler warnings.
				 */

				/* We have a command. */
				c = (volatile CommandPtr) ((CMNamePtr) cm)->u.cmd;
				if ((((CommandPtr) c)->maxargs != kNoMax) && ((((CmdLineInfoPtr) clp)->argCount - 1) > ((CommandPtr) c)->maxargs)) {
					PrintCmdUsage((CommandPtr) c);
					err = -1;
				} else if ((((CommandPtr) c)->minargs != kNoMax) && ((((CmdLineInfoPtr) clp)->argCount - 1) < ((CommandPtr) c)->minargs)) {
					PrintCmdUsage((CommandPtr) c);
					err = -1;
				} else if (((((CommandPtr) c)->flags & kCmdMustBeConnected) != 0) && (gConnected == 0)) {
					Error(kDontPerror, "Not connected.\n");
					err = -1;
				} else if (((((CommandPtr) c)->flags & kCmdMustBeDisconnected) != 0) && (gConnected == 1)) {
					Error(kDontPerror, "You must close the connection first before doing this command.\n");
					err = -1;
				} else {
					if ((((CommandPtr) c)->flags & kCmdWaitMsg) != 0) {
						SetBar(NULL, "RUNNING", NULL, -1, 1);
					}
					/* Run our command finally. */
					if (setjmp(gCommandJmp)) {
						/* Command was interrupted. */
						(void) SIGNAL(SIGINT, SIG_IGN);
						(void) SIGNAL(SIGPIPE, SIG_IGN);
					} else {
						si = (volatile Sig_t) SIGNAL(SIGINT, SIG_IGN);
						sp = (volatile Sig_t) SIGNAL(SIGPIPE, SigPipeExecCmd);
				
						/* Make a copy of the real stdout stream, so we can restore
						 * it after the command finishes.
						 */
						((CmdLineInfoPtr) clp)->savedStdout = gStdout;
						((CmdLineInfoPtr) clp)->outFile = -1;
						
						/* Don't > or | if we this command is the shell
						 * command (!), or any other command that specifies
						 * the kCmdNoRedirect flag.
						 */
						if (!(((CommandPtr) c)->flags & kCmdNoRedirect)) {
							/* Open the output file if the user supplied one.
							 * This file can be something being redirected into,
							 * such as ">outfile" or a file to pipe output into,
							 * such as "| wc."
							 */
							if (*((CmdLineInfoPtr) clp)->outFileName) {
								if (!((CmdLineInfoPtr) clp)->isAppend)
									((CmdLineInfoPtr) clp)->outFile = open(((CmdLineInfoPtr) clp)->outFileName,
										O_WRONLY | O_TRUNC | O_CREAT,
										S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
								else
									((CmdLineInfoPtr) clp)->outFile = open(((CmdLineInfoPtr) clp)->outFileName,
										O_WRONLY | O_APPEND | O_CREAT);

								if (((CmdLineInfoPtr) clp)->outFile == -1) {
									Error(kDoPerror, "Could not open %s for writing.\n", ((CmdLineInfoPtr) clp)->outFileName);
									err = -1;
									goto done;
								}
							} else if ((*((CmdLineInfoPtr) clp)->pipeCmdLine) && !(((CommandPtr) c)->flags & kCmdDelayPipe)) {

								DebugMsg("|: '%s'\n", ((CmdLineInfoPtr) clp)->pipeCmdLine);
								pipefp = (FILE *) POpen(((CmdLineInfoPtr) clp)->pipeCmdLine, "w", 0);
								if (pipefp == NULL) {
									Error(kDoPerror, "Could not pipe out to: %s\n", ((CmdLineInfoPtr) clp)->pipeCmdLine);
									err = -1;
									goto done;
								}
								((CmdLineInfoPtr) clp)->outFile = fileno((FILE *) pipefp);
							}
						}
						if (((CmdLineInfoPtr) clp)->outFile != -1) {
							/* Replace stdout with the FILE pointer we want to
							 * write to, so things like PrintF will print to
							 * the file instead of the screen.
							 */
							gStdout = ((CmdLineInfoPtr) clp)->outFile;
						}
						(void) SIGNAL(SIGINT, SigIntExecCmd);
						err = (*((CommandPtr) c)->proc) (((CmdLineInfoPtr) clp)->argCount, ((CmdLineInfoPtr) clp)->argVector);
						(void) SIGNAL(SIGINT, SIG_IGN);
						(void) SIGNAL(SIGPIPE, SIG_IGN);
						if (err == kUsageErr)
							PrintCmdUsage((CommandPtr) c);
					}
		
					/* We will clean up the mess now.  The command itself may
					 * have opened a pipe and done the initial setup, but
					 * it still depends on us to close everything.
					 */
					if (((CmdLineInfoPtr) clp)->outFile != -1) {
						/* We've run the command, and now it's time to cleanup
						 * our mess.  We close the output file we were writing
						 * to, and replace the stdout variable with the real
						 * stdout stream.
						 */
						gStdout = ((CmdLineInfoPtr) clp)->savedStdout;
						if (*((CmdLineInfoPtr) clp)->outFileName)
							(void) close(((CmdLineInfoPtr) clp)->outFile);
						else if (pipefp != NULL)
							(void) pclose((FILE *) pipefp);
					}
					/* End if we tried to run a command. */
				}
				/* End if we had a command. */
			}
			/* End if we had a macro or command to try. */
		}
		/* End if we had atleast one argument. */
	}
done:
	--depth;
	if (clp != (volatile CmdLineInfoPtr) 0)
		free(clp);
	if (si != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGINT, si);
	if (sp != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGPIPE, sp);
	return err;
}									   /* ExecCommandLine */




/* Look for commands in the FILE pointer supplied, running each
 * one in turn.
 */
void RunScript(FILE *fp)
{
	char line[256];

	while (!gDoneApplication) {
		if (FGets(line, sizeof(line), fp) == NULL)
			break;	/* Done. */
		ExecCommandLine(line);
	}
}	/* RunScript */




void RunStartupScript(void)
{
	FILE *fp;
	longstring path;
	
	(void) OurDirectoryPath(path, sizeof(path), kStartupScript);
	fp = fopen(path, "r");
	if (fp != NULL) {
		RunScript(fp);
		fclose(fp);
	}
}	/* RunStartupScript */




void CommandShell(void)
{
	char line[256];
	time_t cmdStartTime, cmdStopTime;

	/* We now set gEventNumber to 1, meaning that we have entered the
	 * interactive command shell.  Some commands check gEventNumber
	 * against 0, meaning that we hadn't entered the shell yet.
	 */
	gEventNumber = 1L;

	if (setjmp(gCmdLoopJmp)) {
		/* Interrupted. */
		++gNumInterruptions;
		if (gNumInterruptions == 3)
			EPrintF("(Interrupt the program again to kill program.)\n");
		else if (gNumInterruptions > 3)
			gDoneApplication = 1;
		DebugMsg("\nReturning to top level.\n");
	}

	while (!gDoneApplication) {
		(void) SIGNAL(SIGPIPE, SIG_IGN);
		(void) SIGNAL(SIGINT, SigIntCmdLoop);

		/* If the user logged out and left us in the background,
		 * quit, unless a script is running.
		 */
		if (!gDoingScript && !UserLoggedIn())
			break;		/* User hung up. */
		(void) CheckNewMail();
		if (Gets(line, sizeof(line)) == NULL)
			break;	/* Done. */
		if (gWinInit) {
			if (gBlankLines)
				PrintF("\n");
			BoldPrintF("> %s\n", line);
			if (gBlankLines)
				PrintF("\n");
		}
		FlushListWindow();

		time(&cmdStartTime);
		ExecCommandLine(line);
		time(&cmdStopTime);
		if ((int) (cmdStopTime - cmdStartTime) > kBeepAfterCmdTime)
			Beep(1);

		++gEventNumber;
	}
}	/* CommandShell */
