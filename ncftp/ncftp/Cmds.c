/* Cmds.c */

#include "Sys.h"

#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "RCmd.h"
#include "Cmds.h"
#include "Cmdline.h"
#include "List.h"
#include "MakeArgv.h"
#include "Macro.h"
#include "Main.h"
#include "DateSize.h"
#include "Open.h"
#include "Glob.h"
#include "Getopt.h"
#include "FTP.h"
#include "Bookmark.h"
#include "Cpp.h"
#include "Prefs.h"
#include "Tips.h"
#include "Version.h"

/* Full path of the remote current working directory. */
longstring gRemoteCWD = "";

/* The full path of the previous remote working directory. */
longstring gPrevRemoteCWD = "";

/* Full path of the local current working directory. */
longstring gLocalCWD = "";

/* Full path of the previous local working directory. */
longstring gPrevLocalCWD = "";

/* This is the type we use for file transfers.  Note that we always use
 * type ascii for directory listings.
 */
int gTransferType = 'I';

/* This what type is in use at the moment. */
int gCurType;

/* Upon receipt of a signal during paging a local file, we jump here. */
jmp_buf gShellJmp;

/* Flag for debugging mode. */
#if (kAlpha > 0) || (kBeta > 0)
int gDebug = kDebuggingOff;
int gTrace = kTracingOn;
#else
int gDebug = kDebuggingOff;
int gTrace = kTracingOff;
#endif

extern int gNumCommands;
extern Command gCommands[];
extern int gVerbosity, gMode;
extern Bookmark gRmtInfo;
extern UserInfo gUserInfo;
extern CppSymbol gCppSymbols[];
extern int gNumCppSymbols;
extern string gVersion;
extern longstring gPager;
extern int gDoneApplication, gConnected;
extern FILE *gTraceLogFile;
extern int gStdout;
extern MacroNodePtr gFirstMacro;
extern int gNumGlobalMacros, gOtherSessionRunning;
extern char *gOptArg;
extern int gOptInd;
extern struct hostent *GetHostEntry(char *host, struct in_addr *ip_address);
void GetRemoteCWD(char *cdstub, ResponsePtr cwdrp);

/* Runs the "SYST" command, and if the remote host supports it, will return
 * the system type we're connected to, otherwise an empty string.  This is
 * handy to see if we're connected to a UNIX box, or something icky, like
 * MS/DOS, or even more icky, VMS.
 */
int DoSystem(char *systType, size_t siz)
{
	ResponsePtr rp;
	int result;

	rp = InitResponse();
	result = RCmd(rp, "SYST");
	rp->printMode = kDontPrint;
	Strncpy(systType, rp->msg.first->line, siz);
	DoneWithResponse(rp);
	if (result != 2) {
		systType[0] = '\0';
		return (-1);
	}
	return (0);
}	/* DoSystem */




/*ARGSUSED*/
static void SigLocalPage(/* int sigNum */ void)
{
	alarm(0);
	longjmp(gShellJmp, 1);
}	/* SigLocalPage */




int LocalPageCmd(int argc0, char **argv0)
{
	FILE *volatile fp;
	FILE *volatile pager;
	volatile int i;
	volatile int errs;
	int opt;
	longstring pageCmd;
	volatile LineList globFiles;
	volatile LinePtr globFile;
	volatile int useBuiltIn;
	volatile Sig_t si, sp;
	volatile int argc;
	char **volatile argv;
	string str;

	argv = argv0;
	argc = argc0;
	GetoptReset();
	useBuiltIn = 0;
	while ((opt = Getopt(argc, argv, "bp")) >= 0) {
		switch (opt) {
			case 'b':
				useBuiltIn = 1;
				break;
			case 'p':
				useBuiltIn = 0;
				break;
			default:
				return (kUsageErr);
		}
	}
	argv += gOptInd;
	argc -= gOptInd;

	si = SIGNAL(SIGINT, SigLocalPage);
	sp = SIGNAL(SIGPIPE, SigLocalPage);
	errs = 0;
	
	if (useBuiltIn == 0) {
		if (gPager[0] == '\0') {
			EPrintF("You haven't specified a program to use as a pager.\n");
			EPrintF("You can set this from the preferences screen (prefs command).\n");
			errs = -1;
			goto done;
		}
		SaveScreen();
	}
	
	for (i=0; i<argc; i++) {
		InitLineList((LineList *) &globFiles);
		LocalGlob((LineList *) &globFiles, argv[i]);
		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			fp = fopen(globFile->line, "r");
			if (fp == NULL) {
				Error(kDoPerror, "Can't open %s.\n", globFile->line);
				--errs;
			} else if (useBuiltIn == 1) {
				MultiLineInit();
				MultiLinePrintF("*** %s ***\n", globFile->line);
				if (setjmp(gShellJmp) != 0) {
					/* Command was interrupted. */
					(void) SIGNAL(SIGINT, SIG_IGN);
					fclose((FILE *) fp);
					DisposeLineListContents((LineList *) &globFiles);
					--errs;
					goto done;
				} else {
					while (fgets(str, ((int) sizeof(str)) - 1, (FILE *)fp) != NULL)
						MultiLinePrintF("%s", str);
				}
				(void) SIGNAL(SIGINT, SIG_IGN);
				fclose((FILE *) fp);
			} else {
				STRNCPY(pageCmd, gPager);
				STRNCAT(pageCmd, " ");
				STRNCAT(pageCmd, globFile->line);
				pager = NULL;
				if (setjmp(gShellJmp) != 0) {
					/* Command was interrupted. */
					(void) SIGNAL(SIGINT, SIG_IGN);
					(void) SIGNAL(SIGPIPE, SIG_IGN);
					if (pager != ((volatile FILE *) 0))
						PClose((FILE *) pager);
					fclose((FILE *) fp);
					DisposeLineListContents((LineList *) &globFiles);
					--errs;
					goto done;
				} else {
					pager = POpen(pageCmd, "w", 1);
					while (fgets(str, ((int) sizeof(str)) - 1, (FILE *)fp) != NULL)
						fputs(str, (FILE *) pager);
					PClose((FILE *) pager);
					fclose((FILE *) fp);
				}
			}
		}
		DisposeLineListContents((LineList *) &globFiles);
	}

done:
	if (useBuiltIn == 0)
		RestoreScreen(1);
	(void) SIGNAL(SIGINT, si);
	(void) SIGNAL(SIGPIPE, sp);
	Beep(0);	/* User should be aware that it took a while, so no beep. */
	return (errs);
}	/* LocalPageCmd */




/* Returns the full path of the remote working directory. */
void GetRemoteCWD(char *cdstub, ResponsePtr cwdrp)
{
	ResponsePtr rp;
	char *l, *r;
	char *cp1;

	/* Some servers like NcFTPd return the new working
	 * directory in the response.  When you call this
	 * function you can optionally pass the result
	 * of a previous CWD or CDUP command, and see
	 * if we can parse out the new directory without
	 * doing a PWD command.
	 */
	if (cwdrp != NULL) {
		/* "xxxx" is new cwd.
		 * Strip out just the xxxx to copy into the remote cwd.
		 */
		l = strchr(cwdrp->msg.first->line, '"');
		r = strrchr(cwdrp->msg.first->line, '"');
		if ((r != NULL) && (l != NULL) && (l != r) && (STRNEQ(r, "\" is ", 5))) {
			*r = '\0';
			++l;
			STRNCPY(gRemoteCWD, l);
			*r = '"';	/* Restore, so response prints correctly. */
			SetScreenInfo();
			return;
		}
	}

	rp = InitResponse();
	if (RCmd(rp, "PWD") == 2) {
		if ((r = strrchr(rp->msg.first->line, '"')) != NULL) {
			/* "xxxx" is current directory.
			 * Strip out just the xxxx to copy into the remote cwd.
			 */
			l = strchr(rp->msg.first->line, '"');
			if ((l != NULL) && (l != r)) {
				*r = '\0';
				++l;
				STRNCPY(gRemoteCWD, l);
				*r = '"';	/* Restore, so response prints correctly. */
			}
		} else {
			/* xxxx is current directory.
			 * Mostly for VMS.
			 */
			if ((r = strchr(rp->msg.first->line, ' ')) != NULL) {
				*r = '\0';
				STRNCPY(gRemoteCWD, (rp->msg.first->line));
				*r = ' ';	/* Restore, so response prints correctly. */
			}
		}
		SetScreenInfo();
	} else {
		/* Error. */
		if (cdstub != kDidNotChdir) {
			/* We couldn't PWD.  This could happen if we chdir'd to a
			 * directory that looked like d--x--x--x.  We could cd there,
			 * but not read the contents.
			 *
			 * What we can do, since we know we just tried cd'ing here from
			 * a previous directory is fake it and just append the path
			 * we tried to cd at after the previous CWD.
			 */
			if (*cdstub == '/') {
				/* Just cd'd using an absolute path. */
				STRNCPY(gRemoteCWD, cdstub);
			} else {
				/* If "cd .." , remove the lowest directory.
				 * If "cd ." , do nothing.
				 * Don't append the slash if previous directory was
				 * the root.
				 */
				cp1 = strrchr(gRemoteCWD, '/');
				if (STREQ(cdstub, "..") && !STREQ(gRemoteCWD, "/")
					&& (cp1 != NULL))
					*cp1 = '\0';
				else if (STREQ(cdstub, ".")); /* do nothing */
				else {
					if (! STREQ(gRemoteCWD, "/"))
						STRNCAT(gRemoteCWD, "/");
					STRNCAT(gRemoteCWD, cdstub);
				}
			}
			SetScreenInfo();
		}
	}
	DoneWithResponse(rp);
}	/* GetRemoteCWD */




int LocalPwdCmd(void)
{
	(void) GetCWD(gLocalCWD, sizeof(gLocalCWD));
	PrintF("Current local directory is %s.\n", gLocalCWD);
	return 0;
}	/* LocalPwdCmd */




int PwdCmd(void)
{
	GetRemoteCWD(kDidNotChdir, NULL);
	PrintF("Current remote directory: %s\n", gRemoteCWD);
	return 0;
}	/* PwdCmd */




/* If the remote host supports the MDTM command, we can find out the exact
 * modification date of a remote file.
 */
int DoMdtm(char *fName, time_t *mdtm)
{
	ResponsePtr rp;
	int result;

	*mdtm = kModTimeUnknown;
	result = -1;
	/* Don't bother if we know the current host doesn't support it.
	 * We must make sure that the gRmtInfo is properly set each
	 * time a host is opened.
	 */
	if (gRmtInfo.hasMDTM) {
		rp = InitResponse();
		rp->printMode = kDontPrint;
		if (RCmd(rp, "MDTM %s", fName) == 2) {
			/* Reply should look like "213 19930602204445\n" so we will have
			 * "19930602204445" in the first line of the reply string list.
			 */
			*mdtm = UnMDTMDate(rp->msg.first->line);
			result = 0;
		} else if (UNIMPLEMENTED_CMD(rp->code))
			gRmtInfo.hasMDTM = 0;	/* Command not supported. */
		DoneWithResponse(rp);
	}
	return (result);
}	/* DoMdtm */




/* If the remote host supports the SIZE command, we can find out the exact
 * size of a remote file, depending on the transfer type in use.  SIZE
 * returns different values for ascii and binary modes!
 */
int DoSize(char *fName, long *size)
{
	ResponsePtr rp;
	int result;
	
	*size = kSizeUnknown;
	result = -1;
	/* Don't bother if we know the current host doesn't support it.
	 * We must make sure that the gRmtInfo is properly set each
	 * time a host is opened.
	 */
	if (gRmtInfo.hasSIZE) {
		rp = InitResponse();
		rp->printMode = kDontPrint;
		if (RCmd(rp, "SIZE %s", fName) == 2) {
			sscanf(rp->msg.first->line, "%ld", size);
			result = 0;
		} else if (UNIMPLEMENTED_CMD(rp->code))
			gRmtInfo.hasSIZE = 0;	/* Command not supported. */
		DoneWithResponse(rp);
	}
	return (result);
}	/* DoSize */




/* See if we can cd to the dir requested, and if not, that's okay. */
int TryQuietChdir(char *dir)
{
	int result;
	ResponsePtr rp;

	rp = InitResponse();
	rp->printMode = kDontPrint;
	if (STREQ(dir, ".."))
		result = RCmd(rp, "CDUP");
	else {
		if (*dir == '\0')
			dir = "/";
		result = RCmd(rp, "CWD %s", dir);
	}
	if (result == 2) {
		GetRemoteCWD(dir, rp);
		DoneWithResponse(rp);
		return (0);
	}
	DoneWithResponse(rp);
	return (-1);
}	/* TryQuietChdir */




/* Attempt to cd to the directory specifed, reporting an error if we
 * failed (or maybe not, depending on the verbosity level in use).
 */
int DoChdir(char *dir)
{
	int result;
	ResponsePtr rp;

	rp = InitResponse();
	if (STREQ(dir, ".."))
		result = RCmd(rp, "CDUP");
	else {
		if (*dir == '\0')
			dir = "/";
		result = RCmd(rp, "CWD %s", dir);
	}
	GetRemoteCWD(dir, rp);
	DoneWithResponse(rp);
	return (result != 2 ? -1 : 0);
}	/* DoChdir */




int ChdirCmd(int argcUNUSED, char **argv)
{
	LineList globFiles;
	longstring str;
	char *cddir;
	int rglobbed;
	int result;

#if 0
	/* Can't glob a directory name without a major hassle.
	 * We could do a "NLST -d dir*pattern" but most servers have that
	 * damned NLST/-flags/glob-pattern conflict which prevents that.
	 *
	 * We could just do a "NLST dir*pattern" but then that gives us back
	 * entire directory listings, like "dir/file1 dir/file2..." which 
	 * could get large and too wasteful of net bandwidth just because
	 * the user is too lazy to type a directory name.
	 *
	 * We could try a "LIST -d dir*pattern" but then we'd have to parse
	 * a big line of junk.  This may be done sometime later.
	 *
	 * For now, I just don't support globbing wth cd.
	 */
	cddir = argv[1];
	rglobbed = 0;
#else
	InitLineList(&globFiles);
	if (gRmtInfo.isUnix == 0) {
		/* Don't try to glob the directory name unless server is UNIX.
		 * This won't work on VMS, for example.
		 */
		cddir = argv[1];
		rglobbed = 0;
	} else {
		RemoteGlob(&globFiles, argv[1], kListNoFlags);
		if (globFiles.first == NULL) {
			EPrintF("%s: no match.\n", argv[1]);
			DisposeLineListContents(&globFiles);
			return (kCmdErr);
		} else if (globFiles.first->next != NULL) {
			EPrintF("%s: wildcard matches more than one remote item.\n", argv[1]);
			DisposeLineListContents(&globFiles);
			return (kCmdErr);
		} else {
			rglobbed = 1;
			cddir = globFiles.first->line;
		}
	}
#endif

	/* Steal the Korn shell's "cd -" trick, which cd's you to the
	 * directory you were in before.
	 */
	STRNCPY(str, gRemoteCWD);
	if (STREQ(cddir, "-") && (gPrevRemoteCWD[0] != '\0')) {
		result = DoChdir(gPrevRemoteCWD);	/* Sets gRemoteCWD. */
	} else
		result = DoChdir(cddir);
	if (result == 0)
		STRNCPY(gPrevRemoteCWD, str);

	if (rglobbed != 0)
		DisposeLineListContents(&globFiles);
	return (result);
}	/* ChdirCmd */




/* cd to 'dir' on the local host.  The dir specified may have glob
 * characters, or ~stuff in it also.
 */
int DoLocalChdir(char *dir, int quiet)
{
	int result;
	LineList globFiles;

	InitLineList(&globFiles);
	LocalGlob(&globFiles, dir);
	if ((globFiles.first == NULL) || ((dir = globFiles.first->line) == NULL)) {
		Error(kDontPerror, "No match.\n");
		result = kCmdErr;
	} else if (globFiles.first->next != NULL) {
		Error(kDontPerror, "Ambiguous directory name %s.\n", dir);
		result = kCmdErr;
	} else if ((result = chdir(dir)) < 0) {
		Error(kDoPerror, "Could not change local directory to %s.\n", dir);
	} else {
		(void) GetCWD(gLocalCWD, sizeof(gLocalCWD));
		if (!quiet)
			PrintF("Current local directory is %s.\n", gLocalCWD);
	}
	DisposeLineListContents(&globFiles);
	return (result);
}	/* DoLocalChdir */



/* Stub command that lcd's to the appropriate directory, or if none
 * was supplied, carry on the tradition and make that the same as
 * lcd'ing to the home directory.
 */
int LocalChdirCmd(int argc, char **argv)
{
	int result;
	char *cp;
	longstring str;

	if (argc < 2)
		cp = gUserInfo.home;
	else if (STREQ(argv[1], "-") && (gPrevLocalCWD[0] != '\0'))
		cp = gPrevLocalCWD;
	else
		cp = argv[1];
	STRNCPY(str, gLocalCWD);
	result = DoLocalChdir(cp, 0);	/* Sets gRemoteCWD. */
	if (result == 0)
		STRNCPY(gPrevLocalCWD, str);
	return (result);
}	/* LocalChdirCmd */




/* Changes the debugging status, or prints some extra debugging
 * info depending on the parameter given.
 */
int DebugCmd(int argc, char **argv)
{
	char *cp;
	int i;

	if (argc == 1) {
		PrintF("Debug Mode = %d.  Trace Mode = %d.\n", gDebug, gTrace);
	} else {
#if (LIBMALLOC == FAST_MALLOC)
		if (STREQ(argv[1], "memchk")) {
			struct mallinfo mi;
		
			mi = mallinfo();
			PrintF("\
total space in arena:               %d\n\
number of ordinary blocks:          %d\n\
number of small blocks:             %d\n\
number of holding blocks:           %d\n\
space in holding block headers:     %d\n\
space in small blocks in use:       %d\n\
space in free small blocks:         %d\n\
space in ordinary blocks in use:    %d\n\
space in free ordinary blocks:      %d\n\
cost of enabling keep option:       %d\n",
				mi.arena,
				mi.ordblks,
				mi.smblks,
				mi.hblks,
				mi.hblkhd,
				mi.usmblks,
				mi.fsmblks,
				mi.uordblks,
				mi.fordblks,
				mi.keepcost
			);
			return 0;
		}
#endif
#if (LIBMALLOC == DEBUG_MALLOC)
		if (STREQ(argv[1], "memchk")) {
			PrintF("malloc_chain_check: %d\n\n", malloc_chain_check(0));
			PrintF("malloc_inuse: %lu\n", malloc_inuse(NULL));
			return 0;
		}
		if (STREQ(argv[1], "memdump")) {
			malloc_dump(1);
			return 0;
		}
#endif
		for (cp = argv[1]; (*cp != '\0') && isdigit(*cp); )
			++cp;
		if (*cp == '\0') {
			gDebug = atoi(argv[1]);
			return 0;
		} else if (ISTREQ(argv[1], "macro")) {
			/* Dump specified macro, or if NULL, all of them. */
			DumpMacro(argv[2]);
		} else if (ISTREQ(argv[1], "segv")) {
			/* Intentionally bomb the program... */
			*((int *) 0) = 99;
		} else if (ISTREQ(argv[1], "multi")) {
			MultiLineInit();
			for (i=1; i<=60; i++)
				MultiLinePrintF("This is line %d.\n", i);
		} else if (ISTREQ(argv[1], "trace")) {
			if (argc > 2)
				gTrace = atoi(argv[2]);
			else
				gTrace = !gTrace;
			if (gTrace) {
				if (gTraceLogFile == NULL)
					OpenTraceLog();
			} else {
				if (gTraceLogFile != NULL)
					CloseTraceLog();
			}
		} else if (ISTREQ(argv[1], "tips")) {
			/* Dump all the tips. */
			PrintAllTips();
		}
	}
	return 0;
}	/* DebugCmd */




/* Sets the verbosity level of our informational messages. */
int VerboseCmd(int argc, char **argv)
{
	int newVerbose;

	if (argc == 1)
		PrintF("Verbosity = %d.\n", gVerbosity);
	else {
		newVerbose = atoi(argv[1]);
		if (newVerbose < kQuiet)
			newVerbose = kQuiet;
		else if (newVerbose > kVerbose)
			newVerbose = kVerbose;
		(void) SetVerbose(newVerbose);
	}
	return 0;
}	/* VerboseCmd */



/* Sets the data transfer mode to the one specified, if needed, and returns
 * the mode it used or -1 upon failure.  The 'mode' parameter must be
 * an uppercase letter.
 */
int SetMode(int mode)
{
	ResponsePtr rp;
	int result = -1;

	if (mode == gMode) {
		/* Already on this mode, so don't waste network bandwidth. */
		result = 0;
	} else if ((mode == 'S') || (mode == 'B')) {
		rp = InitResponse();
		RCmd(rp, "MODE %c", mode);
		if (rp->codeType == 2) {
			result = 0;
			gRmtInfo.xferMode = gMode = mode;
		}
		DoneWithResponse(rp);
	}
	return (result);
}	/* SetMode */



/* Sets the data transfer type to the one specified, if needed, and returns
 * the type it used or -1 upon failure.  The 'type' parameter must be
 * an uppercase letter.
 */
int SetType(int type)
{
	ResponsePtr rp;
	
	int result = -1;

	if (type == 'B')
		type = 'I';

	if (type == gCurType) {
		/* Already on this type, so don't waste network bandwidth. */
		result = type;
	} else if ((type == 'A') || (type == 'I') || (type == 'T')) {
		rp = InitResponse();
		RCmd(rp, "TYPE %c", type);
		if (rp->codeType == 2) {
			result = 0;
			gCurType = type;
		}
		DoneWithResponse(rp);
	}
	return (result);
}	/* SetType */




void DoType(char *typestr)
{
	int type;

	type = *typestr;
	if (islower(type))
		type = toupper(type);
	
	if (SetType(type) < 0)
		PrintF("Unknown type '%s'\n", typestr);
	else {
		gTransferType = gCurType;
		/* We only "remember" this type for next time, if the user
		 * explicitly issued a type command.
		 */
		gRmtInfo.xferType = gTransferType;
	}
}	/* DoType */




int TypeCmd(int argc, char **argv)
{
	if ((argc == 1) && (argv[0][0] != 't')) {
		/* Check for aliased commands binary and ascii, which come here. */
		DoType(argv[0]);
	} if (argc > 1) {
		DoType(argv[1]);
	} else {
		PrintF("Transfer type is %c.\n", gTransferType);
	}
	return 0;
}	/* TypeCmd */



int ModeCmd(int argc, char **argv)
{
	int c;

	if (argc > 1) {
		c = (int) argv[1][0];
		switch(c) {
			case 's':
			case 'S':
				SetMode('S');
				break;
			case 'b':
			case 'B':
				SetMode('B');
				break;
			default:
				EPrintF("Only supported FTP transfer modes are \"stream\" and \"block.\"\n");
		}
	} else {
		PrintF("Transfer mode is %c.\n", gMode);
	}
	return 0;
}	/* ModeCmd */



void DoQuit(int exitStatus)
{
	/* Only do this once, in case we get caught with infinite recursion. */
	if (++gDoneApplication <= 1) {
		if (gConnected)
			DoClose(1);
		(void) RunPrefixedMacro("quit.", "ncftp");
		(void) RunPrefixedMacro("end.", "ncftp");
		if (gOtherSessionRunning == 0) {
			WriteBookmarkFile();
			CloseLogs();
			WritePrefs();
			SaveHistory();
		}
	}
	Exit(exitStatus);
}	/* DoQuit */



int QuitCmd(void)
{
	DoQuit(kExitNoErr);
	/*NOTREACHED*/
	return 0;
}	/* QuitCmd */




/* Prints the command list, or gives a little more detail about a
 * specified command.
 */
int HelpCmd(int argc, char **argv)
{
	CommandPtr c;
	MacroNodePtr macp;
	int showall = 0, helpall = 0;
	char *arg;
	int i, j, k, n;
	int nRows, nCols;
	int nCmds2Print;
	int screenColumns;
	int len, widestName;
	char *cp, **cmdnames, spec[16];
	CMNamePtr cm;

	MultiLineInit();
	if (argc == 2) {
		showall = (STREQ(argv[1], "showall"));
		helpall = (STREQ(argv[1], "helpall"));
	}
	if (argc == 1 || showall) {
		MultiLinePrintF("\
Commands may be abbreviated.  'help showall' shows aliases, invisible and\n\
unsupported commands.  'help <command>' gives a brief description of <command>.\n\n");

		/* First, see how many commands we will be printing to the screen.
		 * Unless 'showall' was given, we won't be printing the hidden
		 * (i.e. not very useful to the end-user) commands.
		 */
		c = gCommands;
		nCmds2Print = 0;
		for (n = 0; n < gNumCommands; c++, n++)
			if ((!iscntrl(c->name[0])) && (!(c->flags & kCmdHidden) || showall))
				nCmds2Print++;

		if ((cmdnames = (char **) malloc(sizeof(char *) * nCmds2Print)) == NULL)
			OutOfMemory();

		/* Now form the list we'll be printing, and noting what the maximum
		 * length of a command name was, so we can use that when determining
		 * how to print in columns.
		 */
		c = gCommands;
		i = 0;
		widestName = 0;
		for (n = 0; n < gNumCommands; c++, n++) {
			if ((!iscntrl(c->name[0])) && (!(c->flags & kCmdHidden) || showall)) {
				cmdnames[i++] = c->name;
				len = (int) strlen(c->name);
				if (len > widestName)
					widestName = len;
			}
		}

		if ((cp = (char *) getenv("COLUMNS")) == NULL)
			screenColumns = 80;
		else
			screenColumns = atoi(cp);

		/* Leave an extra bit of whitespace for the margins between columns. */
		widestName += 2;
		
		nCols = (screenColumns + 0) / widestName;
		nRows = nCmds2Print / nCols;
		if ((nCmds2Print % nCols) > 0)
			nRows++;

		for (i = 0; i < nRows; i++) {
			for (j = 0; j < nCols; j++) {
				k = nRows * j + i;
				if (k < nCmds2Print) {
					(void) sprintf(spec, "%%-%ds",
						(j < nCols - 1) ? widestName : widestName - 2
					);
					MultiLinePrintF(spec, cmdnames[k]);
				}
			}
			MultiLinePrintF("\n");
		}
		free(cmdnames);
		
		if (gNumGlobalMacros > 0) {
			MultiLinePrintF("\nMacros:\n\n");
			/* Now do the same for the macros. */
			if ((cmdnames = (char **) malloc(sizeof(char *) * gNumGlobalMacros)) == NULL)
				OutOfMemory();
	
			/* Form the list we'll be printing, and noting what the maximum
			 * length of a command name was, so we can use that when determining
			 * how to print in columns.
			 */
			macp = gFirstMacro;
			widestName = 0;
			for (i = 0; macp != NULL; macp = macp->next) {
					cmdnames[i++] = macp->name;
					len = (int) strlen(macp->name);
					if (len > widestName)
						widestName = len;
			}
			nCmds2Print = i;
	
			/* Leave an extra bit of whitespace for the margins between columns. */
			widestName += 2;
			
			nCols = (screenColumns + 0) / widestName;
			nRows = nCmds2Print / nCols;
			if ((nCmds2Print % nCols) > 0)
				nRows++;
	
			for (i = 0; i < nRows; i++) {
				for (j = 0; j < nCols; j++) {
					k = nRows * j + i;
					if (k < nCmds2Print) {
						(void) sprintf(spec, "%%-%ds",
							(j < nCols - 1) ? widestName : widestName - 2
						);
						MultiLinePrintF(spec, cmdnames[k]);
					}
				}
				MultiLinePrintF("\n");
			}
			free(cmdnames);
		}
	} else if (helpall) {
		/* Really intended for me, so I can debug the help strings. */
		for (c = gCommands, n = 0; n < gNumCommands; c++, n++) {
			PrintCmdHelp(c);
			PrintCmdUsage(c);
		}
	} else {
		/* For each command name specified, print its help stuff. */
		while (--argc > 0) {
			arg = *++argv;
			cm = GetCommandOrMacro(arg, kAbbreviatedMatchAllowed);
			if (cm == kAmbiguousName)
				MultiLinePrintF("\"%s:\" Ambiguous command or macro name.\n", arg);
			else if (cm == kNoName)
				MultiLinePrintF("\"%s:\" Invalid command or macro name.\n", arg);
			else if (cm->isCmd) {
				c = cm->u.cmd;
				PrintCmdHelp(c);
				PrintCmdUsage(c);
			} else {
				MultiLinePrintF("\"%s\" is a macro, so no help is available.\n", arg);
			}
		}
	}
	return 0;
}									   /* HelpCmd */



int VersionCmd(void)
{
	int i;
	longstring line;
	longstring sym;
	char num[32];
	int symsOnLine;
	int symLen;
	int lineLen;
	
	MultiLineInit();
	MultiLinePrintF("Version:       %s\n", gVersion);
	MultiLinePrintF("Author:        Mike Gleason (mgleason@NcFTP.com)\n");
	MultiLinePrintF("Archived in:   ftp://ftp.NcFTP.com/ncftp/\n");

#ifdef __DATE__
	MultiLinePrintF("Compile Date:  %s\n", __DATE__);
#endif
#ifdef MK
	MultiLinePrintF("MK: %s\n", MK);
#endif

	MultiLinePrintF("\nCompile options:\n\n");
	line[0] = '\0';
	symsOnLine = 0;
	lineLen = 0;
	for (i=0; i<gNumCppSymbols; i++) {
		STRNCPY(sym, gCppSymbols[i].name);
		if (gCppSymbols[i].symType == 0) {
			if (gCppSymbols[i].l != 1L) {
				sprintf(num, "=%ld", gCppSymbols[i].l);
				STRNCAT(sym, num);
			}
			STRNCAT(sym, "  ");
		} else {
			STRNCAT(sym, "=\"");
			STRNCAT(sym, gCppSymbols[i].s);
			STRNCAT(sym, "\"  ");
		}
		symLen = (int) strlen(sym);
		if (lineLen + symLen > 79) {
			MultiLinePrintF("%s\n", line);
			line[0] = '\0';
			symsOnLine = 0;
			lineLen = 0;
		}
		STRNCAT(line, sym);
		++symsOnLine;
		lineLen += symLen;
	}
	if (symsOnLine) {
		MultiLinePrintF("%s\n", line);
	}
	return 0;
}	/* VersionCmd */




int GenericGlobCmd(int argc, char **argv, char *cmd, int printMode)
{
	ResponsePtr rp;
	int i;
	int result, errs;
	LineList globFiles;
	LinePtr globFile;

	rp = InitResponse();
	for (i=1, errs=0; i<argc; i++) {
		InitLineList(&globFiles);
		RemoteGlob(&globFiles, argv[i], kListNoFlags);
		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			rp->printMode = printMode;
			result = RCmd(rp, "%s %s", cmd, globFile->line);
			if (result != 2) {
				--errs;
				if (UNIMPLEMENTED_CMD(rp->code)) {
					DoneWithResponse(rp);
					DisposeLineListContents(&globFiles);
					return (errs);
				}
			}
			ReInitResponse(rp);
		}
		DisposeLineListContents(&globFiles);
	}

	DoneWithResponse(rp);
	return (errs);
}	/* GenericGlobCmd */




int GenericCmd(int argc, char **argv, char *cmd, int printMode)
{
	ResponsePtr rp;
	int i;
	int result, errs;

	rp = InitResponse();
	for (i=1, errs=0; i<argc; i++) {
		rp->printMode = printMode;
		result = RCmd(rp, "%s %s", cmd, argv[i]);
		if (result != 2) {
			--errs;
			if (UNIMPLEMENTED_CMD(rp->code))
				goto done;
		}
		ReInitResponse(rp);
	}

done:
	DoneWithResponse(rp);
	return (errs);
}	/* GenericCmd */




int DeleteCmd(int argc, char **argv)
{
	return GenericGlobCmd(argc, argv, "DELE", kDefaultPrint);
}	/* DeleteCmd */




int RmdirCmd(int argc, char **argv)
{
	return GenericGlobCmd(argc, argv, "RMD", kDefaultPrint);
}	/* RmdirCmd */




int MkdirCmd(int argc, char **argv)
{
	return GenericCmd(argc, argv, "MKD", kDefaultPrint);
}	/* MkdirCmd */




int RenameCmd(int argcUNUSED, char **argv)
{
	if (RCmd(kDefaultResponse, "RNFR %s", argv[1]) == 3) {
		RCmd(kDefaultResponse, "RNTO %s", argv[2]);
	}
	return 0;
}	/* MkdirCmd */




int QuoteCmd(int argc, char **argv)
{
	longstring str;
	ResponsePtr rp;
	int i;

	str[0] = '\0';
	for (i=1; i<argc; i++) {
		if (i > 1)
			STRNCAT(str, " ");
		STRNCAT(str, argv[i]);
	}

	rp = InitResponse();
	rp->printMode = kDoPrint;
	(void) RCmd(rp, "%s%s",
		argv[0][0] == 's' ? "SITE " : "",
		str
	);
	DoneWithResponse(rp);
	return 0;
}	/* QuoteCmd */




int ClearCmd(void)
{
	UpdateScreen(1);
	return 0;
}	/* ClearCmd */




int RmtHelpCmd(int argc, char **argv)
{
	ResponsePtr rp;

	if (argc > 1)
		GenericCmd(argc, argv, "HELP", kDoPrint);
	else {
		rp = InitResponse();
		rp->printMode = kDoPrint;
		(void) RCmd(rp, "HELP");
		DoneWithResponse(rp);
	}
	return 0;
}	/* RmtHelpCmd */




int ShellCmd(int argc, char **argv)
{
	int result;
	char *volatile theShell;
	char *cmdLine;
	VSig_t si;

	si = (VSig_t) 0;
	if ((theShell = (char *) getenv("SHELL")) == NULL)
		theShell = gUserInfo.shell;
	if (theShell == NULL)
		theShell = "/bin/sh";

	SaveScreen();
	if (setjmp(gShellJmp) != 0) {
		/* Command was interrupted. */
		(void) SIGNAL(SIGINT, SIG_IGN);
		result = 1;
	} else {
		si = SIGNAL(SIGINT, SigLocalPage);
		if (argc < 2)
			result = system(theShell);
		else {
			/* We have a hack where we keep a copy of the original
			 * command line before parsing at position argc + 2.
			 */
			cmdLine = CMDLINEFROMARGS(argc, argv);
			
			/* Skip the ! and whitespace after it. */
			while ((*cmdLine == '!') || isspace(*cmdLine))
				cmdLine++;
			result = system(cmdLine);
		}
	}
	RestoreScreen(1);
	if (si != (VSig_t) 0)
		(void) SIGNAL(SIGINT, si);
	return result;
}	/* ShellCmd */




int EchoCmd(int argc, char **argv)
{
	longstring str;
	int i;
	int noNewLine = 0;

	for (i=1; i<argc; i++) {
		(void) FlagStrCopy(str, sizeof(str), argv[i]);
		/* The above writes an @ sign after the nul if we were supposed
		 * to not issue a final newline.
		 */
		noNewLine = (str[strlen(str) + 1] == '@');
		PrintF("%s%s", (i > 1 ? " " : ""), str);
	}
	if (!noNewLine)
		PrintF("\n");
	return 0;
}	/* EchoCmd */




int LookupCmd(int argc, char **argv)
{
	int i, j;
	struct hostent *hp;
	char *host, **cpp;
	struct in_addr ip_address;
	int shortMode, opt;
	char ipStr[16];

	shortMode = 1;
	
	GetoptReset();
	while ((opt = Getopt(argc, argv, "v")) >= 0) {
		if (opt == 'v')
			shortMode = 0;
		else
			return kUsageErr;
	}

	for (i=gOptInd; i<argc; i++) {
		hp = GetHostEntry((host = argv[i]), &ip_address);
		if ((i > gOptInd) && (shortMode == 0))
			PrintF("\n");
		if (hp == NULL) {
			PrintF("Unable to get information about site %s.\n", host);
		} else if (shortMode) {
			MyInetAddr(ipStr, sizeof(ipStr), hp->h_addr_list, 0);
			PrintF("%-40s %s\n", hp->h_name, ipStr);
		} else {
			PrintF("%s:\n", host);
			PrintF("    Name:     %s\n", hp->h_name);
			for (cpp = hp->h_aliases; *cpp != NULL; cpp++)
				PrintF("    Alias:    %s\n", *cpp);
			for (j = 0, cpp = hp->h_addr_list; *cpp != NULL; cpp++, ++j) {
				MyInetAddr(ipStr, sizeof(ipStr), hp->h_addr_list, j);
				PrintF("    Address:  %s\n", ipStr);	
			}
		}
	}
	return 0;
}	/* LookupCmd */




int BookmarkCmd(int argcUNUSED, char **argv)
{
	SaveBookmark(argv[1]);
	return 0;
}	/* BookmarkCmd */
