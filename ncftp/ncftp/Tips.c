/* Tips.c */

#include "Sys.h"
#include "Curses.h"

#include <ctype.h>

#include "Util.h"
#include "Tips.h"

extern int gScreenWidth;

static char *gTipList[] = {

#ifdef USE_CURSES

/* Keep the curses tips separate so they won't show up if it isn't supported. */

"If you have visual mode turned on, NcFTP can use its own built-in mini \
pager. This makes the \"pdir\"-type commands unnecessary.",

"Remote and local filename completion is available. Use the tab key. \
Thanks to Tim MacKenzie (t.mackenzie@trl.oz.au)",

"If you don't like visual mode, you can start the program with \"ncftp -L\" to \
turn it off at startup. To do that permanently, do a \"set visual 0\" from \
within the program.",

"If you don't like visual mode, you can still edit the \"bookmarks\" and \"prefs\" \
files by hand.  Use your editor on the files in ~/.ncftp.",

"Don't /add sites by hand using the Bookmark Editor. It's easier to just open a \
new site from the command line, and let the program add a new entry \
automatically.",

"Did you know the program keeps transfer statistics for each site? View a \
site in the Bookmark Editor (use /ed) to check the stats.",

"Full-screen (visual) mode has its own built-in command line editor and \
history. Use up/down arrow to scroll through the history. You can use the \
left and right arrow keys to move around on the same line.",

"The Bookmark Editor also acts as an \"open menu.\"  Select the site you want and \
hit the return key to open that site.",

"Unless you're using \"line mode,\" the program saves the command line \
history and restores it the next time you run the program.",

"If you don't remember a host's full name, open the Bookmark Editor (\"bookmarks\") \
and select it from the menu.",

"After opening a new site, NcFTP will assign it a bookmark.  You can then open \
the site by opening it by its bookmark name.  You can use the Bookmark Editor (\"bookmarks\") to \
change the nicknames if you want.",

#endif	/* USE_CURSES */

"You can abbreviate host names if you've opened them at least once before. \
If you opened wuarchive.wustl.edu yesterday, you could try just \"open wuarchive\" \
or just \"open wu\" today.",

"The \"open\" command accepts several flags.  Do a \"help open\" for details.",

"The \"get\" command accepts several neat flags.  Do a \"help get\" for details.",

"The \"get\" command skips files you already have.  To override that feature, \
use the \"-f\" flag.",

"The \"get\" command now acts like \"mget\" did on older versions of NcFTP, \
so you can get multiple files with one command as well as use wildcards.",

"The \"get\" command can now fetch whole directories. Try \"get -R\" sometime.", 
"Use the \"more\" command to view a remote file with your pager.",

#ifdef SYSLOG
"NcFTP was configured to log all of your connections and transfers to the \
system log.",
#endif

"This program is pronounced Nik-F-T-P.",

/* Weight this one a little more. */

"You can get the newest version of NcFTP from ftp.NcFTP.com, in the \
/ncftp directory.  NcFTP is FREEware!",

"You can get the newest version of NcFTP from ftp.NcFTP.com, in the \
/ncftp directory.  NcFTP is FREEware!",

"You can get the newest version of NcFTP from ftp.NcFTP.com, in the \
/ncftp directory.  NcFTP is FREEware!",

"Sometimes an alternate progress meter is used if the remote site isn't \
using the latest FTP protocol command set." ,

/* Weight this one a little more */

"Thank you for using NcFTP.  Ask your system administrator to try NcFTPd \
Server as an alternative to your ftpd or wu-ftpd! <http://www.ncftp.com>",

"Thank you for using NcFTP.  Ask your system administrator to try NcFTPd \
Server as an alternative to your ftpd or wu-ftpd! <http://www.ncftp.com>",

"Thank you for using NcFTP.  Ask your system administrator to try NcFTPd \
Server as an alternative to your ftpd or wu-ftpd! <http://www.ncftp.com>",

"Thank you for using NcFTP.  Ask your system administrator to try NcFTPd \
Server as an alternative to your ftpd or wu-ftpd! <http://www.ncftp.com>",

"Thank you for using NcFTP.  Ask your system administrator to try NcFTPd \
Server as an alternative to your ftpd or wu-ftpd! <http://www.ncftp.com>",

"If you need to report a bug, send me a ~/.ncftp/trace file too.  To enable \
tracing, turn on trace logging from the Prefs window, or type \"set trace 1\" \
from the command line. Then re-create your bug, quit the program, and \
send the trace file to ncftp@NcFTP.com.",

"NcFTP will write all sorts of cool debugging information to a file named \
\"trace\" in your .ncftp directory if you do a \"set trace 1\" at the start.",

"Sick and tired of these tips?  Type \"set tips 0\" any time."
};

void PrintTip(char *tip)
{
	char buf[256];
	char *cp, *dp;
	int i, sWidth, lines;
	
	sWidth = gScreenWidth - 2;
	for (cp = tip, lines = 0; *cp != '\0'; ++lines) {
		while (isspace(*cp))
			cp++;

		dp = buf;
		if (lines == 0) {
			strcpy(dp, "Tip: ");
			i = 5;
			dp += i;
		} else {
			strcpy(dp, "     ");
			i = 5;
			dp += i;
		}
		
		for ( ; (*cp != '\0') && (i<sWidth); i++)
			*dp++ = *cp++;

		if (*cp != '\0') {
			cp--;
			while (!isspace(*cp)) {
				cp--;
				dp--;
			}
		}
		*dp = '\0';
		
		MultiLinePrintF("%s\n", buf);
	}
	MultiLinePrintF("\n");
}	/* PrintTip */




void PrintRandomTip(void)
{
	char *tip;
	int tipNum;
	
	tipNum = rand() % NTIPS;
	tip = gTipList[tipNum];
	MultiLineInit();
	PrintTip(tip);
}	/* PrintRandomTip */






void PrintAllTips(void)
{
	char *tip;
	int tipNum;
	
	MultiLineInit();
	for (tipNum = 0; tipNum < NTIPS; tipNum++) {
		tip = gTipList[tipNum];
		PrintTip(tip);
	}
}	/* PrintAllTips */
