/* Main.c */

#define _main_c_ 1

/* Read the "COPYING" file for licensing terms. */

#include "Sys.h"

#if (LOCK_METHOD == 2)
#	ifdef HAVE_SYS_FILE_H
#		include <sys/file.h>
#	endif
#endif

#ifdef SYSLOG
#	include <syslog.h>
#endif

#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>

#include "Util.h"
#include "Main.h"
#include "Cmds.h"
#include "Open.h"
#include "Cmdline.h"
#include "DateSize.h"
#include "Bookmark.h"
#include "Prefs.h"
#include "FTP.h"
#include "Glob.h"
#include "Getopt.h"
#include "Xfer.h"
#include "Complete.h"
#include "Tips.h"
#include "Version.h"

/* We need to know our fully qualified hostname.  That way we can give
 * a complete email address as a password for anonymous logins.
 */
string gOurHostName;

/* The user's full email address, which we use for the password on
 * anonymous logins.
 */
string gEmailAddress;

/* This is what we'll actually use for the anonymous password.
 * We keep an unmodified copy of gEmailAddress, and let the user
 * change the variable below if they want to.
 */
string gAnonPassword;

/* Since we create a bunch of files, instead of making them all
 * .dot files in the home directory, we have our own .dot directory
 * and keep all our stuff in there.
 */
string gOurDirectoryPath;

/* We keep some basic information about the user.  Most of the structure
 * is just a copy of the user's password file entry.  Instead of keeping
 * just that pointer from getpwnam(), we make our own copy so we can
 * use getpwnam() again later.
 */
UserInfo gUserInfo;

/* Boolean indicating whether input is coming from a live user or
 * a file.
 */
int gIsFromTTY;

/* Boolean indicating whether output is going to the screen or to
 * a file.
 */
int gIsToTTY;

/* Boolean indicating whether input is coming from a script file.
 * This is really just a copy of gIsFromTTY.
 */
int gDoingScript;

/* A FILE pointer to the user's usage log, or NULL if we're not
 * logging anything.
 */
FILE *gLogFile = NULL;

/* We can optionally log all the debugging information to a separate
 * log file.
 */
FILE *gTraceLogFile = NULL;

/* Full pathname of the user's usage log file, in our .dot directory. */
longstring gLogFileName;

/* Maximum size we will let the log grow to before making it smaller.
 * If this is zero, there is no limit.
 */
int gMaxLogSize = 10240;

/* Boolean indicating whether the user wanted to keep a usage log. */
int gLogging = USERLOG;

/* Full pathname of the user's pager.  Maybe be just "more" if the user
 * doesn't have a PAGER environment variable, or didn't set it explicitly.
 */
longstring gPager;

int gVisualMode;
int gDefaultVisualMode = VISUAL;

/* If another ncftp process is running, we don't want to overwrite the
 * the files in our directory.  We will only read them to prevent the
 * user losing changes made in the first process.
 */
int gOtherSessionRunning = 0;

/* We track the number of times the user has run the program.  We may
 * use that information for something else later.
 */
int gTotalRuns = 0;

/* Types of startup messages user wants us to print when we first run. */
int gStartupMsgs = (kStartupMsg | kTips);

string gVersion;

/* If this user variable is set, we will change directory to the last
 * local directory the user was in when the user exited.
 */
int gRememberLCWD = 0;

/* If this user variable is set, we will always chdir to this value each
 * time we run the program.
 */
longstring gDownloadDir = "";

jmp_buf gMainJmp;

/* We save previously entered commands between sessions, so the user can
 * use the history.
 */
LineList gCmdHistory;

/* Name of the file we use to tell if another ncftp process is running. */
longstring gLockFileName = "";

/* Boolean telling whether the splash-screen and interactive startup stuff
 * has been done.
 */
int gStartup = 0;

extern int gStdout, gRealStdout;
extern char *getlogin(void);
extern longstring gLocalCWD;
extern time_t gMailBoxTime;
extern int gWinInit, gOptErr, gTrace, gDebug;
extern char *gSprintfBuf;
extern jmp_buf gCmdLoopJmp;
extern LineList gRedir;



/* This looks up the user's password entry, trying to look by the username.
 * We have a couple of extra hacks in place to increase the probability
 * that we can get the username.
 */
static
struct passwd *GetPwByName(void)
{
	char *cp;
	struct passwd *pw;
	
	cp = getlogin();
	if (cp == NULL) {
		cp = (char *) getenv("LOGNAME");
		if (cp == NULL)
			cp = (char *) getenv("USER");
	}
	pw = NULL;
	if (cp != NULL)
		pw = getpwnam(cp);
	return (pw);
}	/* GetPwByName */




void GetUserInfo(void)
{
	register char			*cp;
	struct passwd			*pw;
	string					str;
	struct stat				st;

	pw = NULL;
	errno = 0;
#ifdef USE_GETPWUID
	/* Try to use getpwuid(), but if we have to, fall back to getpwnam(). */
	if ((pw = getpwuid(getuid())) == NULL)
		pw = GetPwByName();	/* Oh well, try getpwnam() then. */
#else
	/* Try to use getpwnam(), but if we have to, fall back to getpwuid(). */
	if ((pw = GetPwByName()) == NULL)
		pw = getpwuid(getuid());	/* Try getpwnam() then. */
#endif
	if (pw != NULL) {
		gUserInfo.uid = pw->pw_uid;
		(void) STRNCPY(gUserInfo.userName, pw->pw_name);
		gUserInfo.shell = StrDup(pw->pw_shell);
		if ((cp = (char *) getenv("HOME")) != NULL)
			gUserInfo.home = StrDup(cp);
		else
			gUserInfo.home = StrDup(pw->pw_dir);
	} else {
		/* Couldn't get information about this user.  Since this isn't
		 * the end of the world as far as I'm concerned, make up
		 * some stuff that might work good enough.
		 */
		Error(kDoPerror, "Could not get your passwd entry!");
		sleep(1);
		if ((cp = (char *) getenv("LOGNAME")) == NULL)
			cp = "nobody";
		(void) STRNCPY(gUserInfo.userName, cp);
		gUserInfo.shell = StrDup("/bin/sh");
		if ((cp = (char *) getenv("HOME")) == NULL)
			cp = ".";
		gUserInfo.home = StrDup(cp);
		gUserInfo.uid = 999;
	}

	cp = (char *) getenv("MAIL");
	if (cp == NULL)
		cp = (char *) getenv("mail");
	if (cp != NULL) {
		/* We do have a Mail environment variable. */
		(void) STRNCPY(str, cp);
		cp = str;

		/* Mail variable may be like MAIL=(28 /usr/mail/me /usr/mail/you),
		 * so try to find the first mail path.
		 */
		while ((*cp != '/') && (*cp != 0))
			cp++;
		gUserInfo.mail = StrDup(cp);
		if ((cp = strchr(gUserInfo.mail, ' ')) != NULL)
			*cp = '\0';
	} else {
		/* Guess between /usr/mail and /usr/spool/mail as
		 * possible directories.  We'll just choose /usr/spool/mail
		 * if we have to.
		 */
		(void) sprintf(str, "/usr/mail/%s", gUserInfo.userName);
		if (stat(str, &st) < 0)
			(void) sprintf(str, "/usr/spool/mail/%s", gUserInfo.userName);
		gUserInfo.mail = StrDup(str);
	}

	DebugMsg("GetOurHostName returned %d, %s.\n",	
		GetOurHostName(gOurHostName, sizeof(gOurHostName)),
		gOurHostName
	);
	STRNCPY(gEmailAddress, gUserInfo.userName);
	STRNCAT(gEmailAddress, "@");
	STRNCAT(gEmailAddress, gOurHostName);
	STRNCPY(gAnonPassword, gEmailAddress);
}	/* GetUserInfo */




/* Setup the malloc library, if we're using one. */
static void InitMalloc(void)
{
#ifdef DEBUG
#	if (LIBMALLOC == FAST_MALLOC)
	FILE *malloclog;

	mallopt(M_DEBUG, 1);
	mallopt(M_LOG, 1);	/* turn on/off malloc/realloc/free logging */
	/* set the file to use for logging */
	if ((malloclog = fopen("malloc.log", "w")) != NULL)
		mallopt(M_LOGFILE, fileno(malloclog)); 
#	endif
#endif

#if (LIBMALLOC == DEBUG_MALLOC)
	/* Thanks to Conor Cahill for making something like this possible! */
	union dbmalloptarg  m;

#	if 1
	m.i = 1;
	dbmallopt(MALLOC_CKCHAIN, &m);
#	endif

#	if 1
	m.i = M_HANDLE_IGNORE;
	dbmallopt(MALLOC_WARN, &m);
#	endif

#	if 0	/* If 0, stderr. */
	m.str = "malloc.log";
	dbmallopt(MALLOC_ERRFILE, &m);
#	endif
#endif
}	/* InitMalloc */




void OpenTraceLog(void)
{
	string traceLogPath;
	string traceLog2Path;
	string traceLogTmpPath;
	string line;
	FILE *f1, *f2, *f3;
	int i, lim;
	time_t now;

	if (gOtherSessionRunning != 0)
		return;

	if (gTraceLogFile != NULL)
		return;	/* Already open. */

	if (gOurDirectoryPath[0] == '\0')
		return;		/* Don't create in root directory. */

	OurDirectoryPath(traceLogPath, sizeof(traceLogPath), kTraceLogName);	
	if (access(traceLogPath, F_OK) == 0) {
		/* Need to prepend last session's log to the master debug log. */
		OurDirectoryPath(traceLog2Path, sizeof(traceLog2Path), kTraceLog2Name);
		OurDirectoryPath(traceLogTmpPath, sizeof(traceLogTmpPath), kTraceLogTmpName);

		f1 = fopen(traceLogTmpPath, "w");
		if (f1 == NULL) {
			Error(kDoPerror, "Can't open %s for writing.\n", traceLogTmpPath);
			return;
		}
		f2 = fopen(traceLogPath, "r");
		if (f2 == NULL) {
			Error(kDoPerror, "Can't open %s for reading.\n", traceLogPath);
			return;
		}
		
		lim = kMaxTraceLogLines - 1;
		for (i=0; i<lim; i++) {
			if (fgets(line, ((int) sizeof(line)) - 1, f2) == NULL)
				break;
			fputs(line, f1);
		}
		if (i + 2 < lim) {
			fputs("\n\n", f1);
			i += 2;
		}
		(void) fclose(f2);
		f3 = fopen(traceLog2Path, "r");
		if (f3 != NULL) {
			for (; i<lim; i++) {
				if (fgets(line, ((int) sizeof(line)) - 1, f3) == NULL)
					break;
				fputs(line, f1);
			}
			(void) fclose(f3);			
		}

		if (i >= lim)
			fputs("...Remaining lines omitted...\n", f1);

		(void) fclose(f1);
		(void) UNLINK(traceLogPath);
		(void) UNLINK(traceLog2Path);
		(void) rename(traceLogTmpPath, traceLog2Path);
	}

	gTraceLogFile = fopen(traceLogPath, "w");
	if (gTraceLogFile == NULL) {
		Error(kDoPerror, "Can't open %s for writing.\n", traceLogPath);
	} else {
		time(&now);
		fprintf(
			gTraceLogFile,
			"SESSION STARTED at %s%s\n\n",
			ctime(&now),
			"-----------------------------------------------"
		);
	}
}	/* OpenTraceLog */




void OpenLogs(void)
{
	/* First try open the user's private log, unless the user
	 * has this option turned off.
	 */
	OurDirectoryPath(gLogFileName, sizeof(gLogFileName), kLogName);
	gLogFile = NULL;
	if (gOtherSessionRunning == 0) {
		if ((gLogging) && (gMaxLogSize > 0))
			gLogFile = fopen(gLogFileName, "a");

		if (gTrace == kTracingOn)
			OpenTraceLog();
	}

	/* Open a port to the system log, if you're hellbent on knowing
	 * everything users do with the program.
	 */
#ifdef SYSLOG
#	ifdef LOG_LOCAL3
	openlog ("NcFTP", LOG_PID, LOG_LOCAL3);
#	else
	openlog ("NcFTP", LOG_PID);
#	endif
#endif				/* SYSLOG */
}	/* OpenLogs */




/* Create, if necessary, a directory in the user's home directory to
 * put our incredibly important stuff in.
 */
void InitOurDirectory(void)
{
	struct stat st;
	char *cp;

	cp = getenv("NCFTPDIR");
	if (cp != NULL) {
		STRNCPY(gOurDirectoryPath, cp);
	} else if (STREQ(gUserInfo.home, "/")) {
		/* Don't create it if you're root and your home directory
		 * is the root directory.
		 */
		gOurDirectoryPath[0] = '\0';
		return;
	} else {
		(void) Path(gOurDirectoryPath,
			sizeof(gOurDirectoryPath),
			gUserInfo.home,
			kOurDirectoryName
		);
	}

	if (stat(gOurDirectoryPath, &st) < 0) {
		(void) mkdir(gOurDirectoryPath, 00755);
	}
}	/* InitOurDirectory */



/* There would be problems if we had multiple ncftp's going.  They would
 * each try to update the files in ~/.ncftp.  Whichever program exited
 * last would have its stuff written, and the others which may have made
 * changes, would lose the changes they made.
 *
 * This checks to see if another ncftp is running.  To do that we try
 * to lock a special file.  If we could make the lock, then we are the first
 * ncftp running and our changes will be written.  Other ncftps that may
 * start will not be able to save their changes.
 *
 * No matter what locking method in use, whenever we Exit() we try to
 * remove the lock file if we had the lock.  For the first two methods,
 * exiting releases the lock automatically.
 */
static
void CheckForOtherSessions(void)
{
	int fd;
	char pidbuf[64];
	time_t now;

#if (LOCK_METHOD == 1)
	struct flock l;
	int err;

	l.l_type = F_WRLCK;
	l.l_start = 0;
	l.l_whence = SEEK_SET;
	l.l_len = 1;

	OurDirectoryPath(gLockFileName, sizeof(gLockFileName), kLockFileName);
	if ((fd = open(gLockFileName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR)) < 0) {
		Error(kDoPerror, "Could not open lock file %s.\n", gLockFileName);
		sleep(5);

		/* Try to save data anyway. */
		gOtherSessionRunning = 0;
	} else {
		err = fcntl(fd, F_SETLK, &l);
		if (err == 0) {
			/* We now have a lock set on the first byte. */
			gOtherSessionRunning = 0;
			time(&now);
			sprintf(pidbuf, "%5d %s", (int) getpid(), ctime(&now));
			write(fd, pidbuf, strlen(pidbuf) + 1);
		} else {
			/* Could not lock;  maybe another ncftp process has
			 * already locked it.
			 */
			gOtherSessionRunning = 1;
		}
	}
#endif	/* (LOCK_METHOD == 1) */

#if (LOCK_METHOD == 2)	/* BSD's flock */
	int err;

	OurDirectoryPath(gLockFileName, sizeof(gLockFileName), kLockFileName);
	if ((fd = open(gLockFileName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR)) < 0) {
		Error(kDoPerror, "Could not open lock file %s.\n", gLockFileName);
		sleep(5);

		/* Try to save data anyway. */
		gOtherSessionRunning = 0;
	} else {
		err = flock(fd, LOCK_EX | LOCK_NB);
		if (err == 0) {
			/* We now have a lock set on the whole file. */
			gOtherSessionRunning = 0;
			time(&now);
			sprintf(pidbuf, "%5d %s", (int) getpid(), ctime(&now));
			write(fd, pidbuf, strlen(pidbuf) + 1);
		} else {
			/* Could not lock;  maybe another ncftp process has
			 * already locked it.
			 */
			gOtherSessionRunning = 1;
		}
	}
#endif	/* (LOCK_METHOD == 2) */

#if (LOCK_METHOD == 3)	/* Cheezy lock */
	/* For this locking mechanism, we consider it locked if another
	 * process had already created the lockfile.  Unfortunately we
	 * aren't guaranteed to get the lock file removed if the program
	 * crashes somewhere without explicitly removing the lockfile.
	 */

	OurDirectoryPath(gLockFileName, sizeof(gLockFileName), kLockFileName);
	if ((fd = open(gLockFileName, O_CREAT | O_EXCL | O_WRONLY, S_IWUSR)) < 0) {
		gOtherSessionRunning = 1;
	} else {
		gOtherSessionRunning = 0;
		/* Unlike the other two methods, we don't need to keep the file
		 * open.
		 */
		close(fd);
	}
#endif	/* (LOCK_METHOD == 3) */
}	/* CheckForOtherSessions */




void Init(void)
{
	char *cp;
	
	gRealStdout = gStdout = 1;
	InitMalloc();
	InitXferBuffer();
	gSprintfBuf = (char *) malloc((size_t) 4096);
	if (gSprintfBuf == NULL)
		OutOfMemory();

#if (kAlpha > 0)
		sprintf(gVersion, "%s Alpha %d (%s)",
			kVersion,
			kAlpha,
			kVersionDate
		);
#else
#	if (kBeta > 0)
		sprintf(gVersion, "%s Beta %d (%s)",
			kVersion,
			kBeta,
			kVersionDate
		);
#	else
		sprintf(gVersion, "%s (%s)",
			kVersion,
			kVersionDate
		);
#	endif
#endif

	srand((unsigned int) time(NULL));

	GetUserInfo();
	InitDefaultFTPPort();
	if ((cp = (char *) getenv("PAGER")) != NULL)
		STRNCPY(gPager, cp);
	else {
#ifdef MORE
		STRNCPY(gPager, MORE);
#else
		gPager[0] = '\0';
#endif
	}
	gIsFromTTY = gDoingScript = isatty(0);
	gIsToTTY = isatty(1);
	(void) UserLoggedIn();	/* Init parent-death detection. */

	/* Init the mailbox checking code. */
	(void) time(&gMailBoxTime);

	/* Clear the redir buffer. */
	InitLineList(&gRedir);

	(void) GetCWD(gLocalCWD, sizeof(gLocalCWD));

	/* Compute our own private directory's name, and create it
	 * if we have to.  Stuff that depends on this will need
	 * to follow (duh).
	 */
	(void) InitOurDirectory();

	(void) CheckForOtherSessions();
	ReadPrefs();

	OpenLogs();
	ReadBookmarkFile();
	ReadMacroFile();
	InitCommandAndMacroNameList();
	
	++gTotalRuns;
}	/* Init */




void CloseTraceLog(void)
{
	time_t now;

	if (gTraceLogFile != NULL) {
		time(&now);
		fprintf(
			gTraceLogFile,
			"\nSESSION ENDED at %s",
			ctime(&now)
		);
		(void) fclose(gTraceLogFile);
		gTraceLogFile = NULL;
	}
}	/* CloseTraceLog */




void CloseLogs(void)
{
	FILE *new, *old;
	struct stat st;
	long fat;
	string str;
	longstring tmpLog;

#ifdef SYSLOG
	/* Close system log first. */
	closelog();
#endif

	/* Close the debugging log next. */
	CloseTraceLog();

	/* The rest is for the user's log. */
	if (!gLogging)
		return;

	CloseFile(&gLogFile);

	if (gOurDirectoryPath[0] == '\0')
		return;		/* Don't create in root directory. */

	/* If the user wants to, s/he can specify the maximum size of the log file,
	 * so it doesn't waste too much disk space.  If the log is too fat, trim the
	 * older lines (at the top) until we're under the limit.
	 */
	if ((gMaxLogSize < 0) || (stat(gLogFileName, &st) < 0) ||
		((old = fopen(gLogFileName, "r")) == NULL))
		return;						   /* Never trim, or no log. */

	if ((size_t)st.st_size < (size_t)gMaxLogSize)
		return;						   /* Log size not over limit yet. */

	/* Want to make it so we're about 30% below capacity.
	 * That way we won't trim the log each time we run the program.
	 */
	fat = st.st_size - gMaxLogSize + (long) (0.30 * gMaxLogSize);
	DebugMsg("%s was over %ld limit; trimming at least %ld...\n",
		gLogFileName,
		gMaxLogSize,
		fat
	);
	while (fat > 0L) {
		if (fgets(str, (int) sizeof(str), old) == NULL)
			return;
		fat -= (long) strlen(str);
	}
	/* skip lines until a new site was opened */
	while (1) {
		if (fgets(str, (int) sizeof(str), old) == NULL) {
			(void) fclose(old);
			(void) UNLINK(gLogFileName);
			return;					   /* Nothing left, start anew next time. */
		}
		if (! isspace(*str))
			break;
	}

	/* Copy the remaining lines in "old" to "new" */
	OurDirectoryPath(tmpLog, sizeof(tmpLog), kTmpLogName);
	if ((new = fopen(tmpLog, "w")) == NULL) {
		(void) Error(kDoPerror, "Could not open %s.\n", tmpLog);
		return;
	}
	(void) fputs(str, new);
	while (fgets(str, (int) sizeof(str), old) != NULL)
		(void) fputs(str, new);
	(void) fclose(old);
	(void) fclose(new);
	if (UNLINK(gLogFileName) < 0)
		(void) Error(kDoPerror, "Could not delete %s.\n", gLogFileName);
	if (rename(tmpLog, gLogFileName) < 0)
		(void) Error(kDoPerror, "Could not rename %s to %s.\n",
			tmpLog, gLogFileName);
}									   /* CloseLogs */




void SaveHistory(void)
{
	string histFileName;
	FILE *fp;
	LinePtr lp;
	int i;

	if (gWinInit) {
		if (gOurDirectoryPath[0] == '\0')
			return;		/* Don't create in root directory. */
		lp = gCmdHistory.last;
		OurDirectoryPath(histFileName, sizeof(histFileName), kHistoryName);
		fp = fopen(histFileName, "w");
		if ((fp != NULL) && (lp != NULL)) {
			for (i = 1 ; (lp->prev != NULL) && (i < kMaxHistorySaveLines); i++)
				lp = lp->prev;
			for ( ; lp != NULL; lp = lp->next)
				fprintf(fp, "%s\n", lp->line);
			fclose(fp);
		}
	}
}	/* SaveHistory */




void LoadHistory(void)
{
	string histFileName;
	string str;
	FILE *fp;

	InitLineList(&gCmdHistory);
	OurDirectoryPath(histFileName, sizeof(histFileName), kHistoryName);
	fp = fopen(histFileName, "r");
	if (fp != NULL) {
		while (FGets(str, sizeof(str), fp) != NULL)
			AddLine(&gCmdHistory, str);
	}
}	/* LoadHistory */




void StartupMsgs(void)
{
	char curLocalCWD[512];

	if ((gStartupMsgs & kStartupMsg) != 0) {
#if (kAlpha > 0) || (kBeta > 0)
		if (gWinInit == 0)
			PrintF("NcFTP %s, by Mike Gleason.\n", gVersion);
		BoldPrintF(
		"This pre-release is for testing only.  Please do not archive.\n\n");
		BoldPrintF(
		"Check the CHANGELOG file for changes between betas.\n\n");
#else
		if (gWinInit == 0)
			PrintF("NcFTP %s, by Mike Gleason.\n", gVersion);
#endif
	}
	if (gTotalRuns < 2) {
		PrintF("NcFTP is Copyright (C) 1992, Mike Gleason.  All rights reserved.\n\n");

		PrintF("This program is free software; you can redistribute it and/or modify\n");
		PrintF("it under the terms of the GNU General Public License as published by\n");
		PrintF("the Free Software Foundation; either version 2 of the License, or\n");
		PrintF("(at your option) any later version.\n\n");

		PrintF("This program is distributed in the hope that it will be useful,\n");
		PrintF("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
		PrintF("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
		PrintF("GNU General Public License for more details.\n\n");

		PrintF("You should have received a copy of the GNU General Public License\n");
		PrintF("along with this program; if not, write to the Free Software\n");
		PrintF("Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA\n\n");
	} else if ((gStartupMsgs & kTips) != 0) {
		PrintRandomTip();
	}

	if (gOtherSessionRunning) {
		BoldPrintF("Note: Another session is running. Changes made to the\n");
		BoldPrintF("      prefs, bookmarks, and trace files will not be saved.\n\n");
	}

	(void) GetCWD(curLocalCWD, sizeof(curLocalCWD));
	if (gDownloadDir[0] != '\0') {
		/* If set, always try to cd there. */
		ExpandTilde(gDownloadDir, sizeof(gDownloadDir));
		(void) chdir(gDownloadDir);	/* May not succeed. */
	} else if (gRememberLCWD) {
		/* Try restoring the last local directory we were in.
		 * gLocalCWD may be set when the prefs are read in.
		 */
		if (gLocalCWD[0] != '\0')
			(void) chdir(gLocalCWD);	/* May not succeed. */
	}

	/* Print a message if we changed the directory. */
	(void) GetCWD(gLocalCWD, sizeof(gLocalCWD));
	if (strcmp(curLocalCWD, gLocalCWD))
		PrintF("Current local directory is %s.\n", gLocalCWD);
}	/* StartupMsg */




void Startup(void)
{
	if (gStartup == 0) {
		gStartup = 1;
		InitWindows();
		StartupMsgs();
		(void) RunPrefixedMacro("start.", "ncftp");
 		InitReadline();
	}
}	/* Startup */




/*ARGSUSED*/
static void
SigIntMain(/* int sigNum */ void)
{
	SIGNAL(SIGINT, SigIntMain);
	alarm(0);
	EPrintF("\n*Interrupt*\n");
	longjmp(gMainJmp, 1);
}   /* SigIntMain */



void main(int argc, char **argv)
{
	int opt, result;
	OpenOptions openopt;

	Init();
	RunStartupScript();

	GetoptReset();
	gOptErr = 0;
	if (GetOpenOptions(argc, argv, &openopt, 1) == kUsageErr)
		goto usage;

	gOptErr = 1;
	GetoptReset();
	gVisualMode = gDefaultVisualMode;

	/* Note that we getopt on three sets of options, one for open,
	 * one for get, and one for the program.  That means that these
	 * commands' flags and the program must use mutually exclusive
	 * sets of flags.
	 */
	while ((opt = Getopt(argc, argv, "aiup:rd:g:cmCfGRn:zDLVH")) >= 0) {
		if (strchr("aiup:rd:g:cmCfGRn:z", opt) == NULL) {
			switch (opt) {
				case 'D':
					gDebug = kDebuggingOn;
					gTrace = kTracingOn;
					break;
				case 'L': gVisualMode = 0; break;
				case 'V': gVisualMode = 1; break;
				case 'H': VersionCmd(); Exit(kExitNoErr); break;
				default:
				usage:
					EPrintF(
		"Usage: ncftp [options] [hostname[:path]]\n");
					EPrintF("Program options:\n\
  -D   : Turn debug mode and trace mode on.\n\
  -L   : Don't use visual mode (use line mode).\n\
  -V   : Use visual mode.\n\
  -H   : Dump the version information.\n");
					EPrintF("Command-line open options:\n\
  -a   : Open anonymously.\n\
  -u   : Open with username and password prompt.\n\
  -p X : Use port number X when opening.\n\
  -r   : Redial until connected.\n\
  -d X : Redial, delaying X seconds between tries.\n\
  -g X : Give up after X redials without connection.\n");
					EPrintF("Command-line retrieve options:\n\
  -C   : Force continuation (reget).\n\
  -f   : Force overwrite.\n\
  -G   : Don't use wildcard matching.\n\
  -R   : Recursive.  Useful for fetching whole directories.\n\
  -n X : Get selected files only if X days old or newer.\n");
					Exit(kExitUsageErr);
			}
		}
	}

	result = kNoErr;
	if (openopt.hostname[0] != '\0') {
		if (setjmp(gMainJmp)) {
			result = kNoErr;
		} else {
			SIGNAL(SIGINT, SigIntMain);
			SIGNAL(SIGPIPE, SIG_IGN);
			if (setjmp(gCmdLoopJmp)) {
				/* May have lost connection during Open. */
				result = kCmdErr;
			} else {
				result = Open(&openopt);
			}
		}
	}

	if (result == kNoErr) {
		Startup();		/* Init the interactive shell. */
		CommandShell();
		DoQuit(kExitNoErr);
	}
	DoQuit(kExitColonModeFail);
	/*NOTREACHED*/
	Exit(kExitNoErr);
}	/* main */

/* eof Main.c */
