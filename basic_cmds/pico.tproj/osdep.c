#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: osdep.c,v 1.3 2002/02/20 17:51:54 bbraun Exp $";
#endif
/*
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
 *
 * Notes:
 *
 * - SGI IRIX 4.0.1 port by:
 *       johnb@edge.cis.mcmaster.ca,  2 April 1992
 *
 * - Dynix/PTX port by:
 *       Donn Cave, UCS/UW, 15 April 1992
 *
 * - 3B2, 3b1/7300, SCO ports by:
 *       rll@felton.felton.ca.us, 7 Feb. 1993
 *
 * - Probably have to break this up into separate os_type.c files since
 *   the #ifdef's are getting a bit cumbersome.
 *
 */

#include 	<stdio.h>
#include	<errno.h>
#include	<setjmp.h>
#ifndef s40
#include	<time.h>
#endif
#include	<pwd.h>
#if	defined(sv4) || defined(ptx)
#include	<stropts.h>
#include	<poll.h>
#endif
#if	defined(POSIX)
#include	<termios.h>
#if	defined(a32) || defined(cvx)
#include	<sys/ioctl.h>
#endif
#else
#if	defined(sv3) || defined(sgi) || defined(isc) || defined(ct)
#include	<termio.h>
#if	defined(isc)
#include	<sys/sioctl.h>
#include        <sys/bsdtypes.h>
#endif
#else
#include	<sgtty.h>
#endif	/* sv3 || sgi || isc */
#endif	/* POSIX */

#include	"osdep.h"
#include        "pico.h"
#include	"estruct.h"
#include        "edef.h"
#include        "efunc.h"
#include	<fcntl.h>
#include	<sys/wait.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<sys/time.h>
#if	defined(a32)
#include	<sys/select.h>
#endif


/*
 * Immediately below are includes and declarations for the 3 basic
 * terminal drivers supported; POSIX, SysVR3, and BSD
 */
#ifdef	POSIX

struct termios nstate,
		ostate;
#else
#if	defined(sv3) || defined(sgi) || defined(isc)

struct termio nstate,
              ostate;

#else
struct  sgttyb  ostate;				/* saved tty state */
struct  sgttyb  nstate;				/* values for editor mode */
struct  ltchars	oltchars;			/* old term special chars */
struct  ltchars	nltchars = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
struct  tchars	otchars;			/* old term special chars */
struct  tchars	ntchars = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#endif	/* sv3 || sgi || isc */
#endif	/* POSIX */

#if	defined(sv3) || defined(ct)
/*
 * Windowing structure to support JWINSIZE/TIOCSWINSZ/TIOCGWINSZ 
 */
#define ENAMETOOLONG	78

struct winsize {
	unsigned short ws_row;       /* rows, in characters*/
	unsigned short ws_col;       /* columns, in character */
	unsigned short ws_xpixel;    /* horizontal size, pixels */
	unsigned short ws_ypixel;    /* vertical size, pixels */
};
#endif

#ifdef	bsd
int	errno;					/* ya, I know... */
#endif

#if	(defined(bsd) || defined(dyn) || defined(ct)) && !defined(LINUX)
#define	SIGTYPE int
#else
#define	SIGTYPE	void
#endif


#ifdef	ANSI
    int      kbseq(int *);
    SIGTYPE  do_hup_signal();
    SIGTYPE  rtfrmshell();
#ifdef	TIOCGWINSZ
    SIGTYPE  winch_handler();
#endif
#else
    int      kbseq();
    SIGTYPE  do_hup_signal();
    SIGTYPE  rtfrmshell();
#ifdef	TIOCGWINSZ
    SIGTYPE  winch_handler();
#endif
#endif


/*
 * for alt_editor arg[] building
 */
#define	MAXARGS	10

/*
 * ttopen - this function is called once to set up the terminal device 
 *          streams.  if called as pine composer, don't mess with
 *          tty modes, but set signal handlers.
 */
ttopen()
{
    if(Pmaster == NULL){
#ifdef	POSIX
	tcgetattr (0, &ostate);
	tcgetattr (0, &nstate);
	nstate.c_lflag &= ~(ISIG | ICANON | ECHO | IEXTEN);
	nstate.c_iflag &= ~ICRNL;
	nstate.c_oflag &= ~(ONLCR | OPOST);
	nstate.c_cc[VMIN] = 1;
	nstate.c_cc[VTIME] = 0;
	tcsetattr (0, TCSADRAIN, &nstate);
#else
#if	defined(sv3) || defined(sgi) || defined(isc) || defined(ct)
	(void) ioctl(0, TCGETA, &ostate);
	(void) ioctl(0, TCGETA, &nstate);	/** again! **/

	nstate.c_lflag &= ~(ICANON | ISIG | ECHO);	/* noecho raw mode  */
	nstate.c_oflag &= ~(OPOST | ONLCR);
	nstate.c_iflag &= ~ICRNL;
	    
	nstate.c_cc[VMIN] = '\01';  /* minimum # of chars to queue  */
	nstate.c_cc[VTIME] = '\0'; /* minimum time to wait for input */
	(void) ioctl(0, TCSETA, &nstate);
#else
	ioctl(0, TIOCGETP, &ostate);		/* save old state */
	ioctl(0, TIOCGLTC, &oltchars);		/* Save old lcharacters */
	ioctl(0, TIOCGETC, &otchars);		/* Save old characters */
	ioctl(0, TIOCGETP, &nstate);		/* get base of new state */
	nstate.sg_flags |= RAW;
	nstate.sg_flags &= ~(ECHO|CRMOD);	/* no echo for now... */
	ioctl(0, TIOCSETP, &nstate);		/* set mode */

	ioctl(0, TIOCSLTC, &nltchars);		/* put new lcharacter into K */
	ioctl(0, TIOCSETC, &ntchars);		/* put new character into K */
#endif	/* sv3 */
#endif	/* POSIX */
    }

    signal(SIGHUP,  do_hup_signal);	/* deal with SIGHUP */
    signal(SIGTERM, do_hup_signal);	/* deal with SIGTERM */
#ifdef	SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
#ifdef	TIOCGWINSZ
    signal(SIGWINCH, winch_handler); /* window size changes */
#endif
    return(1);
}



/*
 * ttclose - this function gets called just before we go back home to 
 *           the command interpreter.  If called as pine composer, don't
 *           worry about modes, but set signals to default, pine will 
 *           rewire things as needed.
 */
ttclose()
{
    if(Pmaster){
	signal(SIGHUP, SIG_DFL);
#ifdef	SIGCONT
	signal(SIGCONT, SIG_DFL);
#endif
#ifdef	TIOCGWINSZ
	signal(SIGWINCH, SIG_DFL);
#endif
    }
    else{
#ifdef	POSIX
	tcsetattr (0, TCSADRAIN, &ostate);
#else
#if	defined(sv3) || defined(sgi) || defined(isc) || defined(ct)
        ioctl(0, TCSETA, &ostate);
#else
	ioctl(0, TIOCSETP, &ostate);
	ioctl(0, TIOCSLTC, &oltchars);
	ioctl(0, TIOCSETC, &otchars);

	/*
	 * This works around a really weird problem.  On slow speed lines,
	 * if an exit happens with some number of characters still to be
	 * written in the terminal driver, one or more characters will 
	 * be changed when they finally get drained.  This can be reproduced
	 * on a 2400bps line, writing a multi-line buffer on exit using
	 * a vt100 type terminal.  It turns out the last char in the
	 * escape sequence turning off reverse video was getting changed
	 * from 'm' to ' '.  I said it was weird.
	 */
	if(ostate.sg_ospeed <= B2400)
	  sleep(1);
#endif	/* sv3 || sgi || isc */
#endif	/* POSIX */
    }

    return(1);
}


/*
 * ttspeed - return TRUE if tty line speed < 9600 else return FALSE
 */
ttisslow()
{
#if	defined(POSIX)
    struct termios tty;

    return((tcgetattr (1, &tty) == 0) ? cfgetospeed (&tty) < B9600 : FALSE);
#else
#if	defined(sv3) || defined(sgi) || defined(isc)
    struct termio tty;

    return((tcgetattr (1, &tty) == 0) ? cfgetospeed (&tty) < B9600 : FALSE);
#else
    struct  sgttyb tty;

    return((ioctl(1, TIOCGETP, &tty) == 0) ? tty.sg_ospeed < B9600 : FALSE);
#endif
#endif
}


/*
 * ttgetwinsz - set global rows and columns values and return
 */
ttgetwinsz()
{
#ifdef TIOCGWINSZ
    struct winsize win;

    if (ioctl(0, TIOCGWINSZ, &win) == 0) {
        term.t_ncol = (win.ws_col) ? win.ws_col : 80;
        term.t_nrow = (win.ws_row) ? win.ws_row - 1 : 23;
    }
#endif
}


/*
 * ttputc - Write a character to the display. 
 */
ttputc(c)
{
    return(putc(c, stdout));
}


/*
 * ttflush - flush terminal buffer. Does real work where the terminal 
 *           output is buffered up. A no-operation on systems where byte 
 *           at a time terminal I/O is done.
 */
ttflush()
{
    return(fflush(stdout));
}


/*
 * ttgetc - Read a character from the terminal, performing no editing 
 *          and doing no echo at all.
 */
ttgetc()
{
    unsigned char c;
    int i;

    if((i = read(0, &c, 1)) <= 0){
	if(i == 0 && errno == EINTR)		/* only acceptable failure */
	  return(NODATA);
	else
	  kill(getpid(), SIGHUP);		/* fake a hup */
    }
    else
      return((int)c);
}


#if	TYPEAH
/* 
 * typahead - Check to see if any characters are already in the
 *	      keyboard buffer
 */
typahead()
{
    int x;	/* holds # of pending chars */

    return((ioctl(0,FIONREAD,&x) < 0) ? 0 : x);
}
#endif


/*
 * GetKey - Read in a key.
 * Do the standard keyboard preprocessing. Convert the keys to the internal
 * character set.  Resolves escape sequences and returns no-op if global
 * timeout value exceeded.
 */
GetKey()
{
    int    c;

    if(timeout){
	/*
	 * simple use of select/poll here to handle requested timeouts
	 * while waiting for keyboard input...
	 */
#if	defined(ptx) || defined(sv4)
	struct pollfd pollfd;
	int    rv;

	pollfd.fd     = 0;
	pollfd.events = POLLIN;
	while((rv = poll(&pollfd, 1, timeout * 1000)) < 0 && errno == EAGAIN)
	  ;
#else
	struct timeval ts;
	fd_set readfds;
	int    rv;

	FD_ZERO(&readfds);		/* blank out all bits */
	FD_SET(0, &readfds);		/* set stdin's bit */
	ts.tv_sec  = timeout;		/* set the timeout */
	ts.tv_usec = 0;

	rv = select(1, &readfds, 0, &readfds, &ts); /* read stdin */
#endif
	if(rv < 0){
	    if(errno == EINTR){		/* interrupted? */
		return(NODATA);		/* return like we timed out */
	    }
	    else{
		emlwrite("\007Problem reading from keyboard!", NULL);
		kill(getpid(), SIGHUP);	/* Bomb out (saving our work)! */
	    }
	}
	else if(rv == 0)
	  return(NODATA);		/* we really did time out */
    }

    if ((c = (*term.t_getchar)()) == METACH) { /* Apply M- prefix      */
	int status;
	    
	/*
	 * this code should intercept special keypad keys
	 */
	switch(status = kbseq(&c)){
	  case 0 : 	/* no dice */
	    return(c);
	  case  K_PAD_UP		:
	  case  K_PAD_DOWN		:
	  case  K_PAD_RIGHT		:
	  case  K_PAD_LEFT		:
	  case  K_PAD_PREVPAGE	:
	  case  K_PAD_NEXTPAGE	:
	  case  K_PAD_HOME		:
	    return(status);
	  case F1  :
	  case F2  :
	  case F3  :
	  case F4  :
	  case F5  :
	  case F6  :
	  case F7  :
	  case F8  :
	  case F9  :
	  case F10 :
	  case F11 :
	  case F12 :
	    return(status);
	  case BADESC :
	    if(c == '\033'){
		c = (*term.t_getchar)();
		if(islower(c))	/* canonicalize c */
		  c = toupper(c);

		return((isalpha(c) || c == '@' || (c >= '[' && c <= '_'))
		       ? (CTRL | c) : c);
	    }

	  default :				/* punt the whole thing	*/
	    (*term.t_beep)();
	    break;
	}
    }

    if (c>=0x00 && c<=0x1F)                 /* C0 control -> C-     */
      c = CTRL | (c+'@');

    return (c);

}



/* 
 * kbseq - looks at an escape sequence coming from the keyboard and 
 *         compares it to a trie of known keyboard escape sequences, and
 *         performs the function bound to the escape sequence.
 * 
 *         returns: BADESC, the escaped function, or 0 if not found.
 */
kbseq(c)
int	*c;
{
    register char	b;
    register int	first = 1;
    register struct	KBSTREE	*current = kpadseqs;

    if(kpadseqs == NULL)			/* bag it */
      return(BADESC);

    while(1){
	*c = b = (*term.t_getchar)();

	while(current->value != b){
	    if(current->left == NULL){		/* NO MATCH */
		if(first)
		  return(BADESC);
		else
		  return(0);
	    }
	    current = current->left;
	}

	if(current->down == NULL)		/* match!!!*/
	  return(current->func);
	else
	  current = current->down;

	first = 0;
    }
}



/*
 * alt_editor - fork off an alternate editor for mail message composition 
 *              if one is configured and passed from pine.  If not, only
 *              ask for the editor if advanced user flag is set, and 
 *              suggest environment's EDITOR value as default.
 */
alt_editor(f, n)
{
    char   eb[NLINE];				/* buf holding edit command */
    char   *fn;					/* tmp holder for file name */
    char   *cp;
    char   *args[MAXARGS];			/* ptrs into edit command */
    char   *writetmp();
    int	   child, pid, i, done = 0;
    long   l;
#if	defined(POSIX) || defined(sv3) || defined(COHERENT) || defined(isc) || defined(neb)
    int    stat;
#else
    union  wait stat;
#endif
    FILE   *p;
    SIGTYPE (*ohup)(), (*oint)(), (*osize)(), (*ostop)(), (*ostart)();

    if(Pmaster == NULL)
      return(-1);

    if(gmode&MDSCUR){
	emlwrite("Alternate editor not available in restricted mode", NULL);
	return(-1);
    }

    if(Pmaster->alt_ed == NULL){
	if(!(gmode&MDADVN)){
	    emlwrite("\007Unknown Command",NULL);
	    return(-1);
	}

	if(getenv("EDITOR"))
	  strcpy(eb, (char *)getenv("EDITOR"));
	else
	  *eb = '\0';

	while(!done){
	    pid = mlreplyd("Which alternate editor ? ",eb,NLINE,QDEFLT,NULL);

	    switch(pid){
	      case ABORT:
		return(-1);
	      case HELPCH:
		emlwrite("no alternate editor help yet", NULL);

/* take sleep and break out after there's help */
		sleep(3);
		break;
	      case (CTRL|'L'):
		sgarbf = TRUE;
		update();
		break;
	      case TRUE:
	      case FALSE:			/* does editor exist ? */
		if(*eb == '\0'){		/* leave silently? */
		    mlerase();
		    return(-1);
		}

		done++;
		break;
	      default:
		break;
	    }
	}
    }
    else
      strcpy(eb, Pmaster->alt_ed);

    if((fn=writetmp(0, 1)) == NULL){		/* get temp file */
	emlwrite("Problem writing temp file for alt editor", NULL);
	return(-1);
    }

    strcat(eb, " ");
    strcat(eb, fn);

    cp = eb;
    for(i=0; *cp != '\0';i++){			/* build args array */
	if(i < MAXARGS){
	    args[i] = NULL;			/* in case we break out */
	}
	else{
	    emlwrite("Too many args for command!", NULL);
	    return(-1);
	}

	while(isspace(*cp))
	  if(*cp != '\0')
	    cp++;
	  else
	    break;

	args[i] = cp;

	while(!isspace(*cp))
	  if(*cp != '\0')
	    cp++;
	  else
	    break;

	if(*cp != '\0')
	  *cp++ = '\0';
    }

    args[i] = NULL;

    if(Pmaster)
      (*Pmaster->raw_io)(0);			/* turn OFF raw mode */

    emlwrite("Invoking alternate editor...", NULL);

    if(child=fork()){			/* wait for the child to finish */
	ohup = signal(SIGHUP, SIG_IGN);	/* ignore signals for now */
	oint = signal(SIGINT, SIG_IGN);
#ifdef	TIOCGWINSZ
        osize = signal(SIGWINCH, SIG_IGN);
#endif

/*
 * BUG - wait should be made non-blocking and mail_pings or something 
 * need to be done in the loop to keep the imap stream alive
 */
	while((pid=(int)wait(&stat)) != child)
	  ;

	signal(SIGHUP, ohup);	/* restore signals */
	signal(SIGINT, oint);
#ifdef	TIOCGWINSZ
        signal(SIGWINCH, osize);
#endif
    }
    else{				/* spawn editor */
	signal(SIGHUP, SIG_DFL);	/* let editor handle signals */
	signal(SIGINT, SIG_DFL);
#ifdef	TIOCGWINSZ
        signal(SIGWINCH, SIG_DFL);
#endif
	if(execvp(args[0], args) < 0)
	  exit(1);
    }

    if(Pmaster)
      (*Pmaster->raw_io)(1);		/* turn ON raw mode */

    /*
     * replace edited text with new text 
     */
    curbp->b_flag &= ~BFCHG;		/* make sure old text gets blasted */
    readin(fn, 0);			/* read new text overwriting old */
    unlink(fn);				/* blast temp file */
    curbp->b_flag |= BFCHG;		/* mark dirty for packbuf() */
    ttopen();				/* reset the signals */
    refresh(0, 1);			/* redraw */
    return(0);
}



/*
 *  bktoshell - suspend and wait to be woken up
 */
bktoshell()		/* suspend MicroEMACS and wait to wake up */
{
#ifdef	SIGTSTP
    int pid;

    if(!(gmode&MDSSPD)){
	emlwrite("\007Unknown command: ^Z", NULL);
	return;
    }

    if(Pmaster){
	(*Pmaster->raw_io)(0);	/* actually in pine source */

	movecursor(term.t_nrow, 0);
	printf("\n\n\nUse \"fg\" to return to Pine\n");

    }
    else
      vttidy();

    movecursor(term.t_nrow, 0);
    peeol();
    (*term.t_flush)();

    signal(SIGCONT, rtfrmshell);	/* prepare to restart */
    signal(SIGTSTP, SIG_DFL);			/* prepare to stop */
    kill(0, SIGTSTP);
#endif
}


/* 
 * rtfrmshell - back from shell, fix modes and return
 */
SIGTYPE
rtfrmshell()
{
#ifdef	SIGCONT
    signal(SIGCONT, SIG_DFL);

    if(Pmaster){
	(*Pmaster->raw_io)(1);			/* actually in pine source */
	(*Pmaster->keybinit)(gmode&MDFKEY);	/* using f-keys? */
    }

    ttopen();

#ifdef	TIOCGWINSZ
    {
	struct winsize win;
	extern int resize_pico();

	/*
	 * refit pico of window size changed....
	 */
	if (ioctl(0, TIOCGWINSZ, &win) == 0) {
	    if (win.ws_col && win.ws_row)
	      resize_pico(win.ws_row - 1, win.ws_col);
	}
    }
#endif

    sgarbf = TRUE;
    curwp->w_flag = WFHARD;
    refresh(0, 1);
#endif
}



/*
 * do_hup_signal - jump back in the stack to where we can handle this
 */
SIGTYPE
do_hup_signal()
{
    signal(SIGHUP,  SIG_IGN);			/* ignore further SIGHUP's */
    signal(SIGTERM, SIG_IGN);			/* ignore further SIGTERM's */
    if(Pmaster){
	extern jmp_buf finstate;

	longjmp(finstate, COMP_GOTHUP);
    }
    else{
	/*
	 * if we've been interrupted and the buffer is changed,
	 * save it...
	 */
	if(anycb() == TRUE){			/* time to save */
	    if(curbp->b_fname[0] == '\0'){	/* name it */
		strcpy(curbp->b_fname, "pico.save");
	    }
	    else{
		strcat(curbp->b_fname, ".save");
	    }
	    writeout(curbp->b_fname);
	}
	vttidy();
	exit(1);
    }
}


/*
 * big bitmap of ASCII characters allowed in a file name
 * (needs reworking for other char sets)
 */
unsigned char okinfname[32] = {
      0,    0, 			/* ^@ - ^G, ^H - ^O  */
      0,    0,			/* ^P - ^W, ^X - ^_  */
      0,    0x17,		/* SP - ' ,  ( - /   */
      0xff, 0xc0,		/*  0 - 7 ,  8 - ?   */
      0x7f, 0xff,		/*  @ - G ,  H - O   */
      0xff, 0xe1,		/*  P - W ,  X - _   */
      0x7f, 0xff,		/*  ` - g ,  h - o   */
      0xff, 0xf6,		/*  p - w ,  x - DEL */
      0,    0, 			/*  > DEL   */
      0,    0,			/*  > DEL   */
      0,    0, 			/*  > DEL   */
      0,    0, 			/*  > DEL   */
      0,    0 			/*  > DEL   */
};


/*
 * fallowc - returns TRUE if c is allowable in filenames, FALSE otw
 */
fallowc(c)
char c;
{
    return(okinfname[c>>3] & 0x80>>(c&7));
}


/*
 * fexist - returns TRUE if the file exists with mode passed in m, 
 *          FALSE otherwise.  By side effect returns length of file in l
 */
fexist(file, m, l)
char *file;
char *m;					/* files mode: r, w or rw */
long *l;
{
    struct stat	sbuf;

    if(l)
      *l = 0L;

    if(stat(file, &sbuf) < 0){
	switch(errno){
	  case ENOENT :				/* File not found */
	    return(FIOFNF);
#ifdef	ENAMETOOLONG
	  case ENAMETOOLONG :			/* Name is too long */
	    return(FIOLNG);
#endif
	  default:				/* Some other error */
	    return(FIOERR);
	}
    }

    if(l)
      *l = sbuf.st_size;

    if((sbuf.st_mode&S_IFMT) == S_IFDIR)
      return(FIODIR);

    if(m[0] == 'r')				/* read access? */
      return((S_IREAD&sbuf.st_mode) ? FIOSUC : FIONRD);
    else if(m[0] == 'w')			/* write access? */
      return((S_IWRITE&sbuf.st_mode) ? FIOSUC : FIONWT);
    else if(m[0] == 'x')			/* execute access? */
      return((S_IEXEC&sbuf.st_mode) ? FIOSUC : FIONEX);
    return(FIOERR);				/* what? */
}


/*
 * isdir - returns true if fn is a readable directory, false otherwise
 *         silent on errors (we'll let someone else notice the problem;)).
 */
isdir(fn, l)
char *fn;
long *l;
{
    struct stat sbuf;

    if(l)
      *l = 0;

    if(stat(fn, &sbuf) < 0)
      return(0);

    if(l)
      *l = sbuf.st_size;
    return((sbuf.st_mode&S_IFMT) == S_IFDIR);
}


#if	defined(bsd) || defined(nxt) || defined(dyn)
/*
 * getcwd - NeXT uses getwd()
 */
char *
getcwd(pth, len)
char *pth;
int   len;
{
    extern char *getwd();

    return(getwd(pth));
}
#endif


/*
 * gethomedir - returns the users home directory
 *              Note: home is malloc'd for life of pico
 */
char *
gethomedir(l)
int *l;
{
    static char *home = NULL;
    static short hlen = 0;

    if(home == NULL){
	char buf[NLINE];
	strcpy(buf, "~");
	fixpath(buf, NLINE);		/* let fixpath do the work! */
	hlen = strlen(buf);
	if((home = (char *)malloc((hlen + 1) * sizeof(char))) == NULL){
	    emlwrite("Problem allocating space for home dir", NULL);
	    return(0);
	}

	strcpy(home, buf);
    }

    if(l)
      *l = hlen;

    return(home);
}


/*
 * homeless - returns true if given file does not reside in the current
 *            user's home directory tree. 
 */
homeless(f)
char *f;
{
    char *home;
    int   len;

    home = gethomedir(&len);
    return(strncmp(home, f, len));
}



/*
 * errstr - return system error string corresponding to given errno
 *          Note: strerror() is not provided on all systems, so it's 
 *          done here once and for all.
 */
char *
errstr(err)
int err;
{
#if 0
#ifndef	neb
    extern char *sys_errlist[];
    extern int  sys_nerr;
#endif
#endif

    return((err >= 0 && err < sys_nerr) ? sys_errlist[err] : NULL);
}



/*
 * getfnames - return all file names in the given directory in a single 
 *             malloc'd string.  n contains the number of names
 */
char *
getfnames(dn, n)
char *dn;
int  *n;
{
    long           l;
    char          *names, *np, *p;
    struct stat    sbuf;
#if	defined(ct)
    FILE          *dirp;
    char           fn[DIRSIZ+1];
#else
    DIR           *dirp;			/* opened directory */
#endif
#if	defined(POSIX) || defined(aix) || defined(COHERENT) || defined(isc) || defined(sv3)
    struct dirent *dp;
#else
    struct direct *dp;
#endif

    *n = 0;

    if(stat(dn, &sbuf) < 0){
	switch(errno){
	  case ENOENT :				/* File not found */
	    emlwrite("\007File not found: \"%s\"", dn);
	    break;
#ifdef	ENAMETOOLONG
	  case ENAMETOOLONG :			/* Name is too long */
	    emlwrite("\007File name too long: \"%s\"", dn);
	    break;
#endif
	  default:				/* Some other error */
	    emlwrite("\007Error getting file info: \"%s\"", dn);
	    break;
	}
	return(NULL);
    } 
    else{
	l = sbuf.st_size;
	if((sbuf.st_mode&S_IFMT) != S_IFDIR){
	    emlwrite("\007Not a directory: \"%s\"", dn);
	    return(NULL);
	}
    }

    if((names=(char *)malloc(sizeof(char)*l)) == NULL){
	emlwrite("\007Can't malloc space for file names", NULL);
	return(NULL);
    }

    errno = 0;
    if((dirp=opendir(dn)) == NULL){
	char buf[NLINE];
	sprintf(buf,"\007Can't open \"%s\": %s", dn, errstr(errno));
	emlwrite(buf, NULL);
	free((char *)names);
	return(NULL);
    }

    np = names;

#if	defined(ct)
    while(fread(&dp, sizeof(struct direct), 1, dirp) > 0) {
    /* skip empty slots with inode of 0 */
	if(dp.d_ino == 0)
	     continue;
	(*n)++;                     /* count the number of active slots */
	(void)strncpy(fn, dp.d_name, DIRSIZ);
	fn[14] = '\0';
	p = fn;
	while((*np++ = *p++) != '\0')
	  ;
    }
#else
    while((dp = readdir(dirp)) != NULL){
	(*n)++;
	p = dp->d_name;
	while((*np++ = *p++) != '\0')
	  ;
    }
#endif

    closedir(dirp);					/* shut down */
    return(names);
}


/*
 * fioperr - given the error number and file name, display error
 */
void
fioperr(e, f)
int  e;
char *f;
{
    switch(e){
      case FIOFNF:				/* File not found */
	emlwrite("\007File \"%s\" not found", f);
	break;
      case FIOEOF:				/* end of file */
	emlwrite("\007End of file \"%s\" reached", f);
	break;
      case FIOLNG:				/* name too long */
	emlwrite("\007File name \"%s\" too long", f);
	break;
      case FIODIR:				/* file is a directory */
	emlwrite("\007File \"%s\" is a directory", f);
	break;
      case FIONWT:
	emlwrite("\007Write permission denied: %s", f);
	break;
      case FIONRD:
	emlwrite("\007Read permission denied: %s", f);
	break;
      case FIONEX:
	emlwrite("\007Execute permission denied: %s", f);
	break;
      default:
	emlwrite("\007File I/O error: %s", f);
    }
}



/*
 * pfnexpand - pico's function to expand the given file name if there is 
 *	       a leading '~'
 */
char *pfnexpand(fn, len)
char *fn;
int  len;
{
    struct passwd *pw;
    register char *x, *y, *z;
    char name[20];
    
    if(*fn == '~') {
        for(x = fn+1, y = name; *x != '/' && *x != '\0'; *y++ = *x++);
        *y = '\0';
        if(x == fn + 1) 
          pw = getpwuid(getuid());
        else
          pw = getpwnam(name);
        if(pw == NULL)
          return(NULL);
        if(strlen(pw->pw_dir) + strlen(fn) > len) {
            return(NULL);
        }
	/* make room for expanded path */
	for(z=x+strlen(x),y=fn+strlen(x)+strlen(pw->pw_dir);
	    z >= x;
	    *y-- = *z--);
	/* and insert the expanded address */
	for(x=fn,y=pw->pw_dir; *y != '\0'; *x++ = *y++);
    }
    return(fn);
}



/*
 * fixpath - make the given pathname into an absolute path
 */
fixpath(name, len)
char *name;
int  len;
{
    register char *shft;

    /* filenames relative to ~ */
    if(!((name[0] == '/')
          || (name[0] == '.'
              && (name[1] == '/' || (name[1] == '.' && name[2] == '/'))))){
	if(Pmaster && !(gmode&MDCURDIR)
                   && (*name != '~' && strlen(name)+2 <= len)){

	    for(shft = strchr(name, '\0'); shft >= name; shft--)
	      shft[2] = *shft;

	    name[0] = '~';
	    name[1] = '/';
	}

	pfnexpand(name, len);
    }
}


/*
 * compresspath - given a base path and an additional directory, collapse
 *                ".." and "." elements and return absolute path (appending
 *                base if necessary).  
 *
 *                returns  1 if OK, 
 *                         0 if there's a problem
 *                         new path, by side effect, if things went OK
 */
compresspath(base, path, len)
char *base, *path;
int  len;
{
    register int i;
    int  depth = 0;
    char *p;
    char *stack[32];
    char  pathbuf[NLINE];

#define PUSHD(X)  (stack[depth++] = X)
#define POPD()    ((depth > 0) ? stack[--depth] : "")

    if(*path == '~'){
	fixpath(path, len);
	strcpy(pathbuf, path);
    }
    else if(*path != C_FILESEP)
      sprintf(pathbuf, "%s%c%s", base, C_FILESEP, path);
    else
      strcpy(pathbuf, path);

    p = &pathbuf[0];
    for(i=0; pathbuf[i] != '\0'; i++){		/* pass thru path name */
	if(pathbuf[i] == '/'){
	    if(p != pathbuf)
	      PUSHD(p);				/* push dir entry */

	    p = &pathbuf[i+1];			/* advance p */
	    pathbuf[i] = '\0';			/* cap old p off */
	    continue;
	}

	if(pathbuf[i] == '.'){			/* special cases! */
	    if(pathbuf[i+1] == '.' 		/* parent */
	       && (pathbuf[i+2] == '/' || pathbuf[i+2] == '\0')){
		if(!strcmp(POPD(), ""))		/* bad news! */
		  return(0);

		i += 2;
		p = (pathbuf[i] == '\0') ? "" : &pathbuf[i+1];
	    }
	    else if(pathbuf[i+1] == '/' || pathbuf[i+1] == '\0'){
		i++;
		p = (pathbuf[i] == '\0') ? "" : &pathbuf[i+1];
	    }
	}
    }

    if(*p != '\0')
      PUSHD(p);					/* get last element */

    path[0] = '\0';
    for(i = 0; i < depth; i++){
	strcat(path, S_FILESEP);
	strcat(path, stack[i]);
    }

    return(1);					/* everything's ok */
}


/*
 * tmpname - return a temporary file name in the given buffer
 */
void
tmpname(name)
char *name;
{
    sprintf(name, "/tmp/pico.%d", getpid());	/* tmp file name */
}


/*
 * Take a file name, and from it
 * fabricate a buffer name. This routine knows
 * about the syntax of file names on the target system.
 * I suppose that this information could be put in
 * a better place than a line of code.
 */
void
makename(bname, fname)
char    bname[];
char    fname[];
{
    register char   *cp1;
    register char   *cp2;

    cp1 = &fname[0];
    while (*cp1 != 0)
      ++cp1;

    while (cp1!=&fname[0] && cp1[-1]!='/')
      --cp1;

    cp2 = &bname[0];
    while (cp2!=&bname[NBUFN-1] && *cp1!=0 && *cp1!=';')
      *cp2++ = *cp1++;

    *cp2 = 0;
}


/*
 * copy - copy contents of file 'a' into a file named 'b'.  Return error
 *        if either isn't accessible or is a directory
 */
copy(a, b)
char *a, *b;
{
    int    in, out, n, rv = 0;
    char   *cb;
    struct stat tsb, fsb;
    extern int  errno;

    if(stat(a, &fsb) < 0){		/* get source file info */
	emlwrite("Can't Copy: %s", errstr(errno));
	return(-1);
    }

    if(!(fsb.st_mode&S_IREAD)){		/* can we read it? */
	emlwrite("\007Read permission denied: %s", a);
	return(-1);
    }

    if((fsb.st_mode&S_IFMT) == S_IFDIR){ /* is it a directory? */
	emlwrite("\007Can't copy: %s is a directory", a);
	return(-1);
    }

    if(stat(b, &tsb) < 0){		/* get dest file's mode */
	switch(errno){
	  case ENOENT:
	    break;			/* these are OK */
	  default:
	    emlwrite("\007Can't Copy: %s", errstr(errno));
	    return(-1);
	}
    }
    else{
	if(!(tsb.st_mode&S_IWRITE)){	/* can we write it? */
	    emlwrite("\007Write permission denied: %s", b);
	    return(-1);
	}

	if((tsb.st_mode&S_IFMT) == S_IFDIR){	/* is it directory? */
	    emlwrite("\007Can't copy: %s is a directory", b);
	    return(-1);
	}

	if(fsb.st_dev == tsb.st_dev && fsb.st_ino == tsb.st_ino){
	    emlwrite("\007Identical files.  File not copied", NULL);
	    return(-1);
	}
    }

    if((in = open(a, O_RDONLY)) < 0){
	emlwrite("Copy Failed: %s", errstr(errno));
	return(-1);
    }

    if((out=creat(b, fsb.st_mode&0xfff)) < 0){
	emlwrite("Can't Copy: %s", errstr(errno));
	close(in);
	return(-1);
    }

    if((cb = (char *)malloc(NLINE*sizeof(char))) == NULL){
	emlwrite("Can't allocate space for copy buffer!", NULL);
	close(in);
	close(out);
	return(-1);
    }

    while(1){				/* do the copy */
	if((n = read(in, cb, NLINE)) < 0){
	    emlwrite("Can't Read Copy: %s", errstr(errno));
	    rv = -1;
	    break;			/* get out now */
	}

	if(n == 0)			/* done! */
	  break;

	if(write(out, cb, n) != n){
	    emlwrite("Can't Write Copy: %s", errstr(errno));
	    rv = -1;
	    break;
	}
    }

    free(cb);
    close(in);
    close(out);
    return(rv);
}


/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
ffwopen(fn)
char    *fn;
{
    extern FILE *ffp;

    if ((ffp=fopen(fn, "w")) == NULL) {
        emlwrite("Cannot open file for writing", NULL);
        return (FIOERR);
    }

    return (FIOSUC);
}


/*
 * Close a file. Should look at the status in all systems.
 */
ffclose()
{
    extern FILE *ffp;

    if (fclose(ffp) != FALSE) {
        emlwrite("Error closing file", NULL);
        return(FIOERR);
    }

    return(FIOSUC);
}


/*
 * P_open - run the given command in a sub-shell returning a file pointer
 *	    from which to read the output
 *
 * note:
 *	For OS's other than unix, you will have to rewrite this function.
 *	Hopefully it'll be easy to exec the command into a temporary file, 
 *	and return a file pointer to that opened file or something.
 */
FILE *P_open(s)
char *s;
{
    return(popen(s, "r"));
}



/*
 * P_close - close the given descriptor
 *
 */
P_close(fp)
FILE *fp;
{
    return(pclose(fp));
}



/*
 * worthit - generic sort of test to roughly gage usefulness of using 
 *           optimized scrolling.
 *
 * note:
 *	returns the line on the screen, l, that the dot is currently on
 */
worthit(l)
int *l;
{
    int i;			/* l is current line */
    unsigned below;		/* below is avg # of ch/line under . */

    *l = doton(&i, &below);
    below = (i > 0) ? below/(unsigned)i : 0;

    return(below > 3);
}



/*
 * pico_new_mail - just checks mtime and atime of mail file and notifies user 
 *	           if it's possible that they have new mail.
 */
pico_new_mail()
{
    int ret = 0;
    static time_t lastchk = 0;
    struct stat sbuf;
    char   inbox[256], *p;

    if(p = (char *)getenv("MAIL"))
      sprintf(inbox, p);
    else
      sprintf(inbox,"%s/%s", MAILDIR, getlogin());

    if(stat(inbox, &sbuf) == 0){
	ret = sbuf.st_atime <= sbuf.st_mtime &&
	  (lastchk < sbuf.st_mtime && lastchk < sbuf.st_atime);
	lastchk = sbuf.st_mtime;
	return(ret);
    }
    else
      return(ret);
}



/*
 * time_to_check - checks the current time against the last time called 
 *                 and returns true if the elapsed time is > timeout
 */
time_to_check()
{
    static time_t lasttime = 0L;

    if(!timeout)
      return(FALSE);

    if(time((long *) 0) - lasttime > (time_t)timeout){
	lasttime = time((long *) 0);
	return(TRUE);
    }
    else
      return(FALSE);
}


/*
 * sstrcasecmp - compare two pointers to strings case independently
 */
sstrcasecmp(s1, s2)
QcompType *s1, *s2;
{
    register char *a, *b;

    a = *(char **)s1;
    b = *(char **)s2;
    while(toupper(*a) == toupper(*b++))
	if(*a++ == '\0')
	  return(0);

    return(toupper(*a) - toupper(*--b));
}


#ifdef	TIOCGWINSZ
/*
 * winch_handler - handle window change signal
 */
SIGTYPE winch_handler()
{
    struct winsize win;
    extern int resize_pico();

    signal(SIGWINCH, winch_handler);

    if (ioctl(0, TIOCGWINSZ, &win) == 0) {
	if (win.ws_col && win.ws_row)
	  resize_pico(win.ws_row - 1, win.ws_col);
    }
}
#endif	/* TIOCGWINSZ */


#if	defined(sv3) || defined(ct)
/* Placed by rll to handle the rename function not found in AT&T */
rename(oldname, newname)
    char *oldname;
    char *newname;
{
    int rtn;

    if ((rtn = link(oldname, newname)) != 0) {
	perror("Was not able to rename file.");
	return(rtn);
    }

    if ((rtn = unlink(oldname)) != 0)
      perror("Was not able to unlink file.");

    return(rtn);
}
#endif
