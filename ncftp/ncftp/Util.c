/* Util.c */

#include "Sys.h"

#ifdef HAVE_GETCWD
#	ifndef HAVE_UNISTD_H
		extern char *getcwd();
#	endif
#else
#	ifdef HAVE_GETWD
#		include <sys/param.h>
#		ifndef MAXPATHLEN
#			define MAXPATHLEN 1024
#		endif
		extern char *getwd(char *);
#	endif
#endif

#include <errno.h>
#include <ctype.h>

#ifdef HAVE_LIMITS_H
#	include <limits.h>
#endif

#include "Util.h"
#include "Main.h"
#include "Bookmark.h"
#include "Curses.h"

time_t gMailBoxTime;			/* last modified time of mbox */
int gWarnShellBug = 0;
int gMarkTrailingSpace = 0;

extern int gLoggedIn, gWinInit;
extern string gOurDirectoryPath;
extern longstring gRemoteCWD;
extern string gActualHostName;
extern long gEventNumber;
extern string gHost;
extern Bookmark gRmtInfo;
extern UserInfo gUserInfo;

/* Read a line, and axe the end-of-line. */
char *FGets(char *str, size_t size, FILE *fp)
{
	char *cp, *nlptr;
	
	cp = fgets(str, ((int) size) - 1, fp);
	if (cp != NULL) {
		nlptr = cp + strlen(cp) - 1;
		if (*nlptr == '\n')
			*nlptr = '\0';
	}
	return cp;
}	/* FGets */




/* This should only be called if the program wouldn't function
 * usefully without the memory requested.
 */
void OutOfMemory(void)
{
	fprintf(stderr, "Out of memory!\n");
	Exit(kExitOutOfMemory);
}	/* OutOfMemory */



/* A way to strcat to a dynamically allocated area of memory. */
char *PtrCat(char *dst, char *src)
{
	size_t len;

	len = strlen(dst) + strlen(src) + 1;
	if ((dst = Realloc(dst, len)) == NULL)
		return (NULL);
	strcat(dst, src);
	return (dst);
}	/* PtrCat */



/* Extends an area of memory, then cats a '/' and a string afterward. */
char *PtrCatSlash(char *dst, char *src)
{
	size_t dlen;
	char *nu;

	while (*src == '/')
		++src;

	dlen = strlen(dst);
	if (dst[dlen - 1] != '/') {
		dst = PtrCat(dst, "/");
		if (dst == NULL)
			nu = NULL;
		else
			nu = PtrCat(dst, src);
	} else {
		nu = PtrCat(dst, src);
	}
	return (nu);
}	/* PtrCatSlash */



void *Realloc(void *ptr, size_t siz)
{
	if (ptr == NULL)
		return (void *) malloc(siz);
	return ((void *) realloc(ptr, siz));
}	/* Realloc */





void MakeStringPrintable(char *dst, unsigned char *src, size_t siz)
{
	int c;
	size_t i;
	int endnl, numsp;

	i = 0;
	--siz;	/* Leave room for nul. */
	while ((i < siz) && (*src != '\0')) {
		c = *src++;
		if (isprint(c) || (c == '\n') || (c == '\t')
			/* Needed to view chinese... */
			|| (c & 0x80) || (c == 0x1b))
		{
			*dst++ = c;
			++i;
		} else if (iscntrl(c) && (c != 0x7f)) {
			/* Need room for 2 characters, ^x. */
			if (i < siz - 1) {
				c = c + '@';
				*dst++ = '^';
				*dst++ = c;
				i += 2;
			}
		} else {
			/* Need room for 5 characters, \xxx.
			 * The fifth character will be the \0 that is written by
			 * sprintf, but we know we have room for that already since
			 * we already accounted for that above.
			 */
			if (i < siz - 3) {
				sprintf(dst, "\\%03o", c);
				i += 4;
				dst += 4;
			}
		}
	}
	*dst-- = '\0';

	/* See if this line ended with a \n. */
	endnl = 0;
	if (i > 0 && *dst == '\n') {
		endnl = 1;
		--i;
		--dst;
	}

	/* The user may want to be aware if there are trailing spaces
	 * at the end of a line.
	 */
	numsp = 0;
	while (i > 0) {
		c = *dst;
		if (c != ' ')
			break;
		numsp++;
		--i;
		--dst;
	}

	/* Mark trailing spaces as \x where x is a space. */
	++dst;

	if (gMarkTrailingSpace) {
		while ((numsp > 0) && (i < siz)) {
			*dst++ = '\\';
			*dst++ = ' ';
			i += 2;
			--numsp;
		}
	}

	/* Tack the newline back onto the end of the string, if needed. */
	if (endnl)
		*dst++ = '\n';

	*dst = '\0';
}	/* MakeStringPrintable */




/* This will abbreviate a string so that it fits into max characters.
 * It will use ellipses as appropriate.  Make sure the string has
 * at least max + 1 characters allocated for it.
 */
void AbbrevStr(char *dst, char *src, size_t max, int mode)
{
	int len;

	len = (int) strlen(src);
	if (len > (int) max) {
		if (mode == 0) {
			/* ...Put ellipses at left */
			strcpy(dst, "...");
			Strncat(dst, src + len - (int) max + 3, max + 1);
		} else {
			/* Put ellipses at right... */
			Strncpy(dst, src, max + 1);
			strcpy(dst + max - 3, "...");
		}
	} else {
		Strncpy(dst, src, max + 1);
	}
}	/* AbbrevStr */





/* Converts any uppercase characters in the string to lowercase.
 * Never would have guessed that, huh?
 */
void StrLCase(char *dst)
{
	register char *cp;

	for (cp=dst; *cp != '\0'; cp++)
		if (isupper((int) *cp))
			*cp = (char) tolower(*cp);
}	/* StrLCase */




/* Use getcwd/getwd to get the full path of the current local
 * working directory.
 */
char *GetCWD(char *buf, size_t size)
{
#ifdef HAVE_GETCWD
	static char *cwdBuf = NULL;
	static size_t cwdBufSize = 0;

	if (cwdBufSize == 0) {
		cwdBufSize = (size_t) 128;
		cwdBuf = (char *) malloc(cwdBufSize);
	}

	for (errno = 0; ; ) {
		if (cwdBuf == NULL) {
			Error(kDoPerror, "Not enough free memory to get the local working directory path.\n");
			(void) Strncpy(buf, ".", size);
			return NULL;
		}

		if (getcwd(cwdBuf, cwdBufSize) != NULL)
			break;
		if (errno != ERANGE) {
			Error(kDoPerror, "Can't get the local working directory path.\n");
			(void) Strncpy(buf, ".", size);
			return NULL;
		}
		cwdBufSize *= 2;
		cwdBuf = Realloc(cwdBuf, cwdBufSize);
	}
	
	return (Strncpy(buf, cwdBuf, size));
#else
#ifdef HAVE_GETWD
	static char *cwdBuf = NULL;
	char *dp;
	
	/* Due to the way getwd is usually implemented, it's
	 * important to have a buffer large enough to hold the
	 * whole thing.  getwd usually starts at the end of the
	 * buffer, and works backwards, returning you a pointer
	 * to the beginning of it when it finishes.
	 */
	if (size < MAXPATHLEN) {
		/* Buffer not big enough, so use a temporary one,
		 * and then copy the first 'size' bytes of the
		 * temporary buffer to your 'buf.'
		 */
		if (cwdBuf == NULL) {
			cwdBuf = (char *) malloc((size_t) MAXPATHLEN);
			if (cwdBuf == NULL)
				OutOfMemory();
		}
		dp = cwdBuf;
	} else {
		/* Buffer is big enough already. */
		dp = buf;
	}
	*dp = '\0';
	if (getwd(dp) == NULL) {
		/* getwd() should write the reason why in the buffer then,
		 * according to the man pages.
		 */
		Error(kDontPerror, "Can't get the local working directory path. %s\n", dp);
		(void) Strncpy(buf, ".", size);
		return (NULL);
	}
	return (Strncpy(buf, dp, size));
	
#else
	/* Not really a solution, but does anybody not have either of
	 * getcwd or getwd?
	 */
	Error(kDontPerror, "Can't get the cwd path; no getwd() or getcwd().\n");
	return (Strncpy(buf, ".", size));
#endif
#endif
}   /* GetCWD */




char *Path(char *dst, size_t siz, char *parent, char *fname)
{
	(void) Strncpy(dst, parent, siz);
	(void) Strncat(dst, "/", siz);
	return (Strncat(dst, fname, siz));
}	/* Path */




char *OurDirectoryPath(char *dst, size_t siz, char *fname)
{
	return (Path(dst, siz, gOurDirectoryPath, fname));
}	/* OurDirectoryPath */




int
MkDirs(char *newdir)
{
	char s[512];
	int rc;
	char *cp, *sl;
	struct stat st;
	int mode = (S_IRWXU | S_IRWXG | S_IRWXO);	/* umask will affect */

	if (access(newdir, F_OK) == 0) {
		if (stat(newdir, &st) < 0)
			return (-1);
		if (! S_ISDIR(st.st_mode)) {
			errno = ENOTDIR;
			return (-1);
		}
		return 0;
	}

	(void) memcpy(s, newdir, sizeof(s));
	s[sizeof(s) - 1] = '\0';
	cp = strrchr(s, '/');
	if (cp == NULL) {
		rc = mkdir(newdir, mode);
		return (rc);
	} else if (cp[1] == '\0') {
		/* Remove trailing slashes from path. */
		--cp;
		while (cp > s) {
			if (*cp != '/')
				break;
			--cp;
		}
		cp[1] = '\0';
		cp = strrchr(s, '/');
		if (cp == NULL) {
			rc = mkdir(s, mode);
			return (rc);
		}
	}

	/* Find the deepest directory in this
	 * path that already exists.  When
	 * we do, we want to have the 's'
	 * string as it was originally, but
	 * with 'cp' pointing to the first
	 * slash in the path that starts the
	 * part that does not exist.
	 */
	sl = NULL;
	while (1) {
		*cp = '\0';
		rc = access(s, F_OK);
		if (sl != NULL)
			*sl = '/';
		if (rc == 0) {
			*cp = '/';
			break;
		}
		sl = cp;
		cp = strrchr(s, '/');
		if (cp == NULL) {
			/* We do not have any more
			 * slashes, so none of the
			 * new directory's components
			 * existed before, so we will
			 * have to make everything
			 * starting at the first node.
			 */
			if (sl != NULL)
				*sl = '/';
			cp = s - 1;
			break;
		}
	}

	while (1) {
		/* Extend the path we have to
		 * include the next component
		 * to make.
		 */
		sl = strchr(cp + 1, '/');
		if (sl == s) {
			/* If the next slash is pointing
			 * to the start of the string, then
			 * the path is an absolute path and
			 * we don't need to make the root node,
			 * and besides the next mkdir would
			 * try an empty string.
			 */
			++cp;
			sl = strchr(cp + 1, '/');
		}
		if (sl != NULL) {
			*sl = '\0';
		}
		rc = mkdir(s, mode);
		if (rc < 0)
			return rc;
		if (sl == NULL)
			break;
		*sl = '/';
		cp = sl;
	}
	return (0);
}	/* MkDirs */




/* Closes the file supplied, if it isn't a std stream. */
int CloseFile(FILE **f)
{
	if (*f != NULL) {
		if ((*f != stdout) && (*f != stdin) && (*f != stderr)) {
			(void) fclose(*f);
			*f = NULL;
			return (1);
		}
		*f = NULL;
	}
	return (0);
}	/* CloseFile */




/* Returns non-zero if we are the foreground process, or 0
 * if we are a background process at the time of the call.
 */
int InForeGround(void)
{
#if defined(NO_FGTEST) || !defined(HAVE_TCGETPGRP)
	return (1);
#else
#	ifndef GETPGRP_VOID
#		define GETMYPGRP (getpgrp(getpid()))
#	else
#		define GETMYPGRP (getpgrp())
#	endif
	int result, status;
	static int file = -2;
	static int mode = -2;

	result = 1;	
	if (file == -2)
		file = open("/dev/tty", O_RDONLY);
	
	if (file >= 0) {
		status = tcgetpgrp(file);
		if (status >= 0) {
			result = (status == GETMYPGRP);
			if (mode != result) {
				if (mode == 0) {
					TraceMsg("In background.\n");
				} else
					TraceMsg("In foreground.\n");
			}
			mode = result;
		} else if (mode == -2) {
			TraceMsg("Foreground check failed.\n");
			mode = 0;
		}
	}
	return (result);
#endif
}	/* InForeGround */




/* Returns non-zero if it appears the user is still live at the
 * terminal.
 */
int UserLoggedIn(void)
{
	static int inited = 0;
	static int parent_pid, stderr_was_tty;

	if (!inited) {
		stderr_was_tty = isatty(2);
		parent_pid = getppid();
		inited++;
	}
	if ((stderr_was_tty && !isatty(2)) || (getppid() != parent_pid))
		return 0;
	return 1;
}	/* UserLoggedIn */




int CheckNewMail(void)
{
	struct stat stbuf;

	if (*gUserInfo.mail == '\0')
		return 0;

	if (stat(gUserInfo.mail, &stbuf) < 0) {
		/* Can't find mail_path so we'll never check it again */
		*gUserInfo.mail = '\0';	
		return 0;
	}

	/*
	 * Check if the size is non-zero and the access time is less than
	 * the modify time -- this indicates unread mail.
	 */
	if ((stbuf.st_size != 0) && (stbuf.st_atime <= stbuf.st_mtime)) {
		if (stbuf.st_mtime > gMailBoxTime) {
			(void) PrintF("You have new mail.\n");
			gMailBoxTime = stbuf.st_mtime;
		}
		return 1;
	}

	return 0;
}	/* CheckNewMail */





size_t FlagStrCopy(char *dst, size_t siz, char *src)
{
	time_t now;
	register char *p, *q;
	int	flagType;
	int chType;
	int nextCh;
	int nPercents;
	int extraChar;
	size_t maxSize;
	size_t onScreenLen;
	size_t len;
	string tmpStr;
	char *copy;

	nPercents = 0;
	onScreenLen = 0;
	extraChar = 0;
	siz -= 2;		/* Need room for nul, and extra char. */
	maxSize = siz;

	for (p = src, q = dst, *q = 0; *p != '\0'; p++) {
		chType = *p;
		switch (chType) {
			case '%':
				nPercents++;
				goto copyChar;
			case '@':
				flagType = *++p;
				nextCh = p[1];
				switch (flagType) {
					case '\0':
						goto done;
						break;
					case 'Z':
						/* Tell caller not to echo a final newline. */
						extraChar = '@';
						break;
					case 'M':
						if (CheckNewMail() > 0) {
							copy = "(Mail)";
							goto copyVisStr;
						}
						goto copyNothing;

					case 'n':
						if (gLoggedIn) {
							copy = gRmtInfo.bookmarkName;
							goto copyVisStr;
						}
						goto copyNothing;
						
					case 'N':
						copy = "\n";
						goto copyVisStr;
						break;
	
					/* Probably won't implement these. */
					case 'P':	/* reset to no bold, no uline, no inverse, etc. */
						/* copy = "plain...";
						goto copyInvisStr; */
						break;
					case 'B':	/* toggle boldface */
						break;
					case 'U':	/* toggle underline */
						break;
					case 'R':
					case 'I':	/* toggle inverse (reverse) video */
						break;
		
					case 'D':	/* insert current directory */
					case 'J':
						if (gLoggedIn) {
							if ((flagType == 'J') && (gRmtInfo.isUnix)) {
								/* Not the whole path, just the dir name. */
								copy = strrchr(gRemoteCWD, '/');
								if (copy == NULL)
									copy = gRemoteCWD;
								else if ((copy != gRemoteCWD) && (copy[1]))
									++copy;
							} else {
								copy = gRemoteCWD;
							}
							goto copyVisStr;
						}
						goto copyNothing;
		
					case 'H':	/* insert name of connected host */
						if (gLoggedIn) {
							copy = gHost;
							goto copyVisStr;
						}
						goto copyNothing;
		
					case 'h':	/* insert actual name of connected host */
						if (gLoggedIn) {
							copy = gActualHostName;
							goto copyVisStr;
						}
						goto copyNothing;
		
					case '!':
					case 'E':	/* insert event number */
						(void) sprintf(tmpStr, "%ld", gEventNumber);
						copy = tmpStr;
						/*FALLTHROUGH*/
		
					copyVisStr:
						len = strlen(copy);
						if (siz > len) {
							q = strcpy(q, copy) + len;
							siz -= len;
							if (q[-1] == '\n') {
								onScreenLen = 0;
							} else
								onScreenLen += len;
						}
						break;
		
					copyNothing:
						if (isspace(nextCh) || (nextCh == ':'))
							++p;	/* Don't insert next character. */
						break;

					default:
						goto copyChar; /* just copy it; unknown switch */
				}	/* end flagType */
				break;
			
			default:
			copyChar:
				if (siz > 0) {
					*q++ = *p;
					--siz;
					++onScreenLen;
				}
				break;
		}
	}
	
done:
	*q = '\0';

#ifdef HAVE_STRFTIME
	if ((nPercents > 0) && ((copy = StrDup(dst)) != NULL)) {
		/* Only strftime if the user requested it (with a %something). */
		(void) time(&now);
		len = strlen(dst);
		onScreenLen += strftime(dst, maxSize, copy, localtime(&now));
		onScreenLen -= len;
		free(copy);
	}
#endif
	if (extraChar != 0)
		dst[strlen(dst) + 1] = extraChar;
	return (onScreenLen);
}	/* FlagStrCopy */




void OverflowAdd(long *dst, long plus)
{
#ifdef LONG_MAX
	long x;

	x = LONG_MAX - *dst;
	if (x < plus)
		*dst = LONG_MAX;		/* Would overflow! */
	else
		*dst += plus;
#else
	*dst += plus;
#endif
}	/* OverflowAdd */




FILE *POpen(char *cmd, char *mode, int saveScreen)
{
	FILE *fp;
	
#if 1
	if ((++gWarnShellBug <= 2) && (gWinInit == 1) && (*mode == 'w') && (CURSES_SHELL_BUG == 1)) {
		EPrintF("Sorry, that operation would crash the program with this OS.\n");
		errno = 0;
		return (NULL);
	}
#else
	if (++gWarnShellBug == 1) {
		EPrintF("Warning: the screen may not update correctly on this OS.\n\n");
		sleep(2);
	}
#endif
	if (saveScreen == 1)
		SaveScreen();
	fp = popen(cmd, mode);
	return fp;
}	/* POpen */




#ifndef HAVE_MEMMOVE
/* This code is derived from software contributed to Berkeley by
 * Chris Torek.
 */

/*
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef	int word;		/* "word" used for optimal copy speed */

#define	wsize	sizeof(word)
#define	wmask	(wsize - 1)

/*
 * Copy a block of memory, handling overlap.
 * This is the routine that actually implements
 * (the portable versions of) bcopy, memcpy, and memmove.
 */
void *
MemMove(void *dst0, void *src0, size_t length)
{
	register char *dst = (char *) dst0;
	register const char *src = (char *) src0;
	register size_t t;

	if (length == 0 || dst == src)		/* nothing to do */
		return dst;

	/*
	 * Macros: loop-t-times; and loop-t-times, t>0
	 */
#define	TLOOP(s) if (t) TLOOP1(s)
#define	TLOOP1(s) do { s; } while (--t)

	if ((unsigned long)dst < (unsigned long)src) {
		/*
		 * Copy forward.
		 */
		t = (int)src;	/* only need low bits */
		if ((t | (int)dst) & wmask) {
			/*
			 * Try to align operands.  This cannot be done
			 * unless the low bits match.
			 */
			if ((t ^ (int)dst) & wmask || length < wsize)
				t = length;
			else
				t = wsize - (t & wmask);
			length -= t;
			TLOOP1(*dst++ = *src++);
		}
		/*
		 * Copy whole words, then mop up any trailing bytes.
		 */
		t = length / wsize;
		TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
		t = length & wmask;
		TLOOP(*dst++ = *src++);
	} else {
		/*
		 * Copy backwards.  Otherwise essentially the same.
		 * Alignment works as before, except that it takes
		 * (t&wmask) bytes to align, not wsize-(t&wmask).
		 */
		src += length;
		dst += length;
		t = (int)src;
		if ((t | (int)dst) & wmask) {
			if ((t ^ (int)dst) & wmask || length <= wsize)
				t = length;
			else
				t &= wmask;
			length -= t;
			TLOOP1(*--dst = *--src);
		}
		t = length / wsize;
		TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
		t = length & wmask;
		TLOOP(*--dst = *--src);
	}

	return(dst0);
}	/* MemMove */
#endif	/* ! HAVE_MEMMOVE */

/* eof */
