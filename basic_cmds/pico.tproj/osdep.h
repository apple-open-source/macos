/*
 * $Id: osdep.h,v 1.2 2002/01/03 22:16:42 jevans Exp $
 *
 * Program:	Operating system dependent routines - Ultrix 4.1
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1994  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 */

#ifndef	OSDEP_H
#define	OSDEP_H

#if	defined(dyn) || defined(AUX)
#include	<strings.h>
#else
#include	<string.h>
#endif

#undef	CTRL
#include	<signal.h>
#if	defined (ptx) || defined(sv4)
/* DYNIX/ptx signal semantics are AT&T/POSIX; the sigset() call sets
   the handler permanently, more like BSD signal(). */
#define signal(s,f) sigset (s, f)
#endif

/* signal handling is broken in the delivered SCO shells, so punt on 
   suspending the current session */
#if defined(sco) && defined(SIGTSTP)
#undef SIGTSTP
#endif

#include	<ctype.h>
#include	<sys/types.h>

#if	defined(POSIX) || defined(aix) || defined(COHERENT) || defined(isc) || defined(sv3)
#include	<dirent.h>
#else
#include	<sys/dir.h>
#endif

#if	defined(sco)
#include	<sys/stream.h>
#include	<sys/ptem.h>
#endif

#include	<sys/stat.h>

/*
 * 3b1 definition requirements
 */
#if	defined(ct)
#define opendir(dn)	fopen(dn, "r")
#define closedir(dirp)	fclose(dirp)
typedef struct
	{
	int	dd_fd;			/* file descriptor */
	int	dd_loc;			/* offset in block */
	int	dd_size;		/* amount of valid data */
	char	*dd_buf;		/* directory block */
	}	DIR;			/* stream data from opendir() */
#endif

/* Machine/OS definition			*/
#if	defined(ptx) || defined(sgi) || defined(sv4)
#define TERMINFO	1               /* Use TERMINFO                  */
#else
#define TERMCAP		1               /* Use TERMCAP                  */
#endif

/*
 * type qsort() expects
 */
#if	defined(nxt) || defined(neb)
#define	QSType	  void
#define QcompType const void
#else
#define	QSType	  int
#define QcompType void
#endif

/*
 * File name separator, as a char and string
 */
#define	C_FILESEP	'/'
#define	S_FILESEP	"/"

/*
 * Place where mail gets delivered (for pico's new mail checking)
 */
#if	defined(sv3) || defined(ct) || defined(isc) || defined(AUX) || defined(sgi)
#define	MAILDIR		"/usr/mail"
#else
#define	MAILDIR		"/usr/spool/mail"
#endif


/*
 * What and where the tool that checks spelling is located.  If this is
 * undefined, then the spelling checker is not compiled into pico.
 */
#if	defined(COHERENT) || defined(AUX)
#define SPELLER         "/bin/spell"
#else
#define	SPELLER		"/usr/bin/spell"
#endif

/* memcpy() is no good for overlapping blocks.  If that's a problem, use
 * the memmove() in ../c-client
 */
#if	defined (ptx) || defined(sv4) || defined(sco) || defined(isc) || defined(AUX)
#define bcopy(a,b,s) memcpy (b, a, s)
#endif

/* memmove() is a built-in for AIX 3.2 xlc. */
#if	defined (a32) || defined(COHERENT)
#define bcopy(a,b,s) memmove (b, a, s)
#endif

#if	defined(dyn)
#define	strchr	index			/* Dynix doesn't know about strchr */
#define	strrchr	rindex
#endif	/* dyn */

extern struct KBSTREE *kpadseqs;
extern int kbseq();

#if	TERMCAP
extern struct KBSTREE *kpadseqs;
#endif	/* TERMCAP */

#if	defined(bsd) || defined(nxt) || defined(dyn)
#ifdef	ANSI
extern char *getcwd(char *, int);
#else
extern char *getcwd();
#endif
#endif

#if	defined(COHERENT)
#define void char
#endif

/*
 * Mode passed chmod() to make tmp files exclusively user read/write-able
 */
#define	MODE_READONLY	(0600)

#endif	/* OSDEP_H */
