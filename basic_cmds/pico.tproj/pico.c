#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pico.c,v 1.1.1.1 1999/04/15 17:45:13 wsanchez Exp $";
#endif
/*
 * Program:	Main Pine Composer routines
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
 * Copyright 1991-1993  University of Washington
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
 * WEEMACS/PICO NOTES:
 *
 * 01 Nov 89 - MicroEmacs 3.6 vastly pared down and becomes a function call 
 * 	       weemacs() and plugged into the Pine mailer.  Lots of unused
 *	       MicroEmacs code laying around.
 *
 * 17 Jan 90 - weemacs() became weemacs() the composer.  Header editing
 *	       functions added.
 *
 * 09 Sep 91 - weemacs() renamed pico() for the PIne COmposer.
 *
 */


/*
 * This program is in public domain; written by Dave G. Conroy.
 * This file contains the main driving routine, and some keyboard processing
 * code, for the MicroEMACS screen editor.
 *
 * REVISION HISTORY:
 *
 * 1.0  Steve Wilhite, 30-Nov-85
 *      - Removed the old LK201 and VT100 logic. Added code to support the
 *        DEC Rainbow keyboard (which is a LK201 layout) using the the Level
 *        1 Console In ROM INT. See "rainbow.h" for the function key defs
 *      Steve Wilhite, 1-Dec-85
 *      - massive cleanup on code in display.c and search.c
 *
 * 2.0  George Jones, 12-Dec-85
 *      - Ported to Amiga.
 *
 * 3.0  Daniel Lawrence, 29-Dec-85
 *	16-apr-86
 *	- updated documentation and froze development for 3.6 net release
 */

#include        <stdio.h>
#include	<setjmp.h>

/* make global definitions not external */
#define	maindef

#include	"osdep.h"	/* operating system dependent includes */
#include	"pico.h"	/* PIne COmposer definitions */
#include        "estruct.h"	/* global structures and defines */
#include	"efunc.h"	/* function declarations and name table	*/
#include	"edef.h"	/* global definitions */
#include	"ebind.h"	/* default key bindings */


#ifdef	ANSI
    int  func_init(void);
    void breplace(void *);
    int  insline(LINE **, short *);
#else
    int  func_init();
    void breplace();
    int  insline();
#endif


/*
 * function key mappings
 */
static int pfkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'X')},
    { F3,  (CTRL|'C')},
    { F4,  (CTRL|'J')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'W')},
    { F7,  (CTRL|'Y')},
    { F8,  (CTRL|'V')},
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'O')},
#ifdef	SPELLER
    { F12, (CTRL|'T')}
#else
    { F12, (CTRL|'D')}
#endif
};


/*
 * flag for the various functions in pico() to set when ready
 * for pico() to return...
 */
int      pico_all_done = 0;
jmp_buf  finstate;
char    *pico_anchor = NULL;

/*
 * pico - the main routine for Pine's composer.
 *
 */
pico(pm)
PICO *pm;
{
    register int    c;
    register int    f;
    register int    n;
    int      i;
    char     bname[NBUFN];		/* buffer name of file to read */
    extern   struct on_display ods;

    Pmaster       = pm;
    gmode        |= pm->pine_flags;/* high 4 bits rsv'd by pine composer */
    pico_all_done = 0;

    if(!vtinit())			/* Init Displays.      */
      return(COMP_CANCEL);

    strcpy(bname, "main");		/* default buffer name */
    edinit(bname);			/* Buffers, windows.   */

    InitMailHeader(pm);			/* init mail header structure */
    (*Pmaster->clearcur)();

    /* setup to process commands */
    lastflag = 0;			/* Fake last flags.     */
    curbp->b_mode |= gmode;		/* and set default modes*/

    pico_anchor = (char *)malloc((strlen(Pmaster->pine_anchor) + 1)
				 * sizeof(char));
    if(pico_anchor)
      strcpy(pico_anchor, Pmaster->pine_anchor);

    if(pm->msgtext)
      breplace(pm->msgtext);

    pico_all_done = setjmp(finstate);	/* jump out of HUP handler ? */

    if(!pico_all_done){
	if(pm->pine_flags&P_BODY){	/* begin editing the header? */
	    ArrangeHeader();		/* line up pointers */
	}
	else{
	    update();			/* paint screen, */
	    HeaderEditor(0, 0);		/* start editing... */
	}
    }

    while(1){
	if(pico_all_done){
	    c = anycb() ? BUF_CHANGED : 0;
	    switch(pico_all_done){	/* prepare for/handle final events */
	      case COMP_EXIT :		/* already confirmed */
		packheader();
		c |= COMP_EXIT;
		break;

	      case COMP_CANCEL :	/* also already confirmed */
		c = COMP_CANCEL;
		break;

	      case COMP_GOTHUP:
		/* 
		 * pack up and let caller know that we've received a SIGHUP
		 */
		if(ComposerEditing)		/* expand addr if needed */
		  resolve_niks(ods.cur_e);

		packheader();
		c |= COMP_GOTHUP;
		break;

	      case COMP_SUSPEND :
	      default:			/* safest if internal error */
		packheader();
		c |= COMP_SUSPEND;
		break;
	    }

	    free(pico_anchor);
	    vttidy();			/* clean up tty modes */
	    zotdisplay();		/* blast display buffers */
	    zotedit();
	    return(c);
	}
	update();			/* Fix up the screen    */

#if	defined(DOS) && defined(MOUSE)
	register_mfunc(pico_mouse, 2, 0, term.t_nrow-3, term.t_ncol);
#endif
	c = GetKey();	

	if(c == NODATA || time_to_check()){	/* new mail ? */
	    if((*Pmaster->newmail)(&i, 0, c == NODATA ? 0 : 2) >= 0){
		mlerase();
		(*Pmaster->showmsg)(c);
		mpresf = 1;
	    }

	    if(i || mpresf){		/* let em know cursor moved */
		movecursor(0, 0);
		(*Pmaster->clearcur)();
	    }
	}

	if(c == NODATA)		/* no op, getkey timed out */
	  continue;

	if (mpresf != FALSE) {		/* message stay around only  */
	    if (mpresf++ > MESSDELAY)	/* so long! */
	      mlerase();
	}

	f = FALSE;			/* vestigial */
	n = 1;
	
#if	defined(DOS) && defined(MOUSE)
	clear_mfunc();
#endif
	execute(normal(c, pfkm, 2), f, n);	/* Do it.               */
    }
}

/*
 * Initialize all of the buffers and windows. The buffer name is passed down
 * as an argument, because the main routine may have been told to read in a
 * file by default, and we want the buffer name to be right.
 */

/*
 * For the pine composer, we don't want to take over the whole screen
 * for editing.  the first some odd lines are to be used for message 
 * header information editing.
 */
edinit(bname)
char    bname[];
{
    register BUFFER *bp;
    register WINDOW *wp;

    if(Pmaster)
      func_init();

    bp = bfind(bname, TRUE, 0);             /* First buffer         */
    wp = (WINDOW *) malloc(sizeof(WINDOW)); /* First window         */

    if (bp==NULL || wp==NULL){
	if(Pmaster)
	  return(0);
	else
	  exit(1);
    }

    curbp  = bp;                            /* Make this current    */
    wheadp = wp;
    curwp  = wp;
    wp->w_wndp  = NULL;                     /* Initialize window    */
    wp->w_bufp  = bp;
    bp->b_nwnd  = 1;                        /* Displayed.           */
    wp->w_linep = bp->b_linep;
    wp->w_dotp  = bp->b_linep;
    wp->w_doto  = 0;
    wp->w_markp = NULL;
    wp->w_marko = 0;

    if(Pmaster){
	wp->w_toprow = COMPOSER_TOP_LINE;
	wp->w_ntrows = term.t_nrow - COMPOSER_TOP_LINE - 2;
	fillcol = (term.t_ncol > 80) ? 77 : term.t_ncol - 6;
    }
    else{
        wp->w_toprow = 2;
        wp->w_ntrows = term.t_nrow-4;           /* "-1" for mode line.  */
	fillcol = term.t_ncol - 6;              /* set fill column */
    }

    wp->w_force = 0;
    wp->w_flag  = WFMODE|WFHARD;            /* Full.                */
}


/*
 * This is the general command execution routine. It handles the fake binding
 * of all the keys to "self-insert". It also clears out the "thisflag" word,
 * and arranges to move it to the "lastflag", so that the next command can
 * look at it. Return the status of command.
 */
execute(c, f, n)
int c, f, n;
{
    register KEYTAB *ktp;
    register int    status;

    ktp = (Pmaster) ? &keytab[0] : &pkeytab[0];

    while (ktp->k_fp != NULL) {
	if (ktp->k_code == c) {

	    if(lastflag&CFFILL){
		curwp->w_flag |= WFMODE;
		if(Pmaster == NULL)
		  sgarbk = TRUE;
	    }

	    thisflag = 0;
	    status   = (*ktp->k_fp)(f, n);
	    lastflag = thisflag;

	    return (status);
	}
	++ktp;
    }

#ifdef	DOS
    if (c>=0x20 && c<=0xFF) {
#else
    if ((c>=0x20 && c<=0x7E)                /* Self inserting.      */
        ||  (c>=0xA0 && c<=0xFE)) {
#endif

	if (n <= 0) {                   /* Fenceposts.          */
	    lastflag = 0;
	    return (n<0 ? FALSE : TRUE);
	}
	thisflag = 0;                   /* For the future.      */

	/* if we are in overwrite mode, not at eol,
	   and next char is not a tab or we are at a tab stop,
	   delete a char forword			*/
	if (curwp->w_bufp->b_mode & MDOVER &&
	    curwp->w_doto < curwp->w_dotp->l_used &&
	    (lgetc(curwp->w_dotp, curwp->w_doto).c != '\t' ||
	     (curwp->w_doto) % 8 == 7))
	  ldelete(1, FALSE);

	/* do the appropriate insertion */
	/* pico never does C mode, this is simple */
	status = linsert(n, c);

	/*
	 * Check to make sure we didn't go off of the screen
	 * with that character.  If so wrap the line...
	 * note: in pico,  fillcol and wrap-mode are always set and 
	 * 	 wrapword behaves somewhat differently
	 */
	if(((llength(curwp->w_dotp)+2 >= ((Pmaster && term.t_ncol > 80)
	                                       ? 77 : term.t_ncol))
	    || (c == ' ' && getccol(FALSE) > fillcol))
	   && (curwp->w_bufp->b_mode & MDWRAP))
	  wrapword();

	lastflag = thisflag;
	return (status);
    }
    
    if(c&CTRL)
      emlwrite("\007Unknown Command: ^%c", (void *)(c&0xff));
    else
      emlwrite("\007Unknown Command", NULL);

    lastflag = 0;                           /* Fake last flags.     */
    return (FALSE);
}



/*
 * Fancy quit command, as implemented by Norm. If the any buffer has
 * changed do a write on that buffer and exit emacs, otherwise simply exit.
 */
quickexit(f, n)
int f, n;
{
    register BUFFER *bp;	/* scanning pointer to buffers */

    bp = bheadp;
    while (bp != NULL) {
	if ((bp->b_flag&BFCHG) != 0	/* Changed.             */
	    && (bp->b_flag&BFTEMP) == 0) {	/* Real.                */
	    curbp = bp;		/* make that buffer cur	*/
	    filesave(f, n);
	}
	bp = bp->b_bufp;			/* on to the next buffer */
    }
    wquit(f, n);                             /* conditionally quit   */
}



/* 
 * abort_composer - ask the question here, then go quit or 
 *                  return FALSE
 */
abort_composer(f, n)
int f, n;
{
    switch(mlyesno("Cancelling will abandon your mail message.  Cancel", 
		    FALSE)){
      case TRUE:
	pico_all_done = COMP_CANCEL;
	return(TRUE);

      case ABORT:
	emlwrite("\007Cancel Cancelled", NULL);
	break;

      default:
	mlerase();
    }
    return(FALSE);
}


/*
 * suspend_composer - return to pine with what's been edited so far
 */
suspend_composer(f, n)
int f, n;
{
    pico_all_done = COMP_SUSPEND;
}



/*
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out. Normally bound to "C-X C-C".
 */
wquit(f, n)
int f, n;
{
    register int    s;

    if(Pmaster){
	char *prompt;
	int   defans;

	/*
	 * if we're not in header, show some of it as we verify sending...
	 */
	setimark(FALSE, 1);
	if(ComposerTopLine == COMPOSER_TOP_LINE){
	    gotobob(FALSE, 1);
	    update();
	}

	if(AttachError()){
	    prompt = "\007Problem with attachments!  Send anyway";
	    defans = FALSE;
	}
	else{
	    prompt = "Send message";
	    defans = TRUE;
	}

	switch(mlyesno(prompt, defans)){
	  case TRUE:
	    pico_all_done = COMP_EXIT;
	    return(TRUE);
	  case ABORT:
	    emlwrite("\007Send Message Cancelled", NULL);
	    break;
	  default:
	    mlerase();
	}

	swapimark(FALSE, 1);
    }
    else{
        if (f != FALSE                          /* Argument forces it.  */
        || anycb() == FALSE                     /* All buffers clean.   */
						/* User says it's OK.   */
        || (s=mlyesno("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES)", -1)) == FALSE) {
                vttidy();
                exit(0);
        }

	if(s == TRUE){
	    if(filewrite(0,1) == TRUE)
	      wquit(1, 0);
	}
	else if(s == ABORT){
	    emlwrite("\007Exit Cancelled", NULL);
	}
        return(s);
    }

    return(FALSE);
}


/*
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
ctrlg(f, n)
int f, n;
{
    (*term.t_beep)();
    if (kbdmip != NULL) {
	kbdm[0] = (CTLX|')');
	kbdmip  = NULL;
    }
    emlwrite("Cancelled", NULL);
    return (ABORT);
}


/* tell the user that this command is illegal while we are in
 *  VIEW (read-only) mode
 */
rdonly()
{
    (*term.t_beep)();
    emlwrite("Key illegal in VIEW mode", NULL);
    return(FALSE);
}



/*
 * reset all globals to their initial values
 */
func_init()
{
    extern int    vtrow;
    extern int    vtcol;
    extern int    lbound;
    extern int    ttrow;
    extern int    ttcol;

    /*
     * re-initialize global buffer type variables ....
     */
    fillcol = (term.t_ncol > 80) ? 77 : term.t_ncol - 6;
    eolexist = TRUE;
    revexist = FALSE;
    sgarbf = TRUE;
    mpresf = FALSE;
    mline_open = FALSE;
    ComposerEditing = FALSE;

    /*
     * re-initialize hardware display variables ....
     */
    vtrow = vtcol = lbound = 0;
    ttrow = ttcol = HUGE;

    pat[0] = rpat[0] = '\0';
}


/*
 * pico_help - help function for standalone composer
 */
pico_help(text, title, i)
char *text[], *title;
int i;
{
    register    int numline = 0;
    char        **p;
 
    p = text;
    while(*p++ != NULL) 
      numline++;
    return(wscrollw(COMPOSER_TOP_LINE, term.t_nrow-1, text, numline));

}



#if     TERMCAP
/*
 * zottree() - kills the key pad function key search tree
 *                and frees all lines associated with it!!!
 */
zottree(nodeptr)
struct	KBSTREE	*nodeptr;
{
    if(nodeptr != NULL){
	zottree(nodeptr->left);
	zottree(nodeptr->down);
	free((char *) nodeptr);
    }
}
#endif


/*
 * zotedit() - kills the buffer and frees all lines associated with it!!!
 */
zotedit()
{
#ifdef	OLDWAY
    register int	s;

    /*
     * clean up the lines and buffer ...
     */
    curbp->b_flag &= ~BFCHG;

    if ((s=bclear(curbp)) != TRUE)		/* Blow text away.      */
      return(s);
#else
    wheadp->w_linep = wheadp->w_dotp = wheadp->w_markp = NULL;
    bheadp->b_linep = bheadp->b_dotp = bheadp->b_markp = NULL;
#endif

    free((char *) wheadp);			/* clean up window */
    wheadp = NULL;
    curwp  = NULL;

    free((char *) bheadp);			/* clean up buffers */
    bheadp = NULL;
    curbp  = NULL;

    zotheader();				/* blast header lines */

#ifdef	OLDWAY
    zotdisplay();				/* blast display buffers */
#else
    kdelete();					/* blast kill buffer */
#endif

#if     TERMCAP
    zottree(kpadseqs);
    kpadseqs = NULL;
#endif
}


/* 
 * Below are functions for use outside pico to manipulate text
 * in a pico's native format (circular linked list of lines).
 *
 * The idea is to streamline pico use by making it fairly easy
 * for outside programs to prepare text intended for pico's use.
 * The simple char * alternative is messy as it requires two copies
 * of the same text, and isn't very economic in limited memory
 * situations (THANKS BELLEVUE-BILLY.).
 */
typedef struct picotext {
    LINE *linep;
    LINE *dotp;
    short doto;
    short crinread;
} PICOTEXT;

#define PT(X)	((PICOTEXT *)(X))

/*
 * pico_get - return window struct pointer used as a handle
 *            to the other pico_xxx routines.
 */
void *
pico_get()
{
   PICOTEXT *wp = NULL;
   LINE     *lp = NULL;

   if(wp = (PICOTEXT *)malloc(sizeof(PICOTEXT))){
       wp->crinread = 0;
       if((lp = lalloc(0)) == NULL){
	   free(wp);
	   return(NULL);
       }

       wp->dotp = wp->linep = lp->l_fp = lp->l_bp = lp;
       wp->doto = 0;
   }
   else
     emlwrite("Can't allocate space for text", NULL);

   return((void *)wp);
}

/*
 * pico_give - free resources and give up picotext struct
 */
void
pico_give(w)
void *w;
{
    register LINE *lp;
    register LINE *fp;

    fp = lforw(PT(w)->linep);
    while((lp = fp) != PT(w)->linep){
        fp = lforw(lp);
	free(lp);
    }
    free(PT(w)->linep);
    free((PICOTEXT *)w);
}

/*
 * pico_readc - return char at current point.  Up to calling routines
 *              to keep cumulative count of chars.
 */
int
pico_readc(w, c)
void          *w;
unsigned char *c;
{
    int rv     = 0;

    if(PT(w)->crinread){
	*c = '\012';				/* return LF */
	PT(w)->crinread = 0;
	rv++;
    }
    else if(PT(w)->doto < llength(PT(w)->dotp)){ /* normal char to return */
        *c = (unsigned char) lgetc(PT(w)->dotp, (PT(w)->doto)++).c;
	rv++;
    }
    else if(lforw(PT(w)->dotp) != PT(w)->linep){ /* return line break */
	PT(w)->dotp = lforw(PT(w)->dotp);
	PT(w)->doto = 0;
#if	defined(DOS)
	*c = '\015';
	PT(w)->crinread++;
#else
	*c = '\012';				/* return local eol! */
#endif
	rv++;
    }						/* else no chars to return */

    return(rv);
}


/*
 * pico_writec - write a char into picotext and advance pointers.
 *               Up to calling routines to keep track of total chars
 *               written
 */
int
pico_writec(w, c)
void *w;
int   c;
{
    int   rv = 0;

    if(c == '\r')				/* ignore CR's */
      rv++;					/* so fake it */
    else if(c == '\n'){				/* insert newlines on LF */
	
	if(PT(w)->linep == PT(w)->dotp){	/* special case */
	    if(!insline(&(PT(w)->dotp), &(PT(w)->doto)))
	      return(0);
	}

	if(!insline(&(PT(w)->dotp), &(PT(w)->doto)))
	  return(0);

        rv++;
    }
    else
      rv = geninsert(&(PT(w)->dotp), &(PT(w)->doto), PT(w)->linep, c, 0, 1);

    return((rv) ? 1 : 0);			/* return number written */
}


/*
 * pico_puts - just write the given string into the text
 */
int
pico_puts(w, s)
void *w;
char *s;
{
    while(*s != '\0')
      pico_writec(w, (int)*s++);
}


/*
 * pico_seek - position dotp and dot at requested location
 */
int
pico_seek(w, offset, orig)
void *w;
long  offset;
int   orig;
{
    register LINE *lp;

    PT(w)->crinread = 0;
    switch(orig){
      case 0 :				/* SEEK_SET */
	PT(w)->dotp = lforw(PT(w)->linep);
	PT(w)->doto = 0;
      case 1 :				/* SEEK_CUR */
	lp = PT(w)->dotp;
	while(lp != PT(w)->linep){
	    if(offset <= llength(lp)){
		PT(w)->doto = (int)offset;
		PT(w)->dotp = lp;
		break;
	    }

	    offset -= ((long)llength(lp)
#ifdef	DOS
		       + 2L);
#else
		       + 1L);
#endif
	    lp = lforw(lp);
	}
        break;

      case 2 :				/* SEEK_END */
	PT(w)->dotp = lback(PT(w)->linep);
	PT(w)->doto = llength(PT(w)->dotp);
	break;
      default :
        return(-1);
    }

    return(0);
}


/*
 * pico_replace - replace whatever's in the associated text with
 *                the given string
 */
int
pico_replace(w, s)
void *w;
char *s;
{
    return(0);
}

/*
 * breplace - replace the current window's text with the given 
 *            LINEs
 */
void
breplace(w)
void *w;
{
    register LINE *lp;
    register LINE *fp;

    fp = lforw(curbp->b_linep);
    while((lp = fp) != curbp->b_linep){		/* blast old lines */
        fp = lforw(lp);
	free(lp);
    }
    free(curbp->b_linep);

    curbp->b_linep  = PT(w)->linep;			/* arrange pointers */

    curwp->w_linep = lforw(curbp->b_linep);
    curwp->w_dotp  = lforw(curbp->b_linep);
    curwp->w_doto  = 0;
    curwp->w_markp = curwp->w_imarkp = NULL;
    curwp->w_marko = curwp->w_imarko = 0;

    curbp->b_dotp   = curwp->w_dotp;
    curbp->b_doto   = curbp->b_marko  = 0;
    curbp->b_markp  = NULL;

    curwp->w_flag |= WFHARD;
}


/*
 * generic line insert function.
 */
insline(dotp, doto)
LINE **dotp;
short *doto;
{
    register LINE *lp;

    if((lp = lalloc(0)) == NULL){
	emlwrite("Can't allocate space for more characters",NULL);
	return(FALSE);
    }

    lp->l_fp = (*dotp)->l_fp;
    lp->l_bp = (*dotp);
    lp->l_fp->l_bp = (*dotp)->l_fp = lp;
    (*dotp) = lp;
    (*doto) = 0;
    return(TRUE);
}
