#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: file.c,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $";
#endif
/*
 * Program:	High level file input and output routines
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
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 */
#include        <stdio.h>
#include	"osdep.h"
#include        "pico.h"
#include	"estruct.h"
#include        "edef.h"
#include        "efunc.h"


#ifdef	ANSI
    int ifile(char *);
#else
    int ifile();
#endif


/*
 * Read a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "read a file into the current buffer" code.
 * Bound to "C-X C-R".
 */
fileread(f, n)
int f, n;
{
        register int    s;
        char fname[NFILEN];

        if ((s=mlreply("Read file: ", fname, NFILEN, QFFILE)) != TRUE)
                return(s);

	if(gmode&MDSCUR){
	    emlwrite("File reading disabled in secure mode",NULL);
	    return(0);
	}

        return(readin(fname, TRUE));
}




static char *inshelptext[] = {
  "Insert File Help Text",
  " ",
  "\tType in a file name to have it inserted into your editing",
  "\tbuffer between the line that the cursor is currently on",
  "\tand the line directly below it.  You may abort this by ",
  "~\ttyping the ~F~3 (~^~C) key after exiting help.",
  " ",
  "End of Insert File Help",
  " ",
  NULL
};

static char *writehelp[] = {
  "Write File Help Text",
  " ",
  "\tType in a file name to have it written out, thus saving",
  "\tyour buffer, to a file.  You can abort this by typing ",
  "~\tthe ~F~3 (~^~C) key after exiting help.",
  " ",
  "End of Write File Help",
  " ",
  " ",
  NULL
};


/*
 * Insert a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "insert a file into the current buffer" code.
 * Bound to "C-X C-I".
 */
insfile(f, n)
int f, n;
{
        register int    s;
        char fname[NFILEN], dir[NFILEN];
        int	retval, bye = 0;
        char	*prompt = "Insert file: ";
        register int	availen;

        availen = term.t_ncol - strlen(prompt);

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/

        fname[0] = '\0';
        while(!bye){
            wkeyhelp("GC00000T0000","Get Help,Cancel,To Files");

	    if(Pmaster == NULL)
	      sgarbk = TRUE;

            if ((s=mlreplyd(prompt,fname,NFILEN,QDEFLT|QFFILE)) != TRUE){
		switch(s){
		  case (CTRL|'T'):
		    if(*fname && isdir(fname, NULL))
		      strcpy(dir, fname);
		    else
		      strcpy(dir, gethomedir(NULL));

		    if((s = FileBrowse(dir, fname, NULL)) == 1){
			if(gmode&MDSCUR){
			    emlwrite("Can't insert file in restricted mode",
				     NULL);
			    sleep(2);
			}
			else{
			    strcat(dir, S_FILESEP);
			    strcat(dir, fname);
			    retval = ifile(dir);
			}
			bye++;
		    }
		    else
		      fname[0] = '\0';

		    refresh(FALSE, 1);
		    if(s != 1){
			update(); 		/* redraw on return */
			continue;
		    }
		    break;
		  case HELPCH:
		    if(Pmaster){
			(*Pmaster->helper)(Pmaster->ins_help,
					   "Help for Insert File", 1);
		    }
		    else
		      pico_help(inshelptext, "Help for Insert File", 1);
		  case (CTRL|'L'):
		    refresh(FALSE, 1);
		    update();
		    continue;
		  default:
                    retval = s;
		    bye++;
		}
            }
            else{
		bye++;
		if(gmode&MDSCUR){
		    emlwrite("Can't insert file in restricted mode",NULL);
		}
		else{
		    fixpath(fname, NFILEN);
		    retval = ifile(fname);
		}
            }
        }
        curwp->w_flag |= WFMODE|WFHARD;

        return(retval);
}


/*
 * Read file "fname" into the current
 * buffer, blowing away any text found there. Called
 * by both the read and find commands. Return the final
 * status of the read. Also called by the mainline,
 * to read in a file specified on the command line as
 * an argument. If the filename ends in a ".c", CMODE is
 * set for the current buffer.
 */
readin(fname, lockfl)
char    fname[];	/* name of file to read */
int	lockfl;		/* check for file locks? */
{
        register LINE   *lp1;
        register LINE   *lp2;
        register int    i;
        register WINDOW *wp;
        register BUFFER *bp;
        register int    s;
        register int    nbytes;
        register int    nline;
	register char	*sptr;		/* pointer into filename string */
	int		lflag;		/* any lines longer than allowed? */
        char            line[NLINE];
	CELL            ac;

        bp = curbp;                             /* Cheap.               */
	ac.a = 0;
        if ((s=bclear(bp)) != TRUE)             /* Might be old.        */
                return (s);
        bp->b_flag &= ~(BFTEMP|BFCHG);
	/* removed 'C' mode detection */
        strcpy(bp->b_fname, fname);
        if ((s=ffropen(fname)) == FIOERR)       /* Hard file open.      */
                goto out;
        if (s == FIOFNF) {                      /* File not found.      */
                emlwrite("New file", NULL);
                goto out;
        }
        emlwrite("Reading file", NULL);
        nline = 0;
	lflag = FALSE;
        while ((s=ffgetline(line, NLINE)) == FIOSUC || s == FIOLNG) {
		if (s == FIOLNG)
			lflag = TRUE;
                nbytes = strlen(line);
                if ((lp1=lalloc(nbytes)) == NULL) {
                        s = FIOERR;             /* Keep message on the  */
                        break;                  /* display.             */
                }
                lp2 = lback(curbp->b_linep);
                lp2->l_fp = lp1;
                lp1->l_fp = curbp->b_linep;
                lp1->l_bp = lp2;
                curbp->b_linep->l_bp = lp1;
                for (i=0; i<nbytes; ++i){
		    ac.c = line[i];
		    lputc(lp1, i, ac);
		}
                ++nline;
        }
        ffclose();                              /* Ignore errors.       */
        if (s == FIOEOF) {                      /* Don't zap message!   */
                sprintf(line,"Read %d line%s", nline, (nline > 1) ? "s" : "");
                emlwrite(line, NULL);
        }
	if (lflag){
		sprintf(line,"Read %d line%s, Long lines wrapped",
			nline, (nline > 1) ? "s" : "");
                emlwrite(line, NULL);
        }
out:
        for (wp=wheadp; wp!=NULL; wp=wp->w_wndp) {
                if (wp->w_bufp == curbp) {
                        wp->w_linep = lforw(curbp->b_linep);
                        wp->w_dotp  = lforw(curbp->b_linep);
                        wp->w_doto  = 0;
                        wp->w_imarkp = NULL;
                        wp->w_imarko = 0;

			if(Pmaster)
			  wp->w_flag |= WFHARD;
			else
			  wp->w_flag |= WFMODE|WFHARD;
                }
        }
        if (s == FIOERR || s == FIOFNF)		/* False if error.      */
                return(FALSE);
        return (TRUE);
}


/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 * Update the remembered file name and clear the
 * buffer changed flag. This handling of file names
 * is different from the earlier versions, and
 * is more compatable with Gosling EMACS than
 * with ITS EMACS. Bound to "C-X C-W".
 */
filewrite(f, n)
int f, n;
{
        register WINDOW *wp;
        register int    s;
        char            fname[NFILEN];
	char	shows[128], *bufp;
	long	l;		/* length returned from fexist() */

	if(curbp->b_fname[0] != 0)
	  strcpy(fname, curbp->b_fname);
	else
	  fname[0] = '\0';

	for(;;){

	    wkeyhelp("GC00000T0000","Get Help,Cancel,To Files");
	    sgarbk = TRUE;

	    s=mlreplyd("File Name to write : ", fname, NFILEN, QDEFLT|QFFILE);

	    fixpath(fname, NFILEN);		/*  fixup ~ in file name  */

	    switch(s){
	      case FALSE:
		if(strlen(fname) == 0)		/* no file name to write to */
		  return(s);
	      case TRUE:
		break;
	      case (CTRL|'T'):
		if(*fname && isdir(fname, NULL)){
		    strcpy(shows, fname);
		    fname[0] = '\0';
		}
		else
		  strcpy(shows, gethomedir(NULL));

		s = FileBrowse(shows, fname, NULL);
		strcat(shows, S_FILESEP);
		strcat(shows, fname);
		strcpy(fname, shows);

		refresh(FALSE, 1);
		update();
		if(s == 1)
		  break;
		else
		  continue;
	      case HELPCH:
		pico_help(writehelp, "", 1);
	      case (CTRL|'L'):
		refresh(FALSE, 1);
		update();
		continue;
	      default:
		return(s);
		break;
	    }

	    if(strcmp(fname, curbp->b_fname) == 0)
		break;

	    if((s=fexist(fname, "w", &l)) == FIOSUC){ /* exists.  overwrite? */

		sprintf(shows, "File \"%s\" exists, OVERWRITE", fname);
		if((s=mlyesno(shows, FALSE)) != TRUE){
		    if((bufp = strrchr(fname,'/')) == NULL)
		      fname[0] = '\0';
		    else
		      *++bufp = '\0';
		}
		else
		  break;
	    }
	    else if(s == FIOFNF){
		break;				/* go write it */
	    }
	    else{				/* some error, can't write */
		fioperr(s, fname);
		return(ABORT);
	    }
	}
	emlwrite("Writing...", NULL);

        if ((s=writeout(fname)) != -1) {
	        if(!(gmode&MDTOOL)){
		    strcpy(curbp->b_fname, fname);
		    curbp->b_flag &= ~BFCHG;

		    wp = wheadp;                    /* Update mode lines.   */
		    while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			    if((Pmaster && s == TRUE) || Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
		    }
		}

		if(s > 1)
		  emlwrite("Wrote %d lines", (void *)s);
		else
		  emlwrite("Wrote 1 line", NULL);
        }
        return ((s == -1) ? FALSE : TRUE);
}



/*
 * Save the contents of the current
 * buffer in its associatd file. No nothing
 * if nothing has changed (this may be a bug, not a
 * feature). Error if there is no remembered file
 * name for the buffer. Bound to "C-X C-S". May
 * get called by "C-Z".
 */
filesave(f, n)
int f, n;
{
        register WINDOW *wp;
        register int    s;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if ((curbp->b_flag&BFCHG) == 0)         /* Return, no changes.  */
                return (TRUE);
        if (curbp->b_fname[0] == 0) {           /* Must have a name.    */
                emlwrite("No file name", NULL);
		sleep(2);
                return (FALSE);
        }

	emlwrite("Writing...", NULL);
        if ((s=writeout(curbp->b_fname)) != -1) {
                curbp->b_flag &= ~BFCHG;
                wp = wheadp;                    /* Update mode lines.   */
                while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			  if(Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
                }
		if(s > 1){
		    emlwrite("Wrote %d lines", (void *)s);
		}
		else
		  emlwrite("Wrote 1 line", NULL);
        }
        return (s);
}

/*
 * This function performs the details of file
 * writing. Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed. Sadly, it looks inside a LINE; provide
 * a macro for this. Most of the grief is error
 * checking of some sort.
 *
 * CHANGES: 1 Aug 91: returns number of lines written or -1 on error, MSS
 */
writeout(fn)
char    *fn;
{
        register int    s;
        register LINE   *lp;
        register int    nline;
	char     line[80];

        if ((s=ffwopen(fn)) != FIOSUC)          /* Open writes message. */
                return (-1);

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
                if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
                        break;
                ++nline;
                lp = lforw(lp);
        }
        if (s == FIOSUC) {                      /* No write error.      */
                s = ffclose();
        } else                                  /* Ignore close error   */
                ffclose();                      /* if a write error.    */
        if (s != FIOSUC)                        /* Some sort of error.  */
                return (-1);
        return (nline);
}


/*
 * writetmp - write a temporary file for message text, mindful of 
 *	      access restrictions and included text.  If n is true, include
 *	      lines that indicated included message text, otw forget them
 */
char *writetmp(f, n)
int f, n;
{
        static   char	fn[NFILEN];
        register int    s;
        register LINE   *lp;
        register int    nline;

	tmpname(fn);
	
        if ((s=ffwopen(fn)) != FIOSUC)          /* Open writes message. */
                return(NULL);

	chmod(fn, 0600);			/* fix access rights */

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
	    if(n || (!n && lp->l_text[0].c != '>'))
                if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
                        break;
                ++nline;
                lp = lforw(lp);
        }
        if (s == FIOSUC) {                      /* No write error.      */
                s = ffclose();
        } else                                  /* Ignore close error   */
                ffclose();                      /* if a write error.    */
        if (s != FIOSUC){                       /* Some sort of error.  */
	        unlink(fn);
                return(NULL);
	}
        return(fn);
}


/*
 * The command allows the user
 * to modify the file name associated with
 * the current buffer. It is like the "f" command
 * in UNIX "ed". The operation is simple; just zap
 * the name in the BUFFER structure, and mark the windows
 * as needing an update. You can type a blank line at the
 * prompt if you wish.
 */
filename(f, n)
int f, n;
{
        register WINDOW *wp;
        register int    s;
        char            fname[NFILEN];

        if ((s=mlreply("Name: ", fname, NFILEN, QFFILE)) == ABORT)
                return (s);
        if (s == FALSE)
                strcpy(curbp->b_fname, "");
        else
                strcpy(curbp->b_fname, fname);
        wp = wheadp;                            /* Update mode lines.   */
        while (wp != NULL) {
                if (wp->w_bufp == curbp)
		  if(Pmaster == NULL)
                        wp->w_flag |= WFMODE;
                wp = wp->w_wndp;
        }
	curbp->b_mode &= ~MDVIEW;	/* no longer read only mode */
        return (TRUE);
}

/*
 * Insert file "fname" into the current
 * buffer, Called by insert file command. Return the final
 * status of the read.
 */
ifile(fname)
char    fname[];
{
        register LINE   *lp0;
        register LINE   *lp1;
        register LINE   *lp2;
        register int    i;
        register BUFFER *bp;
        register int    s;
        register int    nbytes;
        register int    nline;
	int		lflag;		/* any lines longer than allowed? */
        char            line[NLINE];
        char     dbuf[128];
        register char    *dbufp;
	CELL            ac;

        bp = curbp;                             /* Cheap.               */
        bp->b_flag |= BFCHG;			/* we have changed	*/
	bp->b_flag &= ~BFTEMP;			/* and are not temporary*/
	ac.a = 0;
        if ((s=ffropen(fname)) == FIOERR)       /* Hard file open.      */
                goto out;
        if (s == FIOFNF) {                      /* File not found.      */
		emlwrite("No such file: %s",fname);
		return(FALSE);
        }
        emlwrite("Inserting %s.", fname);

	/* back up a line and save the mark here */
	curwp->w_dotp = lback(curwp->w_dotp);
	curwp->w_doto = 0;
	curwp->w_imarkp = curwp->w_dotp;
	curwp->w_imarko = 0;

        nline = 0;
	lflag = FALSE;
        while ((s=ffgetline(line, NLINE)) == FIOSUC || s == FIOLNG) {
		if (s == FIOLNG)
			lflag = TRUE;
                nbytes = strlen(line);
                if ((lp1=lalloc(nbytes)) == NULL) {
                        s = FIOERR;             /* Keep message on the  */
                        break;                  /* display.             */
                }
		lp0 = curwp->w_dotp;	/* line previous to insert */
		lp2 = lp0->l_fp;	/* line after insert */

		/* re-link new line between lp0 and lp2 */
		lp2->l_bp = lp1;
		lp0->l_fp = lp1;
		lp1->l_bp = lp0;
		lp1->l_fp = lp2;

		/* and advance and write out the current line */
		curwp->w_dotp = lp1;
                for (i=0; i<nbytes; ++i){
		    ac.c = line[i];
		    lputc(lp1, i, ac);
		}
                ++nline;
        }
        ffclose();                              /* Ignore errors.       */
	curwp->w_imarkp = lforw(curwp->w_imarkp);
        if (s == FIOEOF) {                      /* Don't zap message!   */
	        sprintf(dbuf,"Inserted %d line%s",nline,(nline>1) ? "s" : "");
		emlwrite(dbuf, NULL);
        }
	if (lflag) {
		sprintf(dbuf,"Inserted %d line%s, Long lines wrapped.",
			nline, (nline>1) ? "s" : "");
		emlwrite(dbuf, NULL);
        }
out:
	/* advance to the next line and mark the window for changes */
	curwp->w_flag |= WFHARD;

	/* copy window parameters back to the buffer structure */
	curbp->b_dotp = curwp->w_dotp;
	curbp->b_doto = curwp->w_doto;
	curbp->b_markp = curwp->w_imarkp;
	curbp->b_marko = curwp->w_imarko;

        if (s == FIOERR)                        /* False if error.      */
                return (FALSE);
        return (TRUE);
}
