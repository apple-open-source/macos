/*===========================================================================
 Copyright (c) 1998-2000, The Santa Cruz Operation 
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 *Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 *Neither name of The Santa Cruz Operation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission. 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 DAMAGE. 
 =========================================================================*/

/*	cscope - interactive C symbol or text cross-reference
 *
 *	command functions
 */

#include "global.h"
#include <stdlib.h>
#if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <fcntl.h>	/* O_RDONLY */
#include <ctype.h>

static char const rcsid[] = "$Id: command.c,v 1.4 2002/01/09 19:04:03 umeshv Exp $";


int	selecting;
int   curdispline = 0;

BOOL	caseless;		/* ignore letter case when searching */
BOOL	*change;		/* change this line */
BOOL	changing;		/* changing text */
char	newpat[PATLEN + 1];	/* new pattern */
char	pattern[PATLEN + 1];	/* symbol or text pattern */

static	char	appendprompt[] = "Append to file: ";
static	char	pipeprompt[] = "Pipe to shell command: ";
static	char	readprompt[] = "Read from file: ";
static	char	toprompt[] = "To: ";

void	atchange(void);
BOOL	changestring(void);
void	clearprompt(void);
void	mark(int i);
void	scrollbar(MOUSE *p);
static	void	countrefs(void);

/* execute the command */

BOOL
command(int commandc)
{
	char	filename[PATHLEN + 1];	/* file path name */
	MOUSE *p;			/* mouse data */
	int	c, i;
	FILE	*file;
	struct	cmd	 *curritem, *item;	/* command history */
	char	*s;

	switch (commandc) {

	case ctrl('C'):	/* toggle caseless mode */
		if (caseless == NO) {
			caseless = YES;
			postmsg2("Caseless mode is now ON");
		}
		else {
			caseless = NO;
			postmsg2("Caseless mode is now OFF");
		}
		egrepcaseless(caseless);	/* turn on/off -i flag */
		return(NO);

	case ctrl('R'):	/* rebuild the cross reference */
		if (isuptodate == YES) {
			postmsg("The -d option prevents rebuilding the symbol database");
			return(NO);
		}
		exitcurses();
		freefilelist();		/* remake the source file list */
		makefilelist();
		rebuild();
		if (errorsfound == YES) {
			errorsfound = NO;
			askforreturn();
		}		
		entercurses();
		postmsg("");		/* clear any previous message */
		totallines = 0;
		topline = nextline = 1;
		selecting = 0;
		break;

#if UNIXPC
	case ESC:	/* possible unixpc mouse selection */
#endif
	case ctrl('X'):	/* mouse selection */
		if ((p = getmouseaction(DUMMYCHAR)) == NULL) {
			return(NO);	/* unknown control sequence */
		}
		/* if the button number is a scrollbar tag */
		if (p->button == '0') {
			scrollbar(p);
			break;
		} 
		/* ignore a sweep */
		if (p->x2 >= 0) {
			return(NO);
		}
		/* if this is a line selection */
		if (p->y1 < FLDLINE) {

			/* find the selected line */
			/* note: the selection is forced into range */
			for (i = disprefs - 1; i > 0; --i) {
				if (p->y1 >= displine[i]) {
					break;
				}
			}
			/* display it in the file with the editor */
			editref(i);
		}
		else {	/* this is an input field selection */
			field = p->y1 - FLDLINE;
			/* force it into range */
			if (field >= FIELDS) {
				field = FIELDS - 1;
			}
			setfield();
			resetcmd();
			return(NO);
		}
		break;

	case '\t':	/* go to next input field */
		if (disprefs)
		{
			selecting = !selecting;
			if (selecting)
			{
				move(displine[curdispline], 0);
				refresh();
			}
			else
			{
				atfield();
				resetcmd();
			}
		}
		return(NO);

#if TERMINFO
	case KEY_ENTER:
#endif
	case '\r':
	case '\n':	/* go to reference */
		if (selecting)
		{
			editref(curdispline);
			return(YES);
		}
		/* FALLTHROUGH */

	case ctrl('N'):
#if TERMINFO
	case KEY_DOWN:
	case KEY_RIGHT:
#endif
		if (selecting)
		{
			if ((curdispline + 1) < disprefs)
			{
				move(displine[++curdispline], 0);
				refresh();
			}
		}
		else
		{
			field = (field + 1) % FIELDS;
			setfield();
			atfield();
			resetcmd();
		}
		return(NO);

	case ctrl('P'):	/* go to previous input field */
#if TERMINFO
	case KEY_UP:
	case KEY_LEFT:
#endif
		if (selecting)
		{
			if (curdispline)
			{
				move(displine[--curdispline], 0);
				refresh();
			}
		}
		else
		{
			field = (field + (FIELDS - 1)) % FIELDS;
			setfield();
			atfield();
			resetcmd();
		}
		return(NO);
#if TERMINFO
	case KEY_HOME:	/* go to first input field */
		if (selecting)
		{
			curdispline = 0;
			move(REFLINE, 0);
			refresh();
		}
		else
		{
			field = 0;
			setfield();
			atfield();
			resetcmd();
		}
		return(NO);

	case KEY_LL:	/* go to last input field */
		if (selecting)
		{
			move(displine[disprefs - 1], 0);
			refresh();
		}
		else
		{
			field = FIELDS - 1;
			setfield();
			atfield();
			resetcmd();
		}
		return(NO);
#endif
	case ' ':	/* display next page */
	case '+':
	case ctrl('V'):
#if TERMINFO
	case KEY_NPAGE:
#endif
		/* don't redisplay if there are no lines */
		if (totallines == 0) {
			return(NO);
		}
		/* note: seekline() is not used to move to the next 
		 * page because display() leaves the file pointer at
		 * the next page to optimize paging forward
		 */
		curdispline = 0;
		break;

	case ctrl('H'):
	case '-':	/* display previous page */
#if TERMINFO
	case KEY_PPAGE:
#endif
		/* don't redisplay if there are no lines */
		if (totallines == 0) {
			return(NO);
		}

		curdispline = 0;

		/* if there are only two pages, just go to the other one */
		if (totallines <= 2 * mdisprefs) {
			break;
		}
		/* if on first page but not at beginning, go to beginning */
		nextline -= mdisprefs;	/* already at next page */
		if (nextline > 1 && nextline <= mdisprefs) {
			nextline = 1;
		}
		else {
			nextline -= mdisprefs;
			if (nextline < 1) {
				nextline = totallines - mdisprefs + 1;
				if (nextline < 1) {
					nextline = 1;
				}
			}
		}
		seekline(nextline);
		break;

	case '>':	/* write or append the lines to a file */
		if (totallines == 0) {
			postmsg("There are no lines to write to a file");
		}
		else {	/* get the file name */
			(void) move(PRLINE, 0);
			(void) addstr("Write to file: ");
			s = "w";
			if ((c = mygetch()) == '>') {
				(void) move(PRLINE, 0);
				(void) addstr(appendprompt);
				c = '\0';
				s = "a";
			}
			if (c != '\r' &&
			    getline(newpat, COLS - sizeof(appendprompt), c,
			    NO) > 0) {
				shellpath(filename, sizeof(filename), newpat);
				if ((file = myfopen(filename, s)) == NULL) {
					cannotopen(filename);
				}
				else {
					seekline(1);
					while ((c = getc(refsfound)) != EOF) {
						(void) putc(c, file);
					}
					seekline(topline);
					(void) fclose(file);
				}
			}
			clearprompt();
		}
		return(NO);	/* return to the previous field */

	case '<':	/* read lines from a file */
		(void) move(PRLINE, 0);
		(void) addstr(readprompt);
		if (getline(newpat, COLS - sizeof(readprompt), '\0',
		    NO) > 0) {
			clearprompt();
			shellpath(filename, sizeof(filename), newpat);
			if (readrefs(filename) == NO) {
				postmsg2("Ignoring an empty file");
				return(NO);
			}
			return(YES);
		}
		clearprompt();
		return(NO);

	case '^':	/* pipe the lines through a shell command */
	case '|':	/* pipe the lines to a shell command */
		if (totallines == 0) {
			postmsg("There are no lines to pipe to a shell command");
			return(NO);
		}
		/* get the shell command */
		(void) move(PRLINE, 0);
		(void) addstr(pipeprompt);
		if (getline(newpat, COLS - sizeof(pipeprompt), '\0', NO) == 0) {
			clearprompt();
			return(NO);
		}
		/* if the ^ command, redirect output to a temp file */
		if (commandc == '^') {
			(void) strcat(strcat(newpat, " >"), temp2);
		}
		exitcurses();
		if ((file = mypopen(newpat, "w")) == NULL) {
			(void) fprintf(stderr, "cscope: cannot open pipe to shell command: %s\n", newpat);
		}
		else {
			seekline(1);
			while ((c = getc(refsfound)) != EOF) {
				(void) putc(c, file);
			}
			seekline(topline);
#if Darwin
			(void) mypclose(file);
#else
			(void) pclose(file);
#endif
		}
		if (commandc == '^') {
			if (readrefs(temp2) == NO) {
				postmsg("Ignoring empty output of ^ command");
			}
		}
		askforreturn();
		entercurses();
		break;

	case ctrl('L'):	/* redraw screen */
#if TERMINFO
	case KEY_CLEAR:
#endif
                (void) clearmsg2();
		(void) clearok(curscr, TRUE);
		(void) wrefresh(curscr);
		drawscrollbar(topline, bottomline);
		return(NO);

	case '!':	/* shell escape */
		(void) execute(shell, shell, NULL);
		seekline(topline);
		break;

	case '?':	/* help */
		(void) clear();
#if Darwin
		move(0, 0);
#endif
		help();
		(void) clear();
		seekline(topline);
		break;

	case ctrl('E'):	/* edit all lines */
		editall();
		break;

	case ctrl('Y'):	/* repeat last pattern */
		if (*pattern != '\0') {
			(void) addstr(pattern);
			goto repeat;
		}
		break;

	case ctrl('B'):		/* cmd history back */
	case ctrl('F'):		/* cmd history fwd */
		if (selecting)
			return(NO);

		curritem = currentcmd();
		item = (commandc == ctrl('F')) ? nextcmd() : prevcmd();
		clearmsg2();
		if (curritem == item) {	/* inform user that we're at history end */
			postmsg2("End of input field and search pattern history");
		}
		if (item) {
			field = item->field;
			setfield();
			atfield();
			(void) addstr(item->text);
			(void) strcpy(pattern, item->text);
			switch (c = mygetch()) {
			case '\r':
			case '\n':
				goto repeat;
			default:
				(void) myungetch(c);
				atfield();
				(void) clrtoeol();	/* clear current field */
				break;
			}
		}
		return(NO);

	case '\\':	/* next character is not a command */
		(void) addch('\\');	/* display the quote character */

		/* get a character from the terminal */
		if ((commandc = mygetch()) == EOF) {
			return(NO);	/* quit */
		}
		(void) addstr("\b \b");	/* erase the quote character */
		goto ispat;

	case '.':
		postmsg("The . command has been replaced by ^Y");
		atfield();	/* move back to the input field */
		/* FALLTHROUGH */
	default:

		if (selecting && !mouse)
		{
			char		*c;

			if ((c = strchr(dispchars, commandc)))
				editref(c - dispchars);
		}

		/* if this is the start of a pattern */
		else if (isprint(commandc)) {
	ispat:		if (getline(newpat, COLS - fldcolumn - 1, commandc,
			    caseless) > 0) {
					(void) strcpy(pattern, newpat);
					resetcmd();	/* reset command history */
	repeat:
				addcmd(field, pattern);	/* add to command history */
				if (field == CHANGE) {
					
					/* prompt for the new text */
					(void) move(PRLINE, 0);
					(void) addstr(toprompt);
					(void) getline(newpat, COLS - sizeof(toprompt), '\0', NO);
				}
				/* search for the pattern */
				if (search() == YES) {
					curdispline = 0;
					++selecting;

					switch (field) {
					case DEFINITION:
					case FILENAME:
						if (totallines > 1) {
							break;
						}
						topline = 1;
						editref(0);
						break;
					case CHANGE:
						return(changestring());
					}
				}
				/* try to edit the file anyway */
				else if (field == FILENAME && 
				    access(newpat, READ) == 0) {
					edit(newpat, "1");
				}
			}
			else {	/* no pattern--the input was erased */
				return(NO);
			}
		}
		else {	/* control character */
			return(NO);
		}
	}
	return(YES);
}

/* clear the prompt line */

void
clearprompt(void)
{
	(void) move(PRLINE, 0);
	(void) clrtoeol();
}

/* read references from a file */

BOOL
readrefs(char *filename)
{
	FILE	*file;
	int	c;

	if ((file = myfopen(filename, "rb")) == NULL) {
		cannotopen(filename);
		return(NO);
	}
	if ((c = getc(file)) == EOF) {	/* if file is empty */
		return(NO);
	}
	totallines = 0;
	nextline = 1;
	if (writerefsfound() == YES) {
		(void) putc(c, refsfound);
		while ((c = getc(file)) != EOF) {
			(void) putc(c, refsfound);
		}
		(void) fclose(file);
		(void) freopen(temp1, "rb", refsfound);
		countrefs();
	}
	return(YES);
}

/* change one text string to another */

BOOL
changestring(void)
{
	char	newfile[PATHLEN + 1];	/* new file name */
	char	oldfile[PATHLEN + 1];	/* old file name */
	char	linenum[NUMLEN + 1];	/* file line number */
	char	msg[MSGLEN + 1];	/* message */
	FILE	*script;		/* shell script file */
	BOOL	anymarked = NO;		/* any line marked */
	MOUSE *p;			/* mouse data */
	int	c, i;
	char	*s;

	/* open the temporary file */
	if ((script = myfopen(temp2, "w")) == NULL) {
		cannotopen(temp2);
		return(NO);
	}
	/* create the line change indicators */
	change = mycalloc((unsigned) totallines, sizeof(BOOL));
	changing = YES;
	mousemenu();

	/* until the quit command is entered */
	for (;;) {
		/* display the current page of lines */
		display();
	same:
		atchange();
		
		/* get a character from the terminal */
		if ((c = mygetch()) == EOF || c == ctrl('D') || c == ctrl('Z')) {
			break;	/* change lines */
		}
		/* see if the input character is a command */
		switch (c) {
		case ' ':	/* display next page */
		case '+':
		case ctrl('V'):
#if TERMINFO
		case KEY_NPAGE:
#endif
		case '-':	/* display previous page */
#if TERMINFO
		case KEY_PPAGE:
#endif
		case '!':	/* shell escape */
		case '?':	/* help */
			(void) command(c);
			break;

		case ctrl('L'):	/* redraw screen */
#if TERMINFO
		case KEY_CLEAR:
#endif
			(void) command(c);
			goto same;

		case ESC:	/* don't change lines */
#if UNIXPC
			if((p = getmouseaction(DUMMYCHAR)) == NULL) {
				goto nochange;	/* unknown escape sequence */
			}
			break;
#endif
		case ctrl('G'):
			goto nochange;

		case '*':	/* mark/unmark all displayed lines */
			for (i = 0; topline + i < nextline; ++i) {
				mark(i);
			}
			goto same;

		case ctrl('A'):	/* mark/unmark all lines */
			for (i = 0; i < totallines; ++i) {
				if (change[i] == NO) {
					change[i] = YES;
				}
				else {
					change[i] = NO;
				}
			}
			/* show that all have been marked */
			seekline(totallines);
			break;
		case ctrl('X'):	/* mouse selection */
			if ((p = getmouseaction(DUMMYCHAR)) == NULL) {
				goto same;	/* unknown control sequence */
			}
			/* if the button number is a scrollbar tag */
			if (p->button == '0') {
				scrollbar(p);
				break;
			}
			/* find the selected line */
			/* note: the selection is forced into range */
			for (i = disprefs - 1; i > 0; --i) {
				if (p->y1 >= displine[i]) {
					break;
				}
			}
			mark(i);
			goto same;
		default:
		{
			/* if a line was selected */
			char		*cc;

			if ((cc = strchr(dispchars, c)))
				mark(cc - dispchars);

			goto same;
		}
		}
	}
	/* for each line containing the old text */
	(void) fprintf(script, "ed - <<\\!\n");
	*oldfile = '\0';
	seekline(1);
	for (i = 0; fscanf(refsfound, "%s%*s%s%*[^\n]", newfile, linenum) == 2;
	    ++i) {
		/* see if the line is to be changed */
		if (change[i] == YES) {
			anymarked = YES;
		
			/* if this is a new file */
			if (strcmp(newfile, oldfile) != 0) {
				
				/* make sure it can be changed */
				if (access(newfile, WRITE) != 0) {
					(void) sprintf(msg, "Cannot write to file %s", newfile);
					postmsg(msg);
					anymarked = NO;
					break;
				}
				/* if there was an old file */
				if (*oldfile != '\0') {
					(void) fprintf(script, "w\n");	/* save it */
				}
				/* edit the new file */
				(void) strcpy(oldfile, newfile);
				(void) fprintf(script, "e %s\n", oldfile);
			}
			/* output substitute command */
			(void) fprintf(script, "%ss/", linenum);	/* change */
			for (s = pattern; *s != '\0'; ++s) {	/* old text */
				if (strchr("/\\[.^*", *s) != NULL) {
					(void) putc('\\', script);
				}
				if (caseless == YES && isalpha((unsigned char)*s)) {
					(void) putc('[', script);
					if(islower((unsigned char)*s)) {
						(void) putc(toupper((unsigned char)*s), script);
						(void) putc(*s, script);
					}
					else {
						(void) putc(*s, script);
						(void) putc(tolower((unsigned char)*s), script);
					}
					(void) putc(']', script);
				}
				else	
					(void) putc(*s, script);
			}
			(void) putc('/', script);			/* to */
 			for (s = newpat; *s != '\0'; ++s) {	/* new text */
				if (strchr("/\\&", *s) != NULL) {
					(void) putc('\\', script);
				}
				(void) putc(*s, script);
			}
			(void) fprintf(script, "/gp\n");	/* and print */
		}
	}
	(void) fprintf(script, "w\nq\n!\n");	/* write and quit */
	(void) fclose(script);

	/* if any line was marked */
	if (anymarked == YES) {
		
		/* edit the files */
		clearprompt();
		(void) refresh();
		(void) fprintf(stderr, "Changed lines:\n\r");
		(void) execute("sh", "sh", temp2, NULL);
		askforreturn();
		seekline(1);
	}
	else {
nochange:
		clearprompt();
	}
	changing = NO;
	mousemenu();
	free(change);
	return(anymarked);
}

/* mark/unmark this displayed line to be changed */

void
mark(int i)
{
	int	j;
	
	j = i + topline - 1;
	if (j < totallines) {
		(void) move(displine[i], 1);

		if (change[j] == NO) {
			change[j] = YES;
			(void) addch('>');
		}
		else {
			change[j] = NO;
			(void) addch(' ');
		}
	}
}

/* scrollbar actions */

void
scrollbar(MOUSE *p)
{
	/* reposition list if it makes sense */
	if (totallines == 0) {
		return;
	}
	switch (p->percent) {
		
	case 101: /* scroll down one page */
		if (nextline + mdisprefs > totallines) {
			nextline = totallines - mdisprefs + 1;
		}
		break;
		
	case 102: /* scroll up one page */
		nextline = topline - mdisprefs;
		if (nextline < 1) {
			nextline = 1;
		}
		break;

	case 103: /* scroll down one line */
		nextline = topline + 1;
		break;
		
	case 104: /* scroll up one line */
		if (topline > 1) {
			nextline = topline - 1;
		}
		break;
	default:
		nextline = p->percent * totallines / 100;
	}
	seekline(nextline);
}

/* count the references found */

static void
countrefs(void)
{
	char	*subsystem;		/* OGS subsystem name */
	char 	*book;			/* OGS book name */
	char	file[PATHLEN + 1];	/* file name */
	char	function[PATLEN + 1];	/* function name */
	char	linenum[NUMLEN + 1];	/* line number */
	int	i;

	/* count the references found and find the length of the file,
	   function, and line number display fields */
	subsystemlen = 9;	/* strlen("Subsystem") */
	booklen = 4;		/* strlen("Book") */
	filelen = 4;		/* strlen("File") */
	fcnlen = 8;		/* strlen("Function") */
	numlen = 0;
	while ((i = fscanf(refsfound, "%250s%250s%6s %5000[^\n]", file,
	    function, linenum, tempstring)) != EOF) {
		if (i != 4 ||
		    !isgraph((unsigned char)*file) ||
		    !isgraph((unsigned char)*function) ||
		    !isdigit((unsigned char)*linenum)) {
			postmsg("File does not have expected format");
			totallines = 0;
			return;
		}
		if ((i = strlen(pathcomponents(file, dispcomponents))) > filelen) {
			filelen = i;
		}
		if (ogs == YES) {
			ogsnames(file, &subsystem, &book);
			if ((i = strlen(subsystem)) > subsystemlen) {
				subsystemlen = i;
			}
			if ((i = strlen(book)) > booklen) {
				booklen = i;
			}
		}
		if ((i = strlen(function)) > fcnlen) {
			fcnlen = i;
		}
		if ((i = strlen(linenum)) > numlen) {
			numlen = i;
		}
		++totallines;
	}
	rewind(refsfound);

	/* restrict the width of displayed columns */
	i = (COLS - 5) / 3;
	if (ogs == YES) {
		i = (COLS - 7) / 5;
	}
	if (filelen > i && i > 4) {
		filelen = i;
	}
	if (subsystemlen > i && i > 9) {
		subsystemlen = i;
	}
	if (booklen > i && i > 4) {
		booklen = i;
	}
	if (fcnlen > i && i > 8) {
		fcnlen = i;
	}
}
