/* Progress.c */

#include "Sys.h"
#include "Curses.h"

#include "Util.h"
#include "Cmds.h"
#include "Xfer.h"
#include "Progress.h"
#include "GetPass.h"
#include "Bookmark.h"
#include "Main.h"

/* NOTE: There is more commentary at the bottom of this source file. */

/* The progress meter the user requested us to use.  We may not actually be
 * able to use this one, if certain conditions are not met.
 */
int gWhichProgMeter = PROGRESS;

/* Before each report, this is set to the elapsed time between the
 * starting and current time.
 */
struct timeval gElapsedTVal;
										
/* These are the different progress meter functions the user can
 * choose from.  The order must correspond to the equates in progress.h!
 */
ProgressMeterProc gProgressMeterProcs[kPrLast + 1] = {
	PrNone,
	PrPercent,
	PrPhilBar,
	PrKBytes,
	PrDots,
	PrStatBar
};

long gTotalXferHSeconds = 0L, gTotalXferKiloBytes = 0L;

#ifdef SIGTSTP
static int gStoppedInProgressReport;
#endif

extern int gVerbosity, gIsFromTTY, gScreenWidth;
extern FILE *gLogFile;
extern longstring gRemoteCWD;
extern Bookmark gRmtInfo;

#ifdef USE_CURSES
extern int gWinInit;
extern WINDOW *gListWin;
void WAttr(WINDOW *w, int attr, int on);
#endif	/* USE_CURSES */

#ifdef SYSLOG
extern UserInfo gUserInfo;
extern string gActualHostName;
#endif

/* Difftime(), only for timeval structures.  */
void TimeValSubtract(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0) {
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
	}
}									   /* TimeValSubtract */



/* Computes how fast the transfer went. */
double TransferRate(
	long bytes,
	struct timeval *start,
	struct timeval *end,
	char **units,
	double *elapsedSecs
)
{
	struct timeval td;
	double unitsPerSec;

	TimeValSubtract(&td, end, start);
	*elapsedSecs = td.tv_sec + ((double) td.tv_usec / 1000000.0);
	
	unitsPerSec = 0.0;
	*units = "Bytes/s";

	if (*elapsedSecs > 0.0) {
		unitsPerSec = bytes / *elapsedSecs;
		if (unitsPerSec > (1024.0 * 1024.0)) {
			unitsPerSec /= (1024.0 * 1024.0);
			*units = "MB/s";
		} else if (unitsPerSec > 1024.0) {
			unitsPerSec /= 1024.0;
			*units = "kB/s";
		}
	}
	
	return (unitsPerSec);
}	/* TransferRate */




int PrNone(XferSpecPtr xp, int mode)
{
	xp->doReports = 0;
	if (mode == kPrEndMsg && gIsFromTTY)
		return kPrWantStatsMsg;
	return 0;
}	/* PrNone */




/* This progress meter function displays the percentage of the file
 * we've transferred so far;  it's simple and unobtrusive.
 */
int PrPercent(XferSpecPtr xp, int mode)
{
	int result = kPrWantStatsMsg;
	int perc;
	
	switch (mode) {
		case kPrInitMsg:
			EPrintF("%s:     ", xp->localFileName);
			break;
		case kPrUpdateMsg:
			perc = (int) (100 * xp->frac);
			EPrintF("\b\b\b\b%3d%%", perc);
			break;
		case kPrEndMsg:
			result = kPrWantStatsMsg;
			EPrintF("\b\b\b\b");
			break;
	}
	return result;
}	/* PrPercent */




/* This one is the brainchild of my comrade, Phil Dietz.  It shows the
 * progress as a fancy bar graph.
 */
int PrPhilBar(XferSpecPtr xp, int mode)
{
	int result = kPrWantStatsMsg;
	int perc;
	long s;
	int curBarLen;
	static int maxBarLen;
	str64 spec1, spec3;
	static string bar;
	int i;
	int secsLeft, minLeft;

	switch (mode) {
		case kPrInitMsg:
			EPrintF("%s file: %s \n",
				NETREADING(xp) ? "Receiving" : "Sending",
				xp->localFileName
			);
			
			for (i=0; i < (int) sizeof(bar) - 1; i++)
				bar[i] = '=';
			bar[i] = '\0';

			/* Compute the size of the bar itself.  This sits between
			 * two numbers, one on each side of the screen.  So the
			 * bar length will vary, depending on how many digits we
			 * need to display the size of the file.
			 */
			maxBarLen = gScreenWidth - 1 - 28;
			for (s = xp->expectedSize; s > 0; s /= 10L)
				maxBarLen--;
			
			/* Create a specification we can hand to printf. */
			(void) sprintf(spec1, "      0 %%%ds %%ld bytes. ETA: --:--",
				maxBarLen);
				
			/* Print the first invocation, which is an empty graph
			 * plus the other stuff.
			 */
			EPrintF(spec1, " ", xp->expectedSize);
			break;
		case kPrUpdateMsg:
			/* Compute how much of the bar should be colored in. */
			curBarLen = (int) (xp->frac * (double)maxBarLen);

			/* Colored portion must be at least one character so
			 * the spec isn't '%0s' which would screw the right side
			 * of the indicator.
			 */
			if (curBarLen < 1)
				curBarLen = 1;
			
			bar[curBarLen - 1] = '>';
			bar[curBarLen] = '\0';

			/* Make the spec, so we can print the bar and the other stuff. */
			STRNCPY(spec1, "\r%3d%%  0 ");
			(void) sprintf(spec3, "%%%ds %%ld bytes. %s%%3d:%%02d", 
				maxBarLen - curBarLen,
				"ETA:"
			);
			
			/* We also show the percentage as a number at the left side. */
			perc = (int) (100.0 * xp->frac);
			
			/* Guess how much time is remaining in the transfer, based on
			 * the current transfer statistics.
			 */
			secsLeft = (int) ((xp->bytesLeft / xp->bytesPerSec) + 0.5);
			minLeft = secsLeft / 60;
			secsLeft = secsLeft - (minLeft * 60);
			if (minLeft > 999) {
				minLeft = 999;
				secsLeft = 59;
			}
			
			/* Print the updated information. */
#ifdef USE_CURSES
			if (gWinInit) {
				EPrintF(spec1, perc);	
				WAttr(gListWin, kReverse, 1);
				EPrintF("%s", bar);
				WAttr(gListWin, kReverse, 0);
				EPrintF(spec3,
					"",
					xp->expectedSize,
					minLeft,
					secsLeft
				);
			} else
#endif	/* USE_CURSES */
			{
				EPrintF(spec1, perc);	
				EPrintF("%s", bar);
				EPrintF(spec3,
					"",
					xp->expectedSize,
					minLeft,
					secsLeft
				);
			}

			bar[curBarLen - 1] = '=';
			bar[curBarLen] = '=';

			break;
		case kPrEndMsg:
			result = kPrWantStatsMsg;
			EPrintF("\n");
			break;
	}
	return result;
}	/* PrPhilBar */



static double
FileSize(double size, char **uStr0, double *uMult0)
{
	double uMult, uTotal;
	char *uStr;

	if (size > kGigabyte) {
		uStr = "GB";
		uMult = kGigabyte;
	} else if (size > kMegabyte) {
		uStr = "MB";
		uMult = kMegabyte;
	} else if (size > kKilobyte) {
		uStr = "kB";
		uMult = 1024;
	} else {
		uStr = "B";
		uMult = 1;
	}
	if (uStr0 != NULL)
		*uStr0 = uStr;
	if (uMult0 != NULL)
		*uMult0 = uMult;
	uTotal = size / ((double) uMult);
	return (uTotal);
}	/* FileSize */




int PrStatBar(XferSpecPtr xp, int mode)
{
	int result = kPrWantStatsMsg;
	double rate, done;
	int secLeft, minLeft;
	char *rStr;
	static char *uStr;
	static double uTotal, uMult;
	char localName[32];
	string line;
	int i;

	switch (mode) {
		case kPrInitMsg:
			uTotal = FileSize((double) xp->expectedSize, &uStr, &uMult);

			/* Leave room for a ':' and '\0'. */
			AbbrevStr(localName, xp->localFileName, 30, 0);
			STRNCAT(localName, ":");
			EPrintF("%-32s", localName);
			break;
		case kPrUpdateMsg:
			/* Guess how much time is remaining in the transfer, based on
			 * the current transfer statistics.
			 */
			secLeft = (int) ((xp->bytesLeft / xp->bytesPerSec) + 0.5);
			minLeft = secLeft / 60;
			secLeft = secLeft - (minLeft * 60);
			if (minLeft > 999) {
				minLeft = 999;
				secLeft = 59;
			}

			rate = FileSize(xp->bytesPerSec, &rStr, NULL);
			done = (double) xp->bytesTransferred / uMult;

			AbbrevStr(localName, xp->localFileName, sizeof(localName) - 2, 0);
			STRNCAT(localName, ":");

			sprintf(line, "%-32s   %5.1f/%.1f %s   %5.1f %s/s   ETA %3d:%02d",
				localName,
				done,
				uTotal,
				uStr,
				rate,
				rStr,
				minLeft,
				secLeft
			);

			/* Pad the rest of the line with spaces, to erase any
			 * stuff that might have been left over from the last
			 * update.
			 */
			for (i=strlen(line); i < gScreenWidth - 2; i++)
				line[i] = ' ';
			line[i] = '\0';

			/* Print the updated information. */
			EPrintF("\r%s", line);

			break;
		case kPrEndMsg:
			result = kPrWantStatsMsg;
			EPrintF("\r");
			break;
	}
	return result;
}	/* PrStatBar */




/* This one is handy when we don't know the size of the file we're
 * transferring, but we still want to have a progress meter.
 */
int PrKBytes(XferSpecPtr xp, int mode)
{
	int result = 0;

	switch (mode) {
		case kPrInitMsg:
			EPrintF("%s:       ", xp->localFileName);
			break;
		case kPrUpdateMsg:
			EPrintF("\b\b\b\b\b\b%5ldK", (xp->bytesTransferred +
				xp->startPoint) / 1024);
			break;
		case kPrEndMsg:
			result = kPrWantStatsMsg;
			EPrintF("\b\b\b\b\b\b");
			break;
	}
	return result;
}	/* PrKBytes */




/* This progress meter spews as little output as possible.  It uses
 * no backspaces or ANSI escapes nor does it ask for transfer stats to
 * be printed.
 */
int PrDots(XferSpecPtr xp, int mode)
{
	static int dotsPrinted;
	int newDots;

	switch (mode) {
		case kPrInitMsg:
			dotsPrinted = 0;
			EPrintF("%s: ", xp->localFileName);
			break;
		case kPrUpdateMsg:
			if (xp->expectedSize <= 0)
				newDots = 10;
			else
				newDots = (LOCALSIZE(xp) * 10 / xp->expectedSize) + 1;
			while ((dotsPrinted < newDots) && (dotsPrinted < 10)) {
				EPrintF(".");
				dotsPrinted++;
			}
			break;
		case kPrEndMsg:
			for (; dotsPrinted < 10; dotsPrinted++)
				EPrintF(".");
			EPrintF("\n");
			break;
	}
	return 0;
}	/* PrDots */




/* This determines which progress meter we can use, and sends an
 * init message to the progress meter function to be used.  Note
 * that we may have to use an alternative progress meter if the
 * one the user requested won't work under the current conditions (like
 * not being able to get the remote file size would disable any
 * prog meter that needs to know that).
 *
 * This routine takes care of turning off echoing to stdin and flushes
 * the stderr stream, so individual progress meter functions need not
 * worry about that.
 */
int StartProgress(XferSpecPtr xp)
{
	int progMeterInUse;

	/* If the user doesn't want any unnecessary output, or is in the
	 * background, don't have progress reports at all.  Also don't
	 * do reports if the xp said explicitly not to do.  For example,
	 * we don't want any reports for directory listings.
	 */
	if ((gVerbosity == kQuiet) || (!xp->doReports) || (!InForeGround())) {
		progMeterInUse = kPrNone;
		xp->doReports = 0;
	} else {
		progMeterInUse =  gWhichProgMeter;
		
		/* If the progress meter number is out of range, use a default value. */
		if ((progMeterInUse > kPrLast) || (progMeterInUse < 0))
			progMeterInUse = PROGRESS;
		
		/* If we couldn't determine the size of the file in advance,
		 * we can't use any progress meter that needs to know the size.
		 */
		if (xp->expectedSize == kSizeUnknown)
			progMeterInUse = kPrKBytes;	/* Only one that doesn't need size. */
	}

	/* Get a pointer to the progress meter function to use. */
	xp->prProc = gProgressMeterProcs[progMeterInUse];

	/* Make a note of the start time, so we can see how long this takes. */
	(void) Gettimeofday(&xp->startTime);
	
	/* Initialize the seconds counter to the current time.  We don't do
	 * a progress report after each block read.  Instead, we wait at least
	 * 'kDelaySeconds' seconds before attempting an update.  But this time
	 * we set it so we do an update after the very first block.
	 */
	xp->timeOfNextUpdate = xp->startTime.tv_sec;
	
	/* This should have been zeroed automatically when you initialized
	 * the structure, but for completeness, do it ourselves here.
	 */
	xp->bytesTransferred = 0;

	(*xp->prProc)(xp, kPrInitMsg);
	
	/* The progress report proc may decide for itself whether progress
	 * reports may be used, so don't do the following if progress
	 * reports got turned off at the initialization step.
	 */
	if (xp->doReports) {
		FlushListWindow();
		Echo(stdin, 0);
	}
	
	/* Just a nice way for us to quickly tell which one we're using,
	 * without looking at function pointers.
	 */
	xp->progMeterInUse = progMeterInUse;
	return (progMeterInUse);
}									   /* StartProgress */



#ifdef SIGTSTP
static void ProgressSuspsend(int ignored)
{
	gStoppedInProgressReport = 1;
}	/* ProgressSuspsend */
#endif




/* This should be called after each "block" in the transfer.  We check
 * here to see if we should bother updating the progress meter.  We
 * won't update if we had already updated within the last second or so.
 */
void ProgressReport(XferSpecPtr xp, int forceUpdate)
{
	double frac;
#ifdef SIGTSTP
	Sig_t sts;
#endif

	if (xp->doReports) {
		/* Check the current time. */
		(void) Gettimeofday(&xp->endTime);
		
		/* Won't update unless it's past the 'timeOfNextUpdate'
		 * or we got a message saying to do your last update.
		 */
		if ((xp->endTime.tv_sec > xp->timeOfNextUpdate) || forceUpdate) {
			/* Won't do updates anymore if we get backgrounded. */
			xp->doReports = InForeGround();
			if (xp->doReports != 0) {
#ifdef SIGTSTP
				/* The user could hit ^Z right when we are trying to do
				 * a progress report.  This could mean that as soon as the
				 * user put the program in the background, we would output
				 * the progress report, which would suspend the process
				 * because of tty output.
				 *
				 * We try to avoid that scenario by temporarily putting off
				 * the ^Z, doing the progress report, then sending us the
				 * suspend signal again.
				 */
				sts = SIGNAL(SIGTSTP, SIG_IGN);
				if (sts != SIG_IGN) {
					gStoppedInProgressReport = 0;
					SIGNAL(SIGTSTP, ProgressSuspsend);
				}
#endif
				/* Figure out how long the transfer has been going. */
				TimeValSubtract(&gElapsedTVal, &xp->endTime,
					&xp->startTime);
				
				/* Get current transfer duration. */
				xp->secsElap = (gElapsedTVal.tv_usec / 1000000.0)
					+ gElapsedTVal.tv_sec;

				/* Get current transfer rate. */
				if ((xp->secsElap <= 0.0) || (xp->bytesTransferred == 0)) {
					xp->bytesPerSec = 1.0;		/* Don't set to 0. */
				} else {
					xp->bytesPerSec = ((double) xp->bytesTransferred)
						/ xp->secsElap;
				}

				/* Compute how much we've done so far, if we can. */
				if (xp->expectedSize > 0) {
					xp->bytesLeft = xp->expectedSize - LOCALSIZE(xp);

					frac = (double) LOCALSIZE(xp)
						/ (double) xp->expectedSize;
					if (frac > 1.0)
						frac = 1.0;
					else if (frac < 0.0)
						frac = 0.0;
					xp->frac = frac;
				} else {
					xp->frac = 0.0;
				}
				
				/* Have this progress meter do its thing. */
				(*xp->prProc)(xp, kPrUpdateMsg);
			
				FlushListWindow();
				/* Compute the next update time. */
				xp->timeOfNextUpdate = xp->endTime.tv_sec + kDelaySeconds - 1;
#ifdef SIGTSTP
				if (sts != SIG_IGN) {
					SIGNAL(SIGTSTP, sts);
					if (gStoppedInProgressReport != 0)
						kill(getpid(), SIGTSTP);
				}
#endif
				/* Figure out how long the transfer has been going. */
			} else {
				return;
			}
		}
		
		/* Won't do reports if the user isn't logged in to see them. */
		xp->doReports = UserLoggedIn();
	}
}									   /* ProgressReport */




/* This routine should be called after a transfer finishes, even if no
 * progress reports were done.  Besides cleaning up the progress stuff,
 * we also do our logging here.
 */
void EndProgress(XferSpecPtr xp)
{
	double elapsedTime, xRate, xferred;
	char *unitStr;
	char *shortName;
	string statMsg;
	longstring fullRemote;
	long kb, hsecs;
	int wantStats;
	int localFileIsStdout;

	wantStats = 0;
	if ((xp->doReports) && (xp->bytesTransferred > 0) && (gVerbosity != kQuiet)) {
		ProgressReport(xp, kPrLastUpdateMsg);
		wantStats = (InForeGround()) &&
			((*xp->prProc)(xp, kPrEndMsg) == kPrWantStatsMsg);
	}
	(void) Gettimeofday(&xp->endTime);

	/* Compute transfer stats. */
	xRate = TransferRate(
		xp->bytesTransferred,
		&xp->startTime,
		&xp->endTime,
		&unitStr,
		&elapsedTime
	);

	/* Print the stats, if requested. */
	if (wantStats) {
		shortName = strrchr(xp->localFileName, '/');
		if (shortName == NULL)
			shortName = xp->localFileName;
		else
			shortName++;
		sprintf(statMsg, "%s:  %ld bytes %s%s in %.2f seconds",
			shortName,
			xp->bytesTransferred,
			NETREADING(xp) ? "received" : "sent",
			xp->startPoint ? " and appended to existing file" : "",
			elapsedTime
		);
		if (xRate > 0.0) {
			sprintf(statMsg + strlen(statMsg), ", %.2f %s",
				xRate,
				unitStr
			);
		}
		STRNCAT(statMsg, ".");
	
		/* Make sure echoing is back on! */
		Echo(stdin, 1);

		/* Make sure the rest of the line is padded with spaces, so it will
		 * erase junk that may have been leftover from a progress meter.
		 */
		EPrintF("%-79s\n", statMsg);
		FlushListWindow();
	} else {
		if (xRate > 0.0) {
			DebugMsg("%ld bytes transferred in %.2f seconds, %.2f %s.\n",
				xp->bytesTransferred,
				elapsedTime,
				xRate,
				unitStr
			);
		} else {
			DebugMsg("%ld bytes transferred in %.2f seconds.\n",
				xp->bytesTransferred,
				elapsedTime
			);
		}
	}

	/* Only log stuff if there was a remote filename specified.
	 * We don't want to log directory listings or globbings.
	 */
	if ((xp->remoteFileName != NULL)) {
		/* Get kilobytes transferred, rounding to the nearest kB. */
		kb = ((long) xp->bytesTransferred + 512L) / 1024L;
		OverflowAdd(&gTotalXferKiloBytes, kb);
		OverflowAdd(&gRmtInfo.xferKbytes, kb);

		/* Get hundredths of seconds, rounded up to nearest. */
		hsecs = (long) (100.0 * (elapsedTime + 0.0050));
		OverflowAdd(&gTotalXferHSeconds, hsecs);
		OverflowAdd(&gRmtInfo.xferHSeconds, hsecs);
		if (gTotalXferHSeconds < 0)
			gTotalXferHSeconds = 1L;
		if (gRmtInfo.xferHSeconds <= 0)
			gRmtInfo.xferHSeconds = 1L;

		localFileIsStdout =
			(STREQ(xp->remoteFileName, kLocalFileIsStdout));
	    /* If a simple path is given, try to log the full path. */
		if ((xp->remoteFileName[0] == '/') || (localFileIsStdout)) {
			/* Use what we had in the xp. */
			STRNCPY(fullRemote, xp->remoteFileName);
		} else {
			/* Make full path by appending what we had in the xp
			 * to the current remote directory.
			 */
			STRNCPY(fullRemote, gRemoteCWD);
			STRNCAT(fullRemote, "/");
			STRNCAT(fullRemote, xp->remoteFileName);
		}
	
		/* Save transfers to the user's logfile.  We only log something to
		 * the user log if we are actually saving a file;  we don't log
		 * to the user log if we are piping the remote output into something,
		 * or dumping it to stdout.
		 */
		if ((gLogFile != NULL) && (!localFileIsStdout)) {
			xferred = FileSize((double) xp->bytesTransferred, &unitStr, NULL);
			(void) fprintf(gLogFile, "  %-3s  %6.2f %-2s  ftp://%s%s\n",
				NETREADING(xp) ? "get" : "put",
				xferred,
				unitStr,
				gRmtInfo.name,
				fullRemote
			);
			fflush(gLogFile);
		}

#ifdef SYSLOG
    	{
        longstring infoPart1;

        /* Some syslog()'s can't take an unlimited number of arguments,
         * so shorten our call to syslog to 5 arguments total.
         */
        STRNCPY(infoPart1, gUserInfo.userName);
        if (NETREADING(xp)) {
            STRNCAT(infoPart1, " received ");
            STRNCAT(infoPart1, fullRemote);	/* kLocalFileIsStdout is ok. */
            STRNCAT(infoPart1, " as ");
            STRNCAT(infoPart1, xp->localFileName);
            STRNCAT(infoPart1, " from ");
        } else {
            STRNCAT(infoPart1, " sent ");
            STRNCAT(infoPart1, xp->localFileName);
            STRNCAT(infoPart1, " as ");
            STRNCAT(infoPart1, fullRemote);
            STRNCAT(infoPart1, " to ");
        }
        STRNCAT(infoPart1, gActualHostName);
#ifndef LOG_INFO
#	define LOG_INFO 6		/* Don't know if this is standard! */
#endif
        syslog (LOG_INFO, "%s (%ld bytes).", infoPart1, xp->bytesTransferred);
    	}
#endif  /* SYSLOG */
	}
}									   /* EndProgress */

/*****************************************************************************

How progress meters are used.
-----------------------------

Just before the transfer is to be started, StartProgress is called.  This is
called even if the 'doReports' field is set to 0 in the XferSpec. The reason
for this is because this module also handles logging, both to the system log,
and to the user's log.  We have to call both StartProgress and EndProgress to
make sure logging is done correctly.

StartProgress figures out which progress meter to use.  The user may have
decided on a favorite, but sometimes we may not be able to use the one
requested.  The classic example is when we could not get the size of the file
we're transferring in advance.  Things that use percentages or bar graphs
won't work in that case.

If StartProgress found that progress reports will be used, it determines
which progress meter function to use, and sends it a 'kPrInitMsg' so the
function can do any necessary initialization.  The progress meter function
doesn't have to print anything at this point.  Character echoing is turned
off at this point, so any keys typed by the user won't mess up the progress
function's hard work.

During the transfer, which is taken care of in Xfer.c, we call
ProgressReport.  We only bother doing this if progress reports are really on,
since none of the logging stuff is done at this time.

ProgressReport does a few quick checks to see if it should call the progress
meter.  Because reporting could waste too much time, we don't want to do it
too often.  Instead we wait a few seconds between each actual screen update.

ProgressReport also does a few checks to see if reporting should be turned
off.  We don't want any more reports if we've been put into the background,
since as soon as we printed something, the job would stop, because of pending
tty output.  We also don't want any more reports if the user logged out.

ProgressReport then sends a 'kPrUpdateMsg' to the progress function, which
then does the screen update.

After the transfer finishes, we call EndReport.  The first thing it does, if
reports are still on, is to force one last screen update.  This is needed,
for example, by the bar graph meter so the bar will be completely filled.

EndReport then sends the progress function a 'kPrEndMsg' so the function can
do any necessary cleanup.  After turning character echoing back on, the stats
are printed if they were requested, and then the logs are updated.

Adding your own progress meters.
--------------------------------

You need to create a function that answers to the 'kPrInitMsg,' 'kPrUpdateMsg,'
and 'kPrEndMsg' messages.  You'll be given as parameters the XferSpecPtr in
use, and a 'mode' that indicates which of the messages you received.  Have
the function return 0, or kPrWantStatsMsg.

You'll need to have a look at StartProgress and see if your function has any
special requirements.  You'll need to edit Progress.h and make a #define for
your function.  Don't forget to change kPrLast, and add a function prototype
in the header too.  Lastly, add an entry to 'gProgressMeterProcs' with your
function corresponding to whatever value you #defined in the header.

In order for your progress meter to be recognized by the Preferences screen,
edit Prefs.c and add the name of your meter in the "case kProgressPrefsWinItem:"
portion of GetPrefSetting().

*****************************************************************************/

/* eof Progress.c */
