#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: random.c,v 1.3 2003/01/12 01:48:12 bbraun Exp $";
#endif
/*
 * Program:	Random routines
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
/*
 * This file contains the command processing functions for a number of random
 * commands. There is no functional grouping here, for sure.
 */

#include        <stdio.h>
#include	"estruct.h"
#include	"pico.h"
#include        "edef.h"

#ifdef	ANSI
    int getccol(int);
#else
    int getccol();
#endif

int     tabsize;                        /* Tab size (0: use real tabs)  */

/*
 * Set fill column to n.
 */
setfillcol(f, n)
int f, n;
{
    char	numbuf[16];
    register int s;

    sprintf(numbuf,"%d",fillcol);
    s = mlreplyd("Set fill column to : ", numbuf, 4, QDEFLT, NULL);
    fillcol = atoi(numbuf);
    return(s);
}


/*
 * Display the current position of the cursor, in origin 1 X-Y coordinates,
 * the character that is under the cursor (in octal), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 * Normally this is bound to "C-X =".
 */
showcpos(f, n)
int f, n;
{
    register LINE   *clp;
    register long   nch;
    register int    cbo;
    register long   nbc;
    register int    lines;
    register int    thisline;
    char     buffer[80];

    clp = lforw(curbp->b_linep);            /* Grovel the data.     */
    cbo = 0;
    nch = 0L;
    lines = 0;
    for (;;) {
	if (clp==curwp->w_dotp && cbo==curwp->w_doto) {
	    thisline = lines;
	    nbc = nch;
	}
	if (cbo == llength(clp)) {
	    if (clp == curbp->b_linep)
	      break;
	    clp = lforw(clp);
	    cbo = 0;
	    lines++;
	} else
	  ++cbo;
	++nch;
    }

    sprintf(buffer,"line %d of %d (%d%%%%), character %ld of %ld (%d%%%%)",
	    thisline+1, lines+1, (int)((100L*(thisline+1))/(lines+1)),
	    nbc, nch, (nch) ? (int)((100L*nbc)/nch) : 0);

    emlwrite(buffer, NULL);
    return (TRUE);
}


/*
 * Return current column.  Stop at first non-blank given TRUE argument.
 */
getccol(bflg)
int bflg;
{
    register int c, i, col;

    col = 0;
    for (i=0; i<curwp->w_doto; ++i) {
	c = lgetc(curwp->w_dotp, i).c;
	if (c!=' ' && c!='\t' && bflg)
	  break;

	if (c == '\t')
	  col |= 0x07;
	else if (c<0x20 || c==0x7F)
	  ++col;

	++col;
    }

    return(col);
}



/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces. This has to be
 * done in this slightly funny way because the tab (in ASCII) has been turned
 * into "C-I" (in 10 bit code) already. Bound to "C-I".
 */
tab(f, n)
{
    if (n < 0)
      return (FALSE);

    if (n == 0 || n > 1) {
	tabsize = n;
	return(TRUE);
    }

    if (! tabsize)
      return(linsert(1, '\t'));

    return(linsert(tabsize - (getccol(FALSE) % tabsize), ' '));
}


/*
 * Insert a newline. Bound to "C-M".
 */
newline(f, n)
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l)){
	    if(curwp->w_doto != 0)
	      l++;
	    scrolldown(curwp, l, n);
	}
    }

    /* if we are in C mode and this is a default <NL> */
    /* pico's never in C mode */

    /* insert some lines */
    while (n--) {
	if ((s=lnewline()) != TRUE)
	  return (s);
    }
    return (TRUE);
}



/*
 * Delete forward. This is real easy, because the basic delete routine does
 * all of the work. Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument. Normally bound to "C-D".
 */
forwdel(f, n)
{
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (backdel(f, -n));

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && curwp->w_doto == llength(curwp->w_dotp))
	  scrollup(curwp, l+1, 1);
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();
	thisflag |= CFKILL;
    }

    return (ldelete(n, f));
}



/*
 * Delete backwards. This is quite easy too, because it's all done with other
 * functions. Just move the cursor back, and delete forwards. Like delete
 * forward, this actually does a kill if presented with an argument. Bound to
 * both "RUBOUT" and "C-H".
 */
backdel(f, n)
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (forwdel(f, -n));

    if(optimize && curwp->w_dotp != curwp->w_bufp->b_linep){
	int l;
	
	if(worthit(&l) && curwp->w_doto == 0 &&
	   lback(curwp->w_dotp) != curwp->w_bufp->b_linep){
	    if(l == curwp->w_toprow)
	      scrollup(curwp, l+1, 1);
	    else if(llength(lback(curwp->w_dotp)) == 0)
	      scrollup(curwp, l-1, 1);
	    else
	      scrollup(curwp, l, 1);
	}
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();

	thisflag |= CFKILL;
    }

    if ((s=backchar(f, n)) == TRUE)
      s = ldelete(n, f);

    return (s);
}



/*
 * killtext - delete the line that the cursor is currently in.
 *	      a greatly pared down version of its former self.
 */
killtext(f, n)
int f, n;
{
    register int	chunk;

    if (curbp->b_mode&MDVIEW)       /* don't allow this command if  */
      return(rdonly());       /* we are in read only mode     */

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;
	
	if(worthit(&l))
	  scrollup(curwp, l, 1);
    }

    if ((lastflag&CFKILL) == 0)             /* Clear kill buffer if */
      kdelete();                      /* last wasn't a kill.  */

    thisflag |= CFKILL;
    curwp->w_doto = 0;		/* wack from the beginning of line */
    chunk = llength(curwp->w_dotp) + 1;	/* for the whole length. */
    return(ldelete(chunk, TRUE));
}


/*
 * Yank text back from the kill buffer. This is really easy. All of the work
 * is done by the standard insert routines. All you do is run the loop, and
 * check for errors. Bound to "C-Y".
 */
yank(f, n)
int f, n;
{
    register int    c;
    register int    i;
    extern   int    kused;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && !(lastflag&CFFILL)){
	    register int  t = 0; 
	    register int  i = 0;
	    register int  ch;

	    while((ch=kremove(i++)) >= 0)
	      if(ch == '\n')
		t++;
	    if(t+l < curwp->w_toprow+curwp->w_ntrows)
	      scrolldown(curwp, l, t);
	}
    }

    if(lastflag&CFFILL)		/* if last command was fillpara() */
      gotobop(FALSE, 1);		/* then go to the top of the para */
					/* then splat out the saved buffer */
    while (n--) {
	i = 0;
	while ((c=kremove(i)) >= 0) {
	    if (c == '\n') {
		if (lnewline(FALSE, 1) == FALSE)
		  return (FALSE);
	    } else {
		if (linsert(1, c) == FALSE)
		  return (FALSE);
	    }

	    ++i;
	}
    }

    if(lastflag&CFFILL){            /* if last command was fillpara() */
	register LINE *botline;     /* blast the filled paragraph */
	register LINE *topline;
	register int  done = 0;
	
	kdelete();
	topline = curwp->w_dotp;
	gotoeop(FALSE, 1);
	botline = lforw(curwp->w_dotp);
	curwp->w_dotp = topline;
	if(topline != botline){
	    while(!done){
		if(lforw(curwp->w_dotp) == botline)
		  done++;
		curwp->w_doto = 0;
		ldelete((llength(curwp->w_dotp) + 1), FALSE);
	    }
	}
	curwp->w_flag |= WFMODE;
	
	if(Pmaster == NULL){
	    sgarbk = TRUE;
	    emlwrite("");
	}
    }

    return (TRUE);
}
