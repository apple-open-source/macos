#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: word.c,v 1.2 2002/01/03 22:16:43 jevans Exp $";
#endif
/*
 * Program:	Word at a time routines
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
 * The routines in this file implement commands that work word at a time.
 * There are all sorts of word mode commands. If I do any sentence and/or
 * paragraph mode commands, they are likely to be put in this file.
 */

#include        <stdio.h>
#include        "estruct.h"
#include        "pico.h"
#include        <ctype.h>
#include	"edef.h"
#include	"osdep.h"


/* Word wrap on n-spaces. Back-over whatever precedes the point on the current
 * line and stop on the first word-break or the beginning of the line. If we
 * reach the beginning of the line, jump back to the end of the word and start
 * a new line.  Otherwise, break the line at the word-break, eat it, and jump
 * back to the end of the word.
 * Returns TRUE on success, FALSE on errors.
 */
wrapword()
{
    register int   cnt = 0;	/* size of word wrapped to next line */
    register int   bp;

    if(curwp->w_doto <= 0)		/* no line to wrap? */
      return(FALSE);

    /* 
     * back up until we aren't in a word,
     * and make sure there is a break in the line
     */
    bp = llength(curwp->w_dotp);	/* start at end of line */
    do{
	if(--bp <= 0)			/* no place to break the line! */
	  return(FALSE);
	
	switch(cnt){
	  case 0:			/* break BEFORE fillcol */
	    if(bp > fillcol)
	      break;
	    else
	      cnt++;

	  case 1:			/* find first breakable word */
	    if(isspace(lgetc(curwp->w_dotp, bp).c))
	      break;
	    else
	      cnt++;

	  case 2:			/* and break in front of it */
	    if(!isspace(lgetc(curwp->w_dotp, bp).c))
	      break;
	    else
	      cnt = 10;

	  default:
	    break;
	}
    }
    while(cnt != 10);

    /* bp now points to the last character to remain on this line! */
    cnt = curwp->w_doto - ++bp;
    curwp->w_doto = bp;
    
    if(!lnewline())			/* break the line */
      return(FALSE);

    /*
     * if there's a line below, it doesn't start with whitespace 
     * and there's room for this line...
     */
    if(lforw(curwp->w_dotp) != curbp->b_linep 
       && llength(lforw(curwp->w_dotp)) 
       && !isspace(lgetc(lforw(curwp->w_dotp), 0).c)
       && (llength(curwp->w_dotp) + llength(lforw(curwp->w_dotp)) < fillcol)){
	gotoeol(0, 1);			/* then pull text up from below */
	if(lgetc(curwp->w_dotp, curwp->w_doto - 1).c != ' ')
	  linsert(1, ' ');

	forwdel(0, 1);
	gotobol(0, 1);
    }

    if(!forwchar(0, cnt < 0 ? cnt-1 : cnt)) /* restore dot (account for NL) */
      return(FALSE);

    return(TRUE);
}


/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines. Error if you try to
 * move beyond the buffers.
 */
backword(f, n)
{
        if (n < 0)
                return (forwword(f, -n));
        if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (forwchar(FALSE, 1));
}

/*
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 */
forwword(f, n)
{
        if (n < 0)
                return (backword(f, -n));
        while (n--) {
#if	NFWORD
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#endif
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#if	NFWORD == 0
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#endif
        }
	return(TRUE);
}

#ifdef	MAYBELATER
/*
 * Move the cursor forward by the specified number of words. As you move,
 * convert any characters to upper case. Error if you try and move beyond the
 * end of the buffer. Bound to "M-U".
 */
upperword(f, n)
{
        register int    c;
	CELL            ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='a' && c<='z') {
                                ac.c = (c -= 'a'-'A');
                                lputc(curwp->w_dotp, curwp->w_doto, ac);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert characters to lower case. Error if you try and move over the end of
 * the buffer. Bound to "M-L".
 */
lowerword(f, n)
{
        register int    c;
	CELL            ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='A' && c<='Z') {
                                ac.c (c += 'a'-'A');
                                lputc(curwp->w_dotp, curwp->w_doto, ac);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case. Error if you try and move past the end of the
 * buffer. Bound to "M-C".
 */
capword(f, n)
{
        register int    c;
	CELL	        ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                if (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='a' && c<='z') {
			    ac.c = (c -= 'a'-'A');
			    lputc(curwp->w_dotp, curwp->w_doto, ac);
			    lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        while (inword() != FALSE) {
                                c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                                if (c>='A' && c<='Z') {
				    ac.c = (c += 'a'-'A');
				    lputc(curwp->w_dotp, curwp->w_doto, ac);
				    lchange(WFHARD);
                                }
                                if (forwchar(FALSE, 1) == FALSE)
                                        return (FALSE);
                        }
                }
        }
        return (TRUE);
}

/*
 * Kill forward by "n" words. Remember the location of dot. Move forward by
 * the right number of words. Put dot back where it was and issue the kill
 * command for the right number of characters. Bound to "M-D".
 */
delfword(f, n)
{
        register int    size;
        register LINE   *dotp;
        register int    doto;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        dotp = curwp->w_dotp;
        doto = curwp->w_doto;
        size = 0;
        while (n--) {
#if	NFWORD
		while (inword() != FALSE) {
			if (forwchar(FALSE,1) == FALSE)
				return(FALSE);
			++size;
		}
#endif
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
#if	NFWORD == 0
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
#endif
        }
        curwp->w_dotp = dotp;
        curwp->w_doto = doto;
        return (ldelete(size, TRUE));
}

/*
 * Kill backwards by "n" words. Move backwards by the desired number of words,
 * counting the characters. When dot is finally moved to its resting place,
 * fire off the kill command. Bound to "M-Rubout" and to "M-Backspace".
 */
delbword(f, n)
{
        register int    size;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
        size = 0;
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
                while (inword() != FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
        }
        if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        return (ldelete(size, TRUE));
}
#endif	/* MAYBELATER */

/*
 * Return TRUE if the character at dot is a character that is considered to be
 * part of a word. The word character list is hard coded. Should be setable.
 */
inword()
{
        register int    c;

        if (curwp->w_doto == llength(curwp->w_dotp))
                return (FALSE);
        c = lgetc(curwp->w_dotp, curwp->w_doto).c;

        if (c>='a' && c<='z')
                return (TRUE);
        if (c>='A' && c<='Z')
                return (TRUE);
        if (c>='0' && c<='9')
                return (TRUE);
        return (FALSE);
}

fillpara(f, n)	/* Fill the current paragraph according to the current
		   fill column						*/

int f, n;	/* deFault flag and Numeric argument */

{
	register int c;			/* current char durring scan	*/
	register int wordlen;		/* length of current word	*/
	register int clength;		/* position on line during fill	*/
	register int i;			/* index during word copy	*/
	register int newlength;		/* tentative new line length	*/
	register int eopflag;		/* Are we at the End-Of-Paragraph? */
	register int firstflag;		/* first word? (needs no space)	*/
	register LINE *eopline;		/* pointer to line just past EOP */
	register int dotflag;		/* was the last char a period?	*/
	char wbuf[NSTRING];		/* buffer for current word	*/


	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
	if (fillcol == 0) {	/* no fill column set */
		mlwrite("No fill column set");
		return(FALSE);
	}

	/* record the pointer to the line just past the EOP */
	gotoeop(FALSE, 1);
	eopline = lforw(curwp->w_dotp);

	/* and back to the beginning of the paragraph */
	gotobop(FALSE, 1);

	/* let yank() know that it may be restoring a paragraph */
	thisflag |= CFFILL;

	if(Pmaster == NULL)
	  sgarbk = TRUE;

        curwp->w_flag |= WFMODE;
	kdelete();

	/* initialize various info */
	clength = curwp->w_doto;
	if (clength && curwp->w_dotp->l_text[0].c == TAB)
		clength = 8;
	wordlen = 0;
	dotflag = FALSE;

	/* scan through lines, filling words */
	firstflag = TRUE;
	eopflag = FALSE;

	while (!eopflag) {
		/* get the next character in the paragraph */
		if (curwp->w_doto == llength(curwp->w_dotp)) {

			c = ' ';
			if (lforw(curwp->w_dotp) == eopline)
				eopflag = TRUE;
			kinsert('\n');
		} else {
			c = lgetc(curwp->w_dotp, curwp->w_doto).c;
			kinsert(c);
		}

		/* and then delete it */
		ldelete(1, FALSE);

		/* if not a separator, just add it in */
		if (c != ' ' && c != '	') {
			/* 
			 * don't want to limit ourselves to only '.'
			 */
			dotflag = (int)strchr(".?!:;\"", c);	/* dot ? */
			if (wordlen < NSTRING - 1)
				wbuf[wordlen++] = c;
		} else if (wordlen) {
			/* at a word break with a word waiting */
			/* calculate tantitive new length with word added */
			newlength = clength + 1 + wordlen;
			if (newlength <= fillcol) {
				/* add word to current line */
				if (!firstflag) {
					linsert(1, ' '); /* the space */
					++clength;
				}
				firstflag = FALSE;
			} else {
				/* start a new line */
				lnewline();
				clength = 0;
			}

			/* and add the word in in either case */
			for (i=0; i<wordlen; i++) {
				linsert(1, wbuf[i]);
				++clength;
			}

			/*  Strategy:  Handle 3 cases:
			 *     1. if . at end of line put extra space after it
			 *     2. if . and only 1 space, leave only one space 
			 *     3. if . and more than 1 space, leave 2 spaces
			 *
			 *  So, we know the current c is a space. if then 
			 *  is no next c or the next c is a ' ' then we 
			 *  need to insert a space else don't do it.
			 */
			if (dotflag &&
		           ((curwp->w_doto == llength(curwp->w_dotp)) || 
			   (' ' == lgetc(curwp->w_dotp, curwp->w_doto).c))){
				linsert(1, ' ');
				++clength;
			}
			wordlen = 0;
		}
	}

	/* and add a last newline for the end of our new paragraph */
	lnewline();
}
