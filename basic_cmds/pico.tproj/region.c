#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: region.c,v 1.3 2003/01/12 01:48:12 bbraun Exp $";
#endif
/*
 * Program:	Region management routines
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
 * The routines in this file
 * deal with the region, that magic space
 * between "." and mark. Some functions are
 * commands. Some functions are just for
 * internal use.
 */
#include        <stdio.h>
#include	"estruct.h"
#include	"pico.h"
#include        "edef.h"

/*
 * Kill the region. Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 */
killregion(f, n)
{
    REGION          region;
    static long     times = 1L;
    static long     backoff = 4L;

    if (curbp->b_mode&MDVIEW)		/* don't allow this command if	*/
      return(rdonly());			/* we are in read only mode	*/

    if (getregion(&region) != TRUE){
	if((lastflag&CFKILL) == 0){
	    emlwrite("Line Deleted.%s", !(times % backoff) ? 
		     " (Could also use ctrl-^ to mark text for cutting)" : "");

	    if(!(times++ % backoff)) 	/* if message shown, reset backoff */
	      backoff = backoff << 1;
	}
	
	return (killtext(f, n));
    }else {
	mlerase();
    }

    if ((lastflag&CFKILL) == 0)		/* This is a kill type  */
      kdelete();			/* command, so do magic */

    thisflag |= CFKILL;			/* kill buffer stuff.   */
    curwp->w_dotp = region.r_linep;
    curwp->w_doto = region.r_offset;
    curwp->w_markp = NULL;
#ifdef	_WINDOWS
    mswin_allowcopycut(NULL);
#endif

    if(ldelete((int)region.r_size, TRUE)){
	if(curwp->w_dotp == curwp->w_linep && curwp->w_dotp == curbp->b_linep){
	    curwp->w_force = 0;		/* Center dot. */
	    curwp->w_flag |= WFFORCE;
	}

	return(TRUE);
    }

    return (FALSE);
}


/*
 * Copy all of the characters in the
 * region to the kill buffer. Don't move dot
 * at all. This is a bit like a kill region followed
 * by a yank. Bound to "M-W".
 */
copyregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    s;
    REGION          region;

    if ((s=getregion(&region)) != TRUE)
      return (s);

    if ((lastflag&CFKILL) == 0)		/* Kill type command.   */
      kdelete();

    thisflag |= CFKILL;
    linep = region.r_linep;		/* Current line.        */
    loffs = region.r_offset;		/* Current offset.      */
    while (region.r_size--) {
	if (loffs == llength(linep)) {  /* End of line.         */
	    if ((s=kinsert('\n')) != TRUE)
	      return (s);
	    linep = lforw(linep);
	    loffs = 0;
	} else {                        /* Middle of line.      */
	    if ((s=kinsert(lgetc(linep, loffs).c)) != TRUE)
	      return (s);
	    ++loffs;
	}
    }

    return (TRUE);
}


/*
 * Lower case region. Zap all of the upper
 * case characters in the region to lower case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
lowerregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;
    CELL            ac;

    ac.a = 0;
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if ((s=getregion(&region)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    c = lgetc(linep, loffs).c;
	    if (c>='A' && c<='Z'){
		ac.c = c+'a'-'A';
		lputc(linep, loffs, ac);
	    }
	    ++loffs;
	}
    }

    return (TRUE);
}

/*
 * Upper case region. Zap all of the lower
 * case characters in the region to upper case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
upperregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;
    CELL            ac;

    ac.a = 0;
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if ((s=getregion(&region)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    c = lgetc(linep, loffs).c;
	    if (c>='a' && c<='z'){
		ac.c = c - 'a' + 'A';
		lputc(linep, loffs, ac);
	    }
	    ++loffs;
	}
    }

    return (TRUE);
}

/*
 * This routine figures out the
 * bounds of the region in the current window, and
 * fills in the fields of the "REGION" structure pointed
 * to by "rp". Because the dot and mark are usually very
 * close together, we scan outward from dot looking for
 * mark. This should save time. Return a standard code.
 * Callers of this routine should be prepared to get
 * an "ABORT" status; we might make this have the
 * conform thing later.
 */
getregion(rp)
register REGION *rp;
{
    register LINE   *flp;
    register LINE   *blp;
    int    fsize;
    register int    bsize;

    if (curwp->w_markp == NULL) {
	return (FALSE);
    }

    if (curwp->w_dotp == curwp->w_markp) {
	rp->r_linep = curwp->w_dotp;
	if (curwp->w_doto < curwp->w_marko) {
	    rp->r_offset = curwp->w_doto;
	    rp->r_size = curwp->w_marko-curwp->w_doto;
	} else {
	    rp->r_offset = curwp->w_marko;
	    rp->r_size = curwp->w_doto-curwp->w_marko;
	}
	return (TRUE);
    }

    blp = curwp->w_dotp;
    bsize = curwp->w_doto;
    flp = curwp->w_dotp;
    fsize = llength(flp)-curwp->w_doto+1;
    while (flp!=curbp->b_linep || lback(blp)!=curbp->b_linep) {
	if (flp != curbp->b_linep) {
	    flp = lforw(flp);
	    if (flp == curwp->w_markp) {
		rp->r_linep = curwp->w_dotp;
		rp->r_offset = curwp->w_doto;
		rp->r_size = fsize+curwp->w_marko;
		return (TRUE);
	    }

	    fsize += llength(flp)+1;
	}

	if (lback(blp) != curbp->b_linep) {
	    blp = lback(blp);
	    bsize += llength(blp)+1;
	    if (blp == curwp->w_markp) {
		rp->r_linep = blp;
		rp->r_offset = curwp->w_marko;
		rp->r_size = bsize - curwp->w_marko;
		return (TRUE);
	    }
	}
    }

    emlwrite("Bug: lost mark", NULL);
    return (FALSE);
}


/*
 * set the highlight attribute accordingly on all characters in region
 */
markregion(attr)
    int attr;
{
    register LINE   *linep;
    register int    loffs;
    register int    s;
    REGION          region;
    CELL            ac;

    if ((s=getregion(&region)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    ac = lgetc(linep, loffs);
	    ac.a = attr;
	    lputc(linep, loffs, ac);
	    ++loffs;
	}
    }

    return (TRUE);
}


/*
 * clear all the attributes of all the characters in the buffer?
 * this is real dumb.  Movement with mark set needs to be smarter!
 */
unmarkbuffer()
{
    register LINE   *linep;
    register int     n;
    CELL c;

    linep = curwp->w_linep;
    while(lforw(linep) != curwp->w_linep){
	n = llength(linep);
	for(n=0; n < llength(linep); n++){
	    c = lgetc(linep, n);
	    c.a = 0;
	    lputc(linep, n, c);
	}

	linep = lforw(linep);
    }
    
}
