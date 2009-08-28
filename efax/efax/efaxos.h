#ifndef _EFAXOS_H
#define _EFAXOS_H

#include <time.h>

#include "efaxlib.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

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
  void (*onsig)(int sig);
  unsigned char *ip, *iq ;
  unsigned char ibuf [ IBUFSIZE ] ;
  unsigned char *ibitorder, *obitorder ;
  int bytes, pad, lines ;
  int hwfc ;
  time_t start ;
  long mstart ;
  int rd_state ;
  int modem_wait ;
  int signal;
} TFILE ;

/* tgetc() is a macro like getc().  It evaluates to the next
   character from the fax device or EOF after idle time t. */

#define tgetc(f,t) ( (f)->ip >= (f)->iq && tundrflw(f,t) == EOF ? EOF : \
		    *(unsigned char*)(f)->ip++ )

int tundrflw ( TFILE *f, int t ) ;
int tgetd ( TFILE *f, int t ) ;
int tput ( TFILE *f, const char *p, int n ) ;
int tdata ( TFILE *f, int t ) ;
int ttyopen ( TFILE *f, char *fname, int reverse, int hwfc ) ;
int ttyclose ( TFILE *f ) ;
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

#ifdef __APPLE__

/* tdata() return values */
#define TDATA_READY		1
#define TDATA_TIMEOUT		0
#define TDATA_SELECTERR		-2
#define TDATA_SLEEP		-6
#define TDATA_WAKE		-7
#define TDATA_MANANSWER		-8
#define TDATA_CANCEL		-9
#define TDATA_MODEMADDED	-10
#define TDATA_MODEMREMOVED	-11

typedef struct syseventstruct		/*** System event data ****/
{
  unsigned char	event;			/* Event bit field */
  io_connect_t	powerKernelPort;	/* Power context data */
  long		powerNotificationID;	/* Power event data */
} sysevent_t;

extern sysevent_t sysevent;		/* system event data */
extern int waiting;			/* blocked waiting for activity (okay to exit on SIGHUP) */
extern int manual_answer;		/* Manual answer flag (set by client connection) */
extern int answer_wait;			/* blocked waiting for the first fax frame (inclusive of RING messages) */
extern char *faxfile;			/* device to use ("/dev/<*>.modem") */
extern int modem_found;			/* modem found */

extern int  cleanup ( int err );
extern void notify(CFStringRef status, CFTypeRef value);
extern void sysEventMonitorStart(void);
extern void sysEventMonitorStop(void);
#endif	/* __APPLE__ */

#endif
