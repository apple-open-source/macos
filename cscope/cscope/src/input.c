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

/*	cscope - interactive C symbol cross-reference
 *
 *	terminal input functions
 */

#include "global.h"
#if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <setjmp.h>	/* jmp_buf */
#include <stdlib.h>
#if HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

static char const rcsid[] = "$Id: input.c,v 1.6 2002/01/09 23:49:02 umeshv Exp $";

static	jmp_buf	env;		/* setjmp/longjmp buffer */
static	int	prevchar;	/* previous, ungotten character */

/* catch the interrupt signal */

/*ARGSUSED*/
RETSIGTYPE
catchint(int sig)
{
 	(void) sig;		/* 'use' it, to avoid a warning */
	(void) signal(SIGINT, catchint);
	longjmp(env, 1);
}

/* unget a character */

void
myungetch(int c)
{
	prevchar = c;
}

/* get a character from the terminal */

int
mygetch(void)
{
	RETSIGTYPE	(*savesig)();		/* old value of signal */
	int	c;

	/* change an interrupt signal to a break key character */
	if (setjmp(env) == 0) {
		savesig = signal(SIGINT, catchint);
		refresh();	/* update the display */
		mousereinit();	/* curses can change the menu number */
		if(prevchar) {
			c = prevchar;
			prevchar = 0;
		}
		else
			c = getch();	/* get a character from the terminal */
	}
	else {	/* longjmp to here from signal handler */
		c = KEY_BREAK;
	}
	(void) signal(SIGINT, savesig);
	return(c);
}

/* get a line from the terminal in non-canonical mode */

int
getline(char s[], unsigned size, int firstchar, BOOL iscaseless)
{
	int	c, i = 0;
	int	j;
	char *sright;	/* substring to the right of the cursor */
	int ri = 0;		/* position in right-string */

	/* Inserts and deletes are always performed on the left-string,
	 * but we'll also have a right-string 'sright' to hold characters
	 * which are on the right of the cursor [insertion point].
	 *
	 * Think of 'sright' as a stack -- we push chars into it when the cursor
	 * moves left, and we pop chars off it when the cursor moves right again.
	 * At the end of the function, we'll pop off any remaining characters
	 * onto the end of 's'
	 */
	sright = calloc(sizeof(char), size);

	/* if a character already has been typed */
	if (firstchar != '\0') {
		if(iscaseless == YES) {
			firstchar = tolower(firstchar);
		}
		addch(firstchar);	/* display it */
		s[i++] = firstchar;	/* save it */
	}
	/* until the end of the line is reached */
	while ((c = mygetch()) != '\r' && c != '\n' && c != KEY_ENTER) {

#if TERMINFO
		if (c == KEY_LEFT || c == ctrl('B')) {	/* left */
#else
		if (c == ctrl('B')) {	/* left */
#endif
			if (i > 0) {
				addch('\b');
				/* move this char into the second (rhs) string */
				sright[ri++] = s[--i];
			}
		}
#if TERMINFO
		else if (c == KEY_RIGHT || c == ctrl('F')) {	/* right */
#else
		else if (c == ctrl('F')) {	/* right */
#endif
			if (i < size && ri > 0) {
				/* move this char to the left of the cursor */
				s[i++] = sright[--ri];
				addch(s[i-1]);
			}
		}
		else if (
		#ifdef KEY_HOME
			c == KEY_HOME ||
		#endif
			c == ctrl('A') ) {
			while ( i > 0 ) {
				sright[ri++] = s[--i];
				addch('\b');
				addch(s[i]);
				addch('\b');
			}
		}
		else if (
		#ifdef KEY_END
			c == KEY_END ||
		#endif
			c == ctrl('E') ) {
			while ( ri > 0 ) {
				s[i++] = sright[--ri];
				addch(s[i-1]);
			}
		}
		else if (c == erasechar() || c == KEY_BACKSPACE || c == DEL || c == ctrl('H') ) {
			/* erase */
			if (i > 0) {
				if (ri == 0)  {
					addstr("\b \b");
				} else {
					addch('\b');
					delch();
				}
				s[i] = '\0';
				--i;
			}
		}
		else if (c == killchar() || c == KEY_BREAK) {	/* kill */
			for (j = 0; j < i; ++j) {
				addch('\b');
			}
			for (j = 0; j < i; ++j) {
				addch(' ');
			}
			for (j = 0; j < i; ++j) {
				addch('\b');
			}
			i = 0;
		}
		else if (isprint(c) || c == '\t') {		/* printable */
			if(iscaseless == YES) {
				c = tolower(c);
			}
			/* if it will fit on the line */
			if (i < size) {
				s[i++] = c;	/* save it */
				if (ri == 0) {
					addch(c);	/* display it */
				} else {
					insch(c);	/* display it */
					addch(c);	/* advance cursor */
				}
			}
		}
#if UNIXPC
		else if (unixpcmouse == YES && c == ESC) {	/* mouse */
			(void) getmouseaction(ESC);	/* ignore it */
		}
#endif
		else if (mouse == YES && c == ctrl('X')) {
			(void) getmouseaction(ctrl('X'));	/* ignore it */
		}
		else if (c == EOF) {				/* end-of-file */
			break;
		}
		/* return on an empty line to allow a command to be entered */
		if (firstchar != '\0' && (i+ri) == 0) {
			break;
		}
	}

	/* move any remaining chars on the rhs of the cursor
	 * onto the end of our string
	 */
	for (; ri > 0; ) {
		s[i++] = sright[--ri];
	}
	free(sright);

	s[i] = '\0';
	return(i);
}

/* ask user to enter a character after reading the message */

void
askforchar(void)
{
	addstr("Type any character to continue: ");
	(void) mygetch();
}

/* ask user to press the RETURN key after reading the message */

void
askforreturn(void)
{
	(void) fprintf(stderr, "Press the RETURN key to continue: ");
	(void) getchar();
#if Darwin
	(void) clear();
	move(0, 0);
#endif
}

/* expand the ~ and $ shell meta characters in a path */

void
shellpath(char *out, int limit, char *in) 
{
	char	*lastchar;
	char	*s, *v;

	/* skip leading white space */
	while (isspace((unsigned char)*in)) {
		++in;
	}
	lastchar = out + limit - 1;

	/* a tilde (~) by itself represents $HOME; followed by a name it
	   represents the $LOGDIR of that login name */
	if (*in == '~') {
		*out++ = *in++;	/* copy the ~ because it may not be expanded */

		/* get the login name */
		s = out;
		while (s < lastchar && *in != '/' && *in != '\0' && !isspace((unsigned char)*in)) {
			*s++ = *in++;
		}
		*s = '\0';

		/* if the login name is null, then use $HOME */
		if (*out == '\0') {
			v = getenv("HOME");
		}
		else {	/* get the home directory of the login name */
			v = logdir(out);
		}
		/* copy the directory name */
		if (v != NULL) {
			(void) strcpy(out - 1, v);
			out += strlen(v) - 1;
		}
		else {	/* login not found, so ~ must be part of the file name */
			out += strlen(out);
		}
	}
	/* get the rest of the path */
	while (out < lastchar && *in != '\0' && !isspace((unsigned char)*in)) {

		/* look for an environment variable */
		if (*in == '$') {
			*out++ = *in++;	/* copy the $ because it may not be expanded */

			/* get the variable name */
			s = out;
			while (s < lastchar && *in != '/' && *in != '\0' &&
			    !isspace((unsigned char)*in)) {
				*s++ = *in++;
			}
			*s = '\0';
	
			/* get its value */
			if ((v = getenv(out)) != NULL) {
				(void) strcpy(out - 1, v);
				out += strlen(v) - 1;
			}
			else {	/* var not found, so $ must be part of the file name */
				out += strlen(out);
			}
		}
		else {	/* ordinary character */
			*out++ = *in++;
		}
	}
	*out = '\0';
}
