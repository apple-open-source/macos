#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bind.c,v 1.2 2002/01/03 22:16:39 jevans Exp $";
#endif
/*
 * Program:	Key binding routines
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
/*	This file is for functions having to do with key bindings,
	descriptions, help commands, and command line execution.

	written 11-feb-86 by Daniel Lawrence
								*/

#include	<stdio.h>
#include	"estruct.h"
#include	"edef.h"
#include	"pico.h"

#ifdef	ANSI
    int arraylen(char **);
#else
    int arraylen();
#endif

/* 
 * help - help function for pico (UW pared down version of uemacs).
 *	  this function will intentionally garbage with helpful 
 *	  tips and then wait for a ' ' to be hit to then update the 
 *        screen.
 */


static char *helptext[] = {
    "\tPico Help Text",
    " ",
    "\tPico is designed to be a simple, easy-to-use text editor with a",
    "\tlayout very similar to the pine mailer.  The status line at the",
    "\ttop of the display shows pico's version, the current file being",
    "\tedited and whether or not there are outstanding modifications",
    "\tthat have not been saved.  The third line from the bottom is used",
    "\tto report informational messages and for additional command input.",
    "\tThe bottom two lines list the available editing commands.",
    " ",
    "\tEach character typed is automatically inserted into the buffer",
    "\tat the current cursor position.  Editing commands and cursor",
    "\tmovement (besides arrow keys) are given to pico by typing",
    "\tspecial control-key sequences.  A caret, '^', is used to denote",
    "~\tthe control key, sometimes marked \"CTRL\", so the ~C~T~R~L~-~q key",
    "~\tcombination is written as ~^~Q.",
    " ",
    "\tThe following functions are available in pico (where applicable,",
    "\tcorresponding function key commands are in parentheses).",
    " ",
    "~\t~^~G (~F~1)   Display this help text.",
    " ",
    "~\t~^~F        move Forward a character.",
    "~\t~^~B        move Backward a character.",
    "~\t~^~P        move to the Previous line.",
    "~\t~^~N        move to the Next line.",
    "~\t~^~A        move to the beginning of the current line.",
    "~\t~^~E        move to the End of the current line.",
    "~\t~^~V (~F~8)   move forward a page of text.",
    "~\t~^~Y (~F~7)   move backward a page of text.",
    " ",
    "~\t~^~W (~F~6)   Search for (where is) text, neglecting case.",
    "~\t~^~L        Refresh the display.",
    " ",
    "~\t~^~D        Delete the character at the cursor position.",
    "~\t~^~^        Mark cursor position as beginning of selected text.",
    "\t\t  Note: Setting mark when already set unselects text.",
    "~\t~^~K (~F~9)   Cut selected text (displayed in inverse characters).",
    "\t\t  Note: The selected text's boundary on the cursor side",
    "\t\t        ends at the left edge of the cursor.  So, with ",
    "\t\t        selected text to the left of the cursor, the ",
    "\t\t        character under the cursor is not selected.",
    "~\t~^~U (~F~1~0)  Uncut (paste) last cut text inserting it at the",
    "\t\t  current cursor position.",
    "~\t~^~I        Insert a tab at the current cursor position.",
    " ",
    "~\t~^~J (~F~4)   Format (justify) the current paragraph.",
    "\t\t  Note: paragraphs delimited by blank lines or indentation.",
    "~\t~^~T (~F~1~2)  To invoke the spelling checker",
    "~\t~^~C (~F~1~1)  Report current cursor position",
    " ",
    "~\t~^~R (~F~5)   Insert an external file at the current cursor position.",
    "~\t~^~O (~F~3)   Output the current buffer to a file, saving it.",
    "~\t~^~X (~F~2)   Exit pico, saving buffer.",
    "    ",
    "\tPine and Pico are trademarks of the University of Washington.",
    "\tNo commercial use of these trademarks may be made without prior",
    "\twritten permission of the University of Washington.",
    "    ",
    "    End of Help.",
    " ",
    NULL
};


/*
 * arraylen - return the number of bytes in an array of char
 */
arraylen(array)
char **array;
{
    register int i=0;

    while(array[i++] != NULL) ;
    return(i);
}


/*
 * whelp - display help text for the composer and pico
 */
whelp(f, n)
{
    if(Pmaster){
	(*Pmaster->helper)(Pmaster->composer_help, "Help on the Pine Composer", 1);
	curwp->w_flag |= WFMODE;
    }
    else
      pico_help(helptext);

        sgarbf = TRUE;
}

static KEYMENU menu_scroll[] = {
    {NULL, NULL},		{NULL, NULL},	{NULL, NULL},
    {NULL, NULL},		{NULL, NULL},	{NULL, NULL},
    {"^X", "Exit Help"},	{NULL, NULL},	{NULL, NULL},
    {NULL, NULL},		{NULL, NULL},	{NULL, NULL}
};
#define	PREV_KEY	3
#define	NEXT_KEY	9


#define	OVERLAP	2		/* displayed page overlap */

/*
 * scrollw - takes beginning row and ending row to diplay an array
 *           of text lines.  returns either 0 if scrolling terminated
 *           normally or the value of a ctrl character typed to end it.
 *
 * updates - 
 *     01/11/89 - added stripe call if 1st char is tilde - '~'
 *
 */
wscrollw(begrow, endrow, textp, textlen)
int	begrow, endrow;
char	*textp[];
int	textlen;
{
    register int	loffset = 0; 
    register int	prevoffset = -1; 
    register int	dlines;
    register int	i;
    register int	cont;
    register int	done = 0;
    register char	*buf;
    int	 c;
     
    dlines = endrow - begrow - 1;
    while(!done) {
        /*
         * diplay a page loop ...
         */
	if(prevoffset != loffset){
       	    for(i = 0; i < dlines; i++){
                movecursor(i + begrow, 0);
                peeol();
                if((loffset+i) < textlen){
		    buf = &(textp[loffset+i][0]);
		    if(*buf == '~'){
			buf++;
			wstripe(begrow+i, 0, buf, '~');
		    }
		    else{
			pputs(buf, 0);
		    }
                }
            }
	    /*
	     * put up the options prompt
	     */
            movecursor(begrow + dlines, 0);
            cont = (loffset+dlines < textlen);
            if(cont){                               /* continue ? */
		menu_scroll[NEXT_KEY].name  = "^V";
		menu_scroll[NEXT_KEY].label = "Next Pg";
	    }
	    else
	      menu_scroll[NEXT_KEY].name = NULL;

	    if(loffset){
		menu_scroll[PREV_KEY].name  = "^Y";
		menu_scroll[PREV_KEY].label = "Prev Pg";
	    }
	    else
	      menu_scroll[PREV_KEY].name = NULL;

	    wkeyhelp(menu_scroll);
	}

	(*term.t_flush)();

        c = GetKey();

	prevoffset = loffset;
	switch(c){
	    case  (CTRL|'X') :		/* quit */
 	    case  F2  :
		done = 1;
		break;
	    case  (CTRL|'Y') :		/* prev page */
	    case  F7  :			/* prev page */
		if((loffset-dlines-OVERLAP) > 0){
               	    loffset -= (dlines-OVERLAP);
		}
	        else{
		    if(loffset != 0){
			prevoffset = -1;
		    }
		    else{
		    	(*term.t_beep)();
		    }
		    loffset = 0;
	        }
		break;
	    case  (CTRL|'V') :			/* next page */
 	    case  F8  :
		if(cont){
               	   loffset += (dlines-OVERLAP);
		}
		else{
		   (*term.t_beep)();
		}
		break;
	    case  '\016' :		/* prev-line */
	    case  (CTRL|'N') :
	      if(cont)
		loffset++;
	      else
		(*term.t_beep)();
	      break;
	    case  '\020' :		/* prev-line */
	    case  (CTRL|'P') :
	      if(loffset > 0)
		loffset--;
	      else
		(*term.t_beep)();
	      break;
	    case  '\014' :		/* refresh */
	    case  (CTRL|'L') :		/* refresh */
	        modeline(curwp);
		update();
		prevoffset = -1;
		break;
	    case  NODATA :
	        break;
	    default :
		emlwrite("Unknown Command.");
		(*term.t_beep)();
		break;
	}
    }
    return(TRUE);
}


/*
 * normal - given a char and list of function key to command key mappings, 
 *          return, depending on gmode, the right command.  The list is
 *          an array of (Fkey, command-key) pairs.  sc is the index in the
 *          array that means to ignore fkey vs. command key mapping
 *
 *          rules:  1. if c not in table (either fkey or command), let it thru
 *                  2. if c matches, but in other mode, punt it
 */
normal(c, list, sc)
int c, sc;
int list[][2];
{
    register int i;

    for(i=0; i < 12; i++){
	if(c == list[i][(FUNC&c) ? 0 : 1]){	/* in table? */
	    if(i == sc)				/* SPECIAL CASE! */
	      return(list[i][1]);

	    if(list[i][1] == NODATA)		/* no mapping ! */
	      return(c);

	    if(((FUNC&c) == FUNC) ^ ((gmode&MDFKEY) == MDFKEY))
	      return(BADESC);			/* keystroke not allowed! */
	    else
	      return(list[i][1]);		/* never return func keys */
	}
    }

    return(c);
}


/*
 * rebind - replace the first function with the second
 */
rebindfunc(a, b)
int (*a)(), (*b)();
{
    KEYTAB *kp;

    kp = (Pmaster) ? &keytab[0] : &pkeytab[0];

    while(kp->k_fp != NULL){		/* go thru whole list, and */
	if(kp->k_fp == a)
	  kp->k_fp = b;			/* replace all occurances */
	kp++;
    }
}
