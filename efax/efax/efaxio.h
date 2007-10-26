#ifndef _EFAXIO_H
#define _EFAXIO_H

#include "efaxos.h"		/* TFILE definition */

#ifndef uchar
#define uchar unsigned char
#endif

#define CMDBUFSIZE 256		/* longest possible command or response */
#define DLE_ETX "\020\003"	/* DLE-ETX (end of data) string */
#define CAN_STR "\030"	        /* CAN (cancel reception) string */
#define TO_RESET  50		/* t/o for modem reset commands (>>2.6s) */
#define T_CMD     1		/* pause before each modem command */
#define MAXDCEBUF  32		/* max bytes allowed in buffer when write */
#define MINWRITE   64		/* minimum bytes to write() to modem */

enum promptcodes {		/* codes for modem prompts */
   BUSY = 'B', CONNECT = 'C', DATA='D', ERROR = 'E', 
   MODULATION='M', NO = 'N', OK = 'O', RING = 'R', VCONNECT = 'V',
   PLUS = '+' } ;

		      /* Modem features */

extern int c1, c20 ;		/* use class 1/class 2.0 */
extern int c2 ;			/* force class 2 */
extern int cmdpause ;		/* delay before each init command */
extern int vfc ;		/* virtual flow control */
extern char startchar ;		/* character to start reception */
extern int lockpolldelay ;	/* milliseconds between checks of lock files */

				/* response detector lookup tables */
extern uchar rd_allowed[], rd_nexts[] ; 
#define RD_BEGIN 0x01
#define RD_END   0x20

		 /* Modem interface routines */

int cmd ( TFILE *f, char *s , int t ) ;
int ckcmd ( TFILE *f, int *err, char *s, int t, int r ) ;
int modemsync ( TFILE *f ) ;
char *sresponse ( char *s, int *ip ) ;
char *strinresp ( char *s ) ;
int getresp ( char *s, char *buf, int len ) ;

int setup ( TFILE *f, char **cmds, int ignerr ) ;

int sendbuf ( TFILE *f, uchar *p, int n, int dcecps ) ;

int begin_session ( TFILE *f, char *fname, int reverse, int hwfc, char **lkfile, 
		   ttymodes mode, void (*onsig) (int), int wait ) ;

int end_session ( TFILE *f, char **zcmd,  char **lkfile, int sync ) ;

#endif
