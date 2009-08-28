/* 
		efaxos.c - O/S-dependent routines
		    Copyright 1995, Ed Casas
*/

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef FD_SET
#include <sys/select.h>		/* for AIX */
#endif

#include "efaxlib.h"
#include "efaxmsg.h"
#include "efaxos.h"

#ifdef USE_TERMIO
#include <termio.h>
#include <sys/ioctl.h>
#define termios termio
#define tcgetattr(fd, pt) ioctl(fd, TCGETA, pt)
#define tcsetattr(fd, x, pt) ioctl(fd, TCSETAW, pt)
#define cfsetospeed(pt, b) ((pt)->c_cflag = ((pt)->c_cflag & ~CBAUD) | b)
#define cfsetispeed(pt, b)
#define tcdrain(fd)
#else
#include <termios.h>
#endif

#ifdef TIOCSSOFTCAR
#include <sys/ioctl.h>
#endif

#ifdef __APPLE__
#include <sys/ioctl.h>
#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <IOKit/usb/IOUSBLib.h>
#include <pthread.h>

/*
 * Constants...
 */

#define SYSEVENT_CANSLEEP	0x1	/* Decide whether to allow sleep or not */
#define SYSEVENT_WILLSLEEP	0x2	/* Computer will go to sleep */
#define SYSEVENT_WOKE		0x4	/* Computer woke from sleep */
#define SYSEVENT_MODEMADDED	0x8	/* Modem was added */
#define SYSEVENT_MODEMREMOVED	0x10	/* Modem was removed */


/* 
 * Structures... 
 */

typedef struct threaddatastruct		/*** Thread context data  ****/
{
  sysevent_t		sysevent;	/* System event */
} threaddata_t;


/* 
 * Globals... 
 */

static pthread_t	sysEventThread = NULL;		/* Thread to host a runloop */
static pthread_mutex_t	sysEventThreadMutex = { 0 };	/* Coordinates access to shared gloabals */ 
static pthread_cond_t	sysEventThreadCond = { 0 };	/* Thread initialization complete condition */
static CFRunLoopRef	sysEventRunloop = NULL;		/* The runloop. Access must be protected! */
static int		sysEventPipes[2] = { -1, -1 };	/* Pipes for system event notifications */
sysevent_t		sysevent;			/* The system event */

static int		clientEventFd = -1;		/* Listening socket for client commands */
static const char	clientSocketName[] = "/var/run/efax"; 
							/* Listener's domain socket name */

/* 
 * Prototypes... 
 */

static int  sysEventMonitorUpdate(TFILE *f);
static void *sysEventThreadEntry();
static void sysEventPowerNotifier(void *context, io_service_t service, natural_t messageType, void *messageArgument);
static int clientEventUpdate();
static void deviceNotifier(void *context, io_iterator_t iterator);

#endif	/* __APPLE__ */

static int  ttlock ( char *fname, int log );
static int  ckfld ( char *field, int set, int get );
static int  checktermio ( struct termios *t, TFILE *f );
static void tinit ( TFILE *f, int fd, int reverse, int hwfc );
static int  ttlocked ( char *fname, int log );
static int  ttunlock ( char *fname );


#ifndef CRTSCTS
#define CRTSCTS 0
#endif


#if defined(__APPLE__)
/* Send Mac OS X notification */
void notify(CFStringRef status, CFTypeRef value)
{
  CFMutableDictionaryRef notification;
  int i;
  static CFNumberRef pid = NULL;

  if (!pid)
  {
    i = getpid();
    pid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
  }

  notification = CFDictionaryCreateMutable(kCFAllocatorDefault, 3,
				&kCFTypeDictionaryKeyCallBacks, 
				&kCFTypeDictionaryValueCallBacks);

  CFDictionaryAddValue(notification, CFSTR("pid"), pid);
  CFDictionaryAddValue(notification, CFSTR("status"), status);

  if (value)
    CFDictionaryAddValue(notification, CFSTR("value"), value);

  CFNotificationCenterPostNotificationWithOptions(
	CFNotificationCenterGetDistributedCenter(),
	status,
	CFSTR("com.apple.efax"),
	notification, kCFNotificationDeliverImmediately | kCFNotificationPostToAllSessions);

  CFRelease(notification);
}
#endif

/* The milliseconds portion of the current time.  If your system
   does not provide gettimeofday(3) you can safely substitute a
   dummy function that returns 0.  This will cause the
   milliseconds portion of time stamps to be printed as 0. */

int time_ms ( void )
{
  struct timeval tv ;
  gettimeofday ( &tv, NULL ) ;
  return (int) ( tv.tv_usec / 1000 ) ;
}


/* Process elapsed time in milliseconds.  This is used for
   ``virtual flow control'' only. */

long proc_ms ( void )
{
  struct timeval t ;
  static int init=0 ;
  static struct timeval s ;
  if ( ! init ) { 
    gettimeofday ( &s, 0 ) ;
    init = 1 ;
  }
  gettimeofday ( &t, 0 ) ;
  return ( t.tv_sec - s.tv_sec ) * 1000 + ( t.tv_usec - s.tv_usec ) / 1000 ;
}


/* Wait for t millisecond (for systems without usleep). */

void msleep ( int t )
{
  struct timeval timeout ;
  timeout.tv_sec  = t / 1000 ; 
  timeout.tv_usec = ( t % 1000 ) * 1000 ;
  if ( select ( 1 , 0 , 0 , 0 , &timeout ) < 0 ) 
    msg ("ES2select failed in msleep:") ;
}


/* Return number of characters ready to read or < 0 on error.  t
   is tenths of a second of idle time before timing out.  If t is
   negative, waits forever. 
 
    1: characters ready to read
    0: timeout
   -2: select() failed
   -6: preparing to sleep
   -7: woke / manual answer
   -8: modem detected / cancel session
 */

int tdata ( TFILE *f, int t )
{
  int n, err=0 ;
  fd_set fds ;
  int maxfd;
  struct timeval timeout ;
  static int modem_added = 0;

  if (modem_added && f->modem_wait)
  {
    modem_added = 0;
    return TDATA_MODEMADDED;
  }

  timeout.tv_sec  = t / 10 ; 
  timeout.tv_usec = ( t % 10 ) * 100000 ;

  FD_ZERO (&fds);

  if (f->fd != -1)
    FD_SET(f->fd, &fds);

  maxfd = f->fd;

#ifdef __APPLE__
  if ( sysEventPipes[0] != -1)
  {
    FD_SET(sysEventPipes[0], &fds);

    if (sysEventPipes[0] > maxfd)
      maxfd = sysEventPipes[0];
  }

  if ( clientEventFd != -1)
  {
    FD_SET(clientEventFd, &fds);

    if (clientEventFd > maxfd)
      maxfd = clientEventFd;
  }
#endif

  for (;;)
  {
    n = select ( maxfd + 1, &fds, 0, 0, t<0 ? 0 : &timeout ) ;

    if (n == 0)
      break;		/* timeout */

    if (n < 0)
    {
      if (errno == EINTR)
      {
        if (f->signal)
        {
          int sig = f->signal;
          f->signal = 0;

          (f->onsig)(sig);
        }
        else
	  msg("Wselect() interrupted in tdata()");
	continue;
      }

      msg("Eselect() failed in tdata():");
      err = TDATA_SELECTERR;
      break;
    }

    /* else n > 0 */

#ifdef __APPLE__
    if (sysEventPipes[0] != -1 && FD_ISSET(sysEventPipes[0], &fds))
    {
      err = sysEventMonitorUpdate(f);

      if (err == TDATA_MODEMADDED && !f->modem_wait)
      {
        /* If a modem was added but we're not interested just remember it for later... */
	modem_added = 1;
	err = 0;
      }

      if (err)
	break;
    }

    if (clientEventFd != -1 && FD_ISSET(clientEventFd, &fds))
    {
      if ((err = clientEventUpdate()) != 0)
	break;
    }
#endif

    if (f->fd != -1 && FD_ISSET(f->fd, &fds))
    {
      err = 1;
      break;
    }
  }

  return err ;
}


/* tundrflw is called only by the tgetc() macro when the buffer
   is empty.  t is maximum idle time before giving up. Returns
   number of characters read or EOF on timeout or errors.  */

int tundrflw ( TFILE *f, int t )
{ 
  int n ;

  n = tdata ( f, t ) ;

  if ( n > 0 )
    if ( ( n = read( f->fd, f->ibuf, IBUFSIZE ) ) < 0 ) 
      msg ( "ES2fax device read:" ) ;

  f->iq = ( f->ip = f->ibuf ) + ( n > 0 ? n : 0 ) ;

  return n > 0 ? n : EOF ;
} 


/* tgetd returns the next data character after removing DLE
   escapes, DLE-ETX terminators and fixing bit order. Evaluates
   to the next character, EOF on error/timeout, or -2 on DLE-ETX.  */

int tgetd ( TFILE *f, int t )
{ 
  int c ;

  if ( ( c = tgetc(f,t) ) < 0 )
    c = EOF ;
  else
    if ( c != DLE )
      c = f->ibitorder[c] ;
    else {			/* escape sequence */
      c = tgetc(f,t) ;
      if ( c == ETX )
	c = -2 ;
      else
	if ( c == DLE || c == SUB )
	  c = f->ibitorder [ DLE ] ;
	else
	  c = msg ( "W0invalid escape sequence (DLE-%s) in data", cname(c) ) ;
    }
  
  return c ;
}

/* Write buffer to modem.  Returns 0 or EOF on error. */

int tput ( TFILE *f, const char *p, int n )
{
  int m=0 ;

  while ( n > 0 && ( m = write( f->fd, p, n ) ) > 0 ) {
    if ( m != n )
      msg ( "Wonly wrote %d of %d bytes", m, n ) ;
    n -= m ;
    p += m ;
  }

  if ( m < 0 )
    msg ( "ES2fax device write:" ) ;

  return m ;
}


/* Compare current termios state with termios struct t. Returns 0 if equal,
   1 otherwise. */

static int ckfld ( char *field, int set, int get )
{
  return set == get ?
    0 : msg ( "W1 termios.%s is 0%08o, not 0%08o", field, get, set ) ;
}

static int checktermio ( struct termios *t, TFILE *f )
{
  struct termios s ;
  int err=0 ;
  s.c_iflag=s.c_oflag=s.c_lflag=s.c_cflag=s.c_cc[VMIN]=s.c_cc[VTIME]=0 ;
  if ( tcgetattr ( f->fd , &s ) ) 
    err = msg ("ES2tcgetattr failed:") ;
 
 if ( ! err ) return 
   ckfld ( "iflag" , t->c_iflag, s.c_iflag ) ||
     ckfld ( "oflag" , t->c_oflag , s.c_oflag ) ||
       ckfld ( "lflag" , t->c_lflag , s.c_lflag ) ||
	 ckfld ( "cflag" , t->c_cflag & 0xFFFF , s.c_cflag & 0xFFFF) ||
	   ckfld ( "START" , t->c_cc[VSTART] , s.c_cc[VSTART] ) ||
	     ckfld ( "STOP" , t->c_cc[VSTOP] , s.c_cc[VSTOP] ) ||
	       ckfld ( "MIN" , t->c_cc[VMIN] , s.c_cc[VMIN] ) ||
		 ckfld ( "TIME" , t->c_cc[VTIME] , s.c_cc[VTIME] ) ;
  return err ;
}


/* Set serial port mode. Sets raw, 8-bit, 19.2 kbps mode with no
   flow control or as required.  Break and parity errors are
   ignored.  CLOCAL means DCD is ignored since some modems
   apparently drop it during the fax session.  Flow control is
   only used when sending.  Returns 0 or 2 on error. */

int ttymode ( TFILE *f, enum ttymodes mode )
{
  int err=0, i ;         
  static struct termios t, oldt, *pt ;
  static int saved=0 ;

  if ( ! saved ) {
    if ( tcgetattr ( f->fd, &oldt ) ) 
      err = msg ( "ES2tcgetattr on fd=%d failed:", f->fd ) ;
    else
      saved=1 ;
  }

  t.c_iflag = IGNBRK | IGNPAR ;
  t.c_oflag = 0 ;
  t.c_cflag = CS8 | CREAD | CLOCAL ;
  t.c_lflag = 0 ;
  
  for ( i=0 ; i<NCCS ; i++ ) t.c_cc[i] = 0 ;

  t.c_cc[VMIN]  = 1 ; 
  t.c_cc[VTIME] = 0 ; 
  t.c_cc[VSTOP] = XOFF;
  t.c_cc[VSTART] = XON;

  pt = &t ;

  switch ( mode ) {
  case VOICESEND :
    t.c_iflag |= IXON ;
    t.c_cflag |= f->hwfc ? CRTSCTS : 0 ;
  case VOICECOMMAND :                                         
    cfsetospeed ( pt, B38400 ) ; 
    cfsetispeed ( pt, B38400 ) ; 
    break ;
  case SEND : 
    t.c_iflag |= IXON ;
    t.c_cflag |= f->hwfc ? CRTSCTS : 0 ;
  case COMMAND :                                         
    cfsetospeed ( pt, B19200 ) ; 
    cfsetispeed ( pt, B19200 ) ; 
    break ;
  case DROPDTR : 
    cfsetospeed ( pt, B0 ) ;                
    break ;
  case ORIGINAL :
    if ( saved ) pt = &oldt ;
    break ;
  default : 
    err = msg ("E2can't happen(ttymode)") ; 
    break ;
  }
  
  /* msg("lttymode: tcsetattr(%d, TCSADRAIN,...)", f->fd); */

  if ( ! err && tcsetattr ( f->fd, TCSADRAIN, pt ) )
    err = msg ( "ES2tcsetattr on fd=%d failed:", f->fd ) ;

  if ( ! err && checktermio ( pt, f ) ) 
    msg ( "Wterminal mode not set properly" ) ;
  
  tcflow ( f->fd, TCOON ) ;	/* in case XON got lost */

  return err ;
}


/* Initialize TFILE data structure. Bit ordering: serial devices
   transmit LS bit first.  T.4/T.30 says MS bit is sent
   first. `Normal' order therefore reverses bit order.  */

static void tinit ( TFILE *f, int fd, int reverse, int hwfc )
{
  f->ip = f->iq = f->ibuf ;
  f->obitorder = normalbits ;
  f->ibitorder = reverse ? reversebits : normalbits ;
  f->fd = fd ;
  f->hwfc = hwfc ;
  if ( ! normalbits[1] ) initbittab () ;
}


/* Open a serial fax device as a TFILE.  Returns 0 if OK, 1 if
   busy, 2 on error. */

int ttyopen ( TFILE *f, char *fname, int reverse, int hwfc )
{
  int flags, err=0 ;

#if defined(__APPLE__)
  int fd;

  if ((fd = open(fname, O_RDWR | O_NONBLOCK, 0)) < 0 && errno == ENOENT)
  {
   /*
    * Wait for a device added notification...
    */

    /* Let tdata() know we're interested in modem add events... */
    f->modem_wait = 1;

    /* Allow idle sleep while we're waiting for a modem... */
    waiting = 1;

    do
    {
      msg("Iwaiting for modem");

      err = tdata(f, -1);

      if (err == TDATA_MODEMADDED)
	msg("Imodem detected...");

    } while ((fd = open(fname, O_RDWR | O_NONBLOCK, 0)) < 0 && errno == ENOENT);

    /* We're no longer insterested in modem add or idle sleep events... */
    f->modem_wait = 0;
    waiting = 0;
  }

  if (fd >= 0)
  {
    // clear the O_NONBLOCK flag on the port
    fcntl(fd, F_SETFL, 0);

    // Set exclusive open flag, returns EBUSY if somebody beat us to it.
    if ((err = ioctl(fd, TIOCEXCL, 0)) != 0)
    {
      close(fd);
      fd = -1;
      err = 1;
    }
    else
    {
      /*
       * Use a domain socket to receive commands from client applications.
       */

      if ((clientEventFd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
	msg ( "W socket returned error %d - %s", (int)errno, strerror(errno));
      else
      {
	struct sockaddr_un laddr;
	mode_t mask;

	bzero(&laddr, sizeof(laddr));
	laddr.sun_family = AF_LOCAL;
	strlcpy(laddr.sun_path, clientSocketName, sizeof(laddr.sun_path));
	unlink(laddr.sun_path);
	mask = umask(0);
	err = bind(clientEventFd, (struct sockaddr *)&laddr, SUN_LEN(&laddr));
	umask(mask);

	if (err < 0)
	{
	  msg ( "W bind returned error %d - %s", (int)errno, strerror(errno));
	  close(clientEventFd);
	  clientEventFd = -1;
	}
	else
	{
	  if (listen(clientEventFd, 2) < 0)
	  {
	    msg ( "W listen returned error %d - %s(%d)", (int)errno, strerror(errno));
	    unlink(clientSocketName);
	    close(clientEventFd);
	    clientEventFd = -1;
	  }
	}
      }
      err = 0;
    }
  }

  tinit ( f, fd, reverse, hwfc ) ;

#else
  tinit ( f, open ( fname, O_RDWR | O_NDELAY | O_NOCTTY ), reverse, hwfc ) ;
#endif
  if ( f->fd < 0 ) {
    if ( errno == EBUSY ) {
      err = 1 ; 
    } else {
      err = msg ( "ES2can't open serial port %s:", fname ) ;
    }
  }

  if ( ! err ) {
    if ( ( flags = fcntl( f->fd, F_GETFL, 0 ) ) < 0 ||
	fcntl( f->fd, F_SETFL, ( flags & ~O_NDELAY ) ) < 0 )
      err = msg ( "ES2fax device fcntl failed %s:", fname ) ;
  }

#ifdef TIOCSSOFTCAR
  { 
    int arg = 1 ;
    if ( ! err )
      if ( ioctl ( f->fd, TIOCSSOFTCAR, &arg ) )
	msg ("WS unable to set software carrier:" ) ;
  }
#endif

  return err ;
}

/* Close a serial fax device.  Returns 0 if OK. */

int ttyclose ( TFILE *f )
{
  /*
   * Close the listener first so the next efax process can use the same domain socket...
   */

  if ( clientEventFd != -1 ) {
    unlink(clientSocketName);
    close(clientEventFd);
    clientEventFd = -1;
  }

  if ( f->fd != -1 ) {
    close(f->fd);
    f->fd = -1;
  }

  return 0 ;
}

	/* UUCP-style device locking using lock files */

/* Test for UUCP lock file & remove stale locks. Returns 0 on null file
   name or if no longer locked, 1 if locked by another pid, 2 on error, 3
   if locked by us. */

static int ttlocked ( char *fname, int log )
{
  int err=0, ipid ;
  FILE *f ;
  pid_t pid = 0 ;
  char buf [ EFAX_PATH_MAX ] = "" ;

  if ( fname && *fname == BINLKFLAG ) fname++ ;

  if ( fname && ( f = fopen ( fname , "r" ) ) ) {

    if ( fread ( buf, sizeof(char), EFAX_PATH_MAX-1, f )  == sizeof(pid_t) || 
	sscanf ( buf, "%d" , &ipid ) != 1 ) {
      pid = * (pid_t *) buf ;
      if ( log ) msg ("X+ read binary pid %d from %s", (int) pid, fname ) ;
    } else {
      char *p ;
      pid = (pid_t) ipid ;
      if ( log ) {
	msg ( "X+ read HDB pid %d [",  (int) pid ) ;
	for ( p=buf ; *p ; p++ ) msg ( "X+ %s", cname ( *p ) ) ;
	msg ( "X+ ] from %s", fname ) ;
      }
    }

    if ( kill ( pid, 0 ) && errno == ESRCH ) {
      if ( log ) msg ("X  - stale" ) ;
      if ( remove ( fname ) ) 
	err = msg ( "ES2can't remove stale lock %s from pid %d:", 
		   fname, pid ) ;
      else 
	err = msg ( "I0removed stale lock %s from pid %d", fname, pid ) ;
    } else { 
      if ( pid != getpid() ) {
	err = 1 ;
	if ( log ) msg ( "X1  (not our pid)" ) ;
      } else { 
	err = 3 ; 
	if ( log ) msg ( "X3  (our pid)" ) ;
      }
    }
    fclose ( f ) ;
  }
  return err ;
}


/* Create UUCP (text or binary) lock file.  Returns 0 on null
   file name or if created, 1 if locked by another pid, 2 on
   error, 3 if locked by us. */

static int ttlock ( char *fname, int log )
{
  int err=0, dirlen, bin=0 ;    
  FILE *f=0 ;    
  pid_t pid = getpid ( ) ;
  char *p , buf [ EFAX_PATH_MAX ] = "" ;

  if ( fname && *fname == BINLKFLAG ) {
    fname++ ; 
    bin = 1 ;
  }

  err = ttlocked ( fname, log ) ;

  if ( ! err ) {
    dirlen = ( p = strrchr( fname , '/' ) ) ? p-fname+1 : strlen ( fname ) ;
    snprintf ( buf , sizeof(buf), "%.*sTMP..%05d" , dirlen , fname , (int) pid ) ;
    if ( ! ( f = fopen( buf, "w" ) ) ) 
      err = msg ( "ES2can't open pre-lock file %s:", buf ) ;
  }

  if ( ! err && f ) {
    if ( bin ) {
      if ( fwrite ( &pid, sizeof(pid_t), 1, f ) < 1 ) 
	err = msg ( "ES2can't write pre-lock file %s:", buf ) ;
    } else {
      if ( fprintf ( f, "%10d", (int) pid ) < 0 )
	err = msg ( "ES2can't write pre-lock file %s:", buf ) ;
    }
  }

  if ( ! err && f ) {
    if ( rename ( buf , fname ) == 0 ) {
      chmod ( fname , 0444 ) ;
      msg ( "Xcreated %s lock file %s", bin ? "binary" : "text", fname ) ; 
    } else {
      err = ttlocked ( fname, log ) ;
      if ( ! err )
	err = msg ( "ES2can't rename lock file %s to %s:", buf, fname ) ;
    }
  }

  if ( f ) { 
    fclose ( f ) ; 
    if ( err ) remove ( buf ) ; 
  }

  return err ;
}


/* Remove lock file.  Returns 0 on null file name, doesn't exist, or was
   removed, 1 if the lock is to another pid, 2 on errors. */

static int ttunlock ( char *fname )
{
  int err = 0 ;

  if ( fname && *fname == BINLKFLAG ) fname++ ;

  switch ( ttlocked ( fname, 1 ) ) {
  case 0: break ;
  case 1: err = msg ( "E1won't remove lock %s (not ours)" , fname ) ; break ;
  case 2: err = 2 ; break ; 
  case 3:
    if ( remove ( fname ) ) {
      err = msg ( "ES2can't remove lock %s:", fname ) ;
    } else { 
      err = msg ( "X0removed lock file %s", fname ) ;
    }
    break ;
  default:
    err = msg ( "E2can't happen (ttunlock)" ) ;
    break ;
  }
  return err ;
}


/* Lock all lock files and possibly log attempt if log=1.
   Returns 0 if all locks [already] applied, 1 if any are locked
   to other pids, 2 on any errors. */

int lockall ( TFILE *f, char **lkfiles, int log )
{ 
  int err = 0 ;
  char **p = lkfiles ;

#if defined(__APPLE__)
  msg("llockall: disallow premption (fd %d)", f->fd);
  if (f->fd > 0)
  {
    int allowPremption = 0;
    if ((err = ioctl(f->fd, IOSSPREEMPT, &allowPremption)) != 0)
      err = 1;
  }

#endif	/* __APPLE__ */

  while ( *p && ! err ) 
    if ( ( err = ttlock ( *p++, log ) ) == 3 ) err = 0 ; 
  return err ; 
}


/* Remove all lock files.  Returns 0 if all locks removed, 2 on
   errors. */

int unlockall (TFILE *f, char **lkfiles )
{ 
  int err = 0, i ;

  char **p = lkfiles ;
  while ( *p ) 
    if ( ( i = ttunlock ( *p++ ) ) != 0 ) err = i ; 

#if defined(__APPLE__)
  int allowPremption = 1;
  msg("llockall: allow premption (fd %d)", f->fd);
  ioctl(f->fd, IOSSPREEMPT, &allowPremption);
#endif	/* __APPLE__ */

  return err ; 
}

/* Return basename of the argument or the whole thing if can't
   find it. */

char *efaxbasename ( char *p )
{
  return strrchr ( p , '/' ) ? strrchr ( p , '/' ) + 1 : p ;
}


#ifdef __APPLE__

/*
 * 'sysEventMonitorStart()' - Start system event notifications
 */

void sysEventMonitorStart(void)
{
  int flags;

  pipe(sysEventPipes);

 /*
  * Set non-blocking mode on the descriptor we will be receiving notification events on.
  */

  flags = fcntl(sysEventPipes[0], F_GETFL, 0);
  fcntl(sysEventPipes[0], F_SETFL, flags | O_NONBLOCK);

 /*
  * Start the thread that runs the runloop...
  */

  pthread_mutex_init(&sysEventThreadMutex, NULL);
  pthread_cond_init(&sysEventThreadCond, NULL);
  pthread_create(&sysEventThread, NULL, sysEventThreadEntry, NULL);
}


/*
 * 'sysEventMonitorStop()' - Stop system event notifications
 */

void sysEventMonitorStop(void)
{
  CFRunLoopRef	rl;		/* The runloop */


  if (sysEventThread)
  {
   /*
    * Make sure the thread has completed it's initialization and
    * stored it's runloop reference in the shared global.
    */
		
    pthread_mutex_lock(&sysEventThreadMutex);
		
    if (sysEventRunloop == NULL)
      pthread_cond_wait(&sysEventThreadCond, &sysEventThreadMutex);
		
    rl = sysEventRunloop;
    sysEventRunloop = NULL;

    pthread_mutex_unlock(&sysEventThreadMutex);
		
    if (rl)
      CFRunLoopStop(rl);
		
    pthread_join(sysEventThread, NULL);
    pthread_mutex_destroy(&sysEventThreadMutex);
    pthread_cond_destroy(&sysEventThreadCond);
  }

  if (sysEventPipes[0] >= 0)
  {
    close(sysEventPipes[0]);
    close(sysEventPipes[1]);

    sysEventPipes[0] = -1;
    sysEventPipes[1] = -1;
  }
}


/*
 * 'sysEventMonitorUpdate()' - Handle power & network system events.
 *
 *  Returns non-zero if a higher level event needs to be handeled.
 */

static int sysEventMonitorUpdate(TFILE *f)
{
  int		err = 0;

 /*
  * Drain the event pipe...
  */

  if (read((int)sysEventPipes[0], &sysevent, sizeof(sysevent)) == sizeof(sysevent))
  {
    if ((sysevent.event & SYSEVENT_CANSLEEP))
    {
     /*
      * If we're waiting for the phone to ring allow the idle sleep, otherwise
      * block it so we can finish the current session.
      */
      if (waiting)
        IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      else
      {
        msg("Isleep canceled because of active job");
        IOCancelPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      }
    }

    if ((sysevent.event & SYSEVENT_WILLSLEEP))
    {
     /*
      * If we're waiting return an error so answer can reset the modem and close the port,
      * otherwise cancel the current session right here.
      */
      if (waiting)
      {
	msg("Ipreparing to sleep...");
	err = TDATA_SLEEP;
      }
      else
      {
	msg("Iterminating to sleep or shutdown...");
	cleanup(6);
	IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
	exit(6);
      }
    }

    if ((sysevent.event & SYSEVENT_WOKE))
    {
      IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      if (waiting)
	err = TDATA_WAKE;
    }

    if ((sysevent.event & SYSEVENT_MODEMADDED))
      err = TDATA_MODEMADDED;

    if ((sysevent.event & SYSEVENT_MODEMREMOVED))
    {
      msg("Imodem removed...");
      cleanup(4);
      exit(4);
    }
  }

  return err;
}


/*
 * 'sysEventThreadEntry()' - A thread to run a runloop on. 
 *		       Receives power & network change notifications.
 */

static void *sysEventThreadEntry()
{
  io_object_t		  powerNotifierObj;	/* Power notifier object */
  IONotificationPortRef   powerNotifierPort;	/* Power notifier port */
  CFRunLoopSourceRef	  powerRLS = NULL;	/* Power runloop source */
  threaddata_t		  threadData;		/* Thread context data for the runloop notifiers */
  IONotificationPortRef	  addNotification,	/* Add notification port */
			  removeNotification;	/* Remove notification port */
  io_iterator_t		  addIterator,		/* Add iterator */
			  removeIterator;	/* Remove iterator */
  mach_port_t		  masterPort;		/* Master port */
  kern_return_t		  kr;			/* Kernel error */
  CFMutableDictionaryRef  classesToMatch;	/* Dictionary to match */
  static const sysevent_t sysevent_modemadded   = { SYSEVENT_MODEMADDED };
						/* Modem added event */
  static const sysevent_t sysevent_modemremoved = { SYSEVENT_MODEMREMOVED };
						/* Modem removed event */

  bzero(&threadData, sizeof(threadData));
  addNotification    = \
  removeNotification = NULL;
  addIterator    = \
  removeIterator = IO_OBJECT_NULL;

 /*
  * Register for power state change notifications.
  */

  threadData.sysevent.powerKernelPort = IORegisterForSystemPower(&threadData, &powerNotifierPort, sysEventPowerNotifier, &powerNotifierObj);
  if (threadData.sysevent.powerKernelPort)
  {
    powerRLS = IONotificationPortGetRunLoopSource(powerNotifierPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), powerRLS, kCFRunLoopDefaultMode);
  }

 /*
  * Register for IOKit serial device added & removed notifications.
  */

  kr = IOMasterPort(bootstrap_port, &masterPort);

  if (kr == kIOReturnSuccess && masterPort != MACH_PORT_NULL)
  {
    if ((classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue)) != NULL)
    {
      CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDModemType));
  
     /*
      * Each IOServiceAddMatchingNotification() call consumes a dictionary reference
      * so retain it for the second call below.
      */
  
      CFRetain(classesToMatch);
  
      removeNotification = IONotificationPortCreate(masterPort);
      kr = IOServiceAddMatchingNotification( removeNotification,
					     kIOTerminatedNotification,
					     classesToMatch,
					     &deviceNotifier,
					     (void*)&sysevent_modemremoved,
					     &removeIterator);
  
      if (kr == kIOReturnSuccess && removeIterator != IO_OBJECT_NULL)
      {
	deviceNotifier((void*)&sysevent_modemremoved, removeIterator);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(removeNotification), kCFRunLoopDefaultMode);
      }
  
      addNotification = IONotificationPortCreate(masterPort);
      kr = IOServiceAddMatchingNotification(addNotification,
					    kIOMatchedNotification,
					    classesToMatch,
					    &deviceNotifier,
					    (void*)&sysevent_modemadded,
					    &addIterator);
  
      if (kr == kIOReturnSuccess && addIterator != IO_OBJECT_NULL)
      {
	deviceNotifier((void*)&sysevent_modemadded, addIterator);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(addNotification), kCFRunLoopDefaultMode);
      }
    }
    mach_port_deallocate(mach_task_self(), masterPort);
  }

 /*
  * Store our runloop in a global so the main thread can
  * use it to stop us.
  */

  pthread_mutex_lock(&sysEventThreadMutex);

  sysEventRunloop = CFRunLoopGetCurrent();

  pthread_cond_signal(&sysEventThreadCond);
  pthread_mutex_unlock(&sysEventThreadMutex);

 /*
  * Disappear into the runloop until it's stopped by the main thread.
  */

  CFRunLoopRun();

 /*
  * Clean up before exiting.
  */

  if (addIterator != IO_OBJECT_NULL)
    IOObjectRelease(addIterator);

  if (addNotification != NULL)
    IONotificationPortDestroy(addNotification);

  if (removeIterator != IO_OBJECT_NULL)
    IOObjectRelease(removeIterator);

  if (removeNotification != NULL)
    IONotificationPortDestroy(removeNotification);

  if (powerRLS)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), powerRLS, kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(powerRLS);
  }

  if (threadData.sysevent.powerKernelPort)
    IODeregisterForSystemPower(&powerNotifierObj);

  pthread_exit(NULL);
}


/*
 * 'sysEventPowerNotifier()' - .
 */

static void sysEventPowerNotifier(void *context, io_service_t service, natural_t messageType, void *messageArgument)
{
  threaddata_t	*threadData = (threaddata_t *)context;	/* Thread context data */
  (void)service;					/* anti-compiler-warning-code */

  threadData->sysevent.event = 0;

  switch (messageType)
  {
  case kIOMessageCanSystemPowerOff:
  case kIOMessageCanSystemSleep:
    threadData->sysevent.event = SYSEVENT_CANSLEEP;
    break;

  case kIOMessageSystemWillRestart:
  case kIOMessageSystemWillPowerOff:
  case kIOMessageSystemWillSleep:
    threadData->sysevent.event = SYSEVENT_WILLSLEEP;
    break;

  case kIOMessageSystemHasPoweredOn:
    threadData->sysevent.event = SYSEVENT_WOKE;
    break;

  case kIOMessageSystemWillNotPowerOff:
  case kIOMessageSystemWillNotSleep:
  case kIOMessageSystemWillPowerOn:
  default:
    IOAllowPowerChange(threadData->sysevent.powerKernelPort, (long)messageArgument);
    break;
  }

  if (threadData->sysevent.event)
  {
   /* 
    * Send the event to the main thread.
    */
    threadData->sysevent.powerNotificationID = (long)messageArgument;
    write((int)sysEventPipes[1], &threadData->sysevent, sizeof(threadData->sysevent));
  }
}


/*
 * 'clientEventUpdate()' - Read process a command from an incoming client connection.
 */

static int clientEventUpdate()
{
  int n, err = 0;
  int client_fd;
  fd_set client_fds ;
  struct timeval client_timeout ;
  struct sockaddr_un client_addr;
  socklen_t addrlen;
  char client_buf[255];

  /*
   * Accept the incomming connection request...
   */

  if ((client_fd = accept(clientEventFd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
    msg ( "W0client accept error %d - %s", (int)errno, strerror(errno));
  else
  {
    /* 
     * Give the client 1 second to send us a command...
     */

    client_timeout.tv_sec  = 1; 
    client_timeout.tv_usec = 0 ;

    FD_ZERO (&client_fds);
    FD_SET(client_fd, &client_fds);

    n = select ( client_fd + 1, &client_fds, 0, 0, &client_timeout);

    if (n <= 0)
      msg ( "W0client select error %d - %s", (int)errno, strerror(errno));
    else	/* (n > 0) */
    {
      n = recv(client_fd, client_buf, sizeof(client_buf)-1, 0);
      if (n < 0)
	msg ( "W0client recv error %d - %s", (int)errno, strerror(errno));
      else if (n > 0)
      {
	client_buf[n-1] = '\0';

	switch (client_buf[0])
	{
	case 'a':			/* Manual answer... */
	  msg ( "l manual answer");
	  manual_answer = 1;

	  if (answer_wait)		/* Only return an error if we're waiting for activity... */
	    err = TDATA_MANANSWER;
	  break;

	case 'c':			/* Cancel current session... */
	  msg ( "l cancel session");
	  err = TDATA_CANCEL;

	  /*
	   * Close the listen socket so we're not interupped while cleaning up...
	   */

	  unlink(clientSocketName);
	  close(clientEventFd);
	  clientEventFd = -1;
	  close(client_fd);

#if defined(__APPLE__)
	  notify(CFSTR("disconnecting"), NULL);
#endif
	  exit ( cleanup ( 7 ) ) ;
	  break;			/* anti-compiler warning */

	default:
	  msg ( "W unknown client command \"%s\"", client_buf);
	  break;
	}
      }

      close(client_fd);
    }
  }
  return err;
}


/*
 * 'deviceNotifier()' - Called when a serial or modem device is added or removed.
 */

static void deviceNotifier(void *context, io_iterator_t iterator)
{
  int		matched = false;	/* Matched the right device? */
  io_service_t	obj;			/* IOKit object */
  CFTypeRef	cfstr;			/* CFString */
  char		bsdpath[PATH_MAX + 1];	/* BSD path ("/dev/<something>") */

 /*
  * Iterate over the devices looking for one that matches the modem to use
  * (always drain the iterator so we get future notifications).
  */

  while ((obj = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
  {
    if (!matched && (cfstr = IORegistryEntryCreateCFProperty(obj, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0)))
    {
      CFStringGetCString(cfstr, bsdpath, sizeof(bsdpath), kCFStringEncodingUTF8);
      CFRelease(cfstr);
      matched = strcmp(bsdpath, faxfile) == 0;
    }

    if (!matched && (cfstr = IORegistryEntryCreateCFProperty(obj, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0)))
    {
      CFStringGetCString(cfstr, bsdpath, sizeof(bsdpath), kCFStringEncodingUTF8);
      CFRelease(cfstr);
      matched = strcmp(bsdpath, faxfile) == 0;
    }

    IOObjectRelease(obj);
  }

 /* 
  * If we matched send the event to the main thread.
  */

  if (matched)
    write((int)sysEventPipes[1], (sysevent_t *)context, sizeof(sysevent));
}

#endif	/* __APPLE__ */
