/* Prefs.c */

#include "Sys.h"
#include "Curses.h"

#include <ctype.h>
#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "Cmds.h"
#include "Progress.h"
#include "Hostwin.h"
#include "Prefs.h"
#include "RCmd.h"
#include "Bookmark.h"
#include "Main.h"

#ifdef USE_CURSES

#include "WGets.h"

/* This is the full-screen window that pops up when you run the
 * preferences editor.
 */
WINDOW *gPrefsWin = NULL;

extern void WAddCenteredStr(WINDOW *w, int y, char *str);
void WAttr(WINDOW *w, int attr, int on);

#endif	/* USE_CURSES */



jmp_buf gPrefsWinJmp;
extern int gWinInit;
extern string gAnonPassword;
extern int gMayUTime, gStartupMsgs, gRemoteMsgs, gBlankLines;
extern int gAnonOpen, gWhichProgMeter, gMaxLogSize, gMaxBookmarks;
extern int gDefaultVisualMode, gTotalRuns, gRememberLCWD;
extern int gNetworkTimeout, gTrace, gPreferredDataPortMode;
extern longstring gLocalCWD, gPager, gDownloadDir;
extern long gTotalXferKiloBytes, gTotalXferHSeconds;
extern int gMarkTrailingSpace;



PrefOpt gPrefOpts[] = {
	{ "anonopen", "Default open mode:",
		kToggleMsg,
		PREFBOOL(gAnonOpen) },
	{ "anonpass", "Anonymous password:",
		"Type new anonymous password, or hit <RETURN> to continue.",
		PREFSTR(gAnonPassword, kNotOkayIfEmpty, kGetAndEcho) },
	{ "blank-lines", "Blank lines between cmds:",
		kToggleMsg,
		PREFBOOL(gBlankLines) },
	{ "ftp-mode", "Default FTP mode:",
		kToggleMsg,
		PREFTOGGLE(gPreferredDataPortMode, kSendPortMode, kFallBackToSendPortMode) },
	{ "logsize", "User log size:",
		"Enter the maximum number of bytes allowed for log file, or 0 for no log.",
		PREFINT(gMaxLogSize) },
	{ "maxbookmarks", "Max bookmarks to save:",
		"Enter the max number of bookmarks for bookmarks file, or 0 for no limit.",
		PREFINT(gMaxBookmarks) },
	{ "pager", "Pager:",
		"Type the pathname of the program you use to view text a page at a time.",
		PREFSTR(gPager, kOkayIfEmpty, kGetAndEcho) },
	{ "progress-meter", "Progress meter:",
		kToggleMsg,
		PREFTOGGLE(gWhichProgMeter, kPrNone, kPrLast) },
	{ "remote-msgs", "Remote messages:",
		kToggleMsg,
		PREFTOGGLE(gRemoteMsgs, kAllRmtMsgs, (kNoChdirMsgs | kNoConnectMsg)) },
#if 0
	{ "restore-lcwd", "Restore local CWD:",
		kToggleMsg,
		PREFBOOL(gRememberLCWD) },
#endif
	{ "startup-lcwd", "Startup in Local Dir:",
		"Type directory to always lcd to, or hit <RETURN> to not lcd at startup.",
		PREFSTR(gDownloadDir, kOkayIfEmpty, kGetAndEcho) },
	{ "startup-msgs", "Startup messages:",
		kToggleMsg,
		PREFTOGGLE(gStartupMsgs, kNoStartupMsgs, (kStartupMsg | kTips)) },
	{ "timeout", "Network timeout:",
		"Enter the maximum amount of time to wait on a connection before giving up.",
		PREFINT(gNetworkTimeout) },
	{ "trace", "Trace logging:",
		kToggleMsg,
		PREFBOOL(gTrace) },
	{ "utime", "File timestamps:",
		kToggleMsg,
		PREFBOOL(gMayUTime) },
	{ "visual", "Screen graphics:",
		kToggleMsg,
		PREFBOOL(gDefaultVisualMode) },
};

/* These are options that are for information only, or options I don't feel
 * like wasting screen space on in the prefs editor.
 */
PrefOpt gNonEditPrefOpts[] = {
	{ "show-trailing-space", "Show trailing space:",
		kToggleMsg,
		PREFBOOL(gMarkTrailingSpace) },
	{ "total-runs", NULL, NULL,
		PREFINT(gTotalRuns) },
	{ "total-xfer-hundredths-of-seconds", NULL, NULL,
		PREFINT(gTotalXferHSeconds) },
	{ "total-xfer-kbytes", NULL, NULL,
		PREFINT(gTotalXferKiloBytes) },
};

int gNumEditablePrefOpts = ((int)(sizeof(gPrefOpts) / sizeof(PrefOpt)));
#define kNumNonEditablePrefOpts ((int)(sizeof(gNonEditPrefOpts) / sizeof(PrefOpt)))





void TogglePref(int *val, int min, int max)
{
	int newVal;

	newVal = *val + 1;
	if (newVal > max)
		newVal = min;
	*val = newVal;
}	/* TogglePref */





void GetPrefSetting(char *dst, size_t siz, int item)
{
	char *cp;
	string str;
	size_t len;

	*dst = '\0';
	switch (item) {
		case kAnonOpenPrefsWinItem:
			cp = gAnonOpen ? "anonymous" : "user & password";
			(void) Strncpy(dst, cp, siz);
			break;

		case kAnonPassPrefsWinItem:
			(void) Strncpy(dst, gAnonPassword, siz);
			break;

		case kBlankLinesWinItem:
			cp = gBlankLines ? "yes" : "no";
			(void) Strncpy(dst, cp, siz);
			break;

		case kFTPModePrefsWinItem:
			if (gPreferredDataPortMode == kFallBackToSendPortMode)
				cp = "Passive, but fall back to port if needed";
			else if (gPreferredDataPortMode == kPassiveMode)
				cp = "Passive FTP only (PASV)";
			else
				cp = "Send-Port FTP only (PORT)";
			(void) Strncpy(dst, cp, siz);
			break;

		case kLogSizePrefsWinItem:
			if (gMaxLogSize == 0)
				(void) Strncpy(str, "no logging", siz);
			else
				sprintf(str, "%d", gMaxLogSize);
			(void) Strncpy(dst, str, siz);
			break;

		case kMaxBookmarksWinItem:
			if (gMaxBookmarks == kNoBookmarkLimit)
				(void) Strncpy(str, "unlimited", siz);
			else
				sprintf(str, "%d", gMaxBookmarks);
			(void) Strncpy(dst, str, siz);
			break;

		case kPagerPrefsWinItem:
			if ((len = strlen(gPager)) > 47) {
				/* Abbreviate a long program path. */
				STRNCPY(str, "...");
				STRNCAT(str, gPager + len - 44);
			} else {
				if (gPager[0] == '\0')
					STRNCPY(str, "(none)");
				else
					STRNCPY(str, gPager);
			}
			(void) Strncpy(dst, str, siz);
			break;

		case kProgressPrefsWinItem:
			switch (gWhichProgMeter) {
				case kPrPercent: cp = "percent meter"; break;
				case kPrPhilBar: cp = "bar graph"; break;
				case kPrKBytes: cp = "kilobyte meter"; break;
				case kPrDots: cp = "hash dots"; break;
				case kPrStatBar: cp = "stat meter"; break;
				default: cp = "no progress reports";
			}
			(void) Strncpy(dst, cp, siz);
			break;

		case kRmtMsgsPrefsWinItem:
			if ((gRemoteMsgs & (kNoChdirMsgs | kNoConnectMsg)) == (kNoChdirMsgs | kNoConnectMsg))
				cp = "ignore startup messages and chdir messages";
			else if ((gRemoteMsgs & kNoChdirMsgs) != 0)
				cp = "ignore change-directory messages";
			else if ((gRemoteMsgs & kNoConnectMsg) != 0)
				cp = "ignore startup messages";
			else
				cp = "allow startup messages and chdir messages";
			(void) Strncpy(dst, cp, siz);
			break;

		case kStartupLCWDWinItem:
			cp = (gDownloadDir[0] != '\0') ? gDownloadDir :
				"(none)";
			(void) Strncpy(dst, cp, siz);
			break;

		case kStartupMsgsPrefsWinItem:
			if ((gStartupMsgs & (kStartupMsg | kTips)) == (kStartupMsg | kTips))
				cp = "headers and tips";
			else if ((gStartupMsgs & kStartupMsg) != 0)
				cp = "headers only";
			else if ((gStartupMsgs & kTips) != 0)
				cp = "tips only";
			else
				cp = "no startup messages";
			(void) Strncpy(dst, cp, siz);
			break;

		case kTimeoutPrefsWinItem:
			sprintf(dst, "%d", gNetworkTimeout);
			break;

		case kTracePrefsWinItem:
			if (gTrace) {
				cp = "yes";
				OpenTraceLog();
			} else {
				cp = "no";
				CloseTraceLog();
			}
			(void) Strncpy(dst, cp, siz);
			break;

		case kUTimePrefsWinItem:
			cp = gMayUTime ? "try to preserve file timestamps" :
				"do not preserve file timestamps";
			(void) Strncpy(dst, cp, siz);
			break;

		case kVisualPrefsWinItem:
			cp = gDefaultVisualMode == 1 ? "visual (curses)" : "line-oriented";
			(void) Strncpy(dst, cp, siz);
			break;
	}
}	/* GetPrefSetting */



#ifdef USE_CURSES

/* Draws the screen when we're using the host editor's main screen.
 * You can can specify whether to draw each character whether it needs
 * it or not if you like.
 */
void UpdatePrefsWindow(int uptAll)
{
	if (uptAll) {
		touchwin(gPrefsWin);
	}
	wnoutrefresh(gPrefsWin);
	doupdate();
}	/* UpdatePrefsWindow */




/* This displays a message in the preferences window. */
void PrefsWinWinMsg(char *msg)
{
	int maxx, maxy;

	getmaxyx(gPrefsWin, maxy, maxx);
	mvwaddstr(gPrefsWin, maxy - 2, 0, msg);
	wclrtoeol(gPrefsWin);
	wmove(gPrefsWin, maxy - 1, 0);
	wrefresh(gPrefsWin);
}	/* PrefsWinWinMsg */




/* Prompts for a line of input. */
void PrefsWinGetStr(char *dst, int canBeEmpty, int canEcho)
{
	string str;
	WGetsParams wgp;
	int maxx, maxy;

	getmaxyx(gPrefsWin, maxy, maxx);
	WAttr(gPrefsWin, kBold, 1);
	mvwaddstr(gPrefsWin, maxy - 1, 0, "> ");
	WAttr(gPrefsWin, kBold, 0);
	wclrtoeol(gPrefsWin);
	wrefresh(gPrefsWin);
	curs_set(1);

	wgp.w = gPrefsWin;
	wgp.sy = maxy - 1;
	wgp.sx = 2;
	wgp.fieldLen = maxx - 3;
	wgp.dst = str;
	wgp.dstSize = sizeof(str);
	wgp.useCurrentContents = 0;
	wgp.echoMode = canEcho ? wg_RegularEcho : wg_BulletEcho;
	wgp.history = wg_NoHistory;
	(void) wg_Gets(&wgp);
	cbreak();						/* wg_Gets turns off cbreak and delay. */

	TraceMsg("[%s]\n", str);
	
	/* See if the user just hit return.  We may not want to overwrite
	 * the dst here, which would make it an empty string.
	 */
	if ((wgp.changed) || (canBeEmpty == kOkayIfEmpty))
		strcpy(dst, str);

	wmove(gPrefsWin, maxy - 1, 0);
	wclrtoeol(gPrefsWin);
	wrefresh(gPrefsWin);
	curs_set(0);
}	/* PrefsWinGetStr */





/* Prompts for an integer of input. */
void PrefsWinGetNum(int *dst)
{
	string str;
	WGetsParams wgp;
	int maxx, maxy;

	getmaxyx(gPrefsWin, maxy, maxx);
	WAttr(gPrefsWin, kBold, 1);
	mvwaddstr(gPrefsWin, maxy - 1, 0, "> ");
	WAttr(gPrefsWin, kBold, 0);
	wclrtoeol(gPrefsWin);
	wrefresh(gPrefsWin);
	curs_set(1);

	wgp.w = gPrefsWin;
	wgp.sy = maxy - 1;
	wgp.sx = 2;
	wgp.fieldLen = maxx - 3;
	wgp.dst = str;
	wgp.dstSize = sizeof(str);
	wgp.useCurrentContents = 0;
	wgp.echoMode = wg_RegularEcho;
	wgp.history = wg_NoHistory;
	(void) wg_Gets(&wgp);
	cbreak();						/* wg_Gets turns off cbreak and delay. */

	TraceMsg("[%s]\n", str);
	AtoIMaybe(dst, str);
	wmove(gPrefsWin, maxy - 1, 0);
	wclrtoeol(gPrefsWin);
	wrefresh(gPrefsWin);
	curs_set(0);
}	/* PrefsWinGetNum */




/* This is the meat of the preferences window.  We can selectively update
 * portions of the window by using a bitmask with bits set for items
 * we want to update.
 */
void PrefsWinDraw(int flags, int hilite)
{
	int i;
	string value;
	char spec[32];
	int maxx, maxy;

	getmaxyx(gPrefsWin, maxy, maxx);
	/* Draw the keys the user can type in reverse text. */
	WAttr(gPrefsWin, kReverse, 1);
	for (i = kFirstPrefsWinItem; i <= kLastPrefsWinItem; i++) {
		if (TESTBIT(flags, i))
			mvwaddch(gPrefsWin, 2 + i, 2, 'A' + i);
	}
	
	/* The "quit" item is a special item that is offset a line, and
	 * always has the "X" key assigned to it.
	 */
	i = kQuitPrefsWinItem;
	if (TESTBIT(flags, i))
		mvwaddch(gPrefsWin, 3 + i, 2, 'X');
	WAttr(gPrefsWin, kReverse, 0);
	
	/* We can use this to hilite a whole line, to indicate to the
	 * user that a certain item is being edited.
	 */
	if (hilite)
		WAttr(gPrefsWin, kReverse, 1);

	sprintf(spec, " %%-28s%%-%ds",
		maxx - 28 - 6);

	for (i = kFirstPrefsWinItem; i <= kLastPrefsWinItem; i++) {
		if (TESTBIT(flags, i)) {
			GetPrefSetting(value, sizeof(value), i);
			mvwprintw(gPrefsWin, i + 2 , 3, spec, gPrefOpts[i].label, value);
			wclrtoeol(gPrefsWin);
		}
	}

	if (TESTBIT(flags, kQuitPrefsWinItem)) {
		mvwprintw(gPrefsWin, kQuitPrefsWinItem + 3, 3, " %-28s",
			"(Done editing)"
		);
		wclrtoeol(gPrefsWin);
	} 

	if (hilite)
		WAttr(gPrefsWin, kReverse, 0);

	wmove(gPrefsWin, maxy - 1, 0);
	wrefresh(gPrefsWin);
}	/* PrefsWinDraw */



/* The user can hit space to change values.  For these toggle
 * functions we do an update each time so the user can see the change
 * immediately.
 */
void PrefsWinToggle(int *val, int bitNum, int min, int max)
{
	int c;

	while (1) {
		c = wgetch(gPrefsWin);
		TraceMsg("[%c, 0x%x]\n", c, c);
		if ((c == 'q') || (c == 10) || (c == 13)
#ifdef KEY_ENTER
			|| (c == KEY_ENTER)
#endif
			)
			break;
		else if (isspace(c)) {
			TogglePref(val, min, max);
			PrefsWinDraw(BIT(bitNum), kHilite);
		}
	}
}	/* PrefsWinToggle */




/*ARGSUSED*/
void SigIntPrefsWin(void)
{
	alarm(0);
	longjmp(gPrefsWinJmp, 1);
}	/* SigIntPrefsWin */


#endif	/* USE_CURSES */


		


/* Runs the preferences editor. */
int PrefsWindow(void)
{
#ifdef USE_CURSES
	int c, field, i;

	if (gWinInit) {
		gPrefsWin = newwin(LINES, COLS, 0, 0);
		if (gPrefsWin == NULL)
			return (kCmdErr);

		if (setjmp(gPrefsWinJmp) == 0) {
			/* Gracefully cleanup the screen if the user ^C's. */
			SIGNAL(SIGINT, SigIntPrefsWin);

			curs_set(0);
			cbreak();
			/* leaveok(gPrefsWin, TRUE);	* Not sure if I like this... */
			
			/* Set the clear flag for the first update. */
			wclear(gPrefsWin);
	
			WAttr(gPrefsWin, kBold, 1);
			WAddCenteredStr(gPrefsWin, 0, "Preferences");
			WAttr(gPrefsWin, kBold, 0);
	
			PrefsWinDraw(kAllWindowItems, kNoHilite);
			while (1) {
				PrefsWinWinMsg("Select an item to edit by typing its corresponding letter.");
				c = wgetch(gPrefsWin);
				TraceMsg("[%c, 0x%x]\n", c, c);
				if (islower(c))
					c = toupper(c);
				if (!isupper(c))
					continue;
				if (c == 'X')
					break;
				field = c - 'A';
				i = field;
				if (i > kLastPrefsWinItem)
					continue;

				/* Hilite the current item to edit. */
				PrefsWinDraw(BIT(i), kHilite);

				/* Print the instructions. */
				PrefsWinWinMsg(gPrefOpts[i].msg);

				switch (gPrefOpts[i].type) {
					case kPrefInt:
						PrefsWinGetNum((int *) gPrefOpts[i].storage);
						break;
					case kPrefToggle:
						PrefsWinToggle((int *) gPrefOpts[i].storage,
							i, gPrefOpts[i].min, gPrefOpts[i].max);
						break;
					case kPrefStr:
						PrefsWinGetStr((char *) gPrefOpts[i].storage,
							gPrefOpts[i].min,	/* Used for Empty */
							gPrefOpts[i].max	/* Used for Echo */
						);
						break;
				}
				
				/* Update and unhilite it. */
				PrefsWinDraw(BIT(field), kNoHilite);
			}
		}

		delwin(gPrefsWin);
		gPrefsWin = NULL;
		UpdateScreen(1);
		flushinp();
		nocbreak();
		curs_set(1);
	}
#endif	/* USE_CURSES */
	return (kNoErr);
}	/* PrefsWindow */





void ShowAll(void)
{
	int i;
	string value;

	MultiLineInit();
	for (i = kFirstPrefsWinItem; i <= kLastPrefsWinItem; i++) {
		GetPrefSetting(value, sizeof(value), i);
		MultiLinePrintF("%-28s%s\n", gPrefOpts[i].label, value);
	}
}	/* ShowAll */




static
void ShowSetHelp(void)
{
	int i;

	MultiLineInit();
	for (i = kFirstPrefsWinItem; i <= kLastPrefsWinItem; i++) {
		MultiLinePrintF("%-15s %s\n", gPrefOpts[i].name, gPrefOpts[i].label);
	}
}	/* ShowSetHelp */





int SetCmd(int argc, char **argv)
{
	int i, j, match;
	size_t len;
	string value;

	if ((argc == 1) || ISTREQ(argv[1], "all")) {
		ShowAll();
	} else if (ISTREQ(argv[1], "help")) {
		ShowSetHelp();
	} else {
		len = strlen(argv[1]);
		for (i = kFirstPrefsWinItem, match = -1; i <= kLastPrefsWinItem; i++) {
			if (ISTRNEQ(gPrefOpts[i].name, argv[1], len)) {
				if (match >= 0) {
					Error(kDontPerror, "Ambiguous option name \"%s.\"\n",
						argv[1]);
					return (kCmdErr);
				}
				match = i;
			}
		}
		if (match < 0) {
			Error(kDontPerror, "Unknown option name \"%s.\"\n",
				argv[1]);
			return (kCmdErr);
		}
		i = match;
		switch (gPrefOpts[i].type) {
			case kPrefInt:
				if (argc > 2)
					AtoIMaybe((int *) gPrefOpts[i].storage, argv[2]);
				break;
			case kPrefToggle:
				if (argc > 2) {
					/* User can set it directly instead of cycling through
					 * the choices.
					 */
					AtoIMaybe((int *) &j, argv[2]);
					if (j < gPrefOpts[i].min)
						j = gPrefOpts[i].min;
					else if (j > gPrefOpts[i].max)
						j = gPrefOpts[i].max;
					* (int *) gPrefOpts[i].storage = j;
				} else {
					/* User toggled to the next choice. */
					TogglePref((int *) gPrefOpts[i].storage,
						gPrefOpts[i].min, gPrefOpts[i].max);
				}
				break;
			case kPrefStr:
				if (argc > 2)
					(void) Strncpy((char *) gPrefOpts[i].storage,
						argv[2], gPrefOpts[i].siz);
				break;
		}

		/* Print the (possibly new) value. */
		GetPrefSetting(value, sizeof(value), i);
		PrintF("%-28s%s\n", gPrefOpts[i].label, value);
	}
	return (kNoErr);
}	/* SetCmd */





int PrefsCmd(void)
{
#ifdef USE_CURSES
	int err;

	if (!gWinInit) {
		EPrintF("%s\n%s\n",
		"The preferences editor only works in visual mode.",
		"However, you can use the 'set' command to set a single option at a time."
		);
		return (kCmdErr);
	}
	err = PrefsWindow();
	Beep(0);    /* User should be aware that it took a while, so no beep. */
	return (err);
#else
	EPrintF("%s\n%s\n",
	"You can't do this because the program doesn't have the curses library.",
	"However, you can use the 'set' command to set a single option at a time."
	);
	return (kCmdErr);
#endif	/* USE_CURSES */
}	/* PrefsCmd */




void WritePrefs(void)
{
	longstring path;
	FILE *fp;
	int i;

	if (gOurDirectoryPath[0] == '\0')
		return;		/* Don't create in root directory. */

	OurDirectoryPath(path, sizeof(path), kPrefsName);
	fp = fopen(path, "w");
	if (fp == NULL) {
		Error(kDoPerror, "Can't open %s for writing.\n", path);
		return;
	}
	for (i = kFirstPrefsWinItem; i <= kLastPrefsWinItem; i++) {
		switch (gPrefOpts[i].type) {
			case kPrefInt:
			case kPrefToggle:
				fprintf(fp, "%s %d\n", gPrefOpts[i].name,
					* (int *) gPrefOpts[i].storage);
				break;
			case kPrefStr:
				fprintf(fp, "%s %s\n", gPrefOpts[i].name,
					(char *) gPrefOpts[i].storage);
				break;
		}
	}
	for (i = 0; i < kNumNonEditablePrefOpts; i++) {
		switch (gNonEditPrefOpts[i].type) {
			case kPrefInt:
			case kPrefToggle:
				fprintf(fp, "%s %d\n", gNonEditPrefOpts[i].name,
					* (int *) gNonEditPrefOpts[i].storage);
				break;
			case kPrefStr:
				fprintf(fp, "%s %s\n", gNonEditPrefOpts[i].name,
					(char *) gNonEditPrefOpts[i].storage);
				break;
		}
	}

	fclose(fp);
}	/* WritePrefs */





static
int PrefSearchProc(char *key, const PrefOpt *b)
{
	return (ISTRCMP(key, (*b).name));	
}	/* PrefSearchProc */





void ReadPrefs(void)
{
	longstring path;
	FILE *fp;
	string option;
	longstring val;
	longstring line;
	int o;
	PrefOpt *pop;

	OurDirectoryPath(path, sizeof(path), kPrefsName);
	fp = fopen(path, "r");
	if (fp == NULL) {
		/* It's okay if we don't have one. */
		return;
	}

	while (FGets(line, (int) sizeof(line) - 1, fp) != NULL) {
		if (sscanf(line, "%s", option) < 1)
			continue;
		o = strlen(option) + 1;
		STRNCPY(val, line + o);
		pop = (PrefOpt *) BSEARCH(option, gPrefOpts, SZ(gNumEditablePrefOpts),
			sizeof(PrefOpt), PrefSearchProc);
		if (pop == NULL) {
			pop = (PrefOpt *) BSEARCH(option, gNonEditPrefOpts,
				SZ(kNumNonEditablePrefOpts),
				sizeof(PrefOpt), PrefSearchProc);
			if (pop == NULL) {
				Error(kDontPerror, "Unrecognized preference option \"%s\".\n",
					option);
				continue;
			}
		}
		switch (pop->type) {
			case kPrefInt:
			case kPrefToggle:
				* (int *) pop->storage = atoi(val);
				break;
			case kPrefStr:
				(void) Strncpy((char *) pop->storage, val, pop->siz);
				break;
		}
	}

	fclose(fp);
}	/* ReadPrefs */
