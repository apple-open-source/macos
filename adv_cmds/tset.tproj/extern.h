/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *
 *	@(#)extern.h	8.1 (Berkeley) 6/9/93
 */

/* This should be in <termcap.h> instead. */
extern char PC;
extern short ospeed;
int	 tgetent __P((char *, char *));
int	 tgetflag __P((char *));
int	 tgetnum __P((char *));
char	*tgetstr __P((char *, char **));
char	*tgoto __P((char *, int, int));
int	 tputs __P((char *, int, void (*) __P((int))));

extern struct termios mode, oldmode;
extern int columns, isreset, lines;
extern int erasechar, intrchar, killchar;

void	 add_mapping __P((char *, char *));
void	 cat __P((char *));
void	 err __P((const char *, ...));
char	*get_termcap_entry __P((char *, char **));
char	*mapped __P((char *));
void	 outc __P((int));
void	 reset_mode __P((void));
void	 set_control_chars __P((void));
void	 set_conversions __P((int));
void	 set_init __P((void));
void	 wrtermcap __P((char *));
