/* Glob.c */

#include "Sys.h"

#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <setjmp.h>

#include "Util.h"
#include "RCmd.h"
#include "Glob.h"
#include "Xfer.h"
#include "List.h"
#include "Bookmark.h"
#include "Main.h"

/* Needed in case we're interrupted during local globbing. */
jmp_buf gLocalGlobJmp;

extern UserInfo gUserInfo;
extern Bookmark gRmtInfo;
extern int gTrace;

int RGlobCmd(int argc, char **argv)
{
	LineList globFiles;
	LinePtr globFile;
	int i;

	MultiLineInit();	
	for (i=1; i<argc; i++) {
		InitLineList(&globFiles);
		RemoteGlob(&globFiles, argv[i], kListNoFlags);

		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			MultiLinePrintF("%s\n", globFile->line);
		}
		DisposeLineListContents(&globFiles);
	}
	return (0);
}	/* RGlobCmd */




/* This does "tilde-expansion."  Examples:
 * ~/pub         -->  /usr/gleason/pub
 * ~pdietz/junk  -->  /usr/pdietz/junk
 */
void ExpandTilde(char *pattern, size_t siz)
{
	string pat;
	char *cp, *rest, *firstent;
	struct passwd *pw;

	if ((pattern[0] == '~') &&
	(isalnum(pattern[1]) || (pattern[1] == '/') || (pattern[1] == '\0'))) {
		STRNCPY(pat, pattern);
		if ((cp = strchr(pat, '/')) != NULL) {
			*cp = 0;
			rest = cp + 1;	/* Remember stuff after the ~/ part. */
		} else {
			rest = NULL;	/* Was just a ~ or ~username.  */
		}
		if (pat[1] == '\0') {
			/* Was just a ~ or ~/rest type.  */
			firstent = gUserInfo.home;
		} else {
			/* Was just a ~username or ~username/rest type.  */
			pw = getpwnam(pat + 1);
			if (pw != NULL)
				firstent = pw->pw_dir;
			else
				return;		/* Bad user -- leave it alone. */
		}
		
		Strncpy(pattern, firstent, siz);
		if (rest != NULL) {
			Strncat(pattern, "/", siz);
			Strncat(pattern, rest, siz);
		}
	}
}	/* ExpandTilde */



/*ARGSUSED*/
static
void LGlobHandler(void)
{
	alarm(0);
	longjmp(gLocalGlobJmp, 1);
}	/* LGlobHandler */



	
void LocalGlob(LineListPtr fileList, char *pattern)
{
	string pattern2;
	string cmd;
	longstring gfile;
	FILE *volatile fp;
	volatile Sig_t si, sp;

	STRNCPY(pattern2, pattern);	/* Don't nuke the original. */
	
	/* Pre-process for ~'s. */ 
	ExpandTilde(pattern2, sizeof(pattern2));
	
	/* Initialize the list. */
	fileList->first = fileList->last = NULL;
	
	if (GLOBCHARSINSTR(pattern2)) {
		/* Do it the easy way and have the shell do the dirty
		 * work for us.
		 */
		sprintf(cmd, "%s -d %s", LS, pattern2);

		fp = NULL;
		if (setjmp(gLocalGlobJmp) == 0) {			
			fp = POpen(cmd, "r", 0);
			if (fp == NULL) {
				DebugMsg("Could not lglob: %s\n", cmd);
				return;
			}
			sp = SIGNAL(SIGPIPE, LGlobHandler);
			si = SIGNAL(SIGINT, LGlobHandler);
			while (FGets(gfile, sizeof(gfile), (FILE *) fp) != NULL) {
				TraceMsg("Lglob [%s]\n", gfile);
				AddLine(fileList, gfile);
			}
		}
		(void) SIGNAL(SIGPIPE, SIG_IGN);
		if (fp != NULL)
			(void) PClose((FILE *) fp);
		(void) SIGNAL(SIGPIPE, sp);
		(void) SIGNAL(SIGINT, si);
	} else {
		/* Or, if there were no globbing characters in 'pattern', then the
		 * pattern is really just a single pathname.
		 */
		AddLine(fileList, pattern2);
	}
}	/* LocalGlob */



/* We need to use this because using NLST gives us more stuff than
 * we want back sometimes.  For example, say we have:
 *
 * /a		(directory)
 * /a/b		(directory)
 * /a/b/b1
 * /a/b/b2
 * /a/b/b3
 * /a/c		(directory)
 * /a/c/c1
 * /a/c/c2
 * /a/c/c3
 * /a/file
 *
 * If you did an "echo /a/<star>" in a normal unix shell, you would expect
 * to get back /a/b /a/c /a/file.  But NLST gives back:
 *
 * /a/b/b1
 * /a/b/b2
 * /a/b/b3
 * /a/c/c1
 * /a/c/c2
 * /a/c/c3
 * /a/file
 *
 * So we use the following routine to convert into the format I expect.
 */

static
void RemoteGlobCollapse(char *pattern, LineListPtr fileList)
{
	LinePtr lp, nextLine;
	string patPrefix;
	string cur, prev;
	char *endp, *cp, *dp;
	char *pp;
	int wasGlobChar;
	size_t plen;

	/* Copy all characters before the first glob-char. */
	dp = patPrefix;
	endp = dp + sizeof(patPrefix) - 1;
	wasGlobChar = 0;
	for (cp = pattern; dp < endp; ) {
		for (pp=kGlobChars; *pp != '\0'; pp++) {
			if (*pp == *cp) {
				wasGlobChar = 1;
				break;
			}
		}
		if (wasGlobChar)
			break;
		*dp++ = *cp++;
	}
	*dp = '\0';
	plen = (size_t) (dp - patPrefix);

	*prev = '\0';
	for (lp=fileList->first; lp != NULL; lp = nextLine) {
		nextLine = lp->next;
		if (strncmp(lp->line, patPrefix, plen) == 0) {
			STRNCPY(cur, lp->line + plen);
			cp = strchr(cur, '/');
			if (cp != NULL)
				*cp = '\0';
			if (*prev && STREQ(cur, prev)) {
				nextLine = RemoveLine(fileList, lp);
			} else {
				STRNCPY(prev, cur);
				/* We are playing with a dynamically
				 * allocated string, but since the
				 * following expression is guaranteed
				 * to be the same or shorter, we won't
				 * overwrite the bounds.
				 */
				sprintf(lp->line, "%s%s", patPrefix, cur);
			}
		}
	}
}	/* RemoteGlobCollapse */




void RemoteGlob(LineListPtr fileList, char *pattern, char *lsFlags)
{
	char *cp;
	LinePtr lp;

	/* Note that we do attempt to use glob characters even if the remote
	 * host isn't UNIX.  Most non-UNIX remote FTP servers look for UNIX
	 * style wildcards.
	 */
	if (GLOBCHARSINSTR(pattern)) {
		/* Use NLST, which lists files one per line. */
		ListToMemory(fileList, "NLST", lsFlags, pattern);
		if ((fileList->first != NULL) && (fileList->first == fileList->last)) {
			/* If we have only one item in the list, see if it really was
			 * an error message we would recognize.
			 */
			cp = strchr(fileList->first->line, ':');
			if ((cp != NULL) && STREQ(cp, ": No such file or directory")) {
				RemoveLine(fileList, fileList->first);
			}
		}
		RemoteGlobCollapse(pattern, fileList);
		if (gTrace == kTracingOn) {
			for (lp=fileList->first; lp != NULL; lp = lp->next)
				TraceMsg("Rglob [%s]\n", lp->line);
		}
	} else {
		/* Or, if there were no globbing characters in 'pattern', then the
		 * pattern is really just a filename.  So for this case the
		 * file list is really just a single file.
		 */
		fileList->first = fileList->last = NULL;
		AddLine(fileList, pattern);
	}
}	/* RemoteGlob */
