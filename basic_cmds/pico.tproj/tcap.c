#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: tcap.c,v 1.1.1.1 1999/04/15 17:45:14 wsanchez Exp $";
#endif
/*
 * Program:	Display routines
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
/*	tcap:	Unix V5, V7 and BS4.2 Termcap video driver
		for MicroEMACS
*/

#define	termdef	1			/* don't define "term" external */

#include	<stdio.h>
#include        <signal.h>
#include	"osdep.h"
#include	"estruct.h"
#include        "edef.h"

#if TERMCAP
#include        "pico.h"

#define NROW    24
#define NCOL    80
#define	MARGIN	8
#define	SCRSIZ	64
#define BEL     0x07
#define ESC     0x1B

extern int      ttopen();
extern int      ttgetc();
extern int      ttputc();
extern int      ttflush();
extern int      ttclose();
extern int      tcapmove();
extern int      tcapeeol();
extern int      tcapeeop();
extern int      tcapbeep();
extern int	tcaprev();
extern int      tcapopen();
extern int      tcapclose();
extern int      tput();
extern char     *tgoto();

#define TCAPSLEN 315
char tcapbuf[TCAPSLEN];
extern char *UP, PC, *CM, *CE, *CL, *SO, *SE;
/* 
 * PICO extentions 
 */
extern char
	*DL,			/* delete line */
	*AL,			/* insert line */
	*CS,			/* define a scrolling region, vt100 */
	*IC,			/* insert character, preferable to : */
	*IM,			/* set insert mode and, */
	*EI,			/* end insert mode */
	*DC,			/* delete character */
	*DM,			/* set delete mode and, */
	*ED,			/* end delete mode */
	*SF,			/* scroll text up */
	*SR,			/* scroll text down */
	*TI,			/* string to start termcap */
        *TE;			/* string to end termcap */

extern char *KU, *KD, *KL, *KR;
char *KPPU, *KPPD, *KPHOME, *KPEND;

struct KBSTREE *kpadseqs = NULL;

TERM term = {
        NROW-1,
        NCOL,
	MARGIN,
	SCRSIZ,
        tcapopen,
        tcapclose,
        ttgetc,
        ttputc,
        ttflush,
        tcapmove,
        tcapeeol,
        tcapeeop,
        tcapbeep,
        tcaprev
};


tcapopen()
{
    char   *t, *p, *tgetstr();
    char    tcbuf[1024];
    char   *tv_stype;
    char    err_str[72];
    char   *getenv();

    ttgetwinsz();

    /*
     * determine the terminal's communication speed and decide
     * if we need to do optimization ...
     */
    optimize = ttisslow();

    if ((tv_stype = getenv("TERM")) == NULL){
	if(Pmaster){
	    return(FALSE);
	}
	else{
	    puts("Environment variable TERM not defined!");
	    exit(1);
	}
    }

    if((tgetent(tcbuf, tv_stype)) != 1){
	if(Pmaster){
	    return(FALSE);
	}
	else{
	    sprintf(err_str, "Unknown terminal type %s!", tv_stype);
	    puts(err_str);
	    exit(1);
	}
    }

    p = tcapbuf;
    t = tgetstr("pc", &p);
    if(t)
      PC = *t;

    CL = tgetstr("cl", &p);
    CM = tgetstr("cm", &p);
    CE = tgetstr("ce", &p);
    UP = tgetstr("up", &p);
    SE = tgetstr("se", &p);
    SO = tgetstr("so", &p);
    DL = tgetstr("dl", &p);
    AL = tgetstr("al", &p);
    CS = tgetstr("cs", &p);
    IC = tgetstr("ic", &p);
    IM = tgetstr("im", &p);
    EI = tgetstr("ei", &p);
    DC = tgetstr("dc", &p);
    DM = tgetstr("dm", &p);
    ED = tgetstr("ed", &p);
    SF = tgetstr("sf", &p);
    SR = tgetstr("sr", &p);
    TI = tgetstr("ti", &p);
    TE = tgetstr("te", &p);

    eolexist = (CE != NULL);	/* will we be able to use clear to EOL? */
    revexist = (SO != NULL);
    if(DC == NULL && (DM == NULL || ED == NULL))
      delchar = FALSE;
    if(IC == NULL && (IM == NULL || EI == NULL))
      inschar = FALSE;
    if((CS==NULL || SF==NULL || SR==NULL) && (DL==NULL || AL==NULL))
      scrollexist = FALSE;

    if(CL == NULL || CM == NULL || UP == NULL){
	if(Pmaster == NULL){
	    puts("Incomplete termcap entry\n");
	    exit(1);
	}
    }
    else{
	KPPU   = tgetstr("kP", &p);
	KPPD   = tgetstr("kN", &p);
	KPHOME = tgetstr("kh", &p);
	KU     = tgetstr("ku", &p);
	KD     = tgetstr("kd", &p);
	KL     = tgetstr("kl", &p);
	KR     = tgetstr("kr", &p);
	if(KU != NULL && (KL != NULL && (KR != NULL && KD != NULL))){
	    kpinsert(KU,K_PAD_UP);
	    kpinsert(KD,K_PAD_DOWN);
	    kpinsert(KL,K_PAD_LEFT);
	    kpinsert(KR,K_PAD_RIGHT);

	    if(KPPU != NULL)
	      kpinsert(KPPU,K_PAD_PREVPAGE);
	    if(KPPD != NULL)
	      kpinsert(KPPD,K_PAD_NEXTPAGE);
	    if(KPHOME != NULL)
	      kpinsert(KPHOME,K_PAD_HOME);
	}
    }

    /*
     * add default keypad sequences to the trie...
     */
    if(gmode&MDFKEY){
	/*
	 * Initialize UW-modified NCSA telnet to use it's functionkeys
	 */
	if(Pmaster == NULL){
	    puts("\033[99h");
	}

	/*
	 * this is sort of a hack, but it allows us to use
	 * the function keys on pc's running telnet
	 */

	/* 
	 * UW-NDC/UCS vt10[02] application mode.
	 */
	kpinsert("OP",F1);
	kpinsert("OQ",F2);
	kpinsert("OR",F3);
	kpinsert("OS",F4);
	kpinsert("Op",F5);
	kpinsert("Oq",F6);
	kpinsert("Or",F7);
	kpinsert("Os",F8);
	kpinsert("Ot",F9);
	kpinsert("Ou",F10);
	kpinsert("Ov",F11);
	kpinsert("Ow",F12);

	/*
	 * special keypad functions
	 */
	kpinsert("[4J",K_PAD_PREVPAGE);
	kpinsert("[3J",K_PAD_NEXTPAGE);
	kpinsert("[2J",K_PAD_HOME);
	kpinsert("[N",K_PAD_END);

	/* 
	 * ANSI mode.
	 */
	kpinsert("[=a",F1);
	kpinsert("[=b",F2);
	kpinsert("[=c",F3);
	kpinsert("[=d",F4);
	kpinsert("[=e",F5);
	kpinsert("[=f",F6);
	kpinsert("[=g",F7);
	kpinsert("[=h",F8);
	kpinsert("[=i",F9);
	kpinsert("[=j",F10);
	kpinsert("[=k",F11);
	kpinsert("[=l",F12);

	HelpKeyNames = funckeynames;

    }
    else{
	HelpKeyNames = NULL;
    }

    kpinsert("OA",K_PAD_UP);	/* DEC vt100, ANSI and cursor key mode. */
    kpinsert("OB",K_PAD_DOWN);
    kpinsert("OD",K_PAD_LEFT);
    kpinsert("OC",K_PAD_RIGHT);

    kpinsert("[A",K_PAD_UP);	/* DEC vt100, ANSI, cursor key mode reset. */
    kpinsert("[B",K_PAD_DOWN);
    kpinsert("[D",K_PAD_LEFT);
    kpinsert("[C",K_PAD_RIGHT);

    kpinsert("A",K_PAD_UP);	/* DEC vt52 mode. */
    kpinsert("B",K_PAD_DOWN);
    kpinsert("D",K_PAD_LEFT);
    kpinsert("C",K_PAD_RIGHT);

    kpinsert("[215z",K_PAD_UP); /* Sun Console sequences. */
    kpinsert("[221z",K_PAD_DOWN);
    kpinsert("[217z",K_PAD_LEFT);
    kpinsert("[219z",K_PAD_RIGHT);

    if (p >= &tcapbuf[TCAPSLEN]){
	if(Pmaster == NULL){
	    puts("Terminal description too big!\n");
	    exit(1);
	}
    }

    ttopen();

    if(TI && !Pmaster) {
	putpad(TI);			/* any init termcap requires */
	if (CS)
	  putpad(tgoto(CS, term.t_nrow, 0)) ;
    }
}


tcapclose()
{
    if(!Pmaster){
	if(gmode&MDFKEY)
	  puts("\033[99l");		/* reset UW-NCSA telnet keys */

	if(TE)				/* any cleanup termcap requires */
	  putpad(TE);
    }

    ttclose();
}



#define	newnode()	(struct KBSTREE *)malloc(sizeof(struct KBSTREE))
/*
 * kbinsert - insert a keystroke escape sequence into the global search
 *	      structure.
 */
kpinsert(kstr, kval)
char	*kstr;
int	kval;
{
    register	char	*buf;
    register	struct KBSTREE *temp;
    register	struct KBSTREE *trail;

    if(kstr == NULL)
      return;

    temp = trail = kpadseqs;
    if(kstr[0] == '\033')
      buf = kstr+1;			/* can the ^[ character */ 
    else
      buf = kstr;

    for(;;) {
	if(temp == NULL){
	    temp = newnode();
	    temp->value = *buf;
	    temp->func = 0;
	    temp->left = NULL;
	    temp->down = NULL;
	    if(kpadseqs == NULL)
	      kpadseqs = temp;
	    else
	      trail->down = temp;
	}
	else{				/* first entry */
	    while((temp != NULL) && (temp->value != *buf)){
		trail = temp;
		temp = temp->left;
	    }

	    if(temp == NULL){   /* add new val */
		temp = newnode();
		temp->value = *buf;
		temp->func = 0;
		temp->left = NULL;
		temp->down = NULL;
		trail->left = temp;
	    }
	}

	if (*(++buf) == '\0'){
	    break;
	}
	else{
	    trail = temp;
	    temp = temp->down;
	}
    }
    
    if(temp != NULL)
      temp->func = kval;
}



/*
 * tcapinsert - insert a character at the current character position.
 *              IC takes precedence.
 */
tcapinsert(ch)
register char	ch;
{
    if(IC != NULL){
	putpad(IC);
	ttputc(ch);
    }
    else{
	putpad(IM);
	ttputc(ch);
	putpad(EI);
    }
}


/*
 * tcapdelete - delete a character at the current character position.
 */
tcapdelete()
{
    if(DM == NULL && ED == NULL)
      putpad(DC);
    else{
	putpad(DM);
	putpad(DC);
	putpad(ED);
    }
}


/*
 * o_scrolldown - open a line at the given row position.
 *                use either region scrolling or deleteline/insertline
 *                to open a new line.
 */
o_scrolldown(row, n)
register int row;
register int n;
{
    register int i;

    if(CS != NULL){
	putpad(tgoto(CS, term.t_nrow - 3, row));
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad( (SR != NULL && *SR != '\0') ? SR : "\n" );
	putpad(tgoto(CS, term.t_nrow, 0));
	tcapmove(row, 0);
    }
    else{
	/*
	 * this code causes a jiggly motion of the keymenu when scrolling
	 */
	for(i = 0; i < n; i++){
	    tcapmove(term.t_nrow - 3, 0);
	    putpad(DL);
	    tcapmove(row, 0);
	    putpad(AL);
	}
#ifdef	NOWIGGLYLINES
	/*
	 * this code causes a sweeping motion up and down the display
	 */
	tcapmove(term.t_nrow - 2 - n, 0);
	for(i = 0; i < n; i++)
	  putpad(DL);
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(AL);
#endif
    }
}


/*
 * o_scrollup - open a line at the given row position.
 *              use either region scrolling or deleteline/insertline
 *              to open a new line.
 */
o_scrollup(row, n)
register int row;
register int n;
{
    register int i;

    if(CS != NULL){
	putpad(tgoto(CS, term.t_nrow - 3, row));
	/* setting scrolling region moves cursor to home */
	tcapmove(term.t_nrow-3, 0);
	for(i = 0;i < n; i++)
	  putpad((SF == NULL || SF[0] == '\0') ? "\n" : SF);
	putpad(tgoto(CS, term.t_nrow, 0));
	tcapmove(2, 0);
    }
    else{
	for(i = 0; i < n; i++){
	    tcapmove(row, 0);
	    putpad(DL);
	    tcapmove(term.t_nrow - 3, 0);
	    putpad(AL);
	}
#ifdef  NOWIGGLYLINES
	/* see note above */
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(DL);
	tcapmove(term.t_nrow - 2 - n, 0);
	for(i = 0;i < n; i++)
	  putpad(AL);
#endif
    }
}


/*
 * o_insert - use termcap info to optimized character insert
 *            returns: true if it optimized output, false otherwise
 */
o_insert(c)
char c;
{
    if(inschar){
	tcapinsert(c);
	return(1);			/* no problems! */
    }

    return(0);				/* can't do it. */
}


/*
 * o_delete - use termcap info to optimized character insert
 *            returns true if it optimized output, false otherwise
 */
o_delete()
{
    if(delchar){
	tcapdelete();
	return(1);			/* deleted, no problem! */
    }

    return(0);				/* no dice. */
}


tcapmove(row, col)
register int row, col;
{
    putpad(tgoto(CM, col, row));
}


tcapeeol()
{
    putpad(CE);
}


tcapeeop()
{
    putpad(CL);
}


tcaprev(state)		/* change reverse video status */
int state;	        /* FALSE = normal video, TRUE = reverse video */
{
    static int cstate = FALSE;

    if(state == cstate)		/* no op if already set! */
      return(0);

    if(cstate = state){		/* remember last setting */
	if (SO != NULL)
	  putpad(SO);
    } 
    else{
	if (SE != NULL)
	  putpad(SE);
    }
}


tcapbeep()
{
    ttputc(BEL);
}


putpad(str)
char    *str;
{
    tputs(str, 1, ttputc);
}


putnpad(str, n)
char    *str;
{
    tputs(str, n, ttputc);
}

#else

hello()
{
}

#endif /* TERMCAP */
