/* termios for WIN32.

   Derived from sources with the following header comments:
   Written by Doug Evans and Steve Chamberlain of Cygnus Support

   THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef	_SYS_TERMIOS_H
#define _SYS_TERMIOS_H

typedef int pid_t;
#define _POSIX_JOB_CONTROL

#define TCOOFF	0
#define TCOON	1
#define TCIOFF	2
#define TCION	3

#define TCGETA   5
#define TCSETA   6

#define TCIFLUSH	0
#define TCOFLUSH	1
#define TCIOFLUSH	2
#define TCFLSH          3

#define TCSAFLUSH	1
#define TCSANOW		2
#define TCSADRAIN	3
#define TCSADFLUSH	4

#define FIONBIO 5

/* iflag  bits */
#define IGNBRK	0x0001
#define BRKINT	0x0002
#define IGNPAR	0x0004
#define INPCK	0x0010
#define ISTRIP	0x0020
#define INLCR	0x0040
#define IGNCR	0x0080
#define ICRNL	000400
#define IXON	002000
#define IXOFF	010000
#define PARMRK  020000
#define IUCLC   040000
#define IXANY   100000

#define OPOST	000001
#define OCRNL	000004
#define ONLCR	000010
#define ONOCR	000020
#define ONLRET  000040
#define TAB3	014000

#define CLOCAL	004000
#define CREAD	000200
#define CSIZE	000060
#define CS5	0
#define CS6	020
#define CS7	040
#define CS8	060
#define CSTOPB	000100
#define HUPCL	002000
#define PARENB	000400
#define PARODD	001000

/* lflag bits */
#define ISIG	0x0001
#define ICANON	0x0002
#define ECHO	0x0004
#define ECHOE	0x0008
#define ECHOK	0x0010
#define ECHONL	0x0020
#define NOFLSH  0x0040
#define TOSTOP	0x0080
#define IEXTEN	0x0100
#define FLUSHO  0x0200

#define VDISCARD	1
#define VEOL		2
#define VEOL2   	3
#define VEOF		4
#define VERASE		5
#define VINTR		6
#define VKILL		7
#define VLNEXT 		8
#define VMIN		9
#define VQUIT		10
#define VREPRINT 	11
#define VSTART		12
#define VSTOP		13
#define VSUSP		14
#define VSWTC 		15
#define VTIME		16
#define VWERASE 	17

#define NCCS 		18

#define B0	000000
#define B50	000001
#define B75	000002
#define B110	000003
#define B134	000004
#define B150	000005
#define B200	000006
#define B300	000007
#define B600	000010
#define B1200	000011
#define B1800	000012
#define B2400	000013
#define B4800	000014
#define B9600	000015
#define B19200	000016
#define B38400	000017

typedef unsigned char cc_t;
typedef unsigned short tcflag_t;
typedef char speed_t;

struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	char		c_line;
	cc_t		c_cc[NCCS];
	speed_t		c_ispeed;
	speed_t		c_ospeed;
};

#define termio termios

#define cfgetospeed(tp)		((tp)->c_ospeed)
#define cfgetispeed(tp)		((tp)->c_ispeed)
#define cfsetospeed(tp,s)	(((tp)->c_ospeed = (s)), 0)
#define cfsetispeed(tp,s)	(((tp)->c_ispeed = (s)), 0)

/* Extra stuff to make porting stuff easier.  */

struct winsize
{
  unsigned short ws_row, ws_col;
};

#define TIOCGWINSZ (('T' << 8) | 1)
#define TIOCSWINSZ (('T' << 8) | 2)

#endif	/* _SYS_TERMIOS_H */
