#ifndef _EFAXOS_H
#define _EFAXOS_H

#include <time.h>

#include "efaxlib.h"

/* signals to be caught */

#define ANSISIGS  SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, SIGTERM
#define UNIXSIGS  SIGHUP, SIGQUIT, SIGIOT, SIGALRM
#define CATCHSIGS ANSISIGS, UNIXSIGS

/* Bit order reversal table. */

extern unsigned char normalbits [ ] ;

typedef enum ttymodes		/* serial port modes:  */
{
    COMMAND,			/*   19200 8N1, no f/c, DTR high */
    SEND,			/*   19200 send-only XON/XOFF f/c */
    VOICECOMMAND,		/*   38400 8N1, no f/c, DTR high */
    VOICESEND,			/*   38400 send-only XON/XOFF f/c*/
    DROPDTR,			/*   ", DTR low */
    ORIGINAL			/*   restore original settings */
} ttymodes ;

/* OS-specific i/o & delay functions */

/* We define new stream i/o macros because it's not possible to
   do non-blocking reads/writes with C stream i/o [UNIX select()
   gives the status of the file, not the stream buffer].*/

#define IBUFSIZE 1024	    /* read up to this many bytes at a time from fax */
#define OBUFSIZE 1024	    /* maximum bytes to write at a time to fax */

typedef struct tfilestruct {
  int fd ;
  unsigned char *ip, *iq ;
  unsigned char ibuf [ IBUFSIZE ] ;
  unsigned char *ibitorder, *obitorder ;
  int bytes, pad, lines ;
  int hwfc ;
  time_t start ;
  long mstart ;
  int rd_state ;
} TFILE ;

/* tgetc() is a macro like getc().  It evaluates to the next
   character from the fax device or EOF after idle time t. */

#define tgetc(f,t) ( (f)->ip >= (f)->iq && tundrflw(f,t) == EOF ? EOF : \
		    *(unsigned char*)(f)->ip++ )

int tundrflw ( TFILE *f, int t ) ;
int tgetd ( TFILE *f, int t ) ;
int tput ( TFILE *f, unsigned char *p, int n ) ;
int tdata ( TFILE *f, int t ) ;
void tinit ( TFILE *f, int fd, int reverse, int hwfc ) ;
int ttyopen ( TFILE *f, char *fname, int reverse, int hwfc ) ;
int ttymode ( TFILE *f, ttymodes mode ) ;
void msleep ( int t ) ;
long proc_ms ( void ) ;
int time_ms ( void ) ;

/* POSIX execl */

extern int execl ( const char *path, const char *arg , ... ) ;

/* UUCP-style device locks */

#define BINLKFLAG '#'		/* prefix to force binary lock files */

				/* [un]lock serial port using named files */
int lockall ( TFILE *f, char **lkfiles, int log ) ;
int unlockall ( TFILE *f, char **lkfiles ) ;

/* extract program name to be used in messages from argv0 */  

char *efaxbasename ( char *p ) ;

/* default fax modem device */

#define FAXFILE "/dev/modem"

#endif
