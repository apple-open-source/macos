#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: search.c,v 1.1.1.1 1999/04/15 17:45:14 wsanchez Exp $";
#endif
/*
 * Program:	Searching routines
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
 * The functions in this file implement commands that search in the forward
 * and backward directions. There are no special characters in the search
 * strings. Probably should have a regular expression search, or something
 * like that.
 *
 */

#include        <stdio.h>
#include	"osdep.h"
#include	"estruct.h"
#include	"pico.h"
#include        "edef.h"

#ifdef	ANSI
    int eq(int, int);
    int expandp(char *, char *, int);
#else
    int eq();
    int expandp();
#endif


/*
 * Search forward. Get a search string from the user, and search, beginning at
 * ".", for the string. If found, reset the "." to be just after the match
 * string, and [perhaps] repaint the display. Bound to "C-S".
 */

/*	string search input parameters	*/

#define	PTBEG	1	/* leave the point at the begining on search */
#define	PTEND	2	/* leave the point at the end on search */




static char *SearchHelpText[] = {
"Help for Search Command",
" ",
"\tEnter the words or characters you would like to search",
"~\tfor, then press ~R~e~t~u~r~n.  The search then takes place.",
"\tWhen the characters or words that you entered ",
"\tare found, the buffer will be redisplayed with the cursor ",
"\tat the beginning of the selected text.",
" ",
"\tThe most recent string for which a search was made is",
"\tdisplayed in the \"Search\" prompt between the square",
"\tbrackets.  This string is the default search prompt.",
"~        Hitting only ~R~e~t~u~r~n or at the prompt will cause the",
"\tsearch to be made with the default value.",
"  ",
"\tThe text search is not case sensitive, and will examine the",
"\tentire message.",
"  ",
"\tShould the search fail, a message will be displayed.",
"  ",
"End of Search Help.",
"  ",
NULL
};


/*
 * Compare two characters. The "bc" comes from the buffer. It has it's case
 * folded out. The "pc" is from the pattern.
 */
eq(bc, pc)
int bc;
int pc;
{
    if ((curwp->w_bufp->b_mode & MDEXACT) == 0){
	if (bc>='a' && bc<='z')
	  bc -= 0x20;

	if (pc>='a' && pc<='z')
	  pc -= 0x20;
    }

    return(bc == pc);
}


forwsearch(f, n)
{
    register int status;
    int wrapt = FALSE;
    char show[512];

    /* resolve the repeat count */
    if (n == 0)
      n = 1;

    if (n < 1)			/* search backwards */
      return(0);

    /* ask the user for the text of a pattern */
    while(1){
	wkeyhelp("GC0000000000","Get Help,Cancel");
	if(Pmaster == NULL)
	  sgarbk = TRUE;

	if ((status = readpattern("Search")) == TRUE){
	    break;
	}
	else{
	    switch(status){
	      case HELPCH:			/* help requested */
		if(Pmaster)
		  (*Pmaster->helper)(Pmaster->search_help,
				     "Help for Searching", 1);
		else
		  pico_help(SearchHelpText, "Help for Searching", 1);
	      case (CTRL|'L'):			/* redraw requested */
		refresh(FALSE, 1);
		update();
		break;
	      default:
		curwp->w_flag |= WFMODE;
		if(status == ABORT)
		  emlwrite("Search Cancelled", NULL);
		else
		  mlerase();
		return(FALSE);
	    }
	}
    }

    curwp->w_flag |= WFMODE;

    /*
     * This was added late in the game and is kind of a hack.
     * What is wanted is an easy way to move immediately to the top or
     * bottom of the buffer.  This makes it not-too-obvious, but saves
     * key commands.
     */
    if(pat[0] == '\\'){
	if(!strcmp(&pat[1], "top")){
	    gotobob(0, 1);
	    mlerase();
	    return(1);
	}

	if(!strcmp(&pat[1], "bottom")){
	    gotoeob(0, 1);
	    mlerase();
	    return(1);
	}
    }

    /*
     * This code is kind of dumb.  What I want is successive C-W 's to 
     * move dot to successive occurences of the pattern.  So, if dot is
     * already sitting at the beginning of the pattern, then we'll move
     * forward a char before beginning the search.  We'll let the
     * automatic wrapping handle putting the dot back in the right 
     * place...
     */
    status = 0;		/* using "status" as int temporarily! */
    while(1){
	if(pat[status] == '\0'){
	    forwchar(0, 1);
	    break;		/* find next occurance! */
	}

	if(status + curwp->w_doto >= llength(curwp->w_dotp) ||
	   !eq(pat[status],lgetc(curwp->w_dotp, curwp->w_doto + status).c))
	  break;		/* do nothing! */
	status++;
    }

    /* search for the pattern */
    
    while (n-- > 0) {
	if((status = forscan(&wrapt,&pat[0],PTBEG)) == FALSE)
	  break;
    }

    /* and complain if not there */
    if (status == FALSE){
	expandp(&pat[0], &show[0], 50);
	emlwrite("\"%s\" not found", show);
    } 
    else if(wrapt == TRUE){
	emlwrite("Search Wrapped");
    }
    else if(status == TRUE)
      emlwrite("");

    return(status);
}


/*
 * Read a pattern. Stash it in the external variable "pat". The "pat" is not
 * updated if the user types in an empty line. If the user typed an empty line,
 * and there is no old pattern, it is an error. Display the old pattern, in the
 * style of Jeff Lomicka. There is some do-it-yourself control expansion.
 * change to using <ESC> to delemit the end-of-pattern to allow <NL>s in
 * the search string.
 */
readpattern(prompt)
char *prompt;
{
	register int s;
	char tpat[NPAT+20];

	strcpy(tpat, prompt);	/* copy prompt to output string */
        if(pat[0] != '\0'){
	    strcat(tpat, " [");	/* build new prompt string */
	    expandp(&pat[0], &tpat[strlen(tpat)], NPAT/2);	/* add old pattern */
	    strcat(tpat, "]");
        }
	strcat(tpat, " : ");

	s = mlreplyd(tpat, tpat, NPAT, QNORML);	 /* Read pattern */

	if (s == TRUE)				/* Specified */
		strcpy(pat, tpat);
	else if (s == FALSE && pat[0] != 0)	/* CR, but old one */
		s = TRUE;

	return(s);
}


forscan(wrapt,patrn,leavep)	/*	search forward for a <patrn>	*/
int	*wrapt;		/* boolean indicating search wrapped */
char *patrn;		/* string to scan for */
int leavep;		/* place to leave point
				PTBEG = begining of match
				PTEND = at end of match		*/

{
	register LINE *curline;		/* current line during scan */
	register int curoff;		/* position within current line */
	register LINE *lastline;	/* last line position during scan */
	register int lastoff;		/* position within last line */
	register int c;			/* character at current position */
	register LINE *matchline;	/* current line during matching */
	register int matchoff;		/* position in matching line */
	register char *patptr;		/* pointer into pattern */
	register int stopoff;		/* offset to stop search */
	register LINE *stopline;	/* line to stop search */

	*wrapt = FALSE;

	/*
	 * the idea is to set the character to end the search at the 
	 * next character in the buffer.  thus, let the search wrap
	 * completely around the buffer.
	 * 
	 * first, test to see if we are at the end of the line, 
	 * otherwise start searching on the next character. 
	 */
	if(curwp->w_doto == llength(curwp->w_dotp)){
		/*
		 * dot is not on end of a line
		 * start at 0 offset of the next line
		 */
		stopoff = curoff  = 0;
		stopline = curline = lforw(curwp->w_dotp);
		if (curwp->w_dotp == curbp->b_linep)
		  *wrapt = TRUE;
	}
	else{
		stopoff = curoff  = curwp->w_doto;
		stopline = curline = curwp->w_dotp;
	}

	/* scan each character until we hit the head link record */

	/*
	 * maybe wrapping is a good idea
	 */
	while (curline){

	    if (curline == curbp->b_linep)
	        *wrapt = TRUE;

		/* save the current position in case we need to
		   restore it on a match			*/

		lastline = curline;
		lastoff = curoff;

		/* get the current character resolving EOLs */

		if (curoff == llength(curline)) {	/* if at EOL */
			curline = lforw(curline);	/* skip to next line */
			curoff = 0;
			c = '\n';			/* and return a <NL> */
		} else
			c = lgetc(curline, curoff++).c;	/* get the char */

		/* test it against first char in pattern */
		if (eq(c, patrn[0]) != FALSE) {	/* if we find it..*/
			/* setup match pointers */
			matchline = curline;
			matchoff = curoff;
			patptr = &patrn[0];

			/* scan through patrn for a match */
			while (*++patptr != 0) {
				/* advance all the pointers */
				if (matchoff == llength(matchline)) {
					/* advance past EOL */
					matchline = lforw(matchline);
					matchoff = 0;
					c = '\n';
				} else
					c = lgetc(matchline, matchoff++).c;

				/* and test it against the pattern */
				if (eq(*patptr, c) == FALSE)
					goto fail;
			}

			/* A SUCCESSFULL MATCH!!! */
			/* reset the global "." pointers */
			if (leavep == PTEND) {	/* at end of string */
				curwp->w_dotp = matchline;
				curwp->w_doto = matchoff;
			} else {		/* at begining of string */
				curwp->w_dotp = lastline;
				curwp->w_doto = lastoff;
			}
			curwp->w_flag |= WFMOVE; /* flag that we have moved */
			return(TRUE);

		}
fail:;			/* continue to search */
		if((curline == stopline) && (curoff == stopoff))
			break;			/* searched everywhere... */
	}
	/* we could not find a match */

	return(FALSE);
}

/* 	expandp:	expand control key sequences for output		*/

expandp(srcstr, deststr, maxlength)

char *srcstr;	/* string to expand */
char *deststr;	/* destination of expanded string */
int maxlength;	/* maximum chars in destination */

{
	char c;		/* current char to translate */

	/* scan through the string */
	while ((c = *srcstr++) != 0) {
		if (c == '\n') {		/* its an EOL */
			*deststr++ = '<';
			*deststr++ = 'N';
			*deststr++ = 'L';
			*deststr++ = '>';
			maxlength -= 4;
		} else if (c < 0x20 || c == 0x7f) {	/* control character */
			*deststr++ = '^';
			*deststr++ = c ^ 0x40;
			maxlength -= 2;
		} else if (c == '%') {
			*deststr++ = '%';
			*deststr++ = '%';
			maxlength -= 2;

		} else {			/* any other character */
			*deststr++ = c;
			maxlength--;
		}

		/* check for maxlength */
		if (maxlength < 4) {
			*deststr++ = '$';
			*deststr = '\0';
			return(FALSE);
		}
	}
	*deststr = '\0';
	return(TRUE);
}
