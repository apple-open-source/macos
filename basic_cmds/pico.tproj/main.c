#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: main.c,v 1.2 2002/01/03 22:16:39 jevans Exp $";
#endif
/*
 * Program:	Main stand-alone Pine Composer routines
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
 * WEEMACS/PICO NOTES:
 *
 * 08 Jan 92 - removed PINE defines to simplify compiling
 *
 * 08 Apr 92 - removed PINE stub calls
 *
 */

#include        <stdio.h>
#include	<setjmp.h>
#include	"osdep.h"	/* operating system dependent includes */
#include	"pico.h"	/* pine composer definitions */
#include        "estruct.h"	/* global structures and defines */
#include	"efunc.h"	/* function declarations and sans name table */
#include	"edef.h"	/* global definitions */


/*
 * this isn't defined in the library, because it's a pine global
 * which we use for GetKey's timeout
 */
int	timeout = 0;			/* global timeout value		*/

/*
 * function key mappings
 */
static int fkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'X')},
    { F3,  (CTRL|'O')},
    { F4,  (CTRL|'J')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'W')},
    { F7,  (CTRL|'Y')},
    { F8,  (CTRL|'V')},
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'C')},
#ifdef	SPELLER
    { F12, (CTRL|'T')}
#else
    { F12, (CTRL|'D')}
#endif
};


/*
 * main standalone pico routine
 */
#ifdef _WINDOWS
app_main (argc, argv)
#else
main(argc, argv)
#endif
char    *argv[];
{
    register int    c;
    register int    f;
    register int    n;
    register BUFFER *bp;
    register int    carg;		/* current arg to scan 		*/
    int	     viewflag = FALSE;		/* are we starting in view mode?*/
    int	     starton = 0;		/* where's dot to begin with?	*/
    char     bname[NBUFN];		/* buffer name of file to read	*/
    char    *clerr = NULL;		/* garbage on command line	*/
    
    Pmaster = NULL;			/* turn OFF composer functionality */

    /*
     * Read command line flags before initializing, otherwise, we never
     * know to init for f_keys...
     */
    carg = 1;
    while(carg < argc){
	if(argv[carg][0] == '-'){
	    switch(argv[carg][1]){
	      case 'v':			/* -v for View File */
	      case 'V':
		viewflag = !viewflag;
		break;
	      case 'f':			/* -f for function key use */
		gmode ^= MDFKEY;
		break;
	      case 'n':			/* -n for new mail notification */
		timeout = 180;
		if(argv[carg][2] != '\0')
		  if((timeout = atoi(&argv[carg][2])) < 30)
		    timeout = 180;
		break;
	      case 't':			/* special shutdown mode */
		gmode ^= MDTOOL;
		rebindfunc(wquit, quickexit);
		break;
	      case 'z':			/* -z to suspend */
		gmode ^= MDSSPD;
		break;
	      case 'w':			/* -w turn off word wrap */
		gmode ^= MDWRAP;
		break;
#if	defined(DOS)
	      case 'c':			/* -c[nr][fb] colors */
		if(carg + 1 < argc){
		    if(argv[carg][2] == 'n'){
			if(argv[carg][3] == 'f')
			  pico_nfcolor(argv[++carg]);
			else if(argv[carg][3] == 'b')
			  pico_nbcolor(argv[++carg]);
		    }
		    else if(argv[carg][2] == 'r'){
			if(argv[carg][3] == 'f')
			  pico_rfcolor(argv[++carg]);
			else if(argv[carg][3] == 'b')
			  pico_rbcolor(argv[++carg]);
		    }
		}
		else{
		    clerr = "insufficient args for \"-c\"";
		    break;
		}
		break;
#endif
	      default:			/* huh? */
		clerr = argv[carg];
		break;
	    }
	    carg++;
	}
	else if(argv[carg][0] == '+'){	/* leading '+' is special */
	    starton = atoi(&argv[carg][1]);
	    carg++;
	}
	else				/* pick up file name later... */
	  break;
    }

    if(!vtinit())			/* Displays.            */
	exit(1);

    strcpy(bname, "main");		/* default buffer name */
    edinit(bname);			/* Buffers, windows.   */

    update();				/* let the user know we are here */

#ifdef	_WINDOWS
    mswin_allowpaste(MSWIN_PASTE_FULL);
    mswin_setclosetext("Use the ^X command to exit Pico.");
    mswin_allowexit (MSWIN_EXIT_SENDCHAR, NULL, 0x18);
#endif

#if	TERMCAP
    if(kpadseqs == NULL){		/* will arrow keys work ? */
	(*term.t_putchar)('\007');
	emlwrite("Warning: keypad keys may non-functional", NULL);
    }
#endif	/* TERMCAP */

    if(carg < argc){			/* Any file to edit? */

	makename(bname, argv[carg]);	/* set up a buffer for this file */

	bp = curbp;			/* read in first file */
	makename(bname, argv[carg]);
	strcpy(bp->b_bname, bname);
	strcpy(bp->b_fname, argv[carg]);
	if (readin(argv[carg], (viewflag==FALSE)) == ABORT) {
	    strcpy(bp->b_bname, "main");
	    strcpy(bp->b_fname, "");
	}
	bp->b_dotp = bp->b_linep;
	bp->b_doto = 0;

	if (viewflag)			/* set the view mode */
	  bp->b_mode |= MDVIEW;
    }

    /* setup to process commands */
    lastflag = 0;			/* Fake last flags.     */
    curbp->b_mode |= gmode;		/* and set default modes*/

    curwp->w_flag |= WFMODE;		/* and force an update	*/

    if(timeout)
      emlwrite("Checking for new mail every %D seconds", (void *)timeout);

    if(clerr){				/* post any errors on command line */
	if(mpresf)			/* show earlier message though! */
	  sleep(2);
	emlwrite("\007Unknown option: %s", clerr);
    }

    forwline(0, starton - 1);		/* move dot to specified line */

    while(1){

	update();			/* Fix up the screen    */

#if	defined(DOS) && defined(MOUSE)
	register_mfunc(pico_mouse, 2, 0,term.t_nrow-3,term.t_ncol);
#endif
	c = GetKey();	

	if(timeout && (c == NODATA || time_to_check())){
	    if(pico_new_mail())
	      emlwrite("You may possibly have new mail.", NULL);
	}

	if(c == NODATA)
	  continue;

	if(mpresf){			/* erase message line? */
	    if(mpresf++ > MESSDELAY)
	      mlerase();
	}

	f = FALSE;
	n = 1;

#if	defined(DOS) && defined(MOUSE)
	clear_mfunc();
#endif
	execute(normal(c, fkm, 1), f, n);	/* Do it.               */
    }
}
