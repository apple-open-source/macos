#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: browse.c,v 1.2 2002/01/03 22:16:39 jevans Exp $";
#endif
/*
 * Program:	Routines to support file browser in pico and Pine composer
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
 * NOTES:
 *
 *   Misc. thoughts (mss, 5 Apr 92)
 * 
 *      This is supposed to be just a general purpose browser, equally
 *	callable from either pico or the pine composer.  Someday, it could
 * 	even be used to "wrap" the unix file business for really novice 
 *      users.  The stubs are here for renaming, copying, creating directories,
 *      deleting, undeleting (thought is delete just moves files to 
 *      ~/.pico.deleted directory or something and undelete offers the 
 *      files in there for undeletion: kind of like the mac trashcan).
 *
 *   Nice side effects
 *
 *      Since the full path name is always maintained and referencing ".." 
 *      stats the path stripped of its trailing name, the unpleasantness of 
 *      symbolic links is hidden.  
 *
 *   Fleshed out the file managements stuff (mss, 24 June 92)
 *
 *
 */
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "osdep.h"
#include "pico.h"
#include "estruct.h"
#include "edef.h"
#include "efunc.h"

#if	defined(bsd)
extern int errno;
#endif

/*
 * directory cell structure
 */
struct fcell {
    char *fname;				/* file name 	       */
    unsigned mode;				/* file's mode	       */
    char size[16];				/* file's size in s    */
    struct fcell *next;
    struct fcell *prev;
};


/*
 * master browser structure
 */
static struct bmaster {
    struct fcell *head;				/* first cell in list  */
    struct fcell *top;				/* cell in top left    */
    struct fcell *current;			/* currently selected  */
    int    longest;				/* longest file name   */
    int	   fpl;					/* file names per line */
    int    cpf;					/* chars / file / line */
    char   dname[NLINE];			/* this dir's name     */
    char   *names;				/* malloc'd name array */
} *gmp;						/* global master ptr   */

#ifdef	ANSI
    struct bmaster *getfcells(char *);
    int    PaintCell(int, int, int, struct fcell *, int);
    int    PaintBrowser(struct bmaster *, int);
    int    BrowserKeys(void);
    int    layoutcells(struct bmaster *);
    int    PlaceCell(struct bmaster *, struct fcell *, int *, int *);
    int    zotfcells(struct fcell *);
    int    zotmaster(struct bmaster **);
    struct fcell *FindCell(struct bmaster *, char *);
    int    sisin(char *, char *);
    int    BrowserAnchor(char *);
#else
    struct bmaster *getfcells();
    int    PaintCell();
    int    PaintBrowser();
    int    BrowserKeys();
    int    layoutcells();
    int    PlaceCell();
    int    zotfcells();
    int    zotmaster();
    struct fcell *FindCell();
    int    sisin();
    int    BrowserAnchor();
#endif


static	KEYMENU menu_browse[] = {
    {"?", "Get Help"},	{NULL, NULL},		{"R", "Rename"},
    {"-", "Back Pg"},	{"D", "Del File"},	{"M","Make Copy"},
    {"S", "[Select]"},	{"G", "Goto Dir"},	{"W", "Where is"},
    {"Spc", "Fwd Pg"},	{NULL, NULL},		{NULL, NULL}
};
#define	QUIT_KEY	1


/*
 * function key mappings...
 */
static int  bfmappings[12][2] = { { F1,  '?'},	/* function key */
				  { F2,  's'},	/* mappings... */
			          { F3,  'e'},
  			          { F4,  'g'},
			          { F5,  'r'},
 			          { F6,  'w'},
			          { F7,  '-'},
			          { F8,  ' '},
			          { F9,  'd'},
			          { F10, NODATA },
			          { F11, 'm'},
			          { F12, NODATA } };


/*
 * Browser help for pico (pine composer help handed to us by pine)
 */
static char *BrowseHelpText[] = {
"Help for Browse Command",
"  ",
"\tPico's file browser is used to select a file from the",
"\tfile system for inclusion in the edited text.",
"  ",
"~\tBoth directories and files are displayed.  Press ~S",
"~\tor ~R~e~t~u~r~n to select a file or directory.  When a file",
"\tis selected during the \"Read File\" command, it is",
"\tinserted into edited text.  Answering \"yes\" to the",
"\tverification question after a directory is selected causes",
"\tthe contents of that directory to be displayed for selection.",
"  ",
"\tThe file named \"..\" is special, and means the \"parent\"",
"\tof the directory being displayed.  Select this directory",
"\tto move upward in the directory tree.",
"  ",
"End of Browser Help.",
"  ",
NULL
};





/*
 * FileBrowse - display contents of given directory dir
 *
 *	    intput:  
 *		     dir points to initial dir to browse.
 *		     fn  initial file name. 
 *		     flags
 *
 *         returns:
 *                   dir points to currently selected directory (without
 *			trailing file system delimiter)
 *                   fn  points to currently selected file
 *                   sz  points to size of file if ptr passed was non-NULL
 *
 *                   1 if a file's been selected
 *                   0 if no files seleted
 *                  -1 if there where problems
 */
FileBrowse(dir, fn, sz, flags)
char *dir, *fn, *sz;			/* dir, name and optional size */
int   flags;
{
    int status, i, j, c;
    int row, col;
    char *p, child[NLINE], tmp[NLINE];
    struct bmaster *mp, *getfcells();
    struct fcell *tp;

    child[0] = '\0';

    if((gmode&MDSCUR) && homeless(dir)){
	emlwrite("\007Can't read %s in restricted mode", dir);
	sleep(2);
	return(0);
    }

    emlwrite("Building file list...", NULL);

    /* build contents of cell structures */
    if((gmp = getfcells(dir)) == NULL)
      return(-1);

    /* paint screen */
    PaintBrowser(gmp, 0);

    while(1){						/* the big loop */
	movecursor(term.t_nrow-2, 0);

	(*term.t_flush)();

	c = GetKey();

	if(Pmaster){
	    if(c == NODATA || time_to_check()){		/* new mail ? */
		if((*Pmaster->newmail)(&j, 0, c == NODATA ? 0 : 2) >= 0){
		    mlerase();
		    (*Pmaster->showmsg)(c);
		    mpresf = 1;
		}

		if(j || mpresf){
		    (*term.t_move)(term.t_nrow-2, 0);
		    (*Pmaster->clearcur)();
		}

		if(c == NODATA)			/* GetKey timed out */
		  continue;
	    }
	}
	else{
	    if(timeout && (c == NODATA || time_to_check()))
	      if(pico_new_mail())
		emlwrite("You may possibly have new mail.", NULL);
	    
	    if(c == NODATA)
	      continue;
	}

	if(mpresf){				/* blast old messages */
	    if(mpresf++ > MESSDELAY){		/* every few keystrokes */
		mlerase();
	    }
        }

	switch(normal(c, bfmappings, 2)){	/* process commands */

	  case K_PAD_RIGHT:			/* move right */
	  case (CTRL|'@'):
	  case (CTRL|'F'):			/* forward  */
	    if(gmp->current->next == NULL){
		(*term.t_beep)();
		break;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = gmp->current->next;
	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case K_PAD_LEFT:				/* move left */
	  case (CTRL|'B'):				/* back */
	    if(gmp->current->prev == NULL){
		(*term.t_beep)();
		break;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = gmp->current->prev;
	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case (CTRL|'A'):				/* beginning of line */
	    tp = gmp->current;
	    i = col;
	    while(i > 0){
		i -= gmp->cpf;
		if(tp->prev != NULL)
		  tp = tp->prev;
	    }
	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case (CTRL|'E'):				/* end of line */
	    tp = gmp->current;
	    i = col + gmp->cpf;
	    while(i+gmp->cpf <= gmp->cpf * gmp->fpl){
		i += gmp->cpf;
		if(tp->next != NULL)
		  tp = tp->next;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case (CTRL|'V'):				/* page forward */
	  case ' ':
	  case K_PAD_NEXTPAGE :
	  case K_PAD_END :
	    tp = gmp->top;
	    i = term.t_nrow - 4;

	    while(i-- && tp->next != NULL){
		j = 0;
		while(++j <= gmp->fpl  && tp->next != NULL)
		  tp = tp->next;
	    }

	    if(tp == NULL)
	      continue;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case '-' :
	  case (CTRL|'Y'):				/* page backward */
	  case K_PAD_PREVPAGE :
	  case K_PAD_HOME :
	    tp = gmp->top;
	    i = term.t_nrow - 6;
	    while(i-- && tp != NULL){
		j = gmp->fpl;
		while(j-- && tp != NULL)
		  tp = tp->prev;
	    }

	    if(tp || (gmp->current != gmp->top)){	/* clear old hilite */
		PlaceCell(gmp, gmp->current, &row, &col);
		PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    }

	    if(tp)					/* new page ! */
		gmp->current = tp;
	    else if(gmp->current != gmp->top)		/* goto top of page */
		gmp->current = gmp->top;
	    else					/* do nothing */
	      continue;

	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);

	    break;

	  case K_PAD_DOWN :
	  case (CTRL|'N'):				/* next */
	    tp = gmp->current;
	    i = gmp->fpl;
	    while(i--){
		if(tp->next == NULL){
		    (*term.t_beep)();
		    break;
		}
		else
		  tp = tp->next;
	    }
	    if(i != -1)					/* can't go down */
	      break;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case K_PAD_UP :
	  case (CTRL|'P'):				/* previous */
	    tp = gmp->current;
	    i = gmp->fpl;
	    while(i-- && tp)
	      tp = tp->prev;

	    if(tp == NULL)
	      break;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1);
	    }
	    else
	      PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case 'e':					/* user exits */
	  case 'E':
	    zotmaster(&gmp);
	    return(0);

	  case 'x':			            /* user exits wrong */
	  case 'X':
	    if(!(gmode&MDBRONLY)){
		emlwrite("\007Unknown command '%c'", (void *)c);
		break;
	    }

	    zotmaster(&gmp);
	    return(0);

	  case 'd':					/* delete */
	  case 'D':
	    if(gmp->current->mode){
/* BUG: if dir is empty it should be deleted */
		emlwrite("\007Can't delete a directory", NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite("Delete not allowed in restricted mode",NULL);
		break;
	    }

	    sprintf(child, "%s%c%s", gmp->dname, C_FILESEP, 
		    gmp->current->fname);

	    i = 0;
	    while(i++ < 2){		/* verify twice!! */
		if(i == 1){
		    if(fexist(child, "w", (long *)NULL) != FIOSUC)
		      strcpy(tmp, "File is write protected! OVERRIDE");
		    else
		      sprintf(tmp, "Delete file \"%.*s\"", NLINE - 20, child);
		}
		else
		  strcpy(tmp, "File CANNOT be UNdeleted!  Really delete");

		if((status = mlyesno(tmp, FALSE)) != TRUE){
		    emlwrite((status ==  ABORT)
			       ? "Delete Cancelled"
			       : "File Not Deleted",
			     NULL);
		    break;
		}
	    }

	    if(status == TRUE){
		if(unlink(child) < 0){
		    emlwrite("Delete Failed: %s", errstr(errno));
		}
		else{			/* fix up pointers and redraw */
		    tp = gmp->current;
		    if(tp->next){
			gmp->current = tp->next;
			if(tp->next->prev = tp->prev)
			  tp->prev->next = tp->next;
		    }
		    else if(tp->prev) {
			gmp->current = tp->prev;
			if(tp->prev->next = tp->next)
			  tp->next->prev = tp->prev;
		    }

		    tp->fname = NULL;
		    tp->next = tp->prev = NULL;
		    if(tp != gmp->current)
		      free((char *) tp);

		    if((tp = FindCell(gmp, gmp->current->fname)) != NULL){
			gmp->current = tp;
			PlaceCell(gmp, gmp->current, &row, &col);
		    }
			    
		    PaintBrowser(gmp, 1);
		    mlerase();
		}
	    }

	    BrowserKeys();
	    break;

	  case '?':					/* HELP! */
	    if(Pmaster)
	      (*Pmaster->helper)(Pmaster->browse_help,
				 "Help for Browsing", 1);
	    else
	      pico_help(BrowseHelpText, "Help for Browsing", 1);
	    /* fall thru to repaint everything */

	  case (CTRL|'L'):
	    PaintBrowser(gmp, 0);
	    break;

	  case 'g':				/* jump to a directory */
	  case 'G':
	    i = 0;
	    child[0] = '\0';

	    while(!i){

		status = mlreply("Directory to go to: ", child, NLINE, QNORML,
				 NULL);

		switch(status){
		  case HELPCH:
		    emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0);
		    break;
		  case ABORT:
		    emlwrite("Goto cancelled", NULL);
		    i++;
		    break;
		  case FALSE:
		  case TRUE:
		    i++;

		    if(*child == '\0')
		      strcpy(child, gethomedir(NULL));

		    if(!compresspath(gmp->dname, child, NLINE)){
			emlwrite("Invalid Directory: %s", child);
			break;
		    }

		    if((gmode&MDSCUR) && homeless(child)){
			emlwrite("Restricted mode browsing limited to home directory",NULL);
			break;
		    }

		    if(isdir(child, (long *) NULL)){
			if((mp = getfcells(child)) == NULL){
			    /* getfcells should explain what happened */
			    i++;
			    break;
			}

			zotmaster(&gmp);
			gmp = mp;
			PaintBrowser(gmp, 0);
		    }
		    else
		      emlwrite("\007Not a directory: \"%s\"", child);

		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 'm':					/* copy */
	  case 'M':
	    if(gmp->current->mode){
		emlwrite("\007Can't copy a directory", NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite("Copy not allowed in restricted mode",NULL);
		break;
	    }

	    i = 0;
	    child[0] = '\0';

	    while(!i){

		switch(status=mlreply("Name of new copy: ", child, NLINE,
				      QFFILE, NULL)){
		  case HELPCH:
		    emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0);
		    break;
		  case ABORT:
		    emlwrite("Make Copy Cancelled", NULL);
		    i++;
		    break;
		  case FALSE:
		    i++;
		    mlerase();
		    break;
		  case TRUE:
		    i++;

		    if(child[0] == '\0'){
			emlwrite("No destination, file not copied", NULL);
			break;
		    }

		    if(!strcmp(gmp->current->fname, child)){
			emlwrite("\007Can't copy file on to itself!", NULL);
			break;
		    }

		    strcpy(tmp, child); 		/* add full path! */
		    sprintf(child, "%s%c%s", gmp->dname, C_FILESEP, tmp);

		    if((status = fexist(child, "w", (long *)NULL)) == FIOSUC){
			sprintf(tmp,"File \"%.*s\" exists! OVERWRITE",
				NLINE - 20, child);
			if((status = mlyesno(tmp, 0)) != TRUE){
			    emlwrite((status == ABORT)
				      ? "Make copy cancelled" 
				      : "File Not Renamed",
				     NULL);
			    break;
			}
		    }
		    else if(status != FIOFNF){
			fioperr(status, child);
			break;
		    }

		    sprintf(tmp, "%s%c%s", gmp->dname, C_FILESEP, 
			    gmp->current->fname);

		    if(copy(tmp, child) < 0){
			/* copy()  will report any error messages */
			break;
		    }
		    else{			/* highlight new file */
			emlwrite("File copied to %s", child);

			if((p = strrchr(child, C_FILESEP)) == NULL){
			    emlwrite("Problems refiguring browser", NULL);
			    break;
			}

			*p = '\0';
			strcpy(tmp, (p == child) ? S_FILESEP: child);

			/*
			 * new file in same dir? if so, refigure files
			 * and redraw...
			 */
			if(!strcmp(tmp, gmp->dname)){ 
			    strcpy(child, gmp->current->fname);
			    if((mp = getfcells(gmp->dname)) == NULL)
			      /* getfcells should explain what happened */
			      break;

			    zotmaster(&gmp);
			    gmp = mp;
			    if((tp = FindCell(gmp, child)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }

			    PaintBrowser(gmp, 1);
			}
		    }
		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 'r':					/* rename */
	  case 'R':
	    i = 0;
	    child[0] = '\0';

	    if(!strcmp(gmp->current->fname, "..")){
		emlwrite("\007Can't rename \"..\"", NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite("Rename not allowed in restricted mode",NULL);
		break;
	    }

	    while(!i){

		switch(status=mlreply("Rename file to: ", child, NLINE, QFFILE,
				      NULL)){
		  case HELPCH:
		    emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0);
		    break;
		  case ABORT:
		    emlwrite("Rename cancelled", NULL);
		    i++;
		    break;
		  case FALSE:
		  case TRUE:
		    i++;

		    if(child[0] == '\0' || status == FALSE){
			mlerase();
			break;
		    }

		    strcpy(tmp, child);
		    sprintf(child, "%s%c%s", gmp->dname, C_FILESEP, tmp);

		    status = fexist(child, "w", (long *)NULL);
		    if(status == FIOSUC || status == FIOFNF){
			if(status == FIOSUC){
			    sprintf(tmp,"File \"%.*s\" exists! OVERWRITE",
				    NLINE - 20, child);

			    if((status = mlyesno(tmp, FALSE)) != TRUE){
				emlwrite((status ==  ABORT)
					  ? "Rename cancelled"
					  : "Not Renamed",
					 NULL);
				break;
			    }
			}

			sprintf(tmp, "%s%c%s", gmp->dname, C_FILESEP, 
				gmp->current->fname);

			if(rename(tmp, child) < 0){
			    emlwrite("Rename Failed: %s", errstr(errno));
			}
			else{
			    if((p = strrchr(child, C_FILESEP)) == NULL){
				emlwrite("Problems refiguring browser", NULL);
				break;
			    }
			    
			    *p = '\0';
			    strcpy(tmp, (p == child) ? S_FILESEP: child);

			    if((mp = getfcells(tmp)) == NULL)
			      /* getfcells should explain what happened */
			      break;

			    zotmaster(&gmp);
			    gmp = mp;

			    if((tp = FindCell(gmp, ++p)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }

			    PaintBrowser(gmp, 1);
			    mlerase();
			}
		    }
		    else{
			fioperr(status, child);
		    }
		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 's':					/* user's selected */
	  case 'S':
	  case (CTRL|'M'):

	    if(gmp->current->mode){
		*child = '\0';

		while(1){
		    i = mlyesno("A directory is selected, enter it", 1);
		    if(i == TRUE){
			strcpy(tmp, gmp->dname);
			p = gmp->current->fname;
			if(p[0] == '.' && p[1] == '.' && p[2] == '\0'){

			    if((p=strrchr(tmp, C_FILESEP)) != NULL){
				*p = '\0';

				if((gmode&MDSCUR) && homeless(tmp)){
				    emlwrite("\007Can't visit parent in restricted mode", NULL);
				    break;
				}

				strcpy(child, &p[1]);
				if(p == tmp
#ifdef	DOS
				   || (tmp[1] == ':' && tmp[2] == '\0')
#endif
				         ){	/* is it root? */
				    if(*child)
#ifdef	DOS
				      strcat(tmp, S_FILESEP);
#else
				      strcpy(tmp, S_FILESEP);
#endif
				    else{
					emlwrite("\007Can't move up a directory", NULL);
					break;
				    }
				}
			    }
			}
			else{
			    if(tmp[1] != '\0')		/* were in root? */
			      strcat(tmp, S_FILESEP);
			    strcat(tmp, gmp->current->fname);
			}

			if((mp = getfcells(tmp)) == NULL)
			  /* getfcells should explain what happened */
			  break;

			zotmaster(&gmp);
			gmp = mp;

			if(*child){
			    if((tp = FindCell(gmp, child)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }
			    else
			      emlwrite("\007Problem finding dir \"%s\"",child);
			}

			PaintBrowser(gmp, 0);
			break;
		    }
		    else if(i == FALSE){
			mlerase();
			break;
		    }
		    else if(i == ABORT){
			emlwrite("Select cancelled", NULL);
			break;
		    }
		    else if(i == (CTRL|'L')){
			PaintBrowser(gmp, 0);
		    }
		    else
		      (*term.t_beep)();
		}
	    }
	    else{				/* just return */
		strcpy(dir, gmp->dname);
		strcpy(fn, gmp->current->fname);
		if(sz != NULL)			/* size uninteresting */
		  strcpy(sz, gmp->current->size);

#ifndef	_WINDOWS
		if(gmode&MDBRONLY){
		    i = 0;
		    sprintf(child, "%s%c%s", gmp->dname, C_FILESEP, fn);
			/* stat file for executable bit */
		    if(fexist(child, "x", NULL) == FIOSUC){
			while(!i){
			    switch(mlyesno("Program selected, run it", 1)){
			      case TRUE:
				strcpy(child, fn);
				while(!i){
				    status = mlreply("Command line : ",
						     child, NLINE, QNORML,
						     NULL);
				    switch(status){
				      case HELPCH:
					emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
					sleep(3);
					break;
				      case (CTRL|'L'):
					PaintBrowser(gmp, 0);
					break;
				      case ABORT:
					emlwrite("Command cancelled", NULL);
					i++;
					break;
				      case FALSE:
				      case TRUE:
					i++;

					if(child[0]=='\0' || status==FALSE){
					    mlerase();
					    break;
					}

					vttidy();
					system(child);
					ttopen();

					/* recompute browser */
					/* redraw browser */
					PaintBrowser(gmp, 0);
					break;
					default:
					break;
				    }
				}

				break;
			      case FALSE:
				i += 2;
				mlerase();
				break;
			      case ABORT:
				i++;
				emlwrite("Command cancelled", NULL);
				break;
			      case (CTRL|'L'):
				PaintBrowser(gmp, 0);
				break;
			      default:
				(*term.t_beep)();
				break;
			    }
			}
		    }

		    if(i == 2)		/* do something else to executable? */
		      i = 0;

		    while(!i){
			*child = '\0';
			status = mlreply("Program to use on file : ",
				       child, NLINE, QNORML, NULL);
			switch(status){
			  case HELPCH:
			    emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
			    sleep(3);
			    break;
			  case (CTRL|'L'):
			    PaintBrowser(gmp, 0);
			    break;
			  case ABORT:
			    emlwrite("Command cancelled", NULL);
			    i++;
			    break;
			  case FALSE:
			  case TRUE:
			    i++;

			    if(child[0] == '\0' || status == FALSE){
				mlerase();
				break;
			    }

			    strcat(child, " ");
			    strcat(child, gmp->dname);
			    strcat(child, S_FILESEP);
			    strcat(child, fn);

			    vttidy();
			    system(child);
			    ttopen();

			    /* recompute browser */
			    /* redraw browser */
			    PaintBrowser(gmp, 0);
			    break;
			  default:
			    break;
			}
		    }

		    BrowserKeys();
		}
		else{
		    zotmaster(&gmp);
		    return(1);
		}
#else
		/* xxx Can't execute files under windoes, do this... */
		zotmaster (&gmp);
		return (1);
#endif
	    }
	    break;

	  case 'w':				/* Where is */
	  case 'W':
	    i = 0;

	    while(!i){

		switch(readpattern("File name to find")){
		  case HELPCH:
		    emlwrite("\007No help yet!", NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0);
		    break;
		  case ABORT:
		    emlwrite("Whereis cancelled", NULL);
		    i++;
		    break;
		  case FALSE:
		    mlerase();
		    i++;
		    break;
		  case TRUE:
		    if((tp = FindCell(gmp, pat)) != NULL){
			PlaceCell(gmp, gmp->current, &row, &col);
			PaintCell(row, col, gmp->cpf, gmp->current, 0);
			gmp->current = tp;

			if(PlaceCell(gmp, tp, &row, &col)){ /* top changed */
			    PaintBrowser(gmp, 1);
			}
			else
			  PaintCell(row, col, gmp->cpf, gmp->current, 1);
			mlerase();
		    }
		    else
		      emlwrite("\"%s\" not found", pat);

		    i++;
		    break;
		  default:
		    break;
		}
	    }
	    mlerase();
	    BrowserKeys();
	    break;

	  case (CTRL|'Z'):
	    if(gmode&MDSSPD){
		bktoshell();
		PaintBrowser(gmp,0);
		break;
	    }					/* fall thru with error! */

	  default:				/* what? */
	    if(c < 0xff)
	      emlwrite("\007Unknown command: '%c'", (void *) c);
	    else if(c & CTRL)
	      emlwrite("\007Unknown command: ^%c", (void *)(c&0xff));
	    else
	      emlwrite("\007Unknown command", NULL);
	  case NODATA:				/* no op */
	    break;
	}
    }
}



/*
 * getfcells - make a master browser struct and fill it in
 *             return NULL if there's a problem.
 */
struct bmaster *getfcells(dname)
char *dname;
{
    int  i, 					/* various return codes */
         nentries = 0;				/* number of dir ents */
    long l;
    char *np,					/* names of files in dir */
         *dcp,					/* to add file to path */
         **filtnames;				/* array filtered names */
    struct fcell *ncp,				/* new cell pointer */
                 *tcp;				/* trailing cell ptr */
    struct bmaster *mp;

    if((mp=(struct bmaster *)malloc(sizeof(struct bmaster))) == NULL){
	emlwrite("\007Can't malloc space for master filename cell", NULL);
	return(NULL);
    }

    if(dname[0] == '.' && dname[1] == '\0'){		/* remember this dir */
	if(!getcwd(mp->dname, 256))
	  mp->dname[0] = '\0';
    }
    else if(dname[0] == '.' && dname[1] == '.' && dname[2] == '\0'){
	if(!getcwd(mp->dname, 256))
	  mp->dname[0] = '\0';
	else{
	    if((np = (char *)strrchr(mp->dname, C_FILESEP)) != NULL)
	      if(np != mp->dname)
		*np = '\0';
	}
    }
    else
      strcpy(mp->dname, dname);

    mp->head = mp->top = NULL;
    mp->cpf = mp->fpl = 0;
    mp->longest = 5;				/* .. must be labeled! */

    if((mp->names = getfnames(mp->dname, &nentries)) == NULL){
	free((char *) mp);
	return(NULL);
    }

    /*
     * this is the fun part.  build an array of pointers to the fnames we're
     * interested in (i.e., do any filtering), then pass that off to be
     * sorted before building list of cells...
     *
     * right now default filtering on ".*" except "..", but this could
     * easily be made a user option later on...
     */
    if((filtnames=(char **)malloc((nentries+1) * sizeof(char *))) == NULL){
	emlwrite("\007Can't malloc space for name array", NULL);
	zotmaster(&mp);
	return(NULL);
    }

    i = 0;					/* index of filt'd array */
    np = mp->names;
    while(nentries--){
	if(*np == '.'){					/* filter .* */
	    if(np[1] == '\0' || (np[1] != '.' && np[2] != '\0')){
		np += strlen(np) + 1;
		continue;
	    }
	}

	filtnames[i++] = np;

	if((l = strlen(np)) > mp->longest)	/* cast l ? */
	  mp->longest = l;			/* remember longest */
	np += l + 1;				/* advance name pointer */

    }
    nentries = i;				/* new # of entries */

    /* 
     * sort files case independently
     */
    qsort((QSType *)filtnames, (size_t)nentries, sizeof(char *), sstrcasecmp);

    /* 
     * this is so we use absolute path names for stats.
     * remember: be careful using dname as directory name, and fix mp->dname
     * when we're done
     */
    dcp = (char *)strchr(mp->dname, '\0');
    if(dcp == mp->dname || dcp[-1] != C_FILESEP){
	dcp[0] = C_FILESEP;
	dcp[1] = '\0';
    }
    else
      dcp--;

    i = 0;
    while(nentries--){				/* stat filtered files */
	/* get a new cell */
	if((ncp=(struct fcell *)malloc(sizeof(struct fcell))) == NULL){
	    emlwrite("\007Can't malloc cells for browser!", NULL);
	    zotfcells(mp->head);		/* clean up cells */
	    free((char *) filtnames);
	    free((char *) mp);
	    return(NULL);			/* bummer. */
	}
	ncp->next = ncp->prev = NULL;
	ncp->mode = 0;

	if(mp->head == NULL){			/* tie it onto the list */
	    mp->head = mp->top = mp->current = ncp;
	}
	else{
	    tcp->next = ncp;
	    ncp->prev = tcp;
	}
	tcp = ncp;

	/* fill in the new cell */
	ncp->fname = filtnames[i++];

	strcpy(&dcp[1], ncp->fname);		/* use abolute path! */

	if(ncp->fname[0] == '.' && ncp->fname[1] == '.' && ncp->fname[2] == '\0'){
	    ncp->mode = 1;				/* parent is special */
	    strcpy(ncp->size, "(parent dir)");
	}
	else if(isdir(mp->dname, &l)){
	    ncp->mode = 1;
	    strcpy(ncp->size, "(dir)");
	}
	else
	  strcpy(ncp->size,  prettysz(l));
    }

    *dcp = '\0';				/* remember to cap dname */
    free((char *) filtnames);			/* 'n blast filt'd array*/

    layoutcells(mp);

    return(mp);
}



/*
 * PaintCell - print the given cell at the given location on the display
 *             the format of a printed cell is:
 *
 *                       "<fname>       <size>  "
 */
PaintCell(x, y, l, cell, inverted)
struct fcell *cell;
int    x, y, l, inverted;
{
    char *p;					/* temp str pointer */
    int   i = 0,				/* current display count */
          j, sl, fl;				/* lengths */

    if(cell == NULL)
	return(-1);

    fl = strlen(cell->fname);
    sl = strlen(cell->size);

    movecursor(x, y);
    if(inverted)
      (*term.t_rev)(1);
    
    /* room for fname? */
    p = (fl+2 > l) ? &cell->fname[fl-(l-2)] : cell->fname;
    while(*p != '\0'){				/* write file name */
	pputc(*p++, 0);
	i++;
    }

    if(sl+3 <= l-i){				/* room for size? */
	j = (l-i)-(sl+2);			/* put space between */
	i += j;
	while(j--)				/* file name and size */
	  pputc(' ', 0);

	p = cell->size;
	while(*p != '\0'){			/* write file size */
	    pputc(*p++, 0);
	    i++;
	}
    }

    if(inverted)
      (*term.t_rev)(0);

    while(i++ < l)				/* pad ending */
      pputc(' ', 0);

    return(1);
}



/*
 * PaintBrowse - with the current data, display the browser.  if level == 0 
 *               paint the whole thing, if level == 1 just paint the cells
 *               themselves
 */
PaintBrowser(mp, level)
struct bmaster *mp;
int level;
{
    int i, cl;
    struct fcell *tp;

    if(!level){
	cl = (Pmaster) ? term.t_nrow : term.t_nrow - 1;
	for(i = 0; i <= cl; i++){		/* clear screen */
	    movecursor(i, 0);
	    peeol();
	}

	BrowserAnchor(mp->dname);
    }

    i = 0;
    tp = mp->top;
    cl = COMPOSER_TOP_LINE;			/* current display line */
    while(tp){

	PaintCell(cl, mp->cpf * i, mp->cpf, tp, tp == mp->current);

	if(++i >= mp->fpl){
	    i = 0;
	    if(++cl > term.t_nrow-3)
	      break;
	}

	tp = tp->next;
    }

    if(level){
	while(cl <= term.t_nrow - 3){
	    if(!i)
	      movecursor(cl, 0);
	    peeol();
	    movecursor(++cl, 0);
	}
    }
    else{
	BrowserKeys();
    }

    return(1);
}


/*
 * BrowserKeys - just paints the keyhelp at the bottom of the display
 */
BrowserKeys()
{
    menu_browse[QUIT_KEY].name  = (gmode&MDBRONLY) ? "X" : "E";
    menu_browse[QUIT_KEY].label = (gmode&MDBRONLY) ? "Exit" : "Exit Brwsr";
    wkeyhelp(menu_browse);
}


/*
 * layoutcells - figure out max length of cell and how many cells can 
 *               go on a line of the display
 */
layoutcells(mp)
struct bmaster *mp;
{
    int i = 1;

    mp->cpf = mp->longest + 12;			/* max chars / file */

    while(i*mp->cpf < term.t_ncol)		/* no force... */
      i++;					/* like brute force! */

    mp->fpl = i - 1;				/* files per line */
}


/*
 * PlaceCell - given a browser master, return row and col of the display that
 *             it should go.  
 *
 *             return 1 if mp->top has changed, x,y relative to new page
 *             return 0 if otherwise (same page)
 *             return -1 on error
 */
PlaceCell(mp, cp, x, y)
struct bmaster *mp;
struct fcell *cp;
int    *x, *y;
{
    int cl = COMPOSER_TOP_LINE;			/* current line */
    int ci = 0;					/* current index on line */
    int rv = 0;
    int secondtry = 0;
    struct fcell *tp;

    /* will cp fit on screen? */
    tp = mp->top;
    while(1){
	if(tp == cp){				/* bingo! */
	    *x = cl;
	    *y = ci * mp->cpf;
	    break;
	}

	if((tp = tp->next) == NULL){		/* above top? */
	    if(secondtry++){
		emlwrite("\007Internal error: can't find fname cell", NULL);
		return(-1);
	    }
	    else{
		tp = mp->top = mp->head;	/* try from the top! */
		cl = COMPOSER_TOP_LINE;
		ci = 0;
		rv = 1;
		continue;			/* start over! */
	    }
	}

	if(++ci >= mp->fpl){			/* next line? */
	    ci = 0;
	    if(++cl > term.t_nrow-3){		/* next page? */
		ci = mp->fpl;			/* tp is at bottom right */
		while(ci--)			/* find new top */
		  tp = tp->prev;
		mp->top = tp;
		ci = 0;
		cl = COMPOSER_TOP_LINE;		/* keep checking */
		rv = 1;
	    }
	}

    }

    /* not on display! */
    return(rv);
	
}


/*
 * zotfcells - clean up malloc'd cells of file names
 */
zotfcells(hp)
struct fcell *hp;
{
    struct fcell *tp;

    while(hp){
	tp = hp;
	hp = hp->next;
	tp->next = NULL;
	free((char *) tp);
    }
}


/*
 * zotmaster - blast the browser master struct
 */
zotmaster(mp)
struct bmaster **mp;
{
    zotfcells((*mp)->head);			/* free cells       */
    free((char *)(*mp)->names);			/* free file names  */
    free((char *)*mp);				/* free master      */
    *mp = NULL;					/* make double sure */
}


/*
 * FindCell - starting from the current cell find the first occurance of 
 *            the given string wrapping around if necessary
 */
struct fcell *FindCell(mp, string)
struct bmaster *mp;
char   *string;
{
    struct fcell *tp, *fp;

    if(*string == '\0')
      return(NULL);

    fp = NULL;
    tp = mp->current->next;
    
    while(tp && !fp){
	if(sisin(tp->fname, string))
	  fp = tp;
	else
	  tp = tp->next;
    }

    tp = mp->head;
    while(tp != mp->current && !fp){
	if(sisin(tp->fname, string))
	  fp = tp;
	else
	  tp = tp->next;
    }

    return(fp);
}


/*
 * sisin - case insensitive substring matching function
 */
sisin(s1, s2)
char *s1, *s2;
{
    register int j;

    while(*s1){
	j = 0;
	while(toupper(s1[j]) == toupper(s2[j]))
	  if(s2[++j] == '\0')			/* bingo! */
	    return(1);

	s1++;
    }
    return(0);
} 


/*
 * BrowserAnchor - draw the browser's anchor line.
 */
BrowserAnchor(dir)
char *dir;
{
    register char *p;
    register int  i, j, l;
    char          buf[NLINE];

    movecursor(0, 0);
    (*term.t_rev)(1);

    i = 0;
    l = strlen(dir);
    j = (term.t_ncol-(l+16))/2;

    if(Pmaster)
      sprintf(buf, "   PINE %s", Pmaster->pine_version);
    else
      sprintf(buf,"   UW PICO(tm) %s", (gmode&MDBRONLY) ? "BROWSER" : version);

    p = buf;
    while(*p){
	pputc(*p++, 0);
	i++;
    }

    if(l > term.t_ncol - i - 21){		/* fit dir name on line */
	p = dir;
	while((p = strchr(p, C_FILESEP)) != NULL)
	  if((l - (p - dir)) <= term.t_ncol - i - 21)
	    break;
	  else
	    p++;

	if(*p == '\0')				/* no suitable length! */
	  p = &dir[l-(term.t_ncol-i-19)];

	sprintf(buf, "%s Dir ...%s", (gmode&MDBRONLY) ? "" : " BROWSER  ", p);
    }
    else 
      sprintf(buf,"%s  Dir: %s", (gmode&MDBRONLY) ? "" : " BROWSER  ", dir);

    if(i < j)					/* keep it centered */
      j = j - i;				/* as long as we can */
    else
      j = ((term.t_ncol-i)-(strlen(p)+15))/2;

    while(j-- && i++)
      pputc(' ', 0);

    p = buf;
    while(i++ < term.t_ncol && *p)		/* show directory */
      pputc(*p++, 0);

    while(i++ < term.t_ncol)
      pputc(' ', 0);

    (*term.t_rev)(0);
}


/*
 * ResizeBrowser - handle a resize event
 */
ResizeBrowser()
{
    if(gmp){
	layoutcells(gmp);
	PaintBrowser(gmp, 0);
	return(1);
    }
    else
      return(0);
}
