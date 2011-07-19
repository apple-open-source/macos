/* $XFree86: xc/programs/xtrap/chparse.c,v 1.3tsi Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991 by Digital Equipment Corp., Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
/*
**++
**  FACILITY:  chparse - ASCII ESC & CSI parser
**
**  MODULE DESCRIPTION:
**
**      This module accepts single character I/O from
**      stdin and returns a parsed character or sequence
**      of characters.  This is to be used in conjunction
**      with passing data from dragon-speak to XTrap.
**
**  AUTHORS:
**
**      Roy Lomicka (revised by Martin Minow & later Kenneth B. Miller)
**
**  CREATION DATE:  March 26, 1991
**
**  DESIGN ISSUES:
**
**      The algorithm is rather obscure.  However, it was decided
**      that it should be left in its original condition since it
**      closely depicts the state transition diagram for ASCII
**      sequences (about the only good thing goto's are for;) .
**      The old-fashioned bracketing and tabbing schemed were
**      also preserved for posterity.
**
**--
*/

#include	<stdio.h>
#include	<ctype.h>
#ifdef vms
#include	<ssdef.h>
#include	<stsdef.h>
#include	<iodef.h>
#include	<descrip.h>
#else
#include	<sys/types.h>
#include	<sys/time.h>
#endif
#include	<string.h>
#include	<unistd.h>
#include	"chparse.h"

#ifdef __QNX__
#include <sys/select.h>
#endif

#ifndef	VERBOSE
#define	VERBOSE	0
#endif

#ifndef FALSE
#define	FALSE	0
#define	TRUE	1
#endif
#define	EOS	'\0'
#define	ERROR	(-1)
#define	TIMEOUT	(-2)
#define	NPARAM	8
#define	NINTER	8
#define	BUFFLEN	10			/* Size of typeahead buffer	*/
#ifndef	EXIT_SUCCESS
#ifdef vms
#define	EXIT_SUCCESS	(SS$_NORMAL | STS$M_INHIB_MSG)
#define	EXIT_FAILURE	(SS$_ABORT)
#else
#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#endif
#endif

#define	NUL	0x00
#define	CAN	0x18
#define	SUB	0x1A
#define	ESC	0x1B
#define DEL	0x7F
#define SS3     0x8f
#define	DCS	0x90
#define	CSI	0x9B
#define	ST	0x9C
#define	OSC	0x9D
#define	PM	0x9E
#define	APC	0x9F

static int kbinr(int max_delay);

/*
 * Escape sequence parser, obscure but useful.
 */
int chparse(
int		max_delay,
int		rest_delay,
int		*state,		/* Parser state (n.z. if incomplete)	*/
int		*private,	/* Sequence private char, 'X' if error	*/
int		param[],	/* numeric param, starting at param[1]	*/
int		*nparam,	/* Number of parameters			*/
int		inter[],	/* intermediate char, starting at [1]	*/
int		*ninter,	/* Number of intermediates		*/
int		*final)		/* Sequence terminator			*/
{
	register int		c;
	register int		i;

	if (*state == 0)
	    *private = 0;
label1:	c = kbinr(max_delay);
#if 0
	printf("c %02x, *state %02x, *nparam %d\n", c, *state, *nparam);
#endif
	max_delay = rest_delay;
label2:	switch (c) {
	case NUL:
	case DEL:
	    goto label5;
	case ESC:
	case CSI:
        case SS3:
	case DCS:
	case OSC:
	case PM:
	case APC:
	    *state = c;
	    *private = 0;
	    for (i = 0; i < NPARAM; i++)
		param[i] = 0;
	    for (i = 0; i < NINTER; i++)
		inter[i] = EOS;
	    *nparam = *ninter = 0;
	    goto label1;
	}
	if (*state == 0)
	    goto label5;
	if ((c >= 0x80 && c < 0xA0)
	 || c == TIMEOUT
	 || c == ERROR
	 || c == CAN
	 || c == SUB) {
	    *state = 0;
	    goto label5;
	}
	if (c < 0x20)				/* Doesn't stop seq.	*/
	    goto label5;
	if (c <= 0x2F) {
	    if (*ninter < 7)
		inter[++*ninter] = c;
	    goto label1;
	}
	if (*state == ESC) {
	    if (*ninter == 0
	     && (c & 0x3F) < 0x20) {
		c = (c & 0x3F) + 0x80;
		goto label2;
	    }
	    goto label4;
	}
	if (c >= 0x40)
	    goto label3;
	else if (c >= 0x3C) {			/* Private introducer	*/
	    if (*nparam != 0)
		 *private = 'X';
	    else {
		*private = c;
		*nparam = 1;
	    }
	    goto label1;
	}
	if (*nparam == 0)
	    *nparam = 1;
	if (*ninter != 0) {
	    *ninter = 0;
	    *private = 'X';
	}
	if (c == ';') {
	    if (*nparam >= (NPARAM - 1))
		*private = 'X';
	    else {
		++*nparam;
	    }
	    goto label1;
	}
	if (c > '9') {
	    *private = 'X';
	    goto label1;
	}
	param[*nparam] = (param[*nparam] * 10) + (c - '0');
	goto label1;
label3:	if (*nparam == 0)
	    *nparam = 1;
label4:	*final = c;
	c = *state;
	*state = 0;
label5:	return (c);
}

void dumpsequence(
int            state,
int		c,
int		private,	/* Sequence private char, 'X' if error	*/
int		param[],	/* numeric param, starting at param[1]	*/
int		nparam,		/* Number of parameters			*/
int		inter[],	/* intermediate char, starting at [1]	*/
int		ninter,		/* Number of intermediates		*/
int		final,		/* Sequence terminator			*/
short           *column)        /* column display count                 */
{
    register int	i;

    if (isascii(c) && isprint(c)) {
        *column +=2;
        if (*column >= 79) {
            printf("\n");
            *column = 2;
        }
        printf("%c ", c);
    }
    else if (private == 'X') {
        *column += strlen("bad sequence ");
        if (*column >= 79) {
            printf("\n");
            *column = strlen("bad sequence ");
        }
        printf("bad sequence ");
    }
    else if (state != NUL) {
        *column += 32;
        if (*column >= 79) {
            printf("\n");
            *column = 32;
        }
        printf("incomplete sequence (type <%02x>) ", state);
    }
    else {
        *column += 5;    /* update total chars printed */ 
        if (*column >= 79) {
            printf("\n");
            *column = 6;
        }
        switch (c) {
        case ESC:	printf("<ESC>");	break;
        case DCS:	printf("<DCS>");	break;
        case CSI:	printf("<CSI>");	break;
        case SS3:	printf("<SS3>");	break;
        default:	printf("<%02x>", c & 0xFF);
        }
        if (c == ESC || c == DCS || c == CSI || c == SS3) {
            *column += 1 + nparam*2 + ninter + 1;    /* total chars printed */ 
            if (*column >= 79) {
                printf("\n");
                *column = 1 + nparam*2 + ninter + 1;
            }
    	    if (private != NUL && private != 'X')
    	        printf("%c", private);
            for (i = 1; i <= nparam; i++)
    	        printf("%s%d", (i > 1) ? "," : " ", param[i]);
	    for (i = 1; i <= ninter; i++)
	        printf("%c", inter[i]);
	    printf("%c", final);
        }
        (*column)++;
        if (*column >= 79) {
            printf("\n");
            *column = 1;
        }
        printf(" ");
    }
}

#ifdef vms
/*
 * Read a character from the terminal (with selectable timeout)
 */

int
kbinr(timeout)
int		timeout;		/* In seconds, 0 == immediately	*/
/*
 * Get one byte without echoing, if available.  Returns
 *	ERROR		Something is dwrong
 *	TIMEOUT		Nothing read within limit.
 * Note:
 *	timeout = 0	return immediately if nothing is present
 *	timeout = 1	Indeterminate, do not use
 *	timeout = 2	Wait at least one second.
 */
{
	register int	incount;
	static char	buffer[BUFFLEN];
	static char	*bufptr = buffer;
	static char	*bufend = buffer;

	if (bufptr >= bufend) {
	    bufptr = bufend = buffer;
	    incount = vmsread(buffer, BUFFLEN, 0);
	    if (incount == TIMEOUT)
		incount = vmsread(buffer, 1, timeout);
	    if (incount <= 0)
		return (incount);
	    bufend = &buffer[incount];
	}
	return (*bufptr++ & 0xFF);
}

static int		tt_channel;	/* Gets channel number		*/
typedef	struct {
    short int	status;
    short int	term_offset;
    short int	terminator;
    short int	term_size;
} IOSTAB;

int
vmsread(buffer, size, timeout)
char		*buffer;
int		size;
int		timeout;
{
	register int		status;
	IOSTAB			iostab;
	static $DESCRIPTOR(tt_device, "SYS$COMMAND");
	static long	termset[2] = { 0, 0 };	/* No terminator	*/
	static short	opened = FALSE;		/* TRUE when opened	*/

	if (!opened) {
	    status = sys$assign(&tt_device, &tt_channel, 0, 0);
	    if (status != SS$_NORMAL)
		lib$stop(status);
            opened = TRUE;
	}
	status = sys$qiow(
		0,			/* Event flag 			*/
		tt_channel,		/* Input channel		*/
		IO$_READLBLK | IO$M_NOECHO | IO$M_NOFILTR | IO$M_TIMED,
					/* Read, no echo, no translate	*/
		&iostab,		/* I/O status block		*/
		NULL,			/* AST block (none)		*/
		0,			/* AST parameter		*/
		buffer,			/* P1 - input buffer		*/
		size,			/* P2 - buffer length		*/
		timeout,		/* P3 - timeout			*/
		&termset,		/* P4 - terminator set		*/
		NULL,			/* P5 - ignored (prompt buffer)	*/
		0			/* P6 - ignored (prompt size)	*/
	);
	if (status == SS$_TIMEOUT)
	    return (TIMEOUT);
	else if (status != SS$_NORMAL)
	    return (ERROR);
	else {
	    if ((status = iostab.term_offset + iostab.term_size) > 0)
		return (status);
	    return (TIMEOUT);
	}
}
#else
static int
kbinr(int max_delay)
{
	auto int		fdmask;
	struct timeval		timeout;
	int			count;
	static unsigned char	buffer[80];
	static unsigned char	*bend;
	static unsigned char	*bptr;

	if (bptr >= bend) {
	    fdmask = 1 << fileno(stdin);
	    timeout.tv_usec = 0;
	    timeout.tv_sec = max_delay;
	    count = select(fileno(stdin) + 1, (fd_set *)&fdmask, NULL, NULL, &timeout);
	    if (count < 0)
		return (ERROR);
	    else if (count == 0)
		return (TIMEOUT);
	    if (count >= sizeof buffer)
		count = sizeof buffer;
	    count = read(fileno(stdin), buffer, count);
	    if (count <= 0)
		return (ERROR);
	    bptr = buffer;
	    bend = buffer + count;
	}
	return (*bptr++);
}
#endif
