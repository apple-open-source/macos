/* Open.c */

#include "Sys.h"

#ifdef SYSLOG
#	include <syslog.h>
#endif

#include "Util.h"
#include "Open.h"
#include "GetPass.h"
#include "Cmds.h"
#include "RCmd.h"
#include "Bookmark.h"
#include "FTP.h"
#include "Get.h"
#include "Getopt.h"
#include "Macro.h"
#include "Hostwin.h"
#include "Main.h"
#include "Complete.h"

/* This is a preference setting specifying whether we use anonymous logins
 * by default (as opposed to user/passwd logins).
 */
int gAnonOpen = (UOPEN == 0);

/* A structure containing some extra information we've learned about
 * the remote host, so we don't need to find out the hard way each
 * time we do something.
 */
extern Bookmark gRmtInfo;

/* Name of the host we're connected to. */
string gHost;

/* We keep track if we've logged in to the host yet.  It's possible to
 * be connected but not authorized (authenticated, whatever) to do anything.
 */
int gLoggedIn = 0;

/* Need to know if we are trying to connect and login, so if we get
 * hangup during that, we will come back to the open without longjmp'ing
 * out.
 */
int gAttemptingConnection = 0;

/* Need to know if they did a 'open -r' so we don't use the built-in pager
 * for long site-connect messages.  Otherwise the program would stop to
 * wait for the user to hit <space> each time we connected to that site
 * which would sort of defeat the purpose of redialing.
 */
int gRedialModeEnabled = 0;

/* If we connect successfully, we'll want to save this port number
 * when we add a host file entry.
 */
int gPortNumberUsed;

int gSavePasswords = 0;

/* Open.c externs */
extern char *gOptArg;
extern int gOptInd, gIsToTTY;
extern unsigned int gFTPPort;
extern long gEventNumber;
extern char gRemoteCWD[256];
extern char gIPStr[32];
extern int gTransferType, gCurType, gRemoteServerType;
extern int gRmtInfoIsNew, gWantRmtInfoSaved;
extern int gConnected, gHasPASV, gVerbosity;
extern int gDoneApplication, gWinInit, gMode;
extern string gEmailAddress, gAnonPassword, gPager;
extern UserInfo gUserInfo;
extern FILE *gLogFile;
extern LineList gRedir;
extern void GetRemoteCWD(char *cdstub, ResponsePtr cwdrp);


/* This is used primarily for non-anonymous logins.  We'll have to ask
 * the user some questions, like their username, password, etc.
 */
int LoginQuestion(
	char *prompt,
	char *answer,
	size_t siz,
	char *defanswer,
	int noEcho)
{
	string prompt2;
	
	/* Only do this if we have an empty string as the answer. */
	if (*answer == '\0') {
		if (defanswer != NULL)
			sprintf(prompt2, "%s [%s]: ", prompt, defanswer);
		else
			sprintf(prompt2, "%s: ", prompt);
		GetAnswer(prompt2, answer, siz, noEcho);
		if ((*answer == '\0') && (defanswer != NULL))
			Strncpy(answer, defanswer, siz);
	}
	return (*answer == '\0' ? (-1) : 0);
}	/* LoginQuestion */




int Login(char *u, char *p, char *a)
{
	string u2, p2, a2;
	ResponsePtr rp;
	int result = -1;
	int isAnonLogin = 1;

	STRNCPY(u2, u);
	STRNCPY(p2, p);
	STRNCPY(a2, a);
		
	rp = InitResponse();
	if (LoginQuestion("User", u2, sizeof(u2), "anonymous", 0) < 0)
		goto done;
	RCmd(rp, "USER %s", u2);

	for (;;) {
		/* Here's a mini finite-automaton for the login process.
		 *
		 * Originally, the FTP protocol was designed to be entirely
		 * implementable from a FA.  It could be done, but I don't think
		 * it's something an interactive process could be the most
		 * effective with.
		 */
		switch (rp->code) {
			case 230:	/* 230 User logged in, proceed. */
			case 202:	/* Command not implemented, superfluous at this site. */
				goto okay;

			case 421:	/* 421 Service not available, closing control connection. */
				goto done;
				
			case 331:	/* 331 User name okay, need password. */
				isAnonLogin =	STREQ("anonymous", u2) ||
								STREQ("ftp", u2);
				if (isAnonLogin)
					rp->printMode = kDontPrint;

				ReInitResponse(rp);
				(void) LoginQuestion(
					"Password",
					p2,
					sizeof(p2),
					isAnonLogin ? gAnonPassword : NULL,
					1
				);
				RCmd(rp, "PASS %s", p2);
				break;

         	case 332:	/* 332 Need account for login. */
         	case 532: 	/* 532 Need account for storing files. */
				ReInitResponse(rp);
				(void) LoginQuestion("Account", a2, sizeof(a2), NULL, 1);
				RCmd(rp, "ACCT %s", a2);
				break;

			case 501:	/* Syntax error in parameters or arguments. */
			case 503:	/* Bad sequence of commands. */
			case 530:	/* Not logged in. */
			case 550:	/* Can't set guest privileges. */
				goto done;
				
			default:
				if (rp->msg.first == NULL) {
					Error(kDontPerror, "Lost connection during login.\n");
				} else {
					Error(kDontPerror, "Unexpected response: %s\n",
						rp->msg.first->line
					);
				}
				goto done;
		}
	}
okay:
	if (isAnonLogin) {
		gRmtInfo.user[0] = '\0';
		gRmtInfo.pass[0] = '\0';
		STRNCPY(gRmtInfo.acct, a2);
	} else {
		STRNCPY(gRmtInfo.user, u2);
		if (gSavePasswords) {
			STRNCPY(gRmtInfo.pass, p2);
			STRNCPY(gRmtInfo.acct, a2);
		}
	}

	result = 0;
done:
	DoneWithResponse(rp);
	return result;
}	/* Login */




/* After closing a site, set or restore some things to their original
 * states.
 */
void PostCloseStuff(void)
{
#ifdef SYSLOG
	syslog (LOG_INFO, "%s disconnected from %s.", gUserInfo.userName, gHost);
#endif
	if (gLoggedIn) {
		gRmtInfo.hasPASV = gHasPASV;
		gRmtInfo.port = gPortNumberUsed;
		gRmtInfo.nCalls++;
		time((time_t *) &gRmtInfo.lastCall);
		STRNCPY(gRmtInfo.lastIP, gIPStr);
		if (gEventNumber > 0L) {
			/* Only do these if we were not in batch mode (colon-mode). */
			if (gRmtInfo.noSaveDir == 0)
					STRNCPY(gRmtInfo.dir, gRemoteCWD);
			(void) RunPrefixedMacro("close.", gRmtInfo.bookmarkName);
			(void) RunPrefixedMacro("close.", "any");
		} else {
			/* Only do these if we are running colon mode. */
			(void) RunPrefixedMacro("colon.close.", gRmtInfo.bookmarkName);
			(void) RunPrefixedMacro("colon.close.", "any");
		}
		SaveCurHostBookmark(NULL);
	}
	gLoggedIn = 0;
	gRemoteCWD[0] = gHost[0] = '\0';
}	/* PostCloseStuff */




/* Close the connection to the remote host and cleanup. */
void DoClose(int tryQUIT)
{
	ResponsePtr rp;
	
	if (tryQUIT != 0) {
		rp = InitResponse();
		rp->eofOkay = 1;	/* We are expecting EOF after this cmd. */
		RCmd(rp, "QUIT");
		DoneWithResponse(rp);
	}
	
	CloseControlConnection();
	CloseDataConnection(1);
	PostCloseStuff();
}	/* DoClose */




int CloseCmd(void)
{
	DoClose(gConnected);
	if (gWinInit == 0)
			SetScreenInfo();	/* Need for line mode. */
	return kNoErr;
}	/* CloseCmd */




/* Given a pointer to an OpenOptions (structure containing all variables
 * that can be set from the command line), this routine makes sure all
 * the variables have valid values by setting them to their defaults.
 */
 
void InitOpenOptions(OpenOptions *openopt)
{
	/* How do you want to open a site if neither -a or -u are given?
	 * gAnonOpen is true (default to anonymous login), unless
	 * Config.h was edited to set UOPEN to 0 instead.
	 */
	openopt->openmode = gAnonOpen ? kOpenImplicitAnon : kOpenImplicitUser;

	/* Normally you don't want to ignore the entry in your netrc. */
	openopt->ignoreRC = 0;

	/* Set the default delay if the user specifies redial mode without
	 * specifying the redial delay.
	 */
	openopt->redialDelay = kRedialDelay;

	/* Normally, you only want to try once. If you specify redial mode,
	 * this is changed.
	 */
	openopt->maxDials = 1;
	
	/* You don't want to dump the file to stdout by default. */
	openopt->ftpcat = kNoFTPCat;

	/* We'll set this later, but we'll use 0 to mean that the port
	 * is explicitly not set by the user.
	 */
	openopt->port = kPortUnset;

	/* We are not in colon-mode (yet). */
	openopt->colonModePath[0] = 0;

	/* Set the hostname to a null string, since there is no default host. */
	openopt->hostname[0] = 0;
	
	/* Set the opening directory path to a null string. */
	openopt->cdpath[0] = 0;

	openopt->interactiveColonMode = 0;

	/* Since we're opening with a possible colon-mode item, we have to
	 * track options for the GetCmd too.
	 */
	InitGetOptions(&openopt->gopt);
}	/* InitOpenOptions */





/* This is responsible for parsing the command line and setting variables
 * in the OpenOptions structure according to the user's flags.
 */

int GetOpenOptions(int argc, char **argv, OpenOptions *openopt, int fromMain)
{
	int					opt, www;
	char				*cp, *hostp, *cpath;

	/* First setup the openopt variables. */
	InitOpenOptions(openopt);

	/* Tell Getopt() that we want to start over with a new command. */
	GetoptReset();
	while ((opt = Getopt(argc, argv, "aiup:rd:g:cmCfGRn:")) >= 0) {
		switch (opt) {		
			case 'a':
				/* User wants to open anonymously. */
				openopt->openmode = kOpenExplicitAnon;
				break;
				
			case 'u':
				/* User wants to open with a login and password. */
				openopt->openmode = kOpenExplicitUser;
				break;

			case 'i':
				/* User wants to ignore the entry in the netrc. */
				openopt->ignoreRC = 1;
				break;

			case 'p':
				/* User supplied a port number different from the default
				 * ftp port.
				 */
				openopt->port = atoi(gOptArg);
				if (openopt->port <= 0) {
					/* Probably never happen, but just in case. */
					(void) PrintF("%s: bad port number (%s).\n", argv[0],
						gOptArg);
					goto usage;
				}
				break;
				
			case 'd':
				/* User supplied a delay (in seconds) that differs from
				 * the default.
				 */
				openopt->redialDelay = atoi(gOptArg);
				break;
				
			case 'g':
				/* User supplied an upper-bound on the number of redials
				 * to try.
				 */
				openopt->maxDials = atoi(gOptArg);
				break;

			case 'r':
				openopt->maxDials = -1;
				break;

			case 'm':
				/* ftpcat mode is only available from your shell command-line,
				 * not from the ncftp shell.  Do that yourself with 'more zz'.
				 */
				if (gEventNumber == 0L) {
					/* If gEventNumber is zero, then we were called directly
					 * from main(), and before the ftp shell has started.
					 */
					openopt->ftpcat = kFTPMore;
					/* ftpcat mode is really ftpmore mode. */
					break;
				} else {
					PrintF(
"You can only use this form of colon-mode (-m) from your shell command line.\n\
Try 'ncftp -m wuarchive.wustl.edu:/README'\n");
				}
				goto usage;

			case 'c':
				/* ftpcat mode is only available from your shell command-line,
				 * not from the ncftp shell.  Do that yourself with 'get zz -'.
				 */
				if (gEventNumber == 0L) {
					/* If gEventNumber is zero, then we were called directly
					 * from main(), and before the ftp shell has started.
					 */
					openopt->ftpcat = kFTPCat;
					break;
				} else {
					PrintF(
"You can only use ftpcat/colon-mode from your shell command line.\n\
Try 'ncftp -c wuarchive.wustl.edu:/README > file.'\n");
				}
				goto usage;

			/* These are options that can be passed to get. */
			case 'C':
			case 'f':
			case 'G':
			case 'R':
			/* case 'z':  Note that we can't handle the rename here. */
			case 'n':
				if (fromMain) {
					(void) SetGetOption(&openopt->gopt, opt, gOptArg);
					break;
				}
				goto usage;

			default:
				if (fromMain)
					break;
					
			usage:
				return kUsageErr;
		}
	}

	if (argv[gOptInd] == NULL) {
		if (!fromMain) {
			if (openopt->hostname[0] == 0)
				goto usage;
		}
	} else {
		/* The user gave us a host to open.
		 *
		 * First, check to see if they gave us a colon-mode path
		 * along with the hostname.  We also understand a WWW path,
		 * like "ftp://bang.nta.no/pub/fm2html.v.0.8.4.tar.Z".
		 */
		hostp = argv[gOptInd];
		cpath = NULL;
		if ((cp = strchr(hostp, ':')) != NULL) {
			if (fromMain) {
				*cp++ = '\0';
				cpath = cp;
				www = 0;	/* Is 0 or 1, depending on the type of path. */
				if ((*cp == '/') && (cp[1] == '/')) {
					/* First make sure the path was intended to be used
					 * with ftp and not one of the other URLs.
					 */
					if (!ISTREQ(argv[gOptInd], "ftp")) {
						PrintF(
							"Bad URL '%s' -- WWW paths must be prefixed by 'ftp://'.\n",
							argv[gOptInd]
						);
						goto usage;
					}
	
					cp += 2;
					hostp = cp;
					cpath = NULL;	/* It could have been ftp://hostname only. */
	
					if ((cp = strchr(hostp, '/')) != NULL) {
						*cp++ = '\0';
						cpath = cp;
					}
					www = 1;
				}
				if (cpath != NULL) {
					(void) STRNCPY(openopt->colonModePath, www ? "/" : "");
					(void) STRNCAT(openopt->colonModePath, cpath);
					cp = openopt->colonModePath +
						strlen(openopt->colonModePath) - 1;
					if (*cp == '/') {
						/* Colon-mode path ended in a slash, so you said it
						 * was a directory.  That means we should start from
						 * this directory when we open this site.
						 */
						*cp = '\0';
						openopt->interactiveColonMode = 1;
					}
					DebugMsg("Colon-Mode Path = '%s'\n", openopt->colonModePath);
				}

			} else {
				/* I may lift this restriction later. */
				EPrintF("You can't use colon mode in the command shell.\n");
				return kUsageErr;
			}
		}
		(void) STRNCPY(openopt->hostname, hostp);

		if ((openopt->colonModePath[0] == '\0') &&
			(openopt->ftpcat != kNoFTPCat))
		{
			/* User specified ftpcat mode, but didn't supply
			 * the host:file.
			 */
			EPrintF("You didn't use colon mode correctly.\n\
If you use -c or -m, you need to do something like this:\n\
ncftp -c wuarchive.wustl.edu:/pub/README (to cat this file to stdout).\n");
			return kUsageErr;
		}

		GetBookmark(openopt->hostname, sizeof(openopt->hostname));
		if (openopt->port == kPortUnset) {
			/* No port specified, so use same port as last time. */
			if (gRmtInfo.port == kPortUnset)
				openopt->port = gFTPPort;
			else
				openopt->port = gRmtInfo.port;
		}
	}
	return kNoErr;
}	/* GetOpenOptions */




void CheckRemoteSystemType(int force_binary)
{
	string remoteSys;

	if (gRmtInfoIsNew) {
		/* If first time here, check somethings while we can. */
		if (DoSystem(remoteSys, sizeof(remoteSys)) == 0)
			gRmtInfo.isUnix = !strncmp(remoteSys, "UNIX", (size_t) 4);
	
		/* Set to binary mode if any of the following are true:
		 * (a) The user is using colon-mode (force_binary; below);
		 * (b) The reply-string from SYST said it was UNIX with 8-bit chars.
		 */
		if (!strncmp(remoteSys, "UNIX Type: L8", (size_t) 13))
		{
			gRmtInfo.xferType = 'I';
		}
	
		/* Print a warning for that (extremely) rare Tenex machine. */
		if (!strncmp(remoteSys, "TOPS20", (size_t) 6)) {
			gRmtInfo.xferType = 'T';
			(void) PrintF("Using tenex mode to transfer files.\n");
		}

		if (gRemoteServerType == kNcFTPd) {
			/* This server correctly implements
			 * block transfer mode, so turn it
			 * on by default.
			 *
			 * Unfortunately, most servers don't
			 * support it, and those that do
			 * (AIX ftpd) don't do it correctly.
			 */
			gRmtInfo.xferMode = 'B';
		}

	}
	gTransferType = gRmtInfo.xferType;
	
	/* Use binary mode if this site was last set using ascii mode,
	 * and we are using colon-mode.
	 */
	if ((force_binary) && (gTransferType == 'A'))
		gTransferType = 'I';

	if (SetMode(gRmtInfo.xferMode) < 0) {
		gRmtInfo.xferMode = gMode;
	}
}	/* CheckRemoteSystemType */



/* This is called if the user opened the host with a file appended to
 * the host's name, like "wuarchive.wustl.edu:/pub/readme," or
 * "wuarchive.wustl.edu:/pub."  In the former case, we open wuarchive,
 * and fetch "readme."  In the latter case, we open wuarchive, then set
 * the current remote directory to "/pub."  If we are fetching a file,
 * we can do some other tricks if "ftpcat mode" is enabled.  This mode
 * must be selected from your shell's command line, and this allows you
 * to use the program as a one-liner to pipe a remote file into something,
 * like "ncftp -c wu:/pub/README | wc."  If the user uses ftpcat mode,
 * the program immediately quits instead of going into its own command
 * shell.
 */
void ColonMode(OpenOptions *openopt)
{
	int result;

	/* Check for FTP-cat mode, so we call the appropriate
	 * fetching routine.
	 */
	if (openopt->ftpcat == kFTPCat) {
		InitGetOutputMode(&openopt->gopt, kDumpToStdout);
		openopt->gopt.rName = openopt->colonModePath;
		openopt->gopt.doReports = 0;
		result = DoGet(&openopt->gopt);
	} else if (openopt->ftpcat == kFTPMore) {
		result = DoPage(openopt->colonModePath);
	} else {
		/* Regular colon-mode, where we fetch the file, putting the
		 * copy in the current local directory.
		 */
		InitGetOutputMode(&openopt->gopt, kSaveToDisk);
		openopt->gopt.doReports = 0;
		openopt->gopt.rName = openopt->colonModePath;
		result = DoGetWithGlobbingAndRecursion(&openopt->gopt);
	}
	DoQuit(result == 0 ? result : kExitColonModeFail);
	/*NOTREACHED*/
}	/* ColonMode */




/* Now that we're logged in, do some other stuff prior to letting the
 * user type away at the shell.
 */
void PostLoginStuff(OpenOptions *openopt)
{
	time_t now;
	int wasColonMode;
	
	gLoggedIn = 1;

	/* Clear out the old redir buffer, since we are on a new site. */
	DisposeLineListContents(&gRedir);

	/* Since we're logged in okay, we know what we've collected so far
	 * should be saved for next time.
	 */
	gWantRmtInfoSaved = 1;
	
	/* The FTP module keeps its own note of whether the site has PASV
	 * or not, and has already initialized it to true.  If the gRmtInfo
	 * was fetched from our host file, we can tell FTP.c for sure if
	 * PASV should even be attempted.  If this was a new gRmtInfo, we
	 * will just be a little redundant, since new entries also assume
	 * PASV is supported.
	 */
	gHasPASV = gRmtInfo.hasPASV;
	
	/* Since we connected okay, save this port number for later. */
	gPortNumberUsed = openopt->port;

	/* When a new site is opened, ASCII type is assumed (by protocol),
	 * as well as stream transfer mode.
	 */
	gCurType = 'A';
	gMode = 'S';

	STRNCPY(gHost, openopt->hostname);
#ifdef SYSLOG
	syslog (LOG_INFO, "%s logged into %s.", gUserInfo.userName, gHost);
#endif
	if (gLogFile != NULL) {
		(void) time(&now);
		fprintf(gLogFile, "%s at %s", gHost, ctime(&now));
	}

	wasColonMode = openopt->colonModePath[0] != (char)0;

	/* We need to check for unix and see if we should set binary
	 * mode automatically.
	 */
	CheckRemoteSystemType(wasColonMode);

	if (wasColonMode) {
		if (openopt->interactiveColonMode) {
			/* Interactive colon mode simply means that the thing they
			 * gave us was a directory, and we should just cd to that
			 * directory when we start up.
			 */
			(void) DoChdir(openopt->colonModePath);
		} else {
			/* Regular colon-mode is fetching a file by specifying the
			 * pathname of the file on the shell command line.
			 */
			(void) GetRemoteCWD(kDidNotChdir, NULL);
			ColonMode(openopt);		/* Does not return... */
		}
	} else if (gRmtInfo.dir[0]) {
		/* If we didn't have a colon-mode path, we try setting
		 * the current remote directory to cdpath.  The .dir field is
		 * (usually) the last directory we were in the previous
		 * time we called this site.
		 */
		(void) DoChdir(gRmtInfo.dir);
	} else {
		/* Freshen 'cwd' variable for the prompt. */
		(void) GetRemoteCWD(kDidNotChdir, NULL);
	}

	if (wasColonMode) {
		/* Run separate sets of macros for colon-mode opens and regular,
		 * interactive opens.
		 */
		(void) RunPrefixedMacro("colon.open.", "any");
		(void) RunPrefixedMacro("colon.open.", gRmtInfo.bookmarkName);
	} else {
		(void) RunPrefixedMacro("open.", "any");
		(void) RunPrefixedMacro("open.", gRmtInfo.bookmarkName);
		if (gWinInit == 0)
				SetScreenInfo();	/* Need for line mode. */
		ClearDirCache();
	}
}	/* PostLoginStuff */





/* Given a properly set up OpenOptions, we try connecting to the site,
 * redialing if necessary, and do some initialization steps so the user
 * can send commands.
 */
int Open(OpenOptions *openopt)
{
	int					hErr;
	int					dials;
	char				*user, *pass, *r_acct;	
	int					loginResult;
	int					openMode;

	gRedialModeEnabled = (openopt->maxDials != 1);
	if (ISEXPLICITOPEN(openopt->openmode)) {
		/* User specified an open mode explictly, like open -u,
		 * so do what the user said in spite of what we may have had
		 * in the gRmtInfo.
		 */
		openMode = (ISANONOPEN(openopt->openmode)) ? 'a' : 'u';
	} else {
		if (gRmtInfoIsNew == 1) {
			/* First time opening this site.  Open it anonymously
			 * unless you said not to with a -option.
			 */
			openMode = (ISANONOPEN(openopt->openmode)) ? 'a' : 'u';
		} else {
			/* We've been here before, so use what we had last time. */
			openMode = 'r';
		}
	}

	if ((openMode == 'r') && (gRmtInfo.user[0] == '\0'))
		openMode = 'a';
	if (openMode == 'a') {
		user = "anonymous";
		pass = strchr(gRmtInfo.pass, '@') ? gRmtInfo.pass : gAnonPassword;
		r_acct = "";
	} else if (openMode == 'u') {
		user = "";
		pass = "";
		r_acct = "";
	} else {
		user = gRmtInfo.user;
		pass = gRmtInfo.pass;
		r_acct = gRmtInfo.acct;
	}

	if ((openopt->colonModePath[0]) && (!openopt->interactiveColonMode)) {
		/* If we're running from a shell script (or whatever)
		 * don't dump any output.  If the user is doing this from
		 * the shell, we will at least print the error messages.
		 * We also don't want to "pollute" ftpcat mode, but since
		 * error messages are printed to stderr, and we weren't
		 * going to print anything but error messages anyway,
		 * we're okay by using kErrorsOnly.
		 */
		if (gIsToTTY != 0)
			(void) SetVerbose(kErrorsOnly);
		else
			(void) SetVerbose(kQuiet);
	} else {
		/* If we haven't already setup our interactive shell, which
		 * would happen if you gave a host on the command line, then
		 * we need to do that now because we want the remote host's
		 * startup information to be displayed.
		 */
		Startup();
	}

	
	PrintF("Trying to connect to %s...\n", openopt->hostname);
	for (
			dials = 0;
			openopt->maxDials < 0 || dials < openopt->maxDials;
			dials++)
	{
		if (dials > 0) {
			/* If this is the second dial or higher, sleep a bit. */
			PrintF("Sleeping %u seconds...  ",
				(unsigned) openopt->redialDelay);
			FlushListWindow();
			(void) sleep((unsigned) openopt->redialDelay);
			PrintF("Retry Number: %d\n", dials + 1);
		}
		FlushListWindow();

		SetBar(NULL, "CONNECTING", NULL, 1, 1);
		gAttemptingConnection = 1;
		hErr = OpenControlConnection(openopt->hostname, openopt->port);
		if (hErr == kConnectErrFatal) {
			/* Irrecoverable error, so don't bother redialing.
			 * The error message should have already been printed
			 * from OpenControlConnection().
			 */
			DebugMsg("Cannot recover from open error %d.\n", hErr);
			break;
		} else if (hErr == kConnectNoErr) {
			/* We were hooked up successfully. */
			
			gRemoteCWD[0] = '\0';
	
			/* This updates the status bar (for visual mode). */
			SetBar(NULL, "LOGGING IN", NULL, 1, 1);
			SetPostHangupOnServerProc(PostCloseStuff);
			loginResult = Login(user, pass, r_acct);
			
			if (loginResult == 0) {
				PostLoginStuff(openopt);
				if (openopt->maxDials != 1) {
					/* If they selected redial mode, beep at the user
					 * to get their attention.
					 */
					Beep(1);
				}
				gAttemptingConnection = 0;	/* We are connected. */
				gRedialModeEnabled = 0;		/* Not dialing any more. */
				return(kNoErr);	/* Login okay, so done. */
			}
			/* Otherwise, an error during login occurred, so try again. */
		} else /* (hErr == kConnectErrReTryable), so redial. */ {
			/* Display error now. */
			FlushListWindow();
		}
		
		DoClose(gConnected);
		gAttemptingConnection = 0;
	}

	/* Display error now. */
	FlushListWindow();

	if ((openopt->colonModePath[0]) && (!openopt->interactiveColonMode)) {
		/* If we get here, we we're colon-moding and got a non-redialable
		 * error or we ran out of attempts.
		 */
		DoQuit(kExitColonModeFail);
	}
	return (kCmdErr);
}	/* Open */




int OpenCmd(int argc, char **argv)
{
	OpenOptions			openopt;
	int result;

	/* If there is already a site open, close that one so we can
	 * open a new one.
	 */
	DoClose(gConnected);

	if (argc < 2) {
		result = HostWindow();
	} else {
		if ((result = GetOpenOptions(argc, argv, &openopt, 0)) == kNoErr)
			result = Open(&openopt);
	}
	return result;
}	/* OpenCmd */

/* Open.c */
