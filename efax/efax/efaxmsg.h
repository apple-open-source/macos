#ifndef _EFAXMSG_H
#define _EFAXMSG_H

#include <time.h>

#ifndef uchar
#define uchar unsigned char
#endif

		    /* Messages & Program Arguments */

enum  cchar {				/* control characters */
  NUL, SOH, STX, ETX, EOT, ENQ, ACK, BEL, BS,  HT,  LF,
  VT,  FF,  CR,  SO,  SI,  DLE, XON, DC2, XOFF,DC4, NAK,
  SYN, ETB, CAN, EM,  SUB, ESC, FS,  GS,  RS,  US } ;

extern char *verb[] ;		/* types of messages to print */
extern char *argv0 ;		/* program name */

char *cname ( unsigned char c ) ;
time_t tstamp ( time_t last, FILE *f ) ;
int msg ( char *fmt, ... ) ;
const char *setCopyright(const char *copyright);

extern int nxtoptind ;
extern char *nxtoptarg ;

int nextopt( int argc, char **argv, char *args ) ;

#endif


