/*
 * $Id: pico.h,v 1.3 2003/01/12 01:48:11 bbraun Exp $
 *
 * Program:	pico.h - definitions for Pine's composer library
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

#ifndef	PICO_H
#define	PICO_H
/*
 * Defined for attachment support
 */
#define	ATTACHMENTS	1


/*
 * defs of return codes from pine mailer composer.
 */
#define	BUF_CHANGED	0x01
#define	COMP_CANCEL	0x02
#define	COMP_EXIT	0x04
#define	COMP_FAILED	0x08
#define	COMP_SUSPEND	0x10
#define	COMP_GOTHUP	0x20


/*
 * top line from the top of the screen for the editor to do 
 * its stuff
 */
#define	COMPOSER_TOP_LINE	2
#define	COMPOSER_TITLE_LINE	0



/*
 * definitions of Mail header structures 
 */
struct hdr_line {
        char text[256];
        struct  hdr_line        *next;
        struct  hdr_line        *prev;
};
 
 
/* 
 *  This structure controls the header line items on the screen.  An
 * instance of this should be created and passed in as an argument when
 * pico is called. The list is terminated by an entry with the name
 * element NULL.
 */
 
struct headerentry {
        char     *prompt;
	char	 *name;
#ifdef	DOS
	short	  help;
#else
	char	**help;
#endif
        int	  prlen;
        int	  maxlen;
        char    **realaddr;
        int     (*builder)();        /* Function to verify/canonicalize val */
	struct headerentry        *affected_entry;
				     /* entry builder's 4th arg affects     */
        char   *(*selector)();       /* Browser for possible values         */
        char     *key_label;         /* Key label for key to call browser   */
        unsigned  display_it:1;	     /* field is to be displayed by default */
        unsigned  break_on_comma:1;  /* Field breaks on commas              */
        unsigned  is_attach:1;       /* Special case field for attachments  */
        unsigned  rich_header:1;     /* Field is part of rich header        */
        unsigned  only_file_chars:1; /* Field is a file name                */
        unsigned  single_space:1;    /* Crush multiple spaces into one      */
        unsigned  sticky:1;          /* Can't change this via affected_entry*/
        struct    hdr_line        *hd_text;
};


/*
 * structure to keep track of header display
 */
struct on_display {
    int			 p_off;			/* offset into line */
    int			 p_len;			/* length of line   */
    int			 p_line;		/* physical line on screen */
    int			 top_e;			/* topline's header entry */
    struct hdr_line	*top_l;			/* top line on display */
    int			 cur_e;			/* current header entry */
    struct hdr_line	*cur_l;			/* current hd_line */
};						/* global on_display struct */


/*
 * Structure to handle attachments
 */
typedef struct pico_atmt {
    char *description;
    char *filename;
    char *size;
    char *id;
    unsigned short flags;
    struct pico_atmt *next;
} PATMT;


/*
 * Flags for attachment handling
 */
#define	A_FLIT	0x0001			/* Accept literal file and size      */
#define	A_ERR	0x0002			/* Problem with specified attachment */


/*
 * Master pine composer structure.  Right now there's not much checking
 * that any of these are pointing to something, so pine must have them pointing
 * somewhere.
 */
typedef struct pico_struct {
    void  *msgtext;			/* ptrs to malloc'd arrays of char */
    char  *pine_anchor;			/* ptr to pine anchor line */
    char  *pine_version;		/* string containing Pine's version */
    char  *alt_ed;			/* name of alternate editor or NULL */
    PATMT *attachments;			/* linked list of attachments */
    unsigned pine_flags;		/* entry mode flags */
    void  (*helper)();			/* Pine's help function  */
    int   (*showmsg)();			/* Pine's display_message */
    void  (*clearcur)();		/* Pine's clear cursor position func */
    void  (*keybinit)();		/* Pine's keyboard initializer  */
    int   (*raw_io)();			/* Pine's Raw() */
    long  (*newmail)();			/* Pine's report_new_mail */
#ifdef	DOS
    short search_help;
    short ins_help;
    short composer_help;
    short browse_help;
#else
    char  **search_help;
    char  **ins_help;
    char  **composer_help;
    char  **browse_help;
#endif
    struct headerentry *headents;
} PICO;

/*
 * various flags that they may passed to PICO
 */
#define	P_LOCALLF	0x8000		/* use local vs. NVT EOL	   */
#define	P_BODY		0x4000		/* start composer in body	   */
#define	P_HEADEND	0x2000		/* start composer at end of header */
#define	P_FKEYS		MDFKEY		/* run in function key mode 	   */
#define	P_SECURE	MDSCUR		/* run in restricted (demo) mode   */
#define	P_SUSPEND	MDSSPD		/* allow ^Z suspension		   */
#define	P_ADVANCED	MDADVN		/* enable advanced features	   */
#define	P_CURDIR	MDCURDIR	/* use current dir for lookups	   */
#define	P_ALTNOW	MDALTNOW	/* enter alt ed with hesitation	   */

/*
 * definitions for various PICO modes 
 */
#define	MDWRAP		0x0001		/* word wrap			*/
#define	MDSPELL		0x0002		/* spell error parcing		*/
#define	MDEXACT		0x0004		/* Exact matching for searches	*/
#define	MDVIEW		0x0008		/* read-only buffer		*/
#define MDOVER		0x0010		/* overwrite mode		*/
#define MDFKEY		0x0020		/* function key  mode		*/
#define MDSCUR		0x0040		/* secure (for demo) mode	*/
#define MDSSPD		0x0080		/* suspendable mode		*/
#define MDADVN		0x0100		/* Pico's advanced mode		*/
#define MDTOOL		0x0200		/* "tool" mode (quick exit)	*/
#define MDBRONLY	0x0400		/* indicates standalone browser	*/
#define MDCURDIR	0x0800		/* use current dir for lookups	*/
#define MDALTNOW	0x1000		/* enter alt ed with hesitation */


/*
 * Main defs 
 */
#ifdef	maindef
PICO	*Pmaster = NULL;		/* composer specific stuff */
char	*version = "2.5";		/* PICO version number */
#else
extern PICO *Pmaster;			/* composer specific stuff */
extern char *version;			/* pico version! */
#endif


/*
 * Flags for FileBrowser call
 */
#define FB_READ		0x0001		/* Looking for a file to read. */
#define FB_SAVE		0x0002		/* Looking for a file to save. */


/*
 * number of keystrokes to delay removing an error message, or new mail
 * notification
 */
#define	MESSDELAY	25
#define	NMMESSDELAY	60


/*
 * defs for keypad and function keys...
 */
#define K_PAD_UP        0x0811
#define K_PAD_DOWN      0x0812
#define K_PAD_RIGHT     0x0813
#define K_PAD_LEFT      0x0814
#define K_PAD_PREVPAGE  0x0815
#define K_PAD_NEXTPAGE	0x0816
#define K_PAD_HOME	0x0817
#define K_PAD_END	0x0818
#define K_PAD_DELETE	0x0819
#define BADESC          0x0820
#define NODATA          0x08FF
 
/*
 * defines for function keys
 */
#define F1      0x1001                  /* Functin key one              */
#define F2      0x1002                  /* Functin key two              */
#define F3      0x1003                  /* Functin key three            */
#define F4      0x1004                  /* Functin key four             */
#define F5      0x1005                  /* Functin key five             */
#define F6      0x1006                  /* Functin key six              */
#define F7      0x1007                  /* Functin key seven            */
#define F8      0x1008                  /* Functin key eight            */
#define F9      0x1009                  /* Functin key nine             */
#define F10     0x100A                  /* Functin key ten              */
#define F11     0x100B                  /* Functin key eleven           */
#define F12     0x100C                  /* Functin key twelve           */

/*
 * useful function definitions
 */
#ifdef	ANSI
int   pico(PICO *);
void *pico_get(void);
void  pico_give(void *);
int   pico_readc(void *, unsigned char *);
int   pico_writec(void *, int);
int   pico_puts(void *, char *);
int   pico_seek(void *, long, int);
int   pico_replace(void *, char *);
#ifdef	DOS
int   pico_nfsetcolor(char *);
int   pico_nbsetcolor(char *);
int   pico_rfsetcolor(char *);
int   pico_rbsetcolor(char *);
#endif
#else
int   pico();
void *pico_get();
void  pico_give();
int   pico_readc();
int   pico_writec();
int   pico_puts();
int   pico_seek();
int   pico_replace();
#endif

#endif	/* PICO_H */
