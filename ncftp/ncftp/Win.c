/* Win.c */

#include "Sys.h"
#include "Curses.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "Util.h"
#include "Main.h"
#include "Version.h"
#include "Bookmark.h"
#include "RCmd.h"
#include "LGets.h"
#include "GetPass.h"


#ifdef USE_CURSES

#include "WGets.h"

extern int endwin(void);
extern long gEventNumber;
WINDOW *gListWin;
WINDOW *gInputWin;
WINDOW *gPromptWin;
WINDOW *gBarWin;
int gCurRow, gLastRow, gSkipToEnd;
int gPageNum;
int gMultiLineMode = 0;
int gPendingNL = 0;
int gBackgroundProcessing = 0;
int gUsingDefaultBar = 0;
int gUsingTmpCenterBar = 0;
string gBarLeft, gBarCenter, gBarRight;

/* Other protos, whose parameters aren't WINDOW *ptrs, are in Win.h */
void WAddCenteredStr(WINDOW *, int, char *);
void WAttr(WINDOW *w, int attr, int on);

extern WINDOW *gPrefsWin, *gHostWin;

#endif	/* USE_CURSES */

string gPrompt = kLineModePrompt;
int gWinInit = 0;
char *gSprintfBuf = NULL;
int gScreenWidth = 80;

extern longstring gRemoteCWD;
extern longstring gLockFileName;
extern int gConnected, gVerbosity;
extern Bookmark gRmtInfo;
extern string gHost;
extern int gStdout, gRealStdout, gOtherSessionRunning;
extern FILE *gTraceLogFile;
extern int gDebug, gTrace;
extern LineList gCmdHistory;
extern int gVisualMode, gIsToTTY, gIsFromTTY, gStartup;
extern int gRedialModeEnabled;


void EndWin(void)
{
	if (gWinInit) {
		gWinInit = 0;
#ifdef USE_CURSES
		if (gEventNumber > 0L) {
			clear();
			refresh();
		}
		endwin();
#endif	/* USE_CURSES */
	}
}	/* EndWin */




void Exit(int exitStatus)
{
	if (gOtherSessionRunning == 0) {
		(void) UNLINK(gLockFileName);
	}
	EndWin();
	exit(exitStatus);
}	/* Exit */



static void
SigTerm(void)
{
	Exit(kExitSignal);
}	/* SigTerm */




void SaveScreen(void)
{
#ifdef USE_CURSES
	if (gWinInit) {
		/* Clearing the screen is necessary
		 * because ncurses doesn't move the
		 * cursor to the bottom left.
		 *
		 * This also causes the restore
		 * operation to require that we paint
		 * all our windows by hand, because we
		 * have just left the screen blank so
		 * when refresh gets called in the
		 * restore it just returns the screen
		 * to blank.
		 *
		 * If it weren't for this screen clear,
		 * we would be able to get away with
		 * just doing an endwin(), the shell,
		 * and then a refresh() without us
		 * re-drawing any windows manually.
		 */
		clear();
		refresh();
 		endwin();
		fflush(stdout);
		fflush(stderr);
	}
#endif	/* USE_CURSES */
}	/* SaveScreen */



static void TTYWaitForReturn(void)
{
	int tty;
	int junk;

	tty = open("/dev/tty", O_RDWR);
	if (tty != -1) {
		write(tty, "[Hit return]", 12);
		read(tty, &junk, 1);
		close(tty);
	}
}	/* TTYWaitForReturn */



void RestoreScreen(int pressKey)
{
#ifdef USE_CURSES
	if (gWinInit) {
		if (pressKey) {
#	if (CURSES_SHELL_BUG == 0)
			TTYWaitForReturn();
#	else
			sleep(2);
#	endif
		}
		refresh();
		UpdateScreen(1);
	}
#endif	/* USE_CURSES */
}	/* RestoreScreen */



void Beep(int on)
{
	static time_t lastBeepTime = 0;
	time_t now;

	time(&now);

	/* Don't flood the user with beeps. Once per two seconds is reasonable. */
	if ((on > 0) && ((int) (now - lastBeepTime) > 1)) {
#ifdef USE_CURSES
		if (gWinInit)
			beep();
		else
#endif
		{
			fprintf(stderr, "\007");	/* ^G */
			fflush(stderr);
		}
	}
	lastBeepTime = now;
}	/* Beep */




#ifdef USE_CURSES
/* Many old curses libraries don't support wattron() and its attributes.
 * They should support wstandout() though.  This routine is an attempt
 * to use the best substitute available, depending on what the curses
 * library has.
 */
void WAttr(WINDOW *w, int attr, int on)
{
	/* Define PLAIN_TEXT_ONLY if you have the attributes, but don't want
	 * to use them.
	 */
#ifndef PLAIN_TEXT_ONLY
#ifdef A_REVERSE
	if (attr & kReverse) {
		if (on)
			wattron(w, A_REVERSE);
		else
			wattroff(w, A_REVERSE);
	}
#else
	if (attr & kReverse) {
		if (on)
			wstandout(w);
		else
			wstandend(w);

		/* Nothing else will be done anyway, so just return now. */
		return;
	}
#endif	/* A_REVERSE */

#ifdef A_BOLD
	if (attr & kBold) {
		if (on)
			wattron(w, A_BOLD);
		else
			wattroff(w, A_BOLD);
	}
#else
	/* Do nothing.  Plain is best substitute. */
#endif	/* A_BOLD */

#ifdef A_UNDERLINE
	if (attr & kUnderline) {
		if (on)
			wattron(w, A_UNDERLINE);
		else
			wattroff(w, A_UNDERLINE);
	}
#else
	/* Try using standout mode in place of underline. */
	if (attr & kUnderline) {
		if (on)
			wstandout(w);
		else
			wstandend(w);

		/* Nothing else will be done anyway, so just return now. */
		return;
	}
#endif	/* A_UNDERLINE */

#ifdef A_DIM
	if (attr & kDim) {
		if (on)
			wattron(w, A_DIM);
		else
			wattroff(w, A_DIM);
	}
#else
	/* Do nothing.  Plain is best substitute. */
#endif	/* A_DIM */

#ifdef A_NORMAL
	if (attr == kNormal) {
		wattrset(w, A_NORMAL);
		return;
	}
#else
	/* At least make sure standout mode is off. */
	if (attr == kNormal) {
		wstandend(w);
		return;
	}
#endif	/* A_NORMAL */
#endif	/* PLAIN_TEXT_ONLY */
}	/* WAttr */
#endif	/* USE_CURSES */




void UpdateScreen(int wholeScreen)
{
#ifdef USE_CURSES
	if (gWinInit) {
		if (wholeScreen) {
			touchwin(gListWin);
			touchwin(gBarWin);
			touchwin(gPromptWin);
			touchwin(gInputWin);
				
			wnoutrefresh(gListWin);
			wnoutrefresh(gBarWin);
			wnoutrefresh(gPromptWin);
			wnoutrefresh(gInputWin);
		}
		doupdate();
	}
#endif	/* USE_CURSES */
}	/* UpdateScreen */



void FlushListWindow(void)
{
#ifdef USE_CURSES
	if (gWinInit) {
		wnoutrefresh(gListWin);
		doupdate();
		return;
	}
#endif	/* USE_CURSES */
	fflush(stdout);
	fflush(stderr);		/* Overkill, since stderr _should_ be unbuffered. */
}	/* FlushListWindow */




#ifdef USE_CURSES
#ifdef SIGTSTP
static
void SigTStp(int sigNum)
{
/* TO-DO: (doesn't work 100% correctly yet) */
	if (sigNum == SIGTSTP) {
		if ((gPrefsWin != NULL) || (gHostWin != NULL)) {
			/* Doing this with these windows open can cause problems. */
			return;
		}
		SaveScreen();
		TraceMsg("SIGTSTP: Suspended.\n");
		SIGNAL(SIGTSTP, SIG_DFL);
		kill(getpid(), SIGSTOP);	/* Send stop signal to ourselves. */
	} else {
		SIGNAL(SIGTSTP, SigTStp);
		SIGNAL(SIGCONT, SigTStp);
		if (sigNum == SIGCONT) {
			TraceMsg("SIGCONT: Resumed.\n");
		} else {
			TraceMsg("SIG %d.\n", sigNum);
		}
		if (InForeGround())
			RestoreScreen(0);
		else
			gBackgroundProcessing = 1;
	}
}	/* SigTStp */
#endif
#endif	/* USE_CURSES */





/* Read a line of input, and axe the end-of-line. */
char *Gets(char *str, size_t size)
{
	string pr;
#ifdef USE_CURSES
	int result;
	WGetsParams wgp;
	int maxy, maxx;
#endif	/* USE_CURSES */	

#ifdef USE_CURSES
	if (gWinInit) {
		/* Background processing only gets turned on if you use ^Z
		 * from within the program and then "bg" it.  We assume that
		 * when you do that, you want to get a "tty output" message
		 * when the operation finishes.  Note that you wouldn't get
		 * gBackgroundProcessing == 1 if you did "ncftp &" because
		 * you would never get to the part that turns it on (the ^Z
		 * handler).
		 */
		if (gBackgroundProcessing) {
			/* Want to give you a "stopped - tty output" message in
			 * your shell when we get here.
			 */
			PrintF("\nBackground processing has finished.\n");
			FlushListWindow();
			while (! InForeGround())
				sleep(1);
			gBackgroundProcessing = 0;
			RestoreScreen(0);
		}

		str[0] = '\0';
		wnoutrefresh(gListWin);
		if (gMultiLineMode) {
			SetPrompt(gPrompt);
			gMultiLineMode = 0;
		} else {
			werase(gInputWin);
			wnoutrefresh(gInputWin);
		}
		SetScreenInfo();
		doupdate();
		wgp.w = gInputWin;
		wgp.sy = 0;
		wgp.sx = 0;

		getmaxyx(gInputWin, maxy, maxx);
		wgp.fieldLen = maxx - 1;
		wgp.dst = str;
		wgp.dstSize = size;
		wgp.useCurrentContents = 0;
		wgp.echoMode = wg_RegularEcho;
		wgp.history = &gCmdHistory;
		result = wg_Gets(&wgp);
		if (result < 0)
			return (NULL);	/* Error, or EOF. */
		return (str);
	} else
#endif	/* USE_CURSES */
	{
		Echo(stdin, 1);		/* Turn echo on, if it wasn't already. */
		STRNCPY(pr, gPrompt);
		STRNCAT(pr, kPromptTail);
		return LineModeGets(pr, str, size);
	}
}	/* Gets */




void GetAnswer(char *prompt, char *answer, size_t siz, int noEcho)
{
#ifdef USE_CURSES
	WGetsParams wgp;
	int maxy, maxx;
#endif

	PTRZERO(answer, siz);
#ifdef USE_CURSES
	if (gWinInit) {
		wnoutrefresh(gListWin);
		MakeBottomLine(prompt, kReverse, 0);
		doupdate();
		wgp.w = gInputWin;
		wgp.sy = 0;
		wgp.sx = 0;
		getmaxyx(gInputWin, maxy, maxx);
		wgp.fieldLen = maxx - 1;
		wgp.dst = answer;
		wgp.dstSize = siz;
		wgp.useCurrentContents = 0;
		wgp.echoMode = noEcho ? wg_BulletEcho : wg_RegularEcho;
		wgp.history = wg_NoHistory;
		wg_Gets(&wgp);
		return;
	}
#endif	/* USE_CURSES */
	if (noEcho)
		GetPass(prompt, answer, siz);
	else {
		StdioGets(prompt, answer, siz);
	}
}	/* GetAnswer */




void SetBar(char *l, char *c, char *r, int doUp, int tmpCenter)
{
#ifdef USE_CURSES
	int maxy, maxx;
	int i;
	int rmax;
	int llen, rlen, clen;
	string bar;
	string barTmp;

	if (gWinInit) {
		if (l == NULL)
			l = gBarLeft;
		else if (doUp != -1)
			STRNCPY(gBarLeft, l);	

		if (r == NULL)
			r = gBarRight;
		else if (doUp != -1)
			STRNCPY(gBarRight, r);	

		if (c == NULL) {
			c = gBarCenter;
	/*		if (gUsingDefaultBar == 1) {
				*c = '\0';
				gUsingDefaultBar = 0;
			} else */ if (gUsingTmpCenterBar == 1) {
				*c = '\0';
				gUsingTmpCenterBar = 0;
			}
		} else {
			STRNCPY(gBarCenter, c);	
			gUsingTmpCenterBar = 0;
			if (tmpCenter)
				gUsingTmpCenterBar = 1;
		}

		getmaxyx(gBarWin, maxy, maxx);
		for (i=0; i<maxx; i++)
			bar[i] = ' ';

		llen = (int) strlen(l);
		if (llen > maxx - 1)
			llen = maxx - 1;
		rlen = (int) strlen(r);
		if (rlen > maxx - 1)
			rlen = maxx - 1;
		clen = (int) strlen(c);
		if (clen > maxx - 1)
			clen = maxx - 1;

		if /* (rlen + (clen/2) > (maxx/2)) */
			((2*rlen) + (clen) > (maxx))
		{
				/* Put the center part on the left so we can see it. */
				memcpy(bar, c, (size_t) clen);
			
				/* Do the right side. */	
				rmax = maxx - 1 - clen;
				if (rmax > 0) {
					AbbrevStr(barTmp, r, (size_t) rmax, 0);
					rmax = strlen(barTmp);
					memcpy(bar + maxx - rmax, r, (size_t) rmax);
				}
		} else {
				/* Do the left side. */	
				memcpy(bar, l, (size_t) llen);

				/* Do the middle. */	
				if (*c != '\0') {
					rlen = maxx - 1;
					AbbrevStr(barTmp, c, (size_t) rlen, 0);
					rlen = strlen(barTmp);
					memcpy(bar + (maxx - 1 - rlen) / 2, barTmp, (size_t) rlen);
				}
			
				/* Do the right side. */	
				rmax = maxx - 1 - llen;
				if (rmax > 0) {
					AbbrevStr(barTmp, r, (size_t) rmax, 0);
					rmax = strlen(barTmp);
					memcpy(bar + maxx - rmax, r, (size_t) rmax);
				}
		}

		bar[maxx] = '\0';
		
		wmove(gBarWin, 0, 0);
		waddstr(gBarWin, bar);

		if (doUp == 0)	
			wnoutrefresh(gBarWin);
		else
			wrefresh(gBarWin);
	}
#endif	/* USE_CURSES */
}	/* SetBar */




void SetDefaultBar(void)
{
#ifdef USE_CURSES
	string str;

	if (gWinInit) {
		STRNCPY(str, "NcFTP ");
		STRNCAT(str, kVersion);
		STRNCAT(str, " by Mike Gleason (mgleason@NcFTP.com).");
		gUsingDefaultBar = 1;

		SetBar("", str, "", 0, 0);
	} else {
		SetPrompt(kUseDefaultPrompt);
	}
#else
	SetPrompt(kUseDefaultPrompt);
#endif
}	/* SetDefaultBar */




void SetScreenInfo(void)
{
	string pr;
	string pcwd;
	char *cp;
	int len, len2;
	size_t maxPCwdLen;

	MakeStringPrintable(pcwd, (unsigned char *) gRemoteCWD, sizeof(pcwd));
	if (gWinInit) {
		if (gConnected) {
			maxPCwdLen = gScreenWidth - strlen(gRmtInfo.name) - 2;
			AbbrevStr(pr, pcwd, maxPCwdLen, 0);
			SetBar(gRmtInfo.name, NULL, pr, 0, 0);
			SetPrompt(gRmtInfo.bookmarkName);
		} else {
			SetDefaultBar();
			SetPrompt(kUseDefaultPrompt);
		}
	} else {
		if (gConnected) {
			STRNCPY(pr, gRmtInfo.bookmarkName);
			STRNCAT(pr, ":");
			len = (int) strlen(pr);
			len2 = (int) strlen(pcwd);
			if (len + len2 > kPromptLimit) {
				STRNCAT(pr, "...");
				cp = pcwd + len2 - (kPromptLimit - len - 3);
				STRNCAT(pr, cp);
			} else {
				STRNCAT(pr, pcwd);
			}
			SetPrompt(pr);
		} else {
			SetPrompt(kUseDefaultPrompt);
		}
	}
}	/* SetScreenInfo */




#ifdef USE_CURSES
void PrintToListWindow(char *buf, int multi)
{
	char *endp, *startp;
	int c, haveNL;
	int haveCR;
	int y, x;
	string pr;
	int extraLines;

	if (multi != 0) {
		if (gSkipToEnd == 1)
			return;
		/* Don't wait between pages if they are redialing.  We don't
		 * want to page the site's connect message if redialing is
		 * turned on.
		 */
		if (gRedialModeEnabled != 0)
			multi = 0;
	}

	endp = buf;
	startp = buf;

	while (1) {
		if (*startp == '\0')
			break;
		haveCR = 0;
		haveNL = 0;
		for (endp = startp; ; endp++) {
			if (*endp == '\0') {
				endp = NULL;
				break;
			} else if (*endp == '\n') {
				haveNL = 1;
				*endp = '\0';
				break;
			} else if (*endp == '\r') {
				/* Have to do CRs manually because
				 * some systems don't do 'em.  (AIX).
				 */
				haveCR = 1;
				*endp = '\0';
				break;
			}
		}

		/* Take long lines that wrap into account. */
		extraLines = endp == startp ? 0 : (endp - startp - 1) / gScreenWidth;
		gCurRow += extraLines;
		if ((multi) && (gCurRow >= gLastRow) && (!gSkipToEnd) && (!haveCR)) {
			wnoutrefresh(gListWin);
			sprintf(pr, "--Page %d--", (++gPageNum));
			MakeBottomLine(pr, kReverse, 0);
			doupdate();
			cbreak();
			c = mvwgetch(gInputWin, 0, 0);
			nocbreak();
			if (c == 'q') {
				gSkipToEnd = 1;
				gCurRow = 0;
				gPendingNL = 1;
				break;
			}
			gCurRow = extraLines;
		}
		
		if (gPendingNL) {
			waddch(gListWin, '\n');
		}

		/* Weird things happened if I did wprintw(gListWin, "%s", startp). */
		waddstr(gListWin, startp);

		/* May need to gCurRow++ if line wraps around other side... */
		if (haveNL) {
			gCurRow++;
		} else if (haveCR) {
			getyx(gListWin, y, x);
			wmove(gListWin, y, 0);
		}
		gPendingNL = haveNL;
		if (endp == NULL)
			break;
		startp = endp + 1;
	}
}	/* PrintToListWindow */
#endif	/* USE_CURSES */




/* Prints a message, if you have debbuging mode turned on. */

/*VARARGS*/
#ifndef HAVE_STDARG_H
void DebugMsg(va_alist)
	va_dcl
#else
void DebugMsg(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

#ifndef HAVE_STDARG_H
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	if (gDebug == kDebuggingOn) {
		if (gWinInit) {
#ifdef USE_CURSES
			strcpy(gSprintfBuf, "#DB# ");
			(void) vsprintf(gSprintfBuf + 5, fmt, ap);
			PrintToListWindow(gSprintfBuf, 0);
#endif	/* USE_CURSES */
		} else {
			(void) fprintf(kDebugStream, "#DB# ");
			(void) vfprintf(kDebugStream, fmt, ap);
			(void) fflush(kDebugStream);
		}
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		(void) fprintf(gTraceLogFile, "#DB# ");
		(void) vfprintf(gTraceLogFile, fmt, ap);
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* DebugMsg */




/* This is similar to DebugMsg, but only writes to the debug log
 * file.  This is useful for putting messages in the log that
 * shouldn't show up on screen (i.e. would make a mess in visual
 * mode.
 */

/*VARARGS*/
#ifndef HAVE_STDARG_H
void TraceMsg(va_alist)
	va_dcl
#else
void TraceMsg(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

#ifndef HAVE_STDARG_H
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		(void) fprintf(gTraceLogFile, "#TR# ");
		(void) vfprintf(gTraceLogFile, fmt, ap);
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* TraceMsg */




/* Prints to our own standard output stream. */

/*VARARGS*/
#ifndef HAVE_STDARG_H
void PrintF(va_alist)
	va_dcl
#else
void PrintF(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

#ifndef HAVE_STDARG_H
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	/* If it's an important message, don't use this function, use
	 * EPrintF() instead.
	 */
	if (gVerbosity > kErrorsOnly) {
		if ((gWinInit) && (gRealStdout == gStdout)) {
#ifdef USE_CURSES
			(void) vsprintf(gSprintfBuf, fmt, ap);
			PrintToListWindow(gSprintfBuf, 0);
#endif	/* USE_CURSES */
		} else {
			(void) vsprintf(gSprintfBuf, fmt, ap);
			(void) write(gStdout, gSprintfBuf, strlen(gSprintfBuf));
		}
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		(void) vfprintf(gTraceLogFile, fmt, ap);
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* PrintF */



/*VARARGS*/
#ifndef HAVE_STDARG_H
void BoldPrintF(va_alist)
	va_dcl
#else
void BoldPrintF(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

#ifndef HAVE_STDARG_H
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	if (gVerbosity > kErrorsOnly) {
		if ((gWinInit) && (gRealStdout == gStdout)) {
#ifdef USE_CURSES
			(void) vsprintf(gSprintfBuf, fmt, ap);
			WAttr(gListWin, kBold, 1);
			PrintToListWindow(gSprintfBuf, 0);
			WAttr(gListWin, kBold, 0);
#endif	/* USE_CURSES */
		} else {
			(void) vsprintf(gSprintfBuf, fmt, ap);
			(void) write(gStdout, gSprintfBuf, strlen(gSprintfBuf));
		}
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		(void) vfprintf(gTraceLogFile, fmt, ap);
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* BoldPrintF */




/* Prints to stderr. */

/*VARARGS*/
#ifndef HAVE_STDARG_H
void EPrintF(va_alist)
	va_dcl
#else
void EPrintF(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;
	char *cp;

#ifndef HAVE_STDARG_H
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	if (gVerbosity > kQuiet) {
		if (gWinInit) {
#ifdef USE_CURSES
			(void) vsprintf(gSprintfBuf, fmt, ap);
			PrintToListWindow(gSprintfBuf, 0);
			
			/* No buffering on error stream. */
			wnoutrefresh(gListWin);
			doupdate();
#endif	/* USE_CURSES */
		} else {
			(void) vfprintf(stderr, fmt, ap);
		}
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		/* Special hack so when progress meters use \r's we don't
		 * print them in the trace file.
		 */
		(void) vsprintf(gSprintfBuf, fmt, ap);
		for (cp = gSprintfBuf; ; ) {
			/* Replace all carriage returns with newlines. */
			cp = strchr(cp, '\r');
			if (cp == NULL)
				break;
			*cp++ = '\n';
		}
		fputs(gSprintfBuf, gTraceLogFile);
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* EPrintF */





/*VARARGS*/
#ifndef HAVE_STDARG_H
void Error(va_alist)
	va_dcl
#else
void Error(int pError0, char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;
	int pError;
	longstring buf2;
	
#ifndef HAVE_STDARG_H
	va_start(ap);
	pError = va_arg(ap, int);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
	pError = pError0;
#endif

	if (gVerbosity > kQuiet) {
		if (gWinInit) {
#ifdef USE_CURSES
			(void) vsprintf(gSprintfBuf, fmt, ap);
			if (gDebug == kDebuggingOn)
				sprintf(buf2, "Error(%d): ", errno);
			else
				STRNCPY(buf2, "Error: ");
			STRNCAT(buf2, gSprintfBuf);
#ifdef HAVE_STRERROR
			if ((pError == kDoPerror) && (errno > 0)) {
				STRNCAT(buf2, "Reason: ");
				STRNCAT(buf2, strerror(errno));
				STRNCAT(buf2, "\n");
			}
#endif	/* HAVE_STRERROR */
			PrintToListWindow(buf2, 0);
#endif	/* USE_CURSES */
		} else {
			(void) fprintf(stderr, "Error");
			if (gDebug == kDebuggingOn)
				(void) fprintf(stderr, "(%d)", errno);
			(void) fprintf(stderr, ": ");
			(void) vfprintf(stderr, fmt, ap);
			(void) fflush(stderr);
			if ((pError == kDoPerror) && (errno > 0))
				perror("Reason");
		}
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
		(void) fprintf(gTraceLogFile, "Error(%d): ", errno);
		(void) vfprintf(gTraceLogFile, fmt, ap);
#ifdef HAVE_STRERROR
		if ((pError == kDoPerror) && (errno > 0))
			(void) fprintf(gTraceLogFile, "Reason: %s\n", strerror(errno));
#endif
		(void) fflush(gTraceLogFile);
	}
	va_end(ap);
}	/* Error */




void MultiLineInit(void)
{
#ifdef USE_CURSES
	int maxy, maxx;

	if (gWinInit) {
		gCurRow = 0;
		getmaxyx(gListWin, maxy, maxx);
		gLastRow = maxy;
		gSkipToEnd = 0;
		gPageNum = 0;
		gMultiLineMode = 1;
	}
#endif	/* USE_CURSES */
}	/* MultiLineInit */



/*VARARGS*/
#ifndef HAVE_STDARG_H
void MultiLinePrintF(va_alist)
	va_dcl
#else
void MultiLinePrintF(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

	if (gVerbosity > kErrorsOnly) {
#ifndef HAVE_STDARG_H
		va_start(ap);
		fmt = va_arg(ap, char *);
#else
		va_start(ap, fmt0);
		fmt = fmt0;
#endif
		if ((gWinInit) && (gRealStdout == gStdout)) {
#ifdef USE_CURSES		
			(void) vsprintf(gSprintfBuf, fmt, ap);
			PrintToListWindow(gSprintfBuf, 1);
#endif	/* USE_CURSES */
		} else {
			(void) vsprintf(gSprintfBuf, fmt, ap);
			(void) write(gStdout, gSprintfBuf, strlen(gSprintfBuf));
		}
		va_end(ap);
	}
	if ((gTrace == kTracingOn) && (gTraceLogFile != NULL)) {
#ifndef HAVE_STDARG_H
		va_start(ap);
		fmt = va_arg(ap, char *);
#else
		va_start(ap, fmt0);
		fmt = fmt0;
#endif
		(void) vfprintf(gTraceLogFile, fmt, ap);
		(void) fflush(gTraceLogFile);
		va_end(ap);
	}

}	/* MultiLinePrintF */




void MakeBottomLine(char *pr, int flags, int addTail)
{
#ifdef USE_CURSES
	int len;
	int doCreate;
	int maxy, maxx;

	len = (int) strlen(pr);
	if (addTail)
		len += strlen(kPromptTail);
	else if (flags & kReverse)
		len++;	/* Will add a space so you can see cursor. */

	doCreate = 0;
	if (gPromptWin != NULL) {
		getmaxyx(gPromptWin, maxy, maxx);
		if (maxx != len) {
			delwin(gPromptWin);
			delwin(gInputWin);
			doCreate = 1;
		}
	} else {
		doCreate = 1;
	}

	if (doCreate) {
		gPromptWin = newwin(1, len, LINES - 1, 0);
		gInputWin = newwin(1, COLS - len, LINES - 1, len);

		if ((gPromptWin == NULL) || (gInputWin == NULL))
			Exit(kExitWinFail2);
	}

	werase(gPromptWin);
	WAttr(gPromptWin, flags, 1);
	if (addTail) {
		mvwprintw(gPromptWin, 0,0, "%s%s", pr, kPromptTail);
		WAttr(gPromptWin, flags, 0);
	} else {
		mvwprintw(gPromptWin, 0,0, "%s", pr);
		WAttr(gPromptWin, flags, 0);
	}
	wnoutrefresh(gPromptWin);
	werase(gInputWin);
	touchwin(gInputWin);
	wnoutrefresh(gInputWin);
	doupdate();
#endif	/* USE_CURSES */
}	/* MakeBottomLine */




void SetPrompt(char *pr)
{
	string p;

	if ((pr == kUseDefaultPrompt)
		|| STREQ(pr, kLineModePrompt)
		|| STREQ(pr, kVisualModePrompt)) {
		if (gWinInit)
			STRNCPY(p, kVisualModePrompt);
		else
			STRNCPY(p, kLineModePrompt);
	} else {
		STRNCPY(p, pr);
	}
	STRNCPY(gPrompt, p);
#ifdef USE_CURSES
	if (gWinInit)
		MakeBottomLine(p, kBold, 1);
#endif	/* USE_CURSES */
}	/* SetPrompt */



#ifdef USE_CURSES
/* Draws a string centered in a window. */
void WAddCenteredStr(WINDOW *w, int y, char *str)
{
	int x;
	int maxy, maxx;

	getmaxyx(w, maxy, maxx);
	x = (maxx - strlen(str)) / 2;
	if (x < 0)
		x = 0;
	wmove(w, y, x);
	waddstr(w, str);
}	/* WAddCenteredStr */
#endif	/* USE_CURSES */




void InitWindows(void)
{
	char *cp;
	int on;
#ifdef USE_CURSES
	int maxx, maxy;
#endif

	on = gVisualMode && gIsToTTY && gIsFromTTY;

#ifdef USE_CURSES
	if (on) {
		initscr();
		if (stdscr == NULL)
			goto fail;
		gWinInit = 1;
		SIGNAL(SIGTERM, SigTerm);

		nl();

		gPromptWin = gInputWin = NULL;
		gListWin = newwin(LINES - 2, COLS, 0, 0);
		gBarWin = newwin(1, COLS, LINES - 2, 0);

		if ((gListWin == NULL) || (gBarWin == NULL))
			Exit(kExitWinFail1);

		scrollok(gListWin, TRUE);
		idlok(gListWin, TRUE);
		wmove(gListWin, 0, 0);
		noecho();	/* Leave this off until we need it. */

		WAttr(gBarWin, kReverse, 1);

		getmaxyx(gListWin, maxy, maxx);
		if (gScreenWidth > ((int) sizeof(string) - 1)) {
			fprintf(stderr, "Visual mode for NcFTP only supports windows of %d columns or less.\nReduce your window size or use line mode (ncftp -L).\n", (int) sizeof(string) - 1);
			Exit(kExitWinFail1);
		}
		gScreenWidth = maxx;

		gBarLeft[0] = '\0';
		gBarCenter[0] = '\0';
		gBarRight[0] = '\0';

		LoadHistory();

		/* Probably not set yet, unless you specified a host on
		 * the command line.
		 */
		SetScreenInfo();
#ifdef SIGTSTP
		if (SIGNAL(SIGTSTP, SIG_IGN) != SIG_IGN) {
			SIGNAL(SIGTSTP, SigTStp);
			SIGNAL(SIGCONT, SigTStp);
		}
#endif
		return;
	}
fail:
#endif	/* USE_CURSES */
	gWinInit = 0;

	cp = (char *) getenv("COLUMNS");
	if (cp != NULL)
		gScreenWidth = atoi(cp);

	/* Prompt will already be set if connected. */
	if (gConnected == 0)
		SetPrompt(kUseDefaultPrompt);
}	/* InitWindows */

/* eof */

