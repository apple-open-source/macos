/* List.c */

#include "Sys.h"

#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "RCmd.h"
#include "Xfer.h"
#include "Cmds.h"
#include "List.h"
#include "Glob.h"
#include "Sio.h"
#include "Bookmark.h"
#include "Complete.h"

jmp_buf gPDirJmp;
jmp_buf gLocalListJmp;
LineList gRedir;
extern int gStdout;
extern longstring gPager;
extern Bookmark gRmtInfo;
extern int gNetworkTimeout;

int
ListReceiveProc(XferSpecPtr xp)
{
	int nread;
	int nwrote;
	int fd;
	char xbuf[256];
	char buf2[256];

	fd = xp->outStream;
	for (;;) {
		nread = BufferGets(xbuf, sizeof(xbuf), xp);
		if (nread <= 0)
			break;

		if (xp->outIsTTY != 0) {
			if (xbuf[nread - 1] == '\n')
				xbuf[--nread] = '\0';
			MakeStringPrintable(buf2, (unsigned char *) xbuf, sizeof(buf2));
			MultiLinePrintF("%s\n", buf2);
			AddLine(&gRedir, buf2);
			CompleteParse(buf2);
		} else {
			nwrote = Swrite(xp->outStream, xbuf, nread, gNetworkTimeout);
			if (nwrote < 0) {
				nread = nwrote;
				break;
			}
			if (xbuf[nread - 1] == '\n')
				xbuf[--nread] = '\0';
			AddLine(&gRedir, xbuf);
			CompleteParse(xbuf);
		}

	}
	return (nread);	/* 0 or -1 */
}	/* ListReceiveProc */




int
ListToMemoryReceiveProc(XferSpecPtr xp)
{
	int nread;
	char xbuf[256];

	for (;;) {
		nread = BufferGets(xbuf, sizeof(xbuf), xp);
		if (nread <= 0)
			break;

		if (xbuf[nread - 1] == '\n')
			xbuf[--nread] = '\0';
		AddLine((LineListPtr) xp->miscPtr, xbuf);
	}
	return (nread);	/* 0 or -1 */
}	/* ListToMemoryReceiveProc */




void
ListToMemory(LineListPtr fileList, char *cmd, char *flags, char *what)
{
	XferSpecPtr xp;

	fileList->first = fileList->last = NULL;

	/* Setup the parameter block for RDataCmd.  Note that we purposely
	 * leave the other fields unset, since we don't use them, and
	 * being zeroed-out is okay.
	 */
	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->xProc = ListToMemoryReceiveProc;
	xp->outStream = -1;			/* Not used... */
	xp->miscPtr = fileList;
	
	SETASCII;	/* Directory listings should be in ascii mode. */
	if ((flags == NULL) || (gRmtInfo.isUnix == 0))
		(void) RDataCmd(xp, "%s %s", cmd, what);
	else
		(void) RDataCmd(xp, "%s %s %s", cmd, flags, what);
		
	DoneWithXferSpec(xp);			/* Doesn't dispose miscPtr. */
}	/* ListToMemory */




int
FileListReceiveProc(XferSpecPtr xp)
{
	int nread;
	longstring str, buf;
	char *cp;
	int c;

	for (;;) {
		nread = BufferGets(buf, sizeof(buf), xp);
		if (nread <= 0)
			break;
		cp = buf;

		/* Unix dependency: ls message */
		if (strncmp(buf, "can not access", SZ(14)) == 0) {
			EPrintF("%s", buf);
			return (0L);
		}

		cp += nread - 2;	/* Want last char of name, but also have \n. */
		if (gRmtInfo.isUnix) {
			c = *cp;
			switch(c) {
				/* Unix dependency:  ls -F format. */
				case '/': c = 'd'; goto subt;
				case '@': c = 'l'; goto subt;
				case '*': c = '-'; goto subt;
				/* No (supported) suffix, so treat it like a plain file. */
				default:  c = '-'; cp[1] = '\0'; break;
				subt: *cp = '\0';	/* Remove suffix. */
			}
		} else {
			/* For non unix, just have to assume file. */
			c = '-';
			cp[1] = '\0';
		}

		/* Just want the item names, no path prefixes. */
		cp = strrchr(buf, '/');
		if (cp != NULL) {
			cp = Strncpy(str, cp + 1, sizeof(str));
		} else {
			cp = Strncpy(str, buf, sizeof(str));
		}

		/* We write the one-char file type, followed by the item name. */
		*buf = c;
		(void) Strncpy(buf + 1, str, sizeof(buf) - 1);
		AddLine((LineListPtr) xp->miscPtr, buf);
	}
	return (nread);	/* 0 or -1 */
}	/* FileListReceiveProc */




void GetFileList(LineListPtr fileList, char *what)
{
	XferSpecPtr xp;

	fileList->first = fileList->last = NULL;

	/* Setup the parameter block for RDataCmd.  Note that we purposely
	 * leave the other fields unset, since we don't use them, and
	 * being zeroed-out is okay.
	 */
	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->xProc = FileListReceiveProc;
	xp->outStream = -1;			/* Not used... */
	xp->miscPtr = fileList;
	
	SETASCII;	/* Directory listings should be in ascii mode. */
	if (gRmtInfo.isUnix)
		(void) RDataCmd(xp, "NLST -F %s", what);
	else
		(void) RDataCmd(xp, "NLST %s", what);
	DoneWithXferSpec(xp);			/* Doesn't dispose miscPtr. */
}	/* GetFileList */




int DoList(int argc, char **argv, char *lsMode)
{
	char flags[64];
	char flags2[64];
	char *cmd;
	char thingsToList[256];
	int i, wildcards;
	XferSpecPtr xp;

	SETASCII;	/* Directory listings should be in ascii mode. */
	thingsToList[0] = '\0';
	flags2[0] = '\0';
	wildcards = 0;

	if (STREQ(lsMode, kListLongMode)) {
		cmd = "LIST";
		CompleteSetFlags("-l");
		flags[0] = '\0';
	} else {
		cmd = "NLST";
		STRNCPY(flags, lsMode + 1);
		CompleteSetFlags(lsMode);
	}

	/* Go through and find all the things that look like dash-flags,
	 * and combine them into one string.  We do the same thing
	 * for the items to list, so we'll have to groups of things
	 * to hand to RDataCmd.
	 */
	for (i=1; i<argc; i++) {
		CompleteSetFlags(argv[i]);
		if (argv[i][0] == '-')
			STRNCAT(flags2, argv[i] + 1);
		else {
			STRNCAT(thingsToList, " ");
			STRNCAT(thingsToList, argv[i]);			
			if (GLOBCHARSINSTR(argv[i]))
				wildcards = 1;
		}
	}

	/* For some unknown reason, traditional servers can do "LIST -t *.tar"
	 * but not "NLST -t *.tar."  This kludges around that limitation.
	 */
	if ((wildcards) && STREQ(cmd, "NLST")) {
		if (flags2[0] == '\0') {
			/* They didn't give any other flags, so use NLST, but without
			 * our usual -CF, but with the wildcard expression.
			 */
			flags[0] = '\0';
		} else {
			/* They gave some other flags, but print them a warning, and
			 * switch them over to LIST, and retain their flags.
			 */
			cmd = "LIST";
			STRNCPY(flags, flags2);
			BoldPrintF("Warning: cannot use both flags and wildcards with ls, using dir instead.\n");
		}
	} else {
		/* Append their flags to the default flags. */
		STRNCAT(flags, flags2);
	}

	/* Setup the parameter block for RDataCmd.  Note that we purposely
	 * leave the other fields unset, since we don't use them, and
	 * being zeroed-out is okay.
	 */
	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->xProc = ListReceiveProc;
	/* xp->inStream = gDataSocket;  RDataCmd fills this in when it gets it. */
	xp->outStream = gStdout;
	/* The rest of the xp parameters can be left zeroed, since we won't
	 * be using them, and progress reports will not be activated, so
	 * they won't use them either.
	 */
	 
	MultiLineInit();

	/* Dispose previous buffer, and also be set up for a new one. */
	DisposeLineListContents(&gRedir);

	if ((flags[0]) && (gRmtInfo.isUnix != 0))
		(void) RDataCmd(xp, "%s -%s%s", cmd, flags, thingsToList);
	else
		(void) RDataCmd(xp, "%s%s", cmd, thingsToList);

	DoneWithXferSpec(xp);

	return 0;
}	/* DoList */




static
void PDirHandler(void)
{
	alarm(0);
	longjmp(gPDirJmp, 1);
}	/* PDirHandler */




/* Do a remote directory listing, to the screen (or file), or
 * through your pager.
 */
int ListCmd(int argc, char **argv)
{
	char *cp;
	char *volatile lsMode;
	volatile int result;
	volatile int pageMode;
	FILE *volatile pager;
	volatile int saved;
	volatile Sig_t si, sp;

	result = kNoErr;
	cp = argv[0];
	pageMode = kNoPaging;
	pager = NULL;
	saved = -1;
	lsMode = kListShortMode;
	if (*cp == 'p') {
		/* If the command started with 'p' we should be paging this. */
		if (gPager[0] == '\0') {
			EPrintF("You haven't specified a program to use as a pager.\n");
			EPrintF("You can set this from the preferences screen (prefs command).\n");
			return (-1);
		}
		++cp;
		pageMode = kPageMode;
		pager = POpen(gPager, "w", 1);
		if (pager == NULL) {
			RestoreScreen(1);
			Error(kDoPerror, "Could not open: %s\n", gPager);
			return (-1);
		}
		
		/* Now replace our own stdout stream with the pager stream,
		 * so that writes go to the pager instead.
		 */
		saved = (volatile int) gStdout;
		gStdout = fileno((FILE *) pager);
	}
	if (*cp == 'd')
		lsMode = kListLongMode;		/* i.e. "dir" or "pdir" */

	CompleteStart(".");
	if (setjmp(gPDirJmp) == 0) {
		sp = SIGNAL(SIGPIPE, PDirHandler);
		si = SIGNAL(SIGINT, PDirHandler);
		result = DoList(argc, argv, lsMode);
	}
	CompleteFinish();

	if (pageMode == kPageMode) {
		/* Cleanup the mess we made. */
		(void) SIGNAL(SIGPIPE, SIG_IGN);
		(void) SIGNAL(SIGINT, SIG_IGN);
		if (pager != NULL)
			PClose((FILE *) pager);
		gStdout = saved;
		RestoreScreen(1);
	}
	(void) SIGNAL(SIGPIPE, sp);
	(void) SIGNAL(SIGINT, si);
	return (result);
}	/* ListCmd */




static void SigLocalList(void)
{
	alarm(0);
	longjmp(gLocalListJmp, 1);
}	/* SigLocalList */



/* Run ls on the local host.  This is mostly useful for visual mode, where
 * it is troublesome to spawn commands.
 */
int LocalListCmd(int argc, char **argv)
{
	volatile FILE *fp;
	VSig_t si, sp;
	longstring str;
	int i, cf;

	si = (VSig_t) kNoSignalHandler;
	sp = (VSig_t) kNoSignalHandler;

	str[0] = '\0';
	STRNCPY(str, LS);
	cf = 1;
	if (argc > 1) {
		/* Look for "-l", and turn of -CF if we find it. */
		for (i=1; (i < argc) && (argv[i][0] == '-'); i++) {
			if (strchr(argv[i], 'l') != NULL) {
				cf = 0;
				break;
			}
		}
	}
	if (cf)
		STRNCAT(str, " -CF");
	for (i=1; i<argc; i++) {
		STRNCAT(str, " ");
		STRNCAT(str, argv[i]);
	}

	DebugMsg("%s\n", str);
	fp = (volatile FILE *) POpen(str, "r", 0);
	if (fp != NULL) {
		if (setjmp(gLocalListJmp) == 0) {
			/* Command was not interrupted. */
			si = SIGNAL(SIGINT, SigLocalList);
			sp = SIGNAL(SIGPIPE, SigLocalList);
			MultiLineInit();
			while (fgets(str, ((int) sizeof(str)) - 1, (FILE *)fp) != NULL)
				MultiLinePrintF("%s", str);
		}
		(void) SIGNAL(SIGINT, SIG_IGN);
		(void) SIGNAL(SIGPIPE, SIG_IGN);
		PClose((FILE *) fp);
	}

	if (si != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGINT, si);
	if (sp != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGPIPE, sp);
	return (kNoErr);
}	/* LocalListCmd */




int RedirCmd(int argcUNUSED, char **argv)
{
	LinePtr lp;
	volatile FILE *pager;
	VSig_t si, sp;

	si = (VSig_t) kNoSignalHandler;
	sp = (VSig_t) kNoSignalHandler;

	if (argv[0][0] == 'p') {
		/* If the command started with 'p' we should be paging this. */
		if (gPager[0] == '\0') {
			EPrintF("You haven't specified a program to use as a pager.\n");
			EPrintF("You can set this from the preferences screen (prefs command).\n");
			return (-1);
		}
		pager = (volatile FILE *) POpen(gPager, "w", 1);
		if (pager == NULL) {
			Error(kDoPerror, "Could not open: %s\n", gPager);
			return (-1);
		}
		if (setjmp(gPDirJmp) == 0) {
			/* Command was not interrupted. */
			sp = SIGNAL(SIGPIPE, PDirHandler);
			si = SIGNAL(SIGINT, PDirHandler);
			for (lp = gRedir.first; lp != NULL; lp = lp->next)
				fprintf((FILE *) pager, "%s\n", lp->line);
			PClose((FILE *) pager);
			RestoreScreen(1);
		}
	} else {
		MultiLineInit();
		for (lp = gRedir.first; lp != NULL; lp = lp->next)
			MultiLinePrintF("%s\n", lp->line);
	}
	if (si != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGINT, si);
	if (sp != (VSig_t) kNoSignalHandler)
		(void) SIGNAL(SIGPIPE, sp);
	return (kNoErr);
}	/* RedirCmd */
