#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: display.c,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $";
#endif
/*
 * Program:	Display functions
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
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 */

#include        <stdio.h>
#include	"osdep.h"
#include        "pico.h"
#include	"estruct.h"
#include        "edef.h"
#include        "efunc.h"


#ifdef	ANSI
    int vtmove(int, int);
    int vtputc(CELL);
    int vtpute(CELL);
    int vteeol(void);
    int updateline(int, CELL *, CELL *, short *);
    int updext(void);
    int mlputi(int, int);
    int mlputli(long, int);
    int showCompTitle(void);
    int nlforw(void);
    int dumbroot(int, int);
    int dumblroot(long, int);
#else
    int vtmove();
    int vtputc();
    int vtpute();
    int vteeol();
    int updateline();
    int updext();
    int mlputi();
    int mlputli();
    int showCompTitle();
    int nlforw();
    int dumbroot();
    int dumblroot();
#endif


#define WFDEBUG 0                       /* Window flag debug. */

typedef struct  VIDEO {
        short   v_flag;                 /* Flags */
        CELL    v_text[1];              /* Screen data. */
}       VIDEO;

#define VFCHG   0x0001                  /* Changed flag			*/
#define	VFEXT	0x0002			/* extended (beyond column 80)	*/
#define	VFREV	0x0004			/* reverse video status		*/
#define	VFREQ	0x0008			/* reverse video request	*/

int     vtrow   = 0;                    /* Row location of SW cursor */
int     vtcol   = 0;                    /* Column location of SW cursor */
int     ttrow   = HUGE;                 /* Row location of HW cursor */
int     ttcol   = HUGE;                 /* Column location of HW cursor */
int	lbound	= 0;			/* leftmost column of current line
					   being displayed */

VIDEO   **vscreen;                      /* Virtual screen. */
VIDEO   **pscreen;                      /* Physical screen. */


/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up. The operating system's terminal I/O
 * channel is set up. All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get completely
 * redrawn on the first call to "update".
 */
vtinit()
{
    register int i;
    register VIDEO *vp;

    (*term.t_open)();

    (*term.t_rev)(FALSE);
    vscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    if (vscreen == NULL){
	emlwrite("Allocating memory for virtual display failed.", NULL);
        return(FALSE);
    }


    pscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    if (pscreen == NULL){
	free((void *)vscreen);
	emlwrite("Allocating memory for physical display failed.", NULL);
        return(FALSE);
    }


    for (i = 0; i <= term.t_nrow; ++i) {
        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for virtual display lines failed.",
		     NULL);
            return(FALSE);
	}

	vp->v_flag = 0;
        vscreen[i] = vp;
        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
            free((void *)vscreen[i]);
	    while(--i >= 0){
		free((void *)vscreen[i]);
		free((void *)pscreen[i]);
	    }

	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for physical display lines failed.",
		     NULL);
            return(FALSE);
	}

	vp->v_flag = 0;
        pscreen[i] = vp;
    }

    return(TRUE);
}


/*
 * Clean up the virtual terminal system, in anticipation for a return to the
 * operating system. Move down to the last line and clear it out (the next
 * system prompt will be written in the line). Shut down the channel to the
 * terminal.
 */
vttidy()
{
    movecursor(term.t_nrow-1, 0);
    peeol();
    movecursor(term.t_nrow, 0);
    peeol();
    (*term.t_close)();
}


/*
 * Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values; this might be a good
 * idea during the early stages.
 */
vtmove(row, col)
int row, col;
{
    vtrow = row;
    vtcol = col;
}


/*
 * Write a character to the virtual screen. The virtual row and column are
 * updated. If the line is too long put a "$" in the last column. This routine
 * only puts printing characters into the virtual terminal buffers. Only
 * column overflow is checked.
 */
vtputc(c)
CELL c;
{
    register VIDEO      *vp;
    CELL     ac;

    vp = vscreen[vtrow];
    ac.c = ' ';
    ac.a = c.a;

    if (vtcol >= term.t_ncol) {
        vtcol = (vtcol + 0x07) & ~0x07;
	ac.c = '$';
        vp->v_text[term.t_ncol - 1] = ac;
    }
    else if (c.c == '\t') {
        do {
            vtputc(ac);
	}
        while ((vtcol&0x07) != 0);
    }
    else if (c.c < 0x20 || c.c == 0x7F) {
	ac.c = '^';
        vtputc(ac);
	ac.c = (c.c ^ 0x40);
        vtputc(ac);
    }
    else
      vp->v_text[vtcol++] = c;
}


/* put a character to the virtual screen in an extended line. If we are
 * not yet on left edge, don't print it yet. check for overflow on
 * the right margin.
 */
vtpute(c)
CELL c;
{
    register VIDEO      *vp;
    CELL                 ac;

    vp = vscreen[vtrow];
    ac.c = ' ';
    ac.a = c.a;

    if (vtcol >= term.t_ncol) {
        vtcol = (vtcol + 0x07) & ~0x07;
	ac.c = '$';
        vp->v_text[term.t_ncol - 1] = ac;
    }
    else if (c.c == '\t'){
        do {
            vtpute(ac);
        }
        while (((vtcol + lbound)&0x07) != 0 && vtcol < term.t_ncol);
    }
    else if (c.c < 0x20 || c.c == 0x7F) {
	ac.c = '^';
        vtpute(ac);
	ac.c = (c.c ^ 0x40);
        vtpute(ac);
    }
    else {
	if (vtcol >= 0)
	  vp->v_text[vtcol] = c;
	++vtcol;
    }
}


/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
vteeol()
{
    register VIDEO      *vp;
    CELL     c;

    c.c = ' ';
    c.a = 0;
    vp = vscreen[vtrow];
    while (vtcol < term.t_ncol)
      vp->v_text[vtcol++] = c;
}


/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 */
update()
{
    register LINE   *lp;
    register WINDOW *wp;
    register VIDEO  *vp1;
    register VIDEO  *vp2;
    register int     i;
    register int     j;
    register int     scroll = 0;
    CELL	     c;

#if	TYPEAH
    if (typahead())
	return(TRUE);
#endif

/*
 * BUG: setting and unsetting whole region at a time is dumb.  fix this.
 */
    if(curwp->w_markp){
	unmarkbuffer();
	markregion(1);
    }

    wp = wheadp;

    while (wp != NULL){
        /* Look at any window with update flags set on. */

        if (wp->w_flag != 0){
            /* If not force reframe, check the framing. */

            if ((wp->w_flag & WFFORCE) == 0){
                lp = wp->w_linep;

                for (i = 0; i < wp->w_ntrows; ++i){
                    if (lp == wp->w_dotp)
		      goto out;

                    if (lp == wp->w_bufp->b_linep)
		      break;

                    lp = lforw(lp);
		}
	    }

            /* Not acceptable, better compute a new value for the line at the
             * top of the window. Then set the "WFHARD" flag to force full
             * redraw.
             */
            i = wp->w_force;

            if (i > 0){
                --i;

                if (i >= wp->w_ntrows)
                  i = wp->w_ntrows-1;
	    }
            else if (i < 0){
                i += wp->w_ntrows;

                if (i < 0)
		  i = 0;
	    }
            else if(optimize){
		/* 
		 * find dotp, if its been moved just above or below the 
		 * window, use scrollxxx() to facilitate quick redisplay...
		 */
		lp = lforw(wp->w_dotp);
		if(lp != wp->w_dotp){
		    if(lp == wp->w_linep && lp != wp->w_bufp->b_linep){
			scroll = 1;
		    }
		    else {
			lp = wp->w_linep;
			for(j=0;j < wp->w_ntrows; ++j){
			    if(lp != wp->w_bufp->b_linep)
			      lp = lforw(lp);
			    else
			      break;
			}
			if(lp == wp->w_dotp && j == wp->w_ntrows)
			  scroll = 2;
		    }
		}
		j = i = wp->w_ntrows/2;
	    }
	    else
	      i = wp->w_ntrows/2;

            lp = wp->w_dotp;

            while (i != 0 && lback(lp) != wp->w_bufp->b_linep){
                --i;
                lp = lback(lp);
	    }

	    /*
	     * this is supposed to speed things up by using tcap sequences
	     * to efficiently scroll the terminal screen.  the thinking here
	     * is that its much faster to update pscreen[] than to actually
	     * write the stuff to the screen...
	     */
	    if(optimize){
		switch(scroll){
		  case 1:			/* scroll text down */
		    j = j-i+1;			/* add one for dot line */
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
		    if(Pmaster && !ComposerEditing 
		       && (lback(lp) == wp->w_bufp->b_linep)
		       && (ComposerTopLine == COMPOSER_TOP_LINE))
		      j += entry_line(LASTHDR, TRUE);

		    scrolldown(wp, -1, j);
		    break;
		  case 2:			/* scroll text up */
		    j = wp->w_ntrows - (j-i);	/* we chose new top line! */
		    if(Pmaster && j){
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
			if(!ComposerEditing 
			   && (ComposerTopLine != COMPOSER_TOP_LINE))
			  scrollup(wp, COMPOSER_TOP_LINE, 
				   j+entry_line(LASTHDR, TRUE));
			else
			  scrollup(wp, -1, j);
		    }
		    else
		      scrollup(wp, -1, j);
		    break;
		    default :
		      break;
		}
	    }

            wp->w_linep = lp;
            wp->w_flag |= WFHARD;       /* Force full. */
out:
	    /*
	     * if the line at the top of the page is the top line
	     * in the body, show the header...
	     */
	    if(Pmaster && !ComposerEditing){
		if(lback(wp->w_linep) == wp->w_bufp->b_linep){
		    if(ComposerTopLine == COMPOSER_TOP_LINE){
			i = term.t_nrow - 4 - HeaderLen();
			if(i > 0 && nlforw() >= i) {	/* room for header ? */
			    if((i = nlforw()/2) == 0 && term.t_nrow&1)
			      i = 1;
			    while(wp->w_linep != wp->w_bufp->b_linep && i--)
			      wp->w_linep = lforw(wp->w_linep);
			    
			}
			else
			  ToggleHeader(1);
		    }
		}
		else{
		    if(ComposerTopLine != COMPOSER_TOP_LINE)
		      ToggleHeader(0);		/* hide it ! */
		}
	    }

            /* Try to use reduced update. Mode line update has its own special
             * flag. The fast update is used if the only thing to do is within
             * the line editing.
             */
            lp = wp->w_linep;
            i = wp->w_toprow;

            if ((wp->w_flag & ~WFMODE) == WFEDIT){
                while (lp != wp->w_dotp){
                    ++i;
                    lp = lforw(lp);
		}

                vscreen[i]->v_flag |= VFCHG;
                vtmove(i, 0);

                for (j = 0; j < llength(lp); ++j)
                    vtputc(lgetc(lp, j));

                vteeol();
	    }
	    else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0){
                while (i < wp->w_toprow+wp->w_ntrows){
                    vscreen[i]->v_flag |= VFCHG;
                    vtmove(i, 0);

		    /* if line has been changed */
                    if (lp != wp->w_bufp->b_linep){
                        for (j = 0; j < llength(lp); ++j)
                            vtputc(lgetc(lp, j));

                        lp = lforw(lp);
		    }

                    vteeol();
                    ++i;
		}
	    }
#if ~WFDEBUG
            if ((wp->w_flag&WFMODE) != 0)
                modeline(wp);

            wp->w_flag  = 0;
            wp->w_force = 0;
#endif
	}
#if WFDEBUG
        modeline(wp);
        wp->w_flag =  0;
        wp->w_force = 0;
#endif

	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Always recompute the row and column number of the hardware cursor. This
     * is the only update for simple moves.
     */
    lp = curwp->w_linep;
    currow = curwp->w_toprow;

    while (lp != curwp->w_dotp){
        ++currow;
        lp = lforw(lp);
    }

    curcol = 0;
    i = 0;

    while (i < curwp->w_doto){
	c = lgetc(lp, i++);

        if (c.c == '\t')
            curcol |= 0x07;
        else if (c.c < 0x20 || c.c == 0x7F)
            ++curcol;

        ++curcol;
    }

    if (curcol >= term.t_ncol - 1) { 		/* extended line. */
	/* flag we are extended and changed */
	vscreen[currow]->v_flag |= VFEXT | VFCHG;
	updext();				/* and output extended line */
    } else
      lbound = 0;				/* not extended line */

    /* make sure no lines need to be de-extended because the cursor is
     * no longer on them 
     */

    wp = wheadp;

    while (wp != NULL) {
	lp = wp->w_linep;
	i = wp->w_toprow;

	while (i < wp->w_toprow + wp->w_ntrows) {
	    if (vscreen[i]->v_flag & VFEXT) {
		/* always flag extended lines as changed */
		vscreen[i]->v_flag |= VFCHG;
		if ((wp != curwp) || (lp != wp->w_dotp) ||
		    (curcol < term.t_ncol - 1)) {
		    vtmove(i, 0);
		    for (j = 0; j < llength(lp); ++j)
		      vtputc(lgetc(lp, j));
		    vteeol();

		    /* this line no longer is extended */
		    vscreen[i]->v_flag &= ~VFEXT;
		}
	    }
	    lp = lforw(lp);
	    ++i;
	}
	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Special hacking if the screen is garbage. Clear the hardware screen,
     * and update your copy to agree with it. Set all the virtual screen
     * change bits, to force a full update.
     */

    if (sgarbf != FALSE){
	if(Pmaster){
	    showCompTitle();

	    if(ComposerTopLine != COMPOSER_TOP_LINE){
		UpdateHeader();			/* arrange things */
		PaintHeader(COMPOSER_TOP_LINE, TRUE);
	    }

	    /*
	     * since we're using only a portion of the screen and only 
	     * one buffer, only clear enough screen for the current window
	     * which is to say the *only* window.
	     */
	    for(i=wheadp->w_toprow;i<=term.t_nrow; i++){
		movecursor(i, 0);
		peeol();
		vscreen[i]->v_flag |= VFCHG;
	    }
	    movecursor(wheadp->w_toprow, 0);
	}
	else{
	    for (i = 0; i < term.t_nrow - 2; ++i){
		vscreen[i]->v_flag |= VFCHG;
		vp1 = pscreen[i];
		c.c = ' ';
		c.a = 0;
		for (j = 0; j < term.t_ncol; ++j)
		  vp1->v_text[j] = c;
	    }

	    movecursor(0, 0);	               /* Erase the screen. */
	    (*term.t_eeop)();

	}

        sgarbf = FALSE;				/* Erase-page clears */
        mpresf = FALSE;				/* the message area. */

	if(Pmaster)
	  modeline(curwp);
	else
	  sgarbk = TRUE;			/* fix the keyhelp as well...*/
    }

    /* Make sure that the physical and virtual displays agree. Unlike before,
     * the "updateline" code is only called with a line that has been updated
     * for sure.
     */
    if(Pmaster){
	i = curwp->w_toprow;
	c.c = term.t_nrow-2;
    }
    else{
	i = 0;
	c.c = term.t_nrow;
    }

    for (; i < (int)c.c; ++i){

        vp1 = vscreen[i];

	/* for each line that needs to be updated, or that needs its
	   reverse video status changed, call the line updater	*/
	j = vp1->v_flag;
        if (j & VFCHG){

#if	TYPEAH
	    if (typahead())
	        return(TRUE);
#endif
            vp2 = pscreen[i];

            updateline(i, &vp1->v_text[0], &vp2->v_text[0], &vp1->v_flag);

	}
    }

    if(Pmaster == NULL){

	if(sgarbk != FALSE){
	    movecursor(term.t_nrow-1, 0);
	    peeol();
	    movecursor(term.t_nrow, 0);
	    peeol();
	    if(lastflag&CFFILL){
#ifdef	SPELLER
		wkeyhelp("GORYKCXJWVUT",
		         "Get Help,WriteOut,Read File,Prev Pg,Cut Text,Cur Pos,Exit,Justify,Where is,Next Pg,UnJustify,To Spell");

#else
		wkeyhelp("GORYKCXJWVUD",
                         "Get Help,WriteOut,Read File,Prev Pg,Cut Text,Cur Pos,Exit,Justify,Where is,Next Pg,UnJustify,Del Char");

#endif
		emlwrite("Can now UnJustify!", NULL);
		mpresf = HUGE;	/* remove this after next keystroke! */
	    }
	    else{
#ifdef SPELLER
		wkeyhelp("GORYKCXJWVUT",
		         "Get Help,WriteOut,Read File,Prev Pg,Cut Text,Cur Pos,Exit,Justify,Where is,Next Pg,UnCut Text,To Spell");
#else
		wkeyhelp("GORYKCXJWVUD",
		         "Get Help,WriteOut,Read File,Prev Pg,Cut Text,Cur Pos,Exit,Justify,Where is,Next Pg,UnCut Text,Del Char");

#endif
	    }
	    sgarbk = FALSE;
        }
    }

    /* Finally, update the hardware cursor and flush out buffers. */

    movecursor(currow, curcol - lbound);
    (*term.t_flush)();
}

/* updext - update the extended line which the cursor is currently
 *	    on at a column greater than the terminal width. The line
 *	    will be scrolled right or left to let the user see where
 *	    the cursor is
 */
updext()
{
    register int rcursor;		/* real cursor location */
    register LINE *lp;			/* pointer to current line */
    register int j;			/* index into line */

    /* calculate what column the real cursor will end up in */
    rcursor = ((curcol - term.t_ncol) % term.t_scrsiz) + term.t_margin;
    lbound = curcol - rcursor + 1;

    /* scan through the line outputing characters to the virtual screen
     * once we reach the left edge
     */
    vtmove(currow, -lbound);		/* start scanning offscreen */
    lp = curwp->w_dotp;			/* line to output */
    for (j=0; j<llength(lp); ++j)	/* until the end-of-line */
      vtpute(lgetc(lp, j));

    /* truncate the virtual line */
    vteeol();

    /* and put a '$' in column 1 */
    vscreen[currow]->v_text[0].c = '$';
    vscreen[currow]->v_text[0].a = 0;
}


/*
 * Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables. It does try an exploit erase to end of line. The
 * RAINBOW version of this routine uses fast video.
 */
updateline(row, vline, pline, flags)
int  row;
CELL vline[];				/* what we want it to end up as */
CELL pline[];				/* what it looks like now       */
short *flags;				/* and how we want it that way  */
{
    register CELL *cp1;
    register CELL *cp2;
    register CELL *cp3;
    register CELL *cp4;
    register CELL *cp5;
    register CELL *cp6;
    register CELL *cp7;
    register int  display = TRUE;
    register int nbflag;		/* non-blanks to the right flag? */


    /* set up pointers to virtual and physical lines */
    cp1 = &vline[0];
    cp2 = &pline[0];
    cp3 = &vline[term.t_ncol];

    /* advance past any common chars at the left */
    while (cp1 != cp3 && cp1[0].c == cp2[0].c && cp1[0].a == cp2[0].a) {
	++cp1;
	++cp2;
    }

/* This can still happen, even though we only call this routine on changed
 * lines. A hard update is always done when a line splits, a massive
 * change is done, or a buffer is displayed twice. This optimizes out most
 * of the excess updating. A lot of computes are used, but these tend to
 * be hard operations that do a lot of update, so I don't really care.
 */
    /* if both lines are the same, no update needs to be done */
    if (cp1 == cp3){
	*flags &= ~VFCHG;			/* mark it clean */
	return(TRUE);
    }

    /* find out if there is a match on the right */
    nbflag = FALSE;
    cp3 = &vline[term.t_ncol];
    cp4 = &pline[term.t_ncol];

    while (cp3[-1].c == cp4[-1].c && cp3[-1].a == cp4[-1].a) {
	--cp3;
	--cp4;
	if (cp3[0].c != ' ' || cp3[0].a != 0)/* Note if any nonblank */
	  nbflag = TRUE;		/* in right match. */
    }

    cp5 = cp3;

    if (nbflag == FALSE && eolexist == TRUE) {	/* Erase to EOL ? */
	while (cp5 != cp1 && cp5[-1].c == ' ' && cp5[-1].a == 0)
	  --cp5;

	if (cp3-cp5 <= 3)		/* Use only if erase is */
	  cp5 = cp3;			/* fewer characters. */
    }

    movecursor(row, cp1-&vline[0]);		/* Go to start of line. */

    if (!nbflag) {				/* use insert or del char? */
	cp6 = cp3;
	cp7 = cp4;

	if(inschar&&(cp7!=cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a)){
	    while (cp7 != cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp7==cp2 && cp4-cp2 > 3){
		o_insert((char)cp1->c);     /* insert the char */
		display = FALSE;        /* only do it once!! */
	    }
	}
	else if(delchar && cp3 != cp1 && cp7[0].c == cp6[-1].c
		&& cp7[0].a == cp6[-1].a){
	    while (cp6 != cp1 && cp7[0].c==cp6[-1].c && cp7[0].a==cp6[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp6==cp1 && cp5-cp6 > 3){
		o_delete();		/* insert the char */
		display = FALSE;        /* only do it once!! */
	    }
	}
    }

    while (cp1 != cp5) {		/* Ordinary. */
	if(display){
	    (*term.t_rev)(cp1->a);	/* set inverse for this char */
	    (*term.t_putchar)(cp1->c);
	}

	++ttcol;
	*cp2++ = *cp1++;
    }

    (*term.t_rev)(0);			/* turn off inverse anyway! */

    if (cp5 != cp3) {			/* Erase. */
	if(display)
	  peeol();
	while (cp1 != cp3)
	  *cp2++ = *cp1++;
    }
    *flags &= ~VFCHG;			/* flag this line is changed */
}


/*
 * Redisplay the mode line for the window pointed to by the "wp". This is the
 * only routine that has any idea of how the modeline is formatted. You can
 * change the modeline format by hacking at this routine. Called by "update"
 * any time there is a dirty window.
 */
modeline(wp)
WINDOW *wp;
{
    if(Pmaster){
	static char keys[13], labels[160];
	
        if(ComposerEditing)
	  ShowPrompt();
	else{
#ifdef	SPELLER
	    sprintf(keys, "GCRYKOXJ%cVUT", 
		    (Pmaster->alt_ed != NULL) ? '_' : 'W');
	    sprintf(labels, "Get Help,Cancel,Read File,Prev Pg,Cut Text,Postpone,Send,Justify,%s,Next Pg,%s,To Spell",  
#else
	    sprintf(keys, "GCRYKOXJ%cVUD", 
		    (Pmaster->alt_ed != NULL) ? '_' : 'W');
	    sprintf(labels, "Get Help,Cancel,Read File,Prev Pg,Cut Text,Postpone,Send,Justify,%s,Next Pg,%s,Del Char",  
#endif
		    (Pmaster->alt_ed) ? "Alt Edit" : "Where is",
		    (thisflag&CFFILL) ? "UnFill" : "UnCut Text");
	    wkeyhelp(keys, labels);
	}
    }
    else{
	register char *cp;
	register int n;		/* cursor position count */
	register BUFFER *bp;
	register i;		/* loop index */
	register lchar;		/* character to draw line in buffer with */
	char     tline[NLINE];	/* buffer for part of mode line */
	CELL     c;

	n = 0;
	c.a = 1;
#if     MSDOS
	vtmove(1, 0);
	vteeol();
#endif
	vscreen[n]->v_flag |= VFCHG; /* Redraw next time. */
	vtmove(n, 0);		/* Seek to right line. */

#if	REVSTA
	if (revexist)
	  lchar = ' ';
	else
#endif
	  lchar = '-';

	c.c = lchar;
	vtputc(c);
	bp = wp->w_bufp;

	n = 1;

	sprintf((cp=tline), "  UW PICO(tm) %s  ", version);	/* VERSION */
	
	while ((c.c = *cp++) != 0) {
	    vtputc(c);
	    ++n;
	}

	if (bp->b_fname[0] != 0){            /* File name. */
	    sprintf(tline," File: ");
	    cp = &bp->b_fname[0];

	    if(strlen(cp) > (term.t_ncol-n-22)){   /* room for full path ? */
		strcat(tline,".../");
		while(strlen(cp) >= (term.t_ncol-n-22)){
		    if(strchr(cp,'/') == NULL)
		      cp = (char *)strchr(&bp->b_fname[0],'\0')-term.t_ncol-n-22;
		    else
		      cp = (char *)strchr(cp,'/') + 1;
		}
	    }

	    strcat(tline,cp);

	    c.c = ' ';
	    for(i=((term.t_ncol-n-12-strlen(tline))/2); i > 0; n++, i--)
	      vtputc(c);

	    cp = &tline[0];

	    while ((c.c = *cp++) != 0){
		vtputc(c);
		++n;
            }

        }
        else{
	    cp = " New Buffer ";
	    c.c = lchar;
            for(i=(term.t_ncol-strlen(cp))/2; n < i; n++)
	      vtputc(c);
	    while ((c.c = *cp++) != 0) {
		vtputc(c);
		++n;
            }
	}

#if WFDEBUG
	vtputc(lchar);
	vtputc((wp->w_flag&WFMODE)!=0  ? 'M' : lchar);
	vtputc((wp->w_flag&WFHARD)!=0  ? 'H' : lchar);
	vtputc((wp->w_flag&WFEDIT)!=0  ? 'E' : lchar);
	vtputc((wp->w_flag&WFMOVE)!=0  ? 'V' : lchar);
	vtputc((wp->w_flag&WFFORCE)!=0 ? 'F' : lchar);
	n += 6;
#endif

	if ((bp->b_flag&BFCHG) != 0)                /* "MOD" if changed. */
	  cp = "Modified  ";
	else
	  cp = "          ";

	c.c = lchar;
	while (n < (term.t_ncol - strlen(cp))){	    /* Pad to full width. */
	    vtputc(c);
	    ++n;
        }

	while ((c.c = *cp++) != 0){
	    vtputc(c);
	    ++n;
	}
    }
}



/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
movecursor(row, col)
int row, col;
{
    if (row!=ttrow || col!=ttcol) {
        ttrow = row;
        ttcol = col;
        (*term.t_move)(row, col);
    }
}


/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
mlerase()
{
    int i;

    movecursor(term.t_nrow - 2, 0);
    (*term.t_rev)(0);
    if (eolexist == TRUE)
      peeol();
    else {
        for (i = 0; i < term.t_ncol - 1; i++)
	  (*term.t_putchar)(' ');
        movecursor(term.t_nrow, 1);	/* force the move! */
        movecursor(term.t_nrow, 0);
    }
    (*term.t_flush)();
    mpresf = FALSE;
}


/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. if d >= 0, d is the default answer returned. Otherwise there
 * is no default.
 */
mlyesno(prompt, dflt)
char  *prompt;
int   dflt;
{
    int  c;				/* input character */

#ifdef	MAYBELATER
    wkeyhelp("0C0000000000", "Cancel");
    if(Pmaster)
      curwp->w_flag |= WFMODE;
    else
      sgarbf = TRUE;
#endif
    if(dflt >= 0)
      sprintf(s, "%s? [%c] : ", prompt, (dflt) ? 'y' : 'n');
    else
      sprintf(s, "%s (y/n)? ", prompt);

    while(1){
	mlwrite(s, NULL);

	(*term.t_rev)(1);
	while((c = GetKey()) == NODATA)
	  ;				/* don't repaint if timeout */
	(*term.t_rev)(0);

	if(dflt >= 0 && c == (CTRL|'M')){
	    (*term.t_rev)(1);
	    pputs((dflt) ? "Yes" : "No", 1);
	    (*term.t_rev)(0);
	    return(dflt);
	}

	if (c == (CTRL|'C') || c == F3){	/* Bail out! */
	    (*term.t_rev)(1);
	    pputs("ABORT", 1);
	    (*term.t_rev)(0);
	    return(ABORT);
	}

	if (c=='y' || c=='Y'){
	    (*term.t_rev)(1);
	    pputs("Yes", 1);
	    (*term.t_rev)(0);
	    return(TRUE);
	}

	if (c=='n' || c=='N'){
	    (*term.t_rev)(1);
	    pputs("No", 1);
	    (*term.t_rev)(0);
	    return(FALSE);
	}
	else
	  (*term.t_beep)();
    }
}



/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */
mlreply(prompt, buf, nbuf, flg)
char  *prompt, *buf;
int    nbuf, flg;
{
    return(mlreplyd(prompt, buf, nbuf, flg|QDEFLT));
}


/*
 * function key mappings
 */
static int rfkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  0 },
    { F3,  (CTRL|'C')},
    { F4,  (CTRL|'T')},
    { F5,  0 },
    { F6,  0 },
    { F7,  0 },
    { F8,  0 },
    { F9,  0 },
    { F10, 0 },
    { F11, 0 },
    { F12, 0 }
};


/*
 * mlreplyd - write the prompt to the message line along with an default
 *	      answer already typed in.  Carraige return accepts the
 *	      default.  answer returned in buf which also holds the initial
 *            default, nbuf is it's length, def set means use default value,
 *            and ff means for-file which checks that all chars are allowed
 *            in file names.
 */
mlreplyd(prompt, buf, nbuf, flg)
char  *prompt;
char  *buf;
int    nbuf, flg;
{
    register int    c;				/* current char       */
    register char   *b;				/* pointer in buf     */
    register int    i;
    register int    maxl;
    register int    plen;
    int      changed = FALSE;

    mlwrite(prompt, NULL);
    plen = strlen(prompt);
    if(!(flg&QDEFLT))
      *buf = '\0';

    (*term.t_rev)(1);

    maxl = (nbuf < term.t_ncol - plen - 1) ? nbuf : term.t_ncol - plen - 1;

    pputs(buf, 1);
    b = &buf[strlen(buf)];
    
    for (;;) {
	movecursor(ttrow, plen+b-buf);
	(*term.t_flush)();


	while((c = GetKey()) == NODATA)
	  ;

	switch(normal(c, rfkm, 2)){
	  case (CTRL|'A') :			/* CTRL-A beginning     */
	    b = buf;
	    continue;

	  case (CTRL|'B') :			/* CTRL-B back a char   */
	    if(ttcol > plen)
		b--;
	    continue;

	  case (CTRL|'C') :			/* CTRL-C abort		*/
	    pputs("^C", 1);
	    ctrlg(FALSE, 0);
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return(ABORT);

	  case (CTRL|'E') :			/* CTRL-E end of line   */
	    b = &buf[strlen(buf)];
	    continue;

	  case (CTRL|'F') :			/* CTRL-F forward a char*/
	    if(*b != '\0')
		b++;
	    continue;

	  case (CTRL|'G') :			/* CTRL-G help		*/
	    pputs("HELP", 1);
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return(HELPCH);

	  case (CTRL|'H') :			/* CTRL-H backspace	*/
	  case 0x7f :				/*        rubout	*/
	    if (b <= buf)
	      break;
	    b--;
	    ttcol--;				/* cheating!  no pputc */
	    (*term.t_putchar)('\b');

	  case (CTRL|'D') :			/* CTRL-D delete char   */
	    changed=TRUE;
	    i = 0;
	    do					/* blat out left char   */
	      b[i] = b[i+1];
	    while(b[i++] != '\0');
	    break;

	  case (CTRL|'L') :			/* CTRL-L redraw	*/
	    (*term.t_rev)(0);
	    return(CTRL|'L');

	  case (CTRL|'T') :			/* CTRL-T special	*/
	    (*term.t_rev)(0);
	    return(CTRL|'T');

	  case (CTRL|'K') :			/* CTRL-K kill line	*/
	    changed=TRUE;
	    buf[0] = '\0';
	    b = buf;
	    movecursor(ttrow, plen);
	    break;

	  case K_PAD_LEFT:
	    if(ttcol > plen)
	      b--;
	    continue;

	  case K_PAD_RIGHT:
	    if(*b != '\0')
	      b++;
	    continue;

	  case F3 :				/* abort */
	    pputs("ABORT", 1);
	    ctrlg(FALSE, 0);
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return(ABORT);

	  case F1 :				/* sort of same thing */
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return(HELPCH);

	  case (CTRL|'M') :			/*        newline       */
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return(changed);

	  default : 
	    if(strlen(buf) >= maxl){		/* contain the text      */
		(*term.t_beep)();
		continue;
	    }
	    changed=TRUE;

	    if(c&(~0xff)){			/* bag ctrl/special chars */
		(*term.t_beep)();
	    }
	    else{
		i = strlen(b);
		if(flg&QFFILE){
		    if(!fallowc(c)){ 		/* c OK in filename? */
			(*term.t_beep)();
			continue;
		    }
		}
		do				/* blat out left char   */
		  b[i+1] = b[i];
		while(i-- >= 0);
		*b++ = c;
		pputc(c, 0);
	    }
	}

	pputs(b, 1);				/* show default */
	i = term.t_ncol-1;
	while(pscreen[ttrow]->v_text[i].c == ' ' 
	      && pscreen[ttrow]->v_text[i].a == 0)
	  i--;

	while(ttcol <= i)
	  pputc(' ', 0);
    }
}


/*
 * emlwrite() - write the message string to the error half of the screen
 *              center justified.  much like mlwrite (which is still used
 *              to paint the line for prompts and such), except it center
 *              the text.
 */
void
emlwrite(message, arg) 
char	*message;
void	*arg;
{
    register char *bufp = message;
    register char *ap;
    register long l;

    mlerase();

    if((l = strlen(message)) == 0)		/* nothing to write, bag it */
      return;

    /*
     * next, figure out where the to move the cursor so the message 
     * comes out centered
     */
    if((ap=(char *)strchr(message, '%')) != NULL){
	l -= 2;
	switch(ap[1]){
	  case '%':
	  case 'c':
	    l += 1;
	    break;
	  case 'd':
	    l += (long)dumbroot((int)arg, 10);
	    break;
	  case 'D':
	    l += (long)dumblroot((long)arg, 10);
	    break;
	  case 'o':
	    l += (long)dumbroot((int)arg, 8);
	    break;
	  case 'x':
	    l += (long)dumbroot((int)arg, 16);
	    break;
	  case 's':
            l += strlen((char *)arg);
	    break;
	}
    }

    if(l-4 <= term.t_ncol)			/* this wouldn't be good */
      movecursor(term.t_nrow-2, (term.t_ncol - (int)l - 4)/2);
    else
      movecursor(term.t_nrow-2, 0);

    (*term.t_rev)(1);
    pputs("[ ", 1);
    while (*bufp != '\0' && ttcol < term.t_ncol-2){
	if(*bufp == '\007')
	  (*term.t_beep)();
	else if(*bufp == '%'){
	    switch(*++bufp){
	      case 'c':
		pputc((char)(int)arg, 0);
		break;
	      case 'd':
		mlputi((int)arg, 10);
		break;
	      case 'D':
		mlputli((long)arg, 10);
		break;
	      case 'o':
		mlputi((int)arg, 16);
		break;
	      case 'x':
		mlputi((int)arg, 8);
		break;
	      case 's':
		pputs((char *)arg, 0);
		break;
	      case '%':
	      default:
		pputc(*bufp, 0);
		break;
	    }
	}
	else
	  pputc(*bufp, 0);
	bufp++;
    }

    pputs(" ]", 1);
    (*term.t_rev)(0);
    (*term.t_flush)();
    mpresf = TRUE;
}


/*
 * Write a message into the message line. Keep track of the physical cursor
 * position. A small class of printf like format items is handled. Assumes the
 * stack grows down; this assumption is made by the "++" in the argument scan
 * loop. Set the "message line" flag TRUE.
 */
mlwrite(fmt, arg)
char *fmt;
void *arg;
{
    register int c;
    register char *ap;

    /*
     * the idea is to only highlight if there is something to show
     */
    mlerase();

    ttcol = 0;
    (*term.t_rev)(1);
    ap = (char *) &arg;
    while ((c = *fmt++) != 0) {
        if (c != '%') {
            (*term.t_putchar)(c);
            ++ttcol;
	}
        else {
            c = *fmt++;
            switch (c) {
	      case 'd':
		mlputi(*(int *)ap, 10);
		ap += sizeof(int);
		break;

	      case 'o':
		mlputi(*(int *)ap,  8);
		ap += sizeof(int);
		break;

	      case 'x':
		mlputi(*(int *)ap, 16);
		ap += sizeof(int);
		break;

	      case 'D':
		mlputli(*(long *)ap, 10);
		ap += sizeof(long);
		break;

	      case 's':
		pputs(*(char **)ap, 1);
		ap += sizeof(char *);
		break;

              default:
		(*term.t_putchar)(c);
		++ttcol;
	    }
	}
    }

    c = ttcol;
    while(ttcol < term.t_ncol)
      pputc(' ', 0);

    movecursor(term.t_nrow - 2, c);
    (*term.t_rev)(0);
    (*term.t_flush)();
    mpresf = TRUE;
}


/*
 * Write out an integer, in the specified radix. Update the physical cursor
 * position. This will not handle any negative numbers; maybe it should.
 */
mlputi(i, r)
int i, r;
{
    register int q;
    static char hexdigits[] = "0123456789ABCDEF";

    if (i < 0){
        i = -i;
	pputc('-', 1);
    }

    q = i/r;

    if (q != 0)
      mlputi(q, r);

    pputc(hexdigits[i%r], 1);
}


/*
 * do the same except as a long integer.
 */
mlputli(l, r)
long l;
int  r;
{
    register long q;

    if (l < 0){
        l = -l;
        pputc('-', 1);
    }

    q = l/r;

    if (q != 0)
      mlputli(q, r);

    pputc((int)(l%r)+'0', 1);
}


/*
 * scrolldown - use stuff to efficiently move blocks of text on the
 *              display, and update the pscreen array to reflect those
 *              moves...
 *
 *        wp is the window to move in
 *        r  is the row at which to begin scrolling
 *        n  is the number of lines to scrol
 */
scrolldown(wp, r, n)
WINDOW *wp;
int     r, n;
{
#ifdef	TERMCAP
    register int i;
    register int l;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0){
	r = wp->w_toprow;
	l = wp->w_ntrows;
    }
    else{
	if(r > wp->w_toprow)
	    vscreen[r-1]->v_flag |= VFCHG;
	l = wp->w_toprow+wp->w_ntrows-r;
    }

    o_scrolldown(r, n);

    for(i=l-n-1; i >=  0; i--){
	vp1 = pscreen[r+i]; 
	vp2 = pscreen[r+i+n];
	bcopy(vp1, vp2, term.t_ncol * sizeof(CELL));
    }
    pprints(r+n-1, r);
    ttrow = HUGE;
    ttcol = HUGE;
#endif /* TERMCAP */
}


/*
 * scrollup - use tcap stuff to efficiently move blocks of text on the
 *            display, and update the pscreen array to reflect those
 *            moves...
 */
scrollup(wp, r, n)
WINDOW *wp;
int     r, n;
{
#ifdef	TERMCAP
    register int i;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0)
      r = wp->w_toprow;

    o_scrollup(r, n);

    i = 0;
    while(1){
	if(Pmaster){
	    if(!(r+i+n < wp->w_toprow+wp->w_ntrows))
	      break;
	}
	else{
	    if(!((i < wp->w_ntrows-n)&&(r+i+n < wp->w_toprow+wp->w_ntrows)))
	      break;
	}
	vp1 = pscreen[r+i+n]; 
	vp2 = pscreen[r+i];
	bcopy(vp1, vp2, term.t_ncol * sizeof(CELL));
	i++;
    }
    pprints(wp->w_toprow+wp->w_ntrows-n, wp->w_toprow+wp->w_ntrows-1);
    ttrow = HUGE;
    ttcol = HUGE;
#endif /* TERMCAP */
}


/*
 * print spaces in the physical screen starting from row abs(n) working in
 * either the positive or negative direction (depending on sign of n).
 */
pprints(x, y)
int x, y;
{
    register int i;
    register int j;

    if(x < y){
	for(i = x;i <= y; ++i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    else{
	for(i = x;i >= y; --i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    ttrow = y;
    ttcol = 0;
}



/*
 * doton - return the physical line number that the dot is on in the
 *         current window, and by side effect the number of lines remaining
 */
doton(r, chs)
int       *r;
unsigned  *chs;
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    int      l = -1;

    *chs = 0;
    while(i++ < curwp->w_ntrows){
	if(lp == curwp->w_dotp)
	  l = i-1;
	lp = lforw(lp);
	if(lp == curwp->w_bufp->b_linep){
	    i++;
	    break;
	}
	if(l >= 0)
	  (*chs) += llength(lp);
    }
    *r = i-l-2;
    return(l+curwp->w_toprow);
}



/*
 * resize_pico - given new window dimensions, allocate new resources
 */
resize_pico(row, col)
int  row, col;
{
    int old_nrow, old_ncol;
    register int i;
    register VIDEO *vp;

    old_nrow = term.t_nrow;
    old_ncol = term.t_ncol;

    term.t_nrow = row;
    term.t_ncol = col;

    if (old_ncol == term.t_ncol && old_nrow == term.t_nrow)
      return(TRUE);

    curwp->w_toprow = 2;
    curwp->w_ntrows = term.t_nrow-4; 	      /* "-4" for mode line and keys */

    if(Pmaster)
      fillcol = (term.t_ncol > 80) ? 77 : term.t_ncol - 6;
    else
      fillcol = term.t_ncol - 6;	       /* we control the fill column */

    /* 
     * free unused screen space ...
     */
    for(i=term.t_nrow+1; i <= old_nrow; ++i){
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    /* 
     * realloc new space for screen ...
     */
    if((vscreen=(VIDEO **)realloc(vscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    if((pscreen=(VIDEO **)realloc(pscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    for (i = 0; i <= term.t_nrow; ++i) {
	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(vscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);
	vp->v_flag = VFCHG;
	vscreen[i] = vp;
	if(old_ncol < term.t_ncol){  /* don't let any garbage in */
	    vtrow = i;
	    vtcol = (i < old_nrow) ? old_ncol : 0;
	    vteeol();
	}

	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(pscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);

	vp->v_flag = VFCHG;
	pscreen[i] = vp;
    }

    if(!ResizeBrowser()){
	if(Pmaster){
	    ResizeHeader();
	}
	else{
	    lchange(WFHARD);                   /* set update flags... */
	    curwp->w_flag |= WFMODE;           /* and modeline so we  */
	    refresh(0, 1);                     /* redraw the whole enchilada. */
	    update();                          /* do it */
	}
    }

    return(TRUE);
}


/*
 * showCompTitle - display the anchor line passed in from pine
 */
showCompTitle()
{
    if(Pmaster){
	register char *bufp;
	extern   char *pico_anchor;

	if((bufp = pico_anchor) == NULL)
	  return(1);

	movecursor(COMPOSER_TITLE_LINE, 0);
	(*term.t_rev)(1);   
	while (ttcol < term.t_ncol)
	  if(*bufp != '\0')
	    pputc(*bufp++, 1);
          else
	    pputc(' ', 1);

	(*term.t_rev)(0);
	movecursor(COMPOSER_TITLE_LINE + 1, 0);
	peeol();
    }
}



/*
 * zotdisplay - blast malloc'd space created for display maps
 */
zotdisplay()
{
    register int i;

    for (i = 0; i <= term.t_nrow; ++i){		/* free screens */
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    free((char *) vscreen);
    free((char *) pscreen);
}



/*
 * nlforw() - returns the number of lines from the top to the dot
 */
nlforw()
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    
    while(lp != curwp->w_dotp){
	lp = lforw(lp);
	i++;
    }
    return(i);
}



/*
 * pputc - output the given char, keep track of it on the physical screen
 *	   array, and keep track of the cursor
 */
pputc(c, a)
int   c;				/* char to write */
int   a;				/* and its attribute */
{
    if((ttcol >= 0 && ttcol < term.t_ncol) 
       && (ttrow >= 0 && ttrow <= term.t_nrow)){
/*	(*term.t_rev)(a);*/
	(*term.t_putchar)(c);			/* write it */
/*	(*term.t_rev)(!a);*/
	pscreen[ttrow]->v_text[ttcol].c = c;	/* keep track of it */
	pscreen[ttrow]->v_text[ttcol++].a = a;	/* keep track of it */
    }
}


/*
 * pputs - print a string and keep track of the cursor
 */
pputs(s, a)
register char *s;			/* string to write */
register int   a;			/* and its attribute */
{
    while (*s != '\0')
      pputc(*s++, a);
}


/*
 * peeol - physical screen array erase to end of the line.  remember to
 *	   track the cursor.
 */
peeol()
{
    register int r = ttrow;
    register int c = ttcol;
    CELL         cl;

    cl.c = ' ';
    cl.a = 0;
    (*term.t_eeol)();
    while(c < term.t_ncol && c >= 0 && r <= term.t_nrow && r >= 0)
      pscreen[r]->v_text[c++] = cl;
}


/*
 * pscr - return the character cell on the physical screen map on the 
 *        given line, l, and offset, o.
 */
CELL *
pscr(l, o)
int l, o;
{
    if((l >= 0 && l <= term.t_nrow) && (o >= 0 && o < term.t_ncol))
      return(&(pscreen[l]->v_text[o]));
    else
      return(NULL);
}


/*
 * pclear() - clear the physical screen from row x through row y
 */
pclear(x, y)
register int x;
register int y;
{
    register int i;

    for(i=x; i < y; i++){
	movecursor(i, 0);
	peeol();
    }
}


/*
 * dumbroot - just get close 
 */
dumbroot(x, b)
int x, b;
{
    if(x < b)
      return(1);
    else
      return(dumbroot(x/b, b) + 1);
}


/*
 * dumblroot - just get close 
 */
dumblroot(x, b)
long x;
int  b;
{
    if(x < b)
      return(1);
    else
      return(dumblroot(x/b, b) + 1);
}


/*
 * pinsertc - use optimized insert, fixing physical screen map.
 *            returns true if char written, false otherwise
 */
pinsert(c)
CELL c;
{
    register int   i;
    register CELL *p;

    if(o_insert((char)c.c)){		/* if we've got it, use it! */
	p = pscreen[ttrow]->v_text;	/* then clean up physical screen */
	for(i = term.t_ncol-1; i > ttcol; i--)
	  p[i] = p[i-1];		/* shift right */
	p[ttcol++] = c;			/* insert new char */
	
	return(1);
    }

    return(0);
}


/*
 * pdel - use optimized delete to rub out the current char and
 *        fix the physical screen array.
 *        returns true if optimized the delete, false otherwise
 */
pdel()
{
    register int   i;
    register CELL *c;

    if(delchar){			/* if we've got it, use it! */
	(*term.t_putchar)('\b'); 	/* move left a char */
	--ttcol;
	o_delete();			/* and delete it */

	c = pscreen[ttrow]->v_text;	/* then clean up physical screen */
	for(i=ttcol; i < term.t_ncol; i++)
	  c[i] = c[i+1];
	c[i].c = ' ';
	c[i].a = 0;
	
	return(1);
    }

    return(0);
}



/*
 * wstripe - write out the given string at the given location, and reverse
 *           video on flagged characters.  Does the same thing as pine's
 *           stripe.
 */
void
wstripe(line, column, pmt, key)
int	line, column;
char	*pmt;
int      key;
{
    register char *buf;
    register int  i = 0;
    register int  j = 0;
    register int  l;

    l = strlen(pmt);
    while(1){
	if(i >= term.t_ncol || j >= l)
	  return;				/* equal strings */

	if(pmt[j] == key)
	  j++;

	if(pscr(line, i)->c != pmt[j]){
	    if(j >= 1 && pmt[j-1] == key)
	      j--;
	    break;
	}

	j++;
	i++;
    }

    movecursor(line, column+i);
    buf = &pmt[j];
    do{
	if(*buf == key){
	    buf++;
	    (*term.t_rev)(1);
	    pputc(*buf, 1);
	    (*term.t_rev)(0);
	}
	else{
	    pputc(*buf, 0);
	}
    }    
    while(*++buf != '\0');
    peeol();
    (*term.t_flush)();
}



/*
 *  wkeyhelp - paint list of possible commands on the bottom
 *             of the display (yet another pine clone)
 */
wkeyhelp(keys, helptxt)
char	*keys;
char	*helptxt;
{
    char *obufp, *ibufp = helptxt, *kbufp = HelpKeyNames, *startp;
    int  i, j, spaces, index, copy, extra;

    /*
     * make hardware cursor position change known!!
     */
#if	defined(DOS) && defined(MOUSE)
    register_keys(keys, helptxt, NODATA);
#endif
    extra  = (kbufp && kbufp[2] == ',') ? 2 : 3;
    spaces = term.t_ncol/6;
    for(i=1;i>=0;i--){
	*s = '\0';
	obufp = &s[strlen(s)];
	for(j=1;j<=6;j++){
	    index = j + (6*(1 - i));
	    copy = keys[index-1] - '0';
	    if(kbufp != NULL){
		do{
		    if((*kbufp == '~') && (!copy))
		      kbufp++;
		    if(copy)
		      *obufp++ = *kbufp++;
		    else{
			*obufp++ = ' ';
			kbufp++;
		    }
		}
		while((*kbufp != ',')? 1 : !(kbufp++));
	    }
	    else{
		if(copy){
		    *obufp++ = '~';
		    *obufp++ = '^';
		    *obufp++ = '~';
		    *obufp++ = copy + '0';
		}
		else{
		    *obufp++ = ' ';
		    *obufp++ = ' ';
		}
	    }

	    *obufp++ = ' ';
	    startp = obufp;
	    /*
	     * we use "spaces-3" because of length key names
	     */
	    while(obufp - startp < (spaces-extra)) {
		if((copy) && ((*ibufp != '\0') && (*ibufp != ',')))
		  *obufp++ = *ibufp++;
		else
		  *obufp++ = ' ';
	    }
	    if(copy){
		while((*ibufp != ',') && (*ibufp != '\0'))
		  ibufp++;
		if(*ibufp != '\0')
		  ibufp++;
	    }
	    *obufp = '\0';
	}
	wstripe(term.t_nrow-i,0,s,'~');
    }
}

