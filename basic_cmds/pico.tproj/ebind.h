/*
 * $Id: ebind.h,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $
 *
 * Program:	Default key bindings
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
 *
 * NOTES:
 *
 *	This files describes the key bindings for pico and the pine 
 *      composer.  The binds are static, (i.e., no way for the user
 *      to change them) so as to keep pico/composer as simple to use
 *      as possible.  This, of course, means the number of functions is
 *      greatly reduced, but, then again, this is seen as very desirable.
 *
 *      There are very limited number of flat ctrl-key bindings left, and 
 *      most of them are slated for yet-to-be implemented functions, like
 *      invoking an alternate editor in the composer and necessary funcs
 *      for imlementing attachment handling.  We really want to avoid 
 *      going to multiple keystroke functions. -mss
 *
 */

/*	EBIND:		Initial default key to function bindings for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	EBIND_H
#define	EBIND_H


/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  keytab[NBINDS] = {
	{K_PAD_UP,		backline},
	{K_PAD_DOWN,		forwline},
	{K_PAD_RIGHT,		forwchar},
	{K_PAD_LEFT,		backchar},
	{K_PAD_PREVPAGE,	backpage},
	{K_PAD_NEXTPAGE,	forwpage},
	{K_PAD_HOME,		gotobol},
	{K_PAD_END,		gotoeol},
	{K_PAD_DELETE,		forwdel},
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
	{CTRL|'C',		abort_composer},
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		whelp},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		fillpara},
	{CTRL|'K',		killregion},
	{CTRL|'L',		refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		suspend_composer},
	{CTRL|'P',		backline},
	{CTRL|'R',		insfile},
#ifdef	SPELLER
	{CTRL|'T',		spell},
#endif	/* SPELLER */
	{CTRL|'U',		yank},
	{CTRL|'V',		forwpage},
	{CTRL|'W',		forwsearch},
	{CTRL|'X',		wquit},
	{CTRL|'Y',		backpage},
#ifdef	JOB_CONTROL
	{CTRL|'Z',		bktoshell},
#endif
	{CTRL|'@',		forwword},
	{CTRL|'^',		setmark},
#ifdef	JOB_CONTROL
	{CTRL|'_',		alt_editor},
#endif
	{0x7F,			backdel},
	{0,			NULL}
};


/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  pkeytab[NBINDS] = {
	{K_PAD_UP,		backline},
	{K_PAD_DOWN,		forwline},
	{K_PAD_RIGHT,		forwchar},
	{K_PAD_LEFT,		backchar},
	{K_PAD_PREVPAGE,	backpage},
	{K_PAD_NEXTPAGE,	forwpage},
	{K_PAD_HOME,		gotobol},
	{K_PAD_END,		gotoeol},
	{K_PAD_DELETE,		forwdel},
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
	{CTRL|'C',		showcpos},
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		whelp},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		fillpara},
#ifdef	ONLYWHILETESTING
	{CTRL|'K',		killtext},
#else
	{CTRL|'K',		killregion},
#endif
	{CTRL|'L',		refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		filewrite},
	{CTRL|'P',		backline},
	{CTRL|'R',		insfile},
#ifdef	SPELLER
	{CTRL|'T',		spell},
#endif	/* SPELLER */
	{CTRL|'U',		yank},
	{CTRL|'V',		forwpage},
	{CTRL|'W',		forwsearch},
	{CTRL|'X',		wquit},
	{CTRL|'Y',		backpage},
#ifdef	JOB_CONTROL
	{CTRL|'Z',		bktoshell},
#endif
	{CTRL|'@',		forwword},
	{CTRL|'^',		setmark},
	{0x7F,			backdel},
	{0,			NULL}
};

#endif	/* EBIND_H */
