/* Hostwin.c */

#include "Sys.h"
#include "Curses.h"

#include <ctype.h>
#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "Cmds.h"
#include "Bookmark.h"
#include "Open.h"
#include "Hostwin.h"

#ifdef USE_CURSES

#include "WGets.h"

/* This is the full-screen window that pops up when you run the
 * host editor.  Not much is done with it, except display directions
 * and getting host editor commands.
 */
WINDOW *gHostWin = NULL;

/* This is a window devoted solely to serve as a scrolling list
 * of bookmarks.
 */
WINDOW *gHostListWin = NULL;

/* This is another full-screen window that opens when a user wants
 * to edit the parameters for a site.
 */
WINDOW *gEditHostWin = NULL;

extern void WAddCenteredStr(WINDOW *w, int y, char *str);
void WAttr(WINDOW *w, int attr, int on);

#endif	/* USE_CURSES */

/* This is an index into the host list.  This indicates the position
 * in the host list where we draw the current "page" of the host list.
 */
int gHostListWinStart;

/* This index is the currently selected host.  This index must be >=
 * to gHostListWinStart and less than gHostListWinStart + pageLen - 1,
 * so that this host will show up in the current page of the host list.
 */
int gHilitedHost;

/* This is a pointer to the actual information of the currently
 * selected host.
 */
BookmarkPtr gCurHostListItem;

/* How many lines compose a "page" in the host list's scrolling window. */
int gHostListPageSize;

/* A flag saying if we need to erase a message after the next input key. */
int gNeedToClearMsg = 0;

/* When we edit gCurHostListItem's stuff, we actually edit a copy of it.
 * This is so we could restore the information if the user wanted to
 * abort the changes.
 */
Bookmark gEditRsi;

jmp_buf gHostWinJmp;

extern int gWinInit, gNumBookmarks, gRmtInfoIsNew;
extern BookmarkPtrList gBookmarks;
extern BookmarkPtr gHosts;
extern Bookmark gRmtInfo;
extern string gEmailAddress, gAnonPassword;
extern unsigned int gFTPPort;

void AtoIMaybe(int *dst, char *str)
{
	char *cp;
	
	/* Don't change the value if the user just hit return. */
	for (cp = str; *cp != '\0'; cp++)
		if (isdigit(*cp))
			break;
	if (isdigit(*cp))
		*dst = atoi(str);
}	/* AtoIMaybe */




#ifdef USE_CURSES

/* Draws the screen when we're using the host editor's main screen.
 * You can can specify whether to draw each character whether it needs
 * it or not if you like.
 */
void UpdateHostWindows(int uptAll)
{
	if (uptAll) {
		DrawHostList();
		touchwin(gHostListWin);
		touchwin(gHostWin);
	}
	wnoutrefresh(gHostListWin);
	wnoutrefresh(gHostWin);
	doupdate();
}	/* UpdateHostWindows */



/* This draws the scrolling list of bookmarks, and hilites the currently
 * selected host.
 */
void DrawHostList(void)
{
	int lastLine, i;
	BookmarkPtr rsip;
	string str;
	int maxy, maxx;
	int lmaxy, lmaxx;
	int begy, begx;
	char spec[32];

	getmaxyx(gHostListWin, lmaxy, lmaxx);
	getbegyx(gHostListWin, begy, begx);
	getmaxyx(gHostWin, maxy, maxx);
	/* We have a status line saying how many bookmarks there are in
	 * the list.  That way the user knows something is supposed to
	 * be there when the host list is totally empty, and also that
	 * there are more bookmarks to look at when the entire host list
	 * doesn't fit in the scroll window.
	 */
	WAttr(gHostWin, kUnderline, 1);
	mvwprintw(
		gHostWin,
		begy - 1,
		begx,
		"%s",
		"Number of bookmarks"
	);
	WAttr(gHostWin, kUnderline, 0);
	wprintw(
		gHostWin,
		": %3d",
		gNumBookmarks
	);
	sprintf(spec, "%%-16s %%-%ds", lmaxx - 17);
	lastLine = lmaxy + gHostListWinStart;
	for (i=gHostListWinStart; (i<lastLine) && (i<gNumBookmarks); i++) {
		rsip = gBookmarks[i];
		if (rsip == gCurHostListItem)
			WAttr(gHostListWin, kReverse, 1);
		sprintf(str, spec, rsip->bookmarkName, rsip->name);
		str[lmaxx] = '\0';
		mvwaddstr(gHostListWin, i - gHostListWinStart, 0, str);
		if (rsip == gCurHostListItem)
			WAttr(gHostListWin, kReverse, 0);
	}

	/* Add 'vi' style empty-lines. */
	for ( ; i<lastLine; ++i) {
		mvwaddstr(gHostListWin, i - gHostListWinStart, 0, "~");
		wclrtoeol(gHostListWin);
	}
	wmove(gHostWin, maxy - 3, 2);
	sprintf(spec, "%%-%ds", maxx - 4);
	if ((gCurHostListItem == NULL) || (gCurHostListItem->comment[0] == '\0'))
		str[0] = '\0';
	else {
		STRNCPY(str, "``");
		STRNCAT(str, gCurHostListItem->comment);
		AbbrevStr(str + 2, gCurHostListItem->comment, maxx - 8, 1);
		STRNCAT(str, "''");
	}
	wprintw(gHostWin, spec, str);
	wmove(gHostWin, maxy - 1, 0);
	UpdateHostWindows(0);
}	/* DrawHostList */




/* This prompts for a key of input when in the main host editor window. */
int HostWinGetKey(void)
{
	int c;
	int maxy, maxx;

	getmaxyx(gHostWin, maxy, maxx);
	wmove(gHostWin, maxy - 1, 0);
	c = wgetch(gHostWin);
	TraceMsg("[%c, 0x%x]\n", c, c);
	return (c);
}	/* HostWinGetKey */



static
void NewHilitedHostIndex(int newIdx)
{
	int oldIdx, lastLine;

	if (gNumBookmarks <= 0) {
		HostWinMsg(
"No bookmarks in the list.  Try a /new, or open a site manually to add one.");
	} else {
		oldIdx = gHilitedHost;
		if (gNumBookmarks < gHostListPageSize)
			lastLine = gHostListWinStart + gNumBookmarks - 1;
		else
			lastLine = gHostListWinStart + gHostListPageSize - 1;
		if (newIdx < gHostListWinStart) {
			/* Will need to scroll the window up. */
			if (newIdx < 0) {
				newIdx = 0;
				if (oldIdx == newIdx)
					HostWinMsg("You are at the top of the list.");
			}
			gHilitedHost = gHostListWinStart = newIdx;
		} else if (newIdx > lastLine) {
			/* Will need to scroll the window down. */
			if (newIdx > (gNumBookmarks - 1)) {
				newIdx = gNumBookmarks - 1;
				if (oldIdx == newIdx)
					HostWinMsg("You are at the bottom of the list.");
			}
			gHilitedHost = newIdx;
			gHostListWinStart = newIdx - (gHostListPageSize - 1);
			if (gHostListWinStart < 0)
				gHostListWinStart = 0;
		} else {
			/* Don't need to scroll window, just move pointer. */
			gHilitedHost = newIdx;
		}
		gCurHostListItem = gBookmarks[gHilitedHost];
		if (oldIdx != newIdx)
			DrawHostList();
	}
}	/* NewHilitedHostIndex */




/* You can zip to a different area of the list without using the arrow
 * or page scrolling keys.  Just type a letter, and the list will scroll
 * to the first host starting with that letter.
 */
void HostWinZoomTo(int c)
{	
	int i, j;

	if (gNumBookmarks > 0) {
		if (islower(c))
			c = toupper(c);
		for (i=0; i<gNumBookmarks - 1; i++) {
			j = gBookmarks[i]->bookmarkName[0];
			if (islower(j))
				j = toupper(j);
			if (j >= c)
				break;
		}
		NewHilitedHostIndex(i);
	} else {
		HostWinMsg("No bookmarks to select.  Try a /new.");
	}
	DrawHostList();
}	/* HostWinZoomTo */





void HostListLineUp(void)
{
	NewHilitedHostIndex(gHilitedHost - 1);
}	/* HostListLineUp */





void HostListLineDown(void)
{
	NewHilitedHostIndex(gHilitedHost + 1);
}	/* HostListLineDown */




void HostListPageUp(void)
{
	NewHilitedHostIndex(gHilitedHost - gHostListPageSize);
}	/* HostListPageUp */




void HostListPageDown(void)
{
	NewHilitedHostIndex(gHilitedHost + gHostListPageSize);
}	/* HostListPageDown */



/* This marks the start of a section that belongs to the Bookmark Options
 * window.  This window pops up on top of the host editor's main window
 * when you wish to edit a site's settings.  When the user finishes,
 * we close it and the host editor resumes.
 */

/* This displays a message in the Bookmark Options window. */
void EditHostWinMsg(char *msg)
{
	int maxy, maxx;

	getmaxyx(gEditHostWin, maxy, maxx);
	mvwaddstr(gEditHostWin, maxy - 2, 0, msg);
	wclrtoeol(gEditHostWin);
	wmove(gEditHostWin, maxy - 1, 0);
	wrefresh(gEditHostWin);
}	/* EditHostWinMsg */




/* Prompts for a line of input. */
void EditHostWinGetStr(char *dst, size_t size, int canBeEmpty, int canEcho)
{
	string str;
	WGetsParams wgp;
	int maxy, maxx;

	WAttr(gEditHostWin, kBold, 1);
	getmaxyx(gEditHostWin, maxy, maxx);
	mvwaddstr(gEditHostWin, maxy - 1, 0, "> ");
	WAttr(gEditHostWin, kBold, 0);
	wclrtoeol(gEditHostWin);
	wrefresh(gEditHostWin);
	curs_set(1);

	wgp.w = gEditHostWin;
	wgp.sy = maxy - 1;
	wgp.sx = 2;
	wgp.fieldLen = maxx - 3;
	wgp.dst = str;
	wgp.dstSize = size;
	wgp.useCurrentContents = 0;
	wgp.echoMode = canEcho ? wg_RegularEcho : wg_BulletEcho;
	wgp.history = wg_NoHistory;
	(void) wg_Gets(&wgp);
	cbreak();						/* wg_Gets turns off cbreak and delay. */

	TraceMsg("[%s]\n", wgp.dst);
	
	/* See if the user just hit return.  We may not want to overwrite
	 * the dst here, which would make it an empty string.
	 */
	if ((wgp.changed) || (canBeEmpty == kOkayIfEmpty))
		strcpy(dst, str);

	wmove(gEditHostWin, maxy - 1, 0);
	wclrtoeol(gEditHostWin);
	wrefresh(gEditHostWin);
	curs_set(0);
}	/* EditHostWinGetStr */





/* Prompts for an integer of input. */
void EditHostWinGetNum(int *dst)
{
	WGetsParams wgp;
	string str;
	int maxy, maxx;

	getmaxyx(gEditHostWin, maxy, maxx);
	WAttr(gEditHostWin, kBold, 1);
	mvwaddstr(gEditHostWin, maxy - 1, 0, "> ");
	WAttr(gEditHostWin, kBold, 0);
	wclrtoeol(gEditHostWin);
	wrefresh(gEditHostWin);
	curs_set(1);

	wgp.w = gEditHostWin;
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
	wmove(gEditHostWin, maxy - 1, 0);
	wclrtoeol(gEditHostWin);
	wrefresh(gEditHostWin);
	curs_set(0);
}	/* EditHostWinGetNum */




/* This is the meat of the site options window.  We can selectively update
 * portions of the window by using a bitmask with bits set for items
 * we want to update.
 */
void EditHostWinDraw(int flags, int hilite)
{
	int maxy, maxx;
	int i, f;
	string str;
	char spec[32];
	char *cp;

	/* Draw the keys the user can type in reverse text. */
	WAttr(gEditHostWin, kReverse, 1);
	f = 5;
	for (i = kFirstEditWindowItem; i <= kLastEditWindowItem; i++) {
		if (TESTBIT(flags, i))
			mvwaddch(gEditHostWin, f + i, 2, 'A' + i);
	}
	
	/* The "quit" item is a special item that is offset a line, and
	 * always has the "X" key assigned to it.
	 */
	i = kQuitEditWindowItem;
	if (TESTBIT(flags, i))
		mvwaddch(gEditHostWin, 1 + f + i, 2, 'X');
	WAttr(gEditHostWin, kReverse, 0);
	
	/* We can use this to hilite a whole line, to indicate to the
	 * user that a certain item is being edited.
	 */
	if (hilite)
		WAttr(gEditHostWin, kReverse, 1);
	getmaxyx(gEditHostWin, maxy, maxx);
	sprintf(spec, " %%-26s%%-%ds",
		maxx - 32);

	/* Now draw the items on a case-by-case basis. */
	if (TESTBIT(flags, kNicknameEditWindowItem)) {
		mvwprintw(gEditHostWin, kNicknameEditWindowItem + f, 3, spec,
			"Bookmark name:",
			gEditRsi.bookmarkName
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kHostnameEditWindowItem)) {
		mvwprintw(gEditHostWin, kHostnameEditWindowItem + f, 3, spec,
			"Hostname:",
			gEditRsi.name
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kUserEditWindowItem)) {
		mvwprintw(gEditHostWin, kUserEditWindowItem + f, 3, spec,
			"User:",
			gEditRsi.user[0] == '\0' ? "anonymous" : gEditRsi.user
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kPassEditWindowItem)) {
		if (gEditRsi.pass[0] == '\0' && gEditRsi.user[0] == '\0')
			STRNCPY(str, gAnonPassword);
		mvwprintw(gEditHostWin, kPassEditWindowItem + f, 3, spec,
			"Password:",
			strcmp(str, gAnonPassword) ? "********" : str
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kAcctEditWindowItem)) {
		mvwprintw(gEditHostWin, kAcctEditWindowItem + f, 3, spec,
			"Account:",
			gEditRsi.acct[0] == '\0' ? "none" : gEditRsi.acct
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kDirEditWindowItem)) {
		if (gEditRsi.dir[0] == '\0')
			STRNCPY(str, "/");
		else
			AbbrevStr(str, gEditRsi.dir, maxx - 32, 0);
		mvwprintw(gEditHostWin, kDirEditWindowItem + f, 3, spec,
			"Directory:",
			str
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kXferTypeEditWindowItem)) {
		if ((gEditRsi.xferType == 'I') || (gEditRsi.xferType == 'B'))
			cp = "Binary";
		else if (gEditRsi.xferType == 'A')
			cp = "ASCII Text";
		else
			cp = "Tenex";
		mvwprintw(gEditHostWin, kXferTypeEditWindowItem + f, 3, spec,
			"Transfer type:",
			cp
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kPortEditWindowItem)) {
		sprintf(str, "%u", gEditRsi.port);
		mvwprintw(gEditHostWin, kPortEditWindowItem + f, 3, spec,
			"Port:",
			str
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kSizeEditWindowItem)) {
		mvwprintw(gEditHostWin, kSizeEditWindowItem + f, 3, spec,
			"Has SIZE command:",
			gEditRsi.hasSIZE ? "Yes" : "No"
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kMdtmEditWindowItem)) {
		mvwprintw(gEditHostWin, kMdtmEditWindowItem + f, 3, spec,
			"Has MDTM command:",
			gEditRsi.hasMDTM ? "Yes" : "No"
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kPasvEditWindowItem)) {
		mvwprintw(gEditHostWin, kPasvEditWindowItem + f, 3, spec,
			"Can use passive FTP:",
			gEditRsi.hasPASV ? "Yes" : "No"
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kOSEditWindowItem)) {
		mvwprintw(gEditHostWin, kOSEditWindowItem + f, 3, spec,
			"Operating System:",
			(gEditRsi.isUnix == 1) ? "UNIX" : "Non-UNIX"
		);
		wclrtoeol(gEditHostWin);
	} 
	if (TESTBIT(flags, kCommentEditWindowItem)) {
		if (gEditRsi.comment[0] == '\0')
			STRNCPY(str, "(none)");
		else
			AbbrevStr(str, gEditRsi.comment, maxx - 32, 0);
		mvwprintw(gEditHostWin, kCommentEditWindowItem + f, 3, spec,
			"Comment:",
			str
		);
		wclrtoeol(gEditHostWin);
	}
	if (TESTBIT(flags, kQuitEditWindowItem)) {
		mvwprintw(gEditHostWin, kQuitEditWindowItem + f + 1, 3, spec,
			"(Done editing)",
			""
		);
		wclrtoeol(gEditHostWin);
	}

	if (hilite)
		WAttr(gEditHostWin, kReverse, 0);

	wmove(gEditHostWin, maxy - 1, 0);
	wrefresh(gEditHostWin);
}	/* EditHostWinDraw */



/* The user can hit space to change the transfer type.  For these toggle
 * functions we do an update each time so the user can see the change
 * immediately.
 */
void ToggleXferType(void)
{
	int c;

	while (1) {
		c = wgetch(gEditHostWin);
		TraceMsg("[%c, 0x%x]\n", c, c);
		if ((c == 'x') || (c == 10) || (c == 13)
#ifdef KEY_ENTER
			|| (c == KEY_ENTER)
#endif
			)
			break;
		else if (isspace(c)) {
			if (gEditRsi.xferType == 'A')
				gEditRsi.xferType = 'I';
			else if ((gEditRsi.xferType == 'B') || (gEditRsi.xferType == 'I'))
				gEditRsi.xferType = 'T';
			else
				gEditRsi.xferType = 'A';
			EditHostWinDraw(BIT(kXferTypeEditWindowItem), kHilite);
		}
	}
}	/* ToggleXferType */




void EditWinToggle(int *val, int bitNum, int min, int max)
{
	int c;

	while (1) {
		c = wgetch(gEditHostWin);
		TraceMsg("[%c, 0x%x]\n", c, c);
		if ((c == 'x') || (c == 10) || (c == 13)
#ifdef KEY_ENTER
			|| (c == KEY_ENTER)
#endif
			)
			break;
		else if (isspace(c)) {
			*val = *val + 1;
			if (*val > max)
				*val = min;
			EditHostWinDraw(BIT(bitNum), kHilite);
		}
	}
}	/* EditWinToggle */



/* This opens and handles the site options window. */
void HostWinEdit(void)
{
	long amt;
	double rate;
	char rstr[32];
	int c, field;
	int needUpdate;
	string str;

	if (gCurHostListItem != NULL) {
		gEditHostWin = newwin(LINES, COLS, 0, 0);
		if (gEditHostWin == NULL)
			return;
		
		
		/* Set the clear flag for the first update. */
		wclear(gEditHostWin);

		/* leaveok(gEditHostWin, TRUE);	* Not sure if I like this... */
		WAttr(gEditHostWin, kBold, 1);
		WAddCenteredStr(gEditHostWin, 0, "Bookmark Options");
		WAttr(gEditHostWin, kBold, 0);
		
		/* We'll be editing a copy of the current host's settings. */
		gEditRsi = *gCurHostListItem;

		if (gEditRsi.lastCall != (time_t) 0) {
			strcpy(str, ctime(&gEditRsi.lastCall));
			/* Why-o-why does ctime append a newline.  I hate that! */
			str[strlen(str) - 1] = '\0';
			mvwprintw(gEditHostWin, 2, 5,
				"Number of calls: %d.    Last call: %s",
				gEditRsi.nCalls,
				str
			);
			amt = gEditRsi.xferKbytes;
			if (amt > 0) {
				if (amt < 1000) {
					sprintf(str, "%ld kBytes", amt);
				} else if (amt < 1000000) {
					sprintf(str, "%.2f MBytes", ((double) amt / 1000.0));
				} else {
					sprintf(str, "%.2f GBytes", ((double) amt / 1000000.0));
				}

				rate = (double) amt / gEditRsi.xferHSeconds * 100.0;
				if (rate > 0) {
					if (rate < 1000.0) {
						sprintf(rstr, "%.2f kBytes/sec", rate);
					} else if (rate < 1000000.0) {
						sprintf(rstr, "%.2f MBytes/sec", rate / 1000.0);
					} else {
						sprintf(rstr, "%.2f GBytes/sec", rate / 1000000.0);
					}
					mvwprintw(gEditHostWin, 3, 5,
						"You have transferred %s, averaging %s.",
						str,
						rstr
					);
				}
			}
		}

		EditHostWinDraw(kAllWindowItems, kNoHilite);
		field = 1;
		while (1) {
			EditHostWinMsg("Select an item to edit by typing its corresponding letter.");
			c = wgetch(gEditHostWin);
			TraceMsg("[%c, 0x%x]\n", c, c);
			if (islower(c))
				c = toupper(c);
			if (!isupper(c))
				continue;
			if (c == 'X')
				break;
			field = c - 'A';
			needUpdate = 1;
			
			/* Hilite the current item to edit. */
			EditHostWinDraw(BIT(field), kHilite);
			switch(field) {
				case kNicknameEditWindowItem:
					EditHostWinMsg("Type a new bookmark name, or hit <RETURN> to continue.");
					EditHostWinGetStr(gEditRsi.bookmarkName, sizeof(gEditRsi.bookmarkName), kNotOkayIfEmpty, kGetAndEcho);
					break;
					
				case kHostnameEditWindowItem:
					EditHostWinMsg("Type a new hostname, or hit <RETURN> to continue.");
					EditHostWinGetStr(gEditRsi.name, sizeof(gEditRsi.name), kNotOkayIfEmpty, kGetAndEcho);
					break;

				case kUserEditWindowItem:
					EditHostWinMsg("Type a username, or hit <RETURN> to signify anonymous.");
					EditHostWinGetStr(gEditRsi.user, sizeof(gEditRsi.user), kOkayIfEmpty, kGetAndEcho);
					break;

				case kPassEditWindowItem:
					EditHostWinMsg("Type a password, or hit <RETURN> if no password is required.");
					EditHostWinGetStr(gEditRsi.pass, sizeof(gEditRsi.pass), kOkayIfEmpty, kGetNoEcho);
					break;

				case kAcctEditWindowItem:
					EditHostWinMsg("Type an account name, or hit <RETURN> if no account is required.");
					EditHostWinGetStr(gEditRsi.acct, sizeof(gEditRsi.acct), kOkayIfEmpty, kGetAndEcho);
					break;

				case kDirEditWindowItem:
					EditHostWinMsg("Type a directory path to start in after a connection is established.");
					EditHostWinGetStr(gEditRsi.dir, sizeof(gEditRsi.dir), kOkayIfEmpty, kGetAndEcho);
					break;

				case kXferTypeEditWindowItem:
					EditHostWinMsg(kToggleMsg);
					ToggleXferType();
					break;

				case kPortEditWindowItem:
					EditHostWinMsg("Type a port number to use for FTP.");
					EditHostWinGetNum((int *) &gEditRsi.port);
					break;

				case kSizeEditWindowItem:
					EditHostWinMsg(kToggleMsg);
					EditWinToggle(&gEditRsi.hasSIZE, field, 0, 1);
					break;

				case kMdtmEditWindowItem:
					EditHostWinMsg(kToggleMsg);
					EditWinToggle(&gEditRsi.hasMDTM, field, 0, 1);
					break;

				case kPasvEditWindowItem:
					EditHostWinMsg(kToggleMsg);
					EditWinToggle(&gEditRsi.hasPASV, field, 0, 1);
					break;

				case kOSEditWindowItem:
					EditHostWinMsg(kToggleMsg);
					EditWinToggle(&gEditRsi.isUnix, field, 0, 1);
					break;

				case kCommentEditWindowItem:
					EditHostWinMsg("Enter a line of information to store about this site.");
					EditHostWinGetStr(gEditRsi.comment, sizeof(gEditRsi.comment), kOkayIfEmpty, kGetAndEcho);
					break;
				
				default:
					needUpdate = 0;
					break;
			}
			if (needUpdate)
				EditHostWinDraw(BIT(field), kNoHilite);
		}
		delwin(gEditHostWin);
		gEditHostWin = NULL;
		*gCurHostListItem = gEditRsi;
		SortBookmarks();
		NewHilitedHostIndex(gCurHostListItem->index);
		UpdateHostWindows(1);
	}
}	/* HostWinEdit */



/* Clones an existing site in the host list. */
void HostWinDup(void)
{
	if (gCurHostListItem != NULL) {
		gCurHostListItem = DuplicateBookmark(gCurHostListItem);
		gHilitedHost = gCurHostListItem->index;
	} else
		HostWinMsg("Nothing to duplicate.");
	DrawHostList();
}	/* HostWinDup */




/* Removes a site from the host list. */
void HostWinDelete(void)
{
	BookmarkPtr toDelete;
	int newi;
	
	if (gCurHostListItem != NULL) {
		toDelete = gCurHostListItem;

		/* Need to choose a new active host after deletion. */
		if (gHilitedHost == gNumBookmarks - 1) {
			if (gNumBookmarks == 1) {
				newi = -1;	/* None left. */
			} else {
				/* At last one before delete. */
				newi = gHilitedHost - 1;
			}
		} else {
			/* Won't need to increment gHilitedHost here, since after deletion,
			 * the next one will move up into this slot.
			 */
			newi = gHilitedHost;
		}
		DeleteBookmark(toDelete);
		if (newi < 0)
			gCurHostListItem = NULL;
		else {
			gCurHostListItem = gBookmarks[newi];
			gHilitedHost = newi;
		}
	} else
		HostWinMsg("Nothing to delete.");
	DrawHostList();
}	/* HostWinDelete */




/* Adds a new site to the host list, with default settings in place. */
void HostWinNew(void)
{
	BookmarkPtr rsip;
	Bookmark rsi;

	SetNewBookmarkDefaults(&rsi);
	rsip = AddBookmarkPtr(&rsi);
	rsip->port = gFTPPort;
	SortBookmarks();
	gCurHostListItem = rsip;
	gHilitedHost = rsip->index;
	gHostListWinStart = rsip->index - gHostListPageSize + 1;
	if (gHostListWinStart < 0)
		gHostListWinStart = 0;
	DrawHostList();
}	/* HostWinNew */




/* This displays a message in the host editor's main window.
 * Used mostly for error messages.
 */
void HostWinMsg(char *msg)
{
	int maxy, maxx;

	getmaxyx(gHostWin, maxy, maxx);
	mvwaddstr(gHostWin, maxy - 2, 0, msg);
	wclrtoeol(gHostWin);
	wmove(gHostWin, maxy - 1, 0);
	wrefresh(gHostWin);
	beep();
	gNeedToClearMsg = 1;
}	/* HostWinMsg */




/* Prompts for a line of input. */
void HostWinGetStr(char *str, size_t size)
{
	WGetsParams wgp;
	int maxy, maxx;

	getmaxyx(gHostWin, maxy, maxx);
	mvwaddstr(gHostWin, maxy - 1, 0, "/");
	wclrtoeol(gHostWin);
	wrefresh(gHostWin);
	curs_set(1);
	wgp.w = gHostWin;
	wgp.sy = maxy - 1;
	wgp.sx = 1;
	wgp.fieldLen = maxx - 1;
	wgp.dst = str;
	wgp.dstSize = size;
	wgp.useCurrentContents = 0;
	wgp.echoMode = wg_RegularEcho;
	wgp.history = wg_NoHistory;
	(void) wg_Gets(&wgp);
	cbreak();						/* wg_Gets turns off cbreak and delay. */

	TraceMsg("[%s]\n", str);
	wmove(gHostWin, maxy - 1, 0);
	wclrtoeol(gHostWin);
	wrefresh(gHostWin);
	curs_set(0);
}	/* HostWinGetStr */




/*ARGSUSED*/
void SigIntHostWin(/* int sigNum */ void)
{
	alarm(0);
	longjmp(gHostWinJmp, 1);
}	/* SigIntHostWin */


#endif	/* USE_CURSES */


/* Runs the host editor.  Another big use for this is to open sites
 * that are in your host list.
 */
int HostWindow(void)
{
#ifdef USE_CURSES
	int c;
	string cmd;
	volatile BookmarkPtr toOpen;
	VSig_t si;
	int maxy, maxx;
	int lmaxy, lmaxx;
	OpenOptions	openopt;

	si = kNoSignalHandler;
	if (gWinInit) {
		gHostListWin = NULL;
		gHostWin = NULL;

		gHostWin = newwin(LINES, COLS, 0, 0);
		if (gHostWin == NULL)
			return (kCmdErr);

		curs_set(0);
		cbreak();
		
		/* Set the clear flag for the first update. */
		wclear(gHostWin);
		keypad(gHostWin, TRUE);		/* For arrow keys. */
#ifdef HAVE_NOTIMEOUT
		notimeout(gHostWin, TRUE);
#endif

		if (setjmp(gHostWinJmp) == 0) {
			/* Gracefully cleanup the screen if the user ^C's. */
			si = (VSig_t) SIGNAL(SIGINT, SigIntHostWin);
			
			/* Initialize the page start and select a host to be
			 * the current one.
			 */
			gHostListWinStart = 0;
			gHilitedHost = 0;
			if (gNumBookmarks == 0)
				gCurHostListItem = NULL;
			else
				gCurHostListItem = gBookmarks[gHilitedHost];
			
			/* Initially, we don't want to connect to any site in
			 * the host list.
			 */
			toOpen = NULL;
	
			WAttr(gHostWin, kBold, 1);
			WAddCenteredStr(gHostWin, 0, "Bookmark Editor");
			WAttr(gHostWin, kBold, 0);
			
			mvwaddstr(gHostWin, 3, 2, "Open selected site:       <enter>");
			mvwaddstr(gHostWin, 4, 2, "Edit selected site:       /ed");
			mvwaddstr(gHostWin, 5, 2, "Delete selected site:     /del");
			mvwaddstr(gHostWin, 6, 2, "Duplicate selected site:  /dup");
			mvwaddstr(gHostWin, 7, 2, "Add a new site:           /new");
			mvwaddstr(gHostWin, 9, 2, "Up one:                   <u>");
			mvwaddstr(gHostWin, 10, 2, "Down one:                 <d>");
			mvwaddstr(gHostWin, 11, 2, "Previous page:            <p>");
			mvwaddstr(gHostWin, 12, 2, "Next page:                <n>");
			mvwaddstr(gHostWin, 14, 2, "Capital letters selects first");
			mvwaddstr(gHostWin, 15, 2, "  site starting with the letter.");
			mvwaddstr(gHostWin, 17, 2, "Exit the bookmark editor: <x>");
			
			/* Initialize the scrolling host list window. */
			gHostListWin = subwin(
				gHostWin,
				LINES - 7,
				40,
				3,
				COLS - 40 - 2
			);
			if (gHostListWin == NULL)
				return (kCmdErr);
			getmaxyx(gHostListWin, lmaxy, lmaxx);
			getmaxyx(gHostWin, maxy, maxx);
			gHostListPageSize = lmaxy;
			DrawHostList();
			wmove(gHostWin, maxy - 1, 0);
			UpdateHostWindows(1);

			while (1) {
				c = HostWinGetKey();
				if (gNeedToClearMsg) {
					wmove(gHostWin, maxy - 2, 0);
					wclrtoeol(gHostWin);
					wrefresh(gHostWin);
				}
				if ((c >= 'A') && (c <= 'Z')) {
					/* isupper can coredump if wgetch returns a meta key. */
					HostWinZoomTo(c);
				} else if (c == '/') {
					/* Get an "extended" command.  Sort of like vi's
					 * :colon commands.
					 */
					HostWinGetStr(cmd, sizeof(cmd));
	
					if (ISTREQ(cmd, "ed"))
						HostWinEdit();
					else if (ISTREQ(cmd, "dup"))
						HostWinDup();
					else if (ISTREQ(cmd, "del"))
						HostWinDelete();
					else if (ISTREQ(cmd, "new"))
						HostWinNew();
					else
						HostWinMsg("Invalid bookmark editor command.");
				} else switch(c) {
					case 10:	/* ^J == newline */
					case 13:	/* ^M == carriage return */
#ifdef KEY_ENTER
					case KEY_ENTER:
#endif
						if (gCurHostListItem == NULL)
							HostWinMsg("Nothing to open.  Try 'open sitename' from the main screen.");
						else {
							toOpen = (volatile BookmarkPtr) gCurHostListItem;
							goto done;
						}
						break;
	
					case kControl_L:
						UpdateHostWindows(1);
						break;
	
					case 'u':
					case 'k':	/* vi up key */
#ifdef KEY_UP
					case KEY_UP:
#endif
						HostListLineUp();
						break;
					
					case 'd':
					case 'j':	/* vi down key */
#ifdef KEY_DOWN
					case KEY_DOWN:
#endif
						HostListLineDown();
						break;
						
					case 'p':
#ifdef KEY_LEFT
					case KEY_LEFT:
#endif
#ifdef KEY_PPAGE
					case KEY_PPAGE:
#endif
						HostListPageUp();
						break;
						
					case 'n':
#ifdef KEY_RIGHT
					case KEY_RIGHT:
#endif
#ifdef KEY_NPAGE
					case KEY_NPAGE:
#endif
						HostListPageDown();
						break;
	
#ifdef KEY_END
					case KEY_END:
#endif
					case 'x':
					case 'q':
						goto done;
	
					default:
						HostWinMsg("Invalid key.");
						break;
				}
			}
		}
		SIGNAL(SIGINT, SIG_IGN);
done:
		if (gHostListWin != NULL)
			delwin(gHostListWin);
		if (gHostWin != NULL)
			delwin(gHostWin);
		gHostListWin = gHostWin = NULL;
		curs_set(1);
		nocbreak();
		UpdateScreen(1);
		flushinp();
		if (si != (Sig_t) kNoSignalHandler)
			SIGNAL(SIGINT, si);
		if (toOpen != (volatile BookmarkPtr) 0) {
			/* If the user selected a site to open, connect to it now. */
			InitOpenOptions(&openopt);
			STRNCPY(openopt.hostname, ((BookmarkPtr) toOpen)->bookmarkName);
			GetBookmark(openopt.hostname, sizeof(openopt.hostname));
			openopt.port = gRmtInfo.port;
			Beep(0);    /* Reset beep timer. */
			return (Open(&openopt));
		}
	}
#endif	/* USE_CURSES */
	Beep(0);    /* User should be aware that it took a while, so no beep. */
	return (kNoErr);
}	/* HostWindow */




int HostsCmd(void)
{
#ifdef USE_CURSES
	if (!gWinInit) {
		Error(kDontPerror, "This only works in visual mode.\n");
		return (kCmdErr);
	}
	return HostWindow();
#else
	Error(kDontPerror,
	"You can't do this because the program doesn't have the curses library.\n");
	return (kCmdErr);
#endif	/* USE_CURSES */
}	/* HostsCmd */
