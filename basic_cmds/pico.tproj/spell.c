#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: spell.c,v 1.3 2003/01/12 01:48:12 bbraun Exp $";
#endif
/*
 * Program:	spell.c
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

#include <stdio.h>
#include "osdep.h"
#include "estruct.h"
#ifdef	SPELLER
#include "edef.h"
#include "pico.h"

#ifdef	ANSI
    void chword(char *, char *);
    int  movetoword(char *);
#else
    void chword();
    int  movetoword();
#endif


#define  NSHLINES  12

static char *spellhelp[] = {
  "Spell Check Help",
  " ",
  "\tThe spell checker examines all words in the text.  It then",
  "\toffers each misspelled word for correction while simultaneously",
  "\thighlighting it in the text.  To leave a word unchanged simply",
  "~\thit ~R~e~t~u~r~n at the edit prompt.  If a word has been corrected,",
  "\teach occurrence of the incorrect word is offered for replacement.",
  " ",
  "~\tSpell checking can be cancelled at any time by typing ~^~C (~F~3)",
  "\tafter exiting help.",
  " ",
  "End of Spell Check Help",
  " ",
  NULL
};


static char *pinespellhelp[] = {
  "Spell Check Help",
  " ",
  "\tThe spell checker examines all words in the text.  It then",
  "\toffers each misspelled word for correction while simultaneously",
  "\thighlighting it in the text.  To leave a word unchanged simply",
  "\thit Return at the edit prompt.  If a word has been corrected,",
  "\teach occurrence of the incorrect word is offered for replacement.",
  " ",
  "\tSpell checking can be cancelled at any time by typing ^C (F3)",
  "\tafter exiting help.",
  " ",
  "End of Spell Check Help",
  " ",
  NULL
};



/*
 * spell() - check for potentially missspelled words and offer them for
 *           correction
 */
spell(f, n)
{
    int    status, next, ret;
    FILE   *p;
    char   *b, *sp, *fn;
    char   wb[NLINE], cb[NLINE];
    char   *writetmp();
    FILE   *P_open();

    setimark(0, 1);
    emlwrite("Checking spelling..."); 		/* greetings */

    if((fn = writetmp(0, 0)) == NULL){
	emlwrite("Can't write temp file for spell checker");
	return(-1);
    }
    if((sp = (char *)getenv("SPELL")) == NULL)
      sp = SPELLER;

    sprintf(cb, "( %s ) < %s", sp, fn);		/* pre-use buffer! */
    if((p = P_open(cb)) == NULL){ 		/* read output from command */
	unlink(fn);
	emlwrite("Can't fork spell checker", NULL);
	return(-1);
    }

    ret = 1;
    while(fgets(wb, NLINE, p) != NULL && ret){
	if((b = (char *)strchr(wb,'\n')) != NULL)
	  *b = '\0';
	strcpy(cb, wb);

	gotobob(0, 1);

	status = TRUE;
	next = 1;

	while(status){
	    if(next++)
	      if(movetoword(wb) != TRUE)
		break;

	    update();
	    (*term.t_rev)(1);
	    pputs(wb, 1);			/* highlight word */
	    (*term.t_rev)(0);

	    if(strcmp(cb, wb)){
		char prompt[2*NLINE + 32];
		sprintf(prompt, "Replace \"%s\" with \"%s\"", wb, cb);
		status=mlyesno(prompt, TRUE);
	    }
	    else
	      status=mlreplyd("Edit a replacement: ", cb, NLINE, QDEFLT, NULL);


	    curwp->w_flag |= WFMOVE;		/* put cursor back */
	    sgarbk = 0;				/* fake no-keymenu-change! */
	    update();
	    pputs(wb, 0);			/* un-highlight */

	    switch(status){
	      case TRUE:
		chword(wb, cb);			/* correct word    */
	      case FALSE:
		update();			/* place cursor */
		break;
	      case ABORT:
		emlwrite("Spell Checking Cancelled", NULL);
		ret = FALSE;
		status = FALSE;
		break;
	      case HELPCH:
		if(Pmaster)
		  (*Pmaster->helper)(pinespellhelp, 
				     "Help with Spelling Checker", 1);
		else
		  pico_help(spellhelp, "Help with Spelling Checker", 1);
	      case (CTRL|'L'):
		next = 0;			/* don't get next word */
		sgarbf = TRUE;			/* repaint full screen */
		update();
		status = TRUE;
		continue;
	      default:
		emlwrite("Huh?");		/* shouldn't get here, but.. */
		status = TRUE;
		sleep(1);
		break;
	    }
	    forwword(0, 1);			/* goto next word */
	}
    }
    P_close(p);					/* clean up */
    unlink(fn);
    swapimark(0, 1);
    curwp->w_flag |= WFHARD|WFMODE;
    sgarbk = TRUE;

    if(ret)
      emlwrite("Done checking spelling");
    return(ret);
}




/* 
 * chword() - change the given word, wp, pointed to by the curwp->w_dot 
 *            pointers to the word in cb
 */
void
chword(wb, cb)
  char *wb;					/* word buffer */
  char *cb;					/* changed buffer */
{
    ldelete(strlen(wb), 0);			/* not saved in kill buffer */
    while(*cb != '\0')
      linsert(1, *cb++);

    curwp->w_flag |= WFEDIT;
}




/* 
 * movetoword() - move to the first occurance of the word w
 *
 *	returns:
 *		TRUE upon success
 *		FALSE otherwise
 */
movetoword(w)
  char *w;
{
    int      i;
    int      ret  = FALSE;
    int	     olddoto;
    LINE     *olddotp;
    register int   off;				/* curwp offset */
    register LINE *lp;				/* curwp line   */

    olddoto = curwp->w_doto;			/* save where we are */
    olddotp = curwp->w_dotp;

    curwp->w_bufp->b_mode |= MDEXACT;		/* case sensitive */
    while(forscan(&i, w, 1) == TRUE){
	if(i)
	  break;				/* wrap NOT allowed! */

	lp  = curwp->w_dotp;			/* for convenience */
	off = curwp->w_doto;

	/*
	 * We want to minimize the number of substrings that we report
	 * as matching a misspelled word...
	 */
	if(off == 0 || !isalpha(lgetc(lp, off - 1).c)){
	    off += strlen(w);
	    if((!isalpha(lgetc(lp, off).c) || off == llength(lp)) 
	       && lgetc(lp, 0).c != '>'){
		ret = TRUE;
		break;
	    }
	}

	forwchar(0, 1);				/* move on... */

    }
    curwp->w_bufp->b_mode ^= MDEXACT;		/* case insensitive */

    if(ret == FALSE){
	curwp->w_dotp = olddotp;
	curwp->w_doto = olddoto;
    }
    else
      curwp->w_flag |= WFHARD;

    return(ret);
}
#endif	/* SPELLER */
