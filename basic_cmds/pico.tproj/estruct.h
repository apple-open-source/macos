/*
 * $Id: estruct.h,v 1.3 2003/01/12 01:48:05 bbraun Exp $
 *
 * Program:	Struct and preprocessor definitions
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
/*	ESTRUCT:	Structure and preprocesser defined for
			MicroEMACS 3.6

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	ESTRUCT_H
#define	ESTRUCT_H

/*	Configuration options	*/

#define CVMVAS  1	/* arguments to page forward/back in pages	*/
#define	NFWORD	1	/* forward word jumps to begining of word	*/
#define	TYPEAH	0	/* type ahead causes update to be skipped	*/
#define	REVSTA	1	/* Status line appears in reverse video		*/


/*	internal constants	*/

#define	NBINDS	50			/* max # of bound keys		*/
#define NFILEN  80                      /* # of bytes, file name        */
#define NBUFN   16                      /* # of bytes, buffer name      */
#define NLINE   256                     /* # of bytes, line             */
#define	NSTRING	256			/* # of bytes, string buffers	*/
#define NPAT    80                      /* # of bytes, pattern          */
#undef	HUGE
#define HUGE    1000                    /* Huge number                  */
#define	NLOCKS	100			/* max # of file locks active	*/

#define AGRAVE  0x60                    /* M- prefix,   Grave (LK201)   */
#define METACH  0x1B                    /* M- prefix,   Control-[, ESC  */
#define CTMECH  0x1C                    /* C-M- prefix, Control-\       */
#define EXITCH  0x1D                    /* Exit level,  Control-]       */
#define CTRLCH  0x1E                    /* C- prefix,   Control-^       */
#define HELPCH  0x1F                    /* Help key,    Control-_       */

#undef  CTRL
#define CTRL    0x0100                  /* Control flag, or'ed in       */
#define META    0x0200                  /* Meta flag, or'ed in          */
#define CTLX    0x0400                  /* ^X flag, or'ed in            */
#define	SPEC	0x0800			/* special key (arrow's, etc)	*/
#define	FUNC	0x1000			/* special key (function keys)	*/
#if	MSDOS
#define	SHFT	0x2000			/* shifted (for function keys)	*/
#define	ALTD	0x4000			/* ALT key...			*/
#endif

#define	QNORML	0x0000			/* Flag meaning no flag ;)	*/
#define	QFFILE	0x0001			/* Flag buffer for file neme	*/
#define	QDEFLT	0x0002			/* Flag to use default answer	*/
#define	QPAGE	0x0004			/* Flag to allow ^V and ^Y	*/

#undef	FALSE
#define FALSE   0                       /* False, no, bad, etc.         */
#undef	TRUE
#define TRUE    1                       /* True, yes, good, etc.        */
#define ABORT   2                       /* Death, ^G, abort, etc.       */

#define FIOSUC  0                       /* File I/O, success.           */
#define FIOFNF  1                       /* File I/O, file not found.    */
#define FIOEOF  2                       /* File I/O, end of file.       */
#define FIOERR  3                       /* File I/O, error.             */
#define	FIOLNG	4			/*line longer than allowed len	*/
#define	FIODIR	5			/* File is a directory		*/
#define	FIONWT	6			/* File lacks write permission	*/
#define	FIONRD	7			/* File lacks read permission	*/
#define	FIONEX	8			/* File lacks exec permission	*/


#define CFCPCN  0x0001                  /* Last command was C-P, C-N    */
#define CFKILL  0x0002                  /* Last command was a kill      */
#define CFFILL  0x0004                  /* Last command was a kill      */

#define	BELL	0x07			/* a bell character		*/
#define	TAB	0x09			/* a tab character		*/


/*
 * There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay; although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */
typedef struct  WINDOW {
        struct  WINDOW *w_wndp;         /* Next window                  */
        struct  BUFFER *w_bufp;         /* Buffer displayed in window   */
        struct  LINE *w_linep;          /* Top line in the window       */
        struct  LINE *w_dotp;           /* Line containing "."          */
        short   w_doto;                 /* Byte offset for "."          */
        struct  LINE *w_markp;          /* Line containing "mark"       */
        short   w_marko;                /* Byte offset for "mark"       */
        struct  LINE *w_imarkp;         /* INTERNAL Line with "mark"    */
        short   w_imarko;               /* INTERNAL "mark" byte offset  */
        char    w_toprow;               /* Origin 0 top row of window   */
        char    w_ntrows;               /* # of rows of text in window  */
        char    w_force;                /* If NZ, forcing row.          */
        char    w_flag;                 /* Flags.                       */
}       WINDOW;

#define WFFORCE 0x01                    /* Window needs forced reframe  */
#define WFMOVE  0x02                    /* Movement from line to line   */
#define WFEDIT  0x04                    /* Editing within a line        */
#define WFHARD  0x08                    /* Better to a full display     */
#define WFMODE  0x10                    /* Update mode line.            */

/*
 * Text is kept in buffers. A buffer header, described below, exists for every
 * buffer in the system. The buffers are kept in a big list, so that commands
 * that search for a buffer by name can find the buffer header. There is a
 * safe store for the dot and mark in the header, but this is only valid if
 * the buffer is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with a pointer to
 * the header line in "b_linep".
 * 	Buffers may be "Inactive" which means the files accosiated with them
 * have not been read in yet. These get read in at "use buffer" time.
 */
typedef struct  BUFFER {
        struct  BUFFER *b_bufp;         /* Link to next BUFFER          */
        struct  LINE *b_dotp;           /* Link to "." LINE structure   */
        short   b_doto;                 /* Offset of "." in above LINE  */
        struct  LINE *b_markp;          /* The same as the above two,   */
        short   b_marko;                /* but for the "mark"           */
        struct  LINE *b_linep;          /* Link to the header LINE      */
	char	b_active;		/* window activated flag	*/
        char    b_nwnd;                 /* Count of windows on buffer   */
        char    b_flag;                 /* Flags                        */
	char	b_mode;			/* editor mode of this buffer	*/
        char    b_fname[NFILEN];        /* File name                    */
        char    b_bname[NBUFN];         /* Buffer name                  */
}       BUFFER;

#if TERMCAP || TERMINFO
struct  KBSTREE {
	char	value;
        int     func;              /* Routine to handle it         */
	struct	KBSTREE *down; 
	struct	KBSTREE	*left;
};
#endif

#define BFTEMP  0x01                    /* Internal temporary buffer    */
#define BFCHG   0x02                    /* Changed since last write     */


/*
 * The starting position of a region, and the size of the region in
 * characters, is kept in a region structure.  Used by the region commands.
 */
typedef struct  {
        struct  LINE *r_linep;          /* Origin LINE address.         */
        short   r_offset;               /* Origin LINE offset.          */
        long    r_size;                 /* Length in characters.        */
}       REGION;


/*
 * character and attribute pair.  The basic building block
 * of the editor.  The bitfields may have to be changed to a char
 * and short if there are problems...
 */
typedef	struct CELL {
	unsigned int c : 8;		/* Character value in cell      */
	unsigned int a : 8;		/* Its attributes               */
} CELL;


/*
 * All text is kept in circularly linked lists of "LINE" structures. These
 * begin at the header line (which is the blank line beyond the end of the
 * buffer). This line is pointed to by the "BUFFER". Each line contains a the
 * number of bytes in the line (the "used" size), the size of the text array,
 * and the text. The end of line is not stored as a byte; it's implied. Future
 * additions will include update hints, and a list of marks into the line.
 */
typedef struct  LINE {
        struct  LINE *l_fp;             /* Link to the next line        */
        struct  LINE *l_bp;             /* Link to the previous line    */
        short   l_size;                 /* Allocated size               */
        short   l_used;                 /* Used size                    */
        CELL    l_text[1];              /* A bunch of characters.       */
}       LINE;

#define lforw(lp)       ((lp)->l_fp)
#define lback(lp)       ((lp)->l_bp)
#define lgetc(lp, n)    ((lp)->l_text[(n)])
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define llength(lp)     ((lp)->l_used)

/*
 * The editor communicates with the display using a high level interface. A
 * "TERM" structure holds useful variables, and indirect pointers to routines
 * that do useful operations. The low level get and put routines are here too.
 * This lets a terminal, in addition to having non standard commands, have
 * funny get and put character code too. The calls might get changed to
 * "termp->t_field" style in the future, to make it possible to run more than
 * one terminal type.
 */
typedef struct  {
        short   t_nrow;                 /* Number of rows.              */
        short   t_ncol;                 /* Number of columns.           */
	short	t_margin;		/* min margin for extended lines*/
	short	t_scrsiz;		/* size of scroll region "	*/
        int     (*t_open)();            /* Open terminal at the start.  */
        int     (*t_close)();           /* Close terminal at end.       */
        int     (*t_getchar)();         /* Get character from keyboard. */
        int     (*t_putchar)();         /* Put character to display.    */
        int     (*t_flush)();           /* Flush output buffers.        */
        int     (*t_move)();            /* Move the cursor, origin 0.   */
        int     (*t_eeol)();            /* Erase to end of line.        */
        int     (*t_eeop)();            /* Erase to end of page.        */
        int     (*t_beep)();            /* Beep.                        */
	int	(*t_rev)();		/* set reverse video state	*/
}       TERM;

/*	structure for the table of initial key bindings		*/

typedef struct  {
        short   k_code;                 /* Key code                     */
        int     (*k_fp)();              /* Routine to handle it         */
}       KEYTAB;

/*      sturcture used for key menu painting         */

typedef struct {
	char	*name;			/* key to display  		*/
	char	*label;			/* function name key envokes	*/
}	KEYMENU;


#endif	/* ESTRUCT_H */
