#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: basic.c,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $";
#endif
/*
 * Program:	Cursor manipulation functions
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
 */
/*
 * The routines in this file move the cursor around on the screen. They
 * compute a new value for the cursor, then adjust ".". The display code
 * always updates the cursor location, so only moves between lines, or
 * functions that adjust the top line in the window and invalidate the
 * framing, are hard.
 */
#include        <stdio.h>
#include	"estruct.h"
#include        "edef.h"
#include        "pico.h"


#ifdef	ANSI
    int getgoal(struct LINE *);
#else
    int getgoal();
#endif


/*
 * Move the cursor to the
 * beginning of the current line.
 * Trivial.
 */
gotobol(f, n)
int f, n;
{
    curwp->w_doto  = 0;
    return (TRUE);
}

/*
 * Move the cursor backwards by "n" characters. If "n" is less than zero call
 * "forwchar" to actually do the move. Otherwise compute the new cursor
 * location. Error if you try and move out of the buffer. Set the flag if the
 * line pointer for dot changes.
 */
backchar(f, n)
int             f;
register int    n;
{
    register LINE   *lp;
    register int    status;

    if (n < 0)
      return (forwchar(f, -n));

    while (n--) {
	if (curwp->w_doto == 0) {
	    if ((lp=lback(curwp->w_dotp)) == curbp->b_linep)
	      if(Pmaster){
		  /*
		   * go up into editing the mail header if on 
		   * the top line and the user hits the left arrow!!!
		   *
		   * if the editor returns anything except -1, the 
		   * user requested something special, so let 
		   * pico know...
		   */
		  if((status = HeaderEditor(2, 1)) != -1){
		      return(META|status);
		  }
		  else
		    return (FALSE);
	      }
	      else
		return (FALSE);

	    curwp->w_dotp  = lp;
	    curwp->w_doto  = llength(lp);
	    curwp->w_flag |= WFMOVE;
	} else
	  curwp->w_doto--;
    }

    return (TRUE);
}


/*
 * Move the cursor to the end of the current line. Trivial. No errors.
 */
gotoeol(f, n)
int f, n;
{
    curwp->w_doto  = llength(curwp->w_dotp);
    return (TRUE);
}


/*
 * Move the cursor forwwards by "n" characters. If "n" is less than zero call
 * "backchar" to actually do the move. Otherwise compute the new cursor
 * location, and move ".". Error if you try and move off the end of the
 * buffer. Set the flag if the line pointer for dot changes.
 */
forwchar(f, n)
int             f;
register int    n;
{
    if (n < 0)
      return (backchar(f, -n));

    while (n--) {
	if (curwp->w_doto == llength(curwp->w_dotp)) {
	    if (curwp->w_dotp == curbp->b_linep)
	      return (FALSE);

	    curwp->w_dotp  = lforw(curwp->w_dotp);
	    curwp->w_doto  = 0;
	    curwp->w_flag |= WFMOVE;
	}
	else
	  curwp->w_doto++;
    }
    
    return (TRUE);
}


/*
 * move to a particular line.
 * argument (n) must be a positive integer for
 * this to actually do anything
 */
gotoline(f, n)
int f, n;
{
    if (n < 1)		/* if a bogus argument...then leave */
      return(FALSE);

    /* first, we go to the start of the buffer */
    curwp->w_dotp  = lforw(curbp->b_linep);
    curwp->w_doto  = 0;
    return(forwline(f, n-1));
}


/*
 * Goto the beginning of the buffer. Massive adjustment of dot. This is
 * considered to be hard motion; it really isn't if the original value of dot
 * is the same as the new value of dot. Normally bound to "M-<".
 */
gotobob(f, n)
int f, n;
{
    curwp->w_dotp  = lforw(curbp->b_linep);
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    return (TRUE);
}


/*
 * Move to the end of the buffer. Dot is always put at the end of the file
 * (ZJ). The standard screen code does most of the hard parts of update.
 * Bound to "M->".
 */
gotoeob(f, n)
int f, n;
{
    curwp->w_dotp  = curbp->b_linep;
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    return (TRUE);
}


/*
 * Move forward by full lines. If the number of lines to move is less than
 * zero, call the backward line function to actually do it. The last command
 * controls how the goal column is set. Bound to "C-N". No errors are
 * possible.
 */
forwline(f, n)
int f, n;
{
    register LINE   *dlp;

    if (n < 0)
      return (backline(f, -n));

    if ((lastflag&CFCPCN) == 0)             /* Reset goal if last   */
      curgoal = getccol(FALSE);       /* not C-P or C-N       */

    thisflag |= CFCPCN;
    dlp = curwp->w_dotp;
    while (n-- && dlp!=curbp->b_linep)
      dlp = lforw(dlp);

    curwp->w_dotp  = dlp;
    curwp->w_doto  = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * This function is like "forwline", but goes backwards. The scheme is exactly
 * the same. Check for arguments that are less than zero and call your
 * alternate. Figure out the new line and call "movedot" to perform the
 * motion. No errors are possible. Bound to "C-P".
 */
backline(f, n)
int f, n;
{
    register LINE   *dlp;
    register int    status;

    if (n < 0)
      return (forwline(f, -n));

    if(Pmaster){
	/*
	 * go up into editing the mail header if on the top line
	 * and the user hits the up arrow!!!
	 */
	if (lback(curwp->w_dotp) == curbp->b_linep){
	    /*
	     * if the editor returns anything except -1 then the user
	     * has requested something special, so let pico know...
	     */
	    if((status = HeaderEditor(1, 1)) != -1){
		return(META|status);
	    }
	}
    }

    if ((lastflag&CFCPCN) == 0)             /* Reset goal if the    */
      curgoal = getccol(FALSE);       /* last isn't C-P, C-N  */

    thisflag |= CFCPCN;
    dlp = curwp->w_dotp;
    while (n-- && lback(dlp)!=curbp->b_linep)
      dlp = lback(dlp);

    curwp->w_dotp  = dlp;
    curwp->w_doto  = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * go back to the begining of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph	
 */
gotobop(f, n)
int f, n;	/* default Flag & Numeric argument */
{
    if (n < 0)	/* the other way...*/
      return(gotoeop(f, -n));

    while (n-- > 0) {	/* for each one asked for */

	/* first scan back until we are in a word */
	while(!inword())
	  if(lback(curwp->w_dotp) == curbp->b_linep 
	     && curwp->w_doto == 0)
	    /* top line and nowhere else to go */
	    return(FALSE);
	  else if(backchar(FALSE, 1) == FALSE)
	    break;

	curwp->w_doto = 0;	/* and go to the B-O-Line */

	/* and scan back until we hit a <NL><NL> or <NL><TAB>
	   or a <NL><SPACE>					*/
	while (lback(curwp->w_dotp) != curbp->b_linep)
	  if (llength(curwp->w_dotp) != 0 &&
	      lgetc(curwp->w_dotp, curwp->w_doto).c != TAB &&
	      lgetc(curwp->w_dotp, curwp->w_doto).c != ' ')
	    curwp->w_dotp = lback(curwp->w_dotp);
	  else
	    break;

	/* and then forward until we are in a word */
	while(!inword())
	  if(forwchar(FALSE,1) == FALSE)
	    break;
    }

    curwp->w_flag |= WFMOVE;	/* force screen update */
}


/* 
 * go forword to the end of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph
 */
gotoeop(f, n)
int f, n;	/* default Flag & Numeric argument */

{
    register int suc;	/* success of last backchar */

    if (n < 0)	/* the other way...*/
      return(gotobop(f, -n));

    while (n-- > 0) {	/* for each one asked for */
	
	/* first scan forward until we are in a word */
	if(curwp->w_dotp != curbp->b_linep){
	    curwp->w_doto = 0;		/* go to the B-O-Line */
	    curwp->w_dotp = lforw(curwp->w_dotp);
	}

	/* and scan forword until we hit a <NL><NL> or <NL><TAB>
	   or a <NL><SPACE>					*/
	while (curwp->w_dotp != curbp->b_linep) {
	    if (llength(curwp->w_dotp) != 0 &&
		lgetc(curwp->w_dotp, curwp->w_doto).c != TAB &&
		lgetc(curwp->w_dotp, curwp->w_doto).c != ' ')
	      curwp->w_dotp = lforw(curwp->w_dotp);
	    else
	      break;
	}
	
	/* and then backward until we are in a word */
	suc = TRUE;
	while (suc && !inword()) {
	    if(lback(curwp->w_dotp) == curbp->b_linep 
	       && curwp->w_doto == 0)
	      break;
	    suc = backchar(FALSE, 1);
	}
	curwp->w_doto = llength(curwp->w_dotp);	/* and to the EOL */
    }

    curwp->w_flag |= WFMOVE;	/* force screen update */
    return(TRUE);
}

/*
 * This routine, given a pointer to a LINE, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
getgoal(dlp)
register LINE   *dlp;
{
    register int    c;
    register int    col;
    register int    newcol;
    register int    dbo;

    col = 0;
    dbo = 0;
    while (dbo != llength(dlp)) {
	c = lgetc(dlp, dbo).c;
	newcol = col;
	if (c == '\t')
	  newcol |= 0x07;
	else if (c<0x20 || c==0x7F)
	  ++newcol;

	++newcol;
	if (newcol > curgoal)
	  break;

	col = newcol;
	++dbo;
    }

    return (dbo);
}

/*
 * Scroll forward by a specified number of lines, or by a full page if no
 * argument. Bound to "C-V". The "2" in the arithmetic on the window size is
 * the overlap; this value is the default overlap value in ITS EMACS. Because
 * this zaps the top line in the display window, we have to do a hard update.
 */
forwpage(f, n)
int             f;
register int    n;
{
    register LINE   *lp;
    register int    nl;

    if (f == FALSE) {
	n = curwp->w_ntrows - 2;        /* Default scroll.      */
	if (n <= 0)                     /* Forget the overlap   */
	  n = 1;                  /* if tiny window.      */
    } else if (n < 0)
      return (backpage(f, -n));
#if     CVMVAS
    else                                    /* Convert from pages   */
      n *= curwp->w_ntrows;           /* to lines.            */
#endif
    nl = n;
    lp = curwp->w_linep;
    while (n-- && lp!=curbp->b_linep)
      lp = lforw(lp);

    curwp->w_dotp  = lp;
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    if(lp == curbp->b_linep)
      return(TRUE);
    else
      curwp->w_linep = lp;

    /*
     * if the header is open, close it ...
     */
    if(Pmaster && ComposerTopLine != COMPOSER_TOP_LINE){
	n -= ComposerTopLine - COMPOSER_TOP_LINE;
	ToggleHeader(0);
    }

    /*
     * scroll down from the top the same number of lines we've moved 
     * forward
     */
    if(optimize)
      scrollup(curwp, -1, nl-n-1);

    return (TRUE);
}


/*
 * This command is like "forwpage", but it goes backwards. The "2", like
 * above, is the overlap between the two windows. The value is from the ITS
 * EMACS manual. Bound to "M-V". We do a hard update for exactly the same
 * reason.
 */
backpage(f, n)
int             f;
register int    n;
{
    register LINE   *lp;
    register int    nl;

    if (f == FALSE) {
	n = curwp->w_ntrows - 2;        /* Default scroll.      */
	if (n <= 0)                     /* Don't blow up if the */
	  n = 1;                  /* window is tiny.      */
    } else if (n < 0)
      return (forwpage(f, -n));
#if     CVMVAS
    else                                    /* Convert from pages   */
      n *= curwp->w_ntrows;           /* to lines.            */
#endif
    nl = n;
    lp = curwp->w_linep;
    while (n-- && lback(lp)!=curbp->b_linep)
      lp = lback(lp);

    curwp->w_linep = lp;
    curwp->w_dotp  = lp;
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;

	/*
	 * scroll down from the top the same number of lines we've moved 
	 * forward
	 *
	 * This isn't too cool, but it has to be this way so we can 
	 * gracefully scroll in the message header
	 */
    if(Pmaster){
	if((lback(lp)==curbp->b_linep) && (ComposerTopLine==COMPOSER_TOP_LINE))
	  n -= entry_line(LASTHDR, TRUE);
	if(nl-n-1 < curwp->w_ntrows)
	  if(optimize)
	    scrolldown(curwp, -1, nl-n-1);
    }
    else
      if(optimize)
	scrolldown(curwp, -1, nl-n-1);

    if(Pmaster){
	/*
	 * if we're at the top of the page, and the header is closed, 
	 * open it ...
	 */
	if((lback(lp) == curbp->b_linep) 
	   && (ComposerTopLine == COMPOSER_TOP_LINE)){
	    ToggleHeader(1);
	    movecursor(ComposerTopLine, 0);
	}
    }

    return (TRUE);
}


/*
 * Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".  If told to set an already set mark
 * unset it.
 */
setmark(f, n)
int f, n;
{
    if(!curwp->w_markp){
        curwp->w_markp = curwp->w_dotp;
        curwp->w_marko = curwp->w_doto;
	emlwrite("Mark Set", NULL);
    }
    else{
	/* clear inverse chars between here and dot */
	markregion(0);
	curwp->w_markp = NULL;
	emlwrite("Mark UNset", NULL);
    }

    return (TRUE);
}


/*
 * Swap the values of "." and "mark" in the current window. This is pretty
 * easy, bacause all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
swapmark(f, n)
int f, n;
{
    register LINE   *odotp;
    register int    odoto;

    if (curwp->w_markp == NULL) {
	if(Pmaster == NULL)
	  mlwrite("No mark in this window");
	return (FALSE);
    }

    odotp = curwp->w_dotp;
    odoto = curwp->w_doto;
    curwp->w_dotp  = curwp->w_markp;
    curwp->w_doto  = curwp->w_marko;
    curwp->w_markp = odotp;
    curwp->w_marko = odoto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".  If told to set an already set mark
 * unset it.
 */
setimark(f, n)
int f, n;
{
    curwp->w_imarkp = curwp->w_dotp;
    curwp->w_imarko = curwp->w_doto;
    return(TRUE);
}


/*
 * Swap the values of "." and "mark" in the current window. This is pretty
 * easy, bacause all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
swapimark(f, n)
int f, n;
{
    register LINE   *odotp;
    register int    odoto;

    if (curwp->w_imarkp == NULL) {
	if(Pmaster == NULL)
	  emlwrite("Programmer botch! No mark in this window");
	return (FALSE);
    }

    odotp = curwp->w_dotp;
    odoto = curwp->w_doto;
    curwp->w_dotp  = curwp->w_imarkp;
    curwp->w_doto  = curwp->w_imarko;
    curwp->w_imarkp = odotp;
    curwp->w_imarko = odoto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}
