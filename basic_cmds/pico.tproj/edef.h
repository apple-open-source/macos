/*
 * $Id: edef.h,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $
 *
 * Program:	Global definitions and initializations
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
/*	EDEF:		Global variable definitions for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	EDEF_H
#define	EDEF_H

#ifdef	maindef

/* for MAIN.C */

/* initialized global definitions */

int     fillcol = 72;                   /* Current fill column          */
short   kbdm[NKBDM] = {CTLX|')'};       /* Macro                        */
char    pat[NPAT];                      /* Search pattern		*/
char	rpat[NPAT];			/* replacement pattern		*/
char	sarg[NSTRING] = "";		/* string argument for line exec*/
int	eolexist = TRUE;		/* does clear to EOL exist	*/
int	optimize = FALSE;		/* optimize flag(cf line speed)	*/
int	scrollexist = TRUE;		/* does insert line exist	*/
int	inschar = TRUE;			/* does insert character exist	*/
int	delchar = TRUE;			/* does delete character exist	*/
int     sgarbk = TRUE;                  /* TRUE if keyhelp garbaged     */
char	s[16384];			/* global temporary string	*/
int     mline_open = FALSE;             /* TRUE if message line is open */
int	ComposerTopLine = 2;		/* TRUE if message line is open */
int	ComposerEditing = FALSE;	/* TRUE if message line is open */
int	revexist = FALSE;		/* does reverse video exist?	*/
char	*modename[] = {			/* name of modes		*/
	"WRAP", "CMODE", "SPELL", "EXACT", "VIEW", "OVER"};
char	modecode[] = "WCSEVO";		/* letters to represent modes	*/
int	gmode = MDSSPD;			/* global editor mode		*/
int     sgarbf  = TRUE;                 /* TRUE if screen is garbage	*/
int     mpresf  = FALSE;                /* TRUE if message in last line */
int	clexec	= FALSE;		/* command line execution flag	*/

/* uninitialized global definitions */
int     currow;                 /* Cursor row                   */
int     curcol;                 /* Cursor column                */
int     thisflag;               /* Flags, this command          */
int     lastflag;               /* Flags, last command          */
int     curgoal;                /* Goal for C-P, C-N            */
WINDOW  *curwp;                 /* Current window               */
BUFFER  *curbp;                 /* Current buffer               */
WINDOW  *wheadp;                /* Head of list of windows      */
BUFFER  *bheadp;                /* Head of list of buffers      */
BUFFER  *blistp;                /* Buffer for C-X C-B           */
short   *kbdmip;                /* Input pointer for above      */
short   *kbdmop;                /* Output pointer for above     */

BUFFER  *bfind();               /* Lookup a buffer by name      */
WINDOW  *wpopup();              /* Pop up window creation       */
LINE    *lalloc();              /* Allocate a line              */

#else

/* for all the other .C files */

/* initialized global external declarations */

extern  int     fillcol;                /* Fill column                  */
extern  short   kbdm[];                 /* Holds kayboard macro data    */
extern  char    pat[];                  /* Search pattern               */
extern	char	rpat[];			/* Replacement pattern		*/
extern	char	sarg[];			/* string argument for line exec*/
extern	int	eolexist;		/* does clear to EOL exist?	*/
extern	int	optimize;		/* optimize flag(cf line speed)	*/
extern	int	scrollexist;		/* does insert line exist	*/
extern	int	inschar;		/* does insert character exist	*/
extern	int	delchar;		/* does delete character exist	*/
extern  int     sgarbk;
extern  char	s[];			/* global temporary string	*/
extern  int     mline_open;             /* Message line is open         */
extern	int	ComposerTopLine;	/* TRUE if message line is open */
extern	int	ComposerEditing;	/* TRUE if message line is open */
extern	int	timeout;		/* how long we wait in GetKey	*/
extern	int	revexist;		/* does reverse video exist?	*/
extern	char *modename[];		/* text names of modes		*/
extern	char	modecode[];		/* letters to represent modes	*/
extern	KEYTAB keytab[];		/* key bind to functions table	*/
extern	KEYTAB pkeytab[];		/* pico's function table	*/
extern	NBIND names[];			/* name to function table	*/
extern	int	gmode;			/* global editor mode		*/
extern  int     sgarbf;                 /* State of screen unknown      */
extern  int     mpresf;                 /* Stuff in message line        */
extern	int	clexec;			/* command line execution flag	*/

/* initialized global external declarations */
extern  int     currow;                 /* Cursor row                   */
extern  int     curcol;                 /* Cursor column                */
extern  int     thisflag;               /* Flags, this command          */
extern  int     lastflag;               /* Flags, last command          */
extern  int     curgoal;                /* Goal for C-P, C-N            */
extern  WINDOW  *curwp;                 /* Current window               */
extern  BUFFER  *curbp;                 /* Current buffer               */
extern  WINDOW  *wheadp;                /* Head of list of windows      */
extern  BUFFER  *bheadp;                /* Head of list of buffers      */
extern  BUFFER  *blistp;                /* Buffer for C-X C-B           */
extern  short   *kbdmip;                /* Input pointer for above      */
extern  short   *kbdmop;                /* Output pointer for above     */

extern  BUFFER  *bfind();               /* Lookup a buffer by name      */
extern  WINDOW  *wpopup();              /* Pop up window creation       */
extern  LINE    *lalloc();              /* Allocate a line              */

#endif

/* terminal table defined only in TERM.C */

#ifndef	termdef
extern  TERM    term;                   /* Terminal information.        */
#endif

#endif	/* EDEF_H */
