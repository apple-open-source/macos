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

#if defined(__APPLE__)
#include <sys/ioctl.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#endif

#ifndef CRTSCTS
#define CRTSCTS 0
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
   negative, waits forever. */

int tdata ( TFILE *f, int t )
{
  int n, err=0 ;
  fd_set fds ;

  struct timeval timeout ;

  if ( f->fd < 0 ) msg ( "Ecan't happen (faxdata)" ) ;

  timeout.tv_sec  = t / 10 ; 
  timeout.tv_usec = ( t % 10 ) * 100000 ;

  FD_ZERO ( &fds ) ;
  FD_SET ( f->fd, &fds ) ;

  do { 
    n = select ( f->fd + 1, &fds, 0, 0, t<0 ? 0 : &timeout ) ;
    if ( n < 0 ) {
      if ( errno == EINTR ) {
	msg ( "W0  select() interrupted in tdata()" ) ;
      } else {
	err = msg ( "ES2 select() failed in tdata():" ) ;
      }
    }
  } while ( n < 0 && ! err ) ;

  return n ;
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

int tput ( TFILE *f, uchar *p, int n )
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

int ckfld ( char *field, int set, int get )
{
  return set == get ?
    0 : msg ( "W1 termios.%s is 0%08o, not 0%08o", field, get, set ) ;
}

int checktermio ( struct termios *t, TFILE *f )
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
	 ckfld ( "cflag" , t->c_cflag , s.c_cflag ) ||
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

void tinit ( TFILE *f, int fd, int reverse, int hwfc )
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

  if ((fd = open(fname, O_RDWR | O_NONBLOCK, 0)) >= 0)
  {
    // clear the O_NONBLOCK flag on the port
    fcntl(fd, F_SETFL, 0);

    // Set exclusive open flag, returns EBUSY if somebody beat us to it.
    if ((err = ioctl(fd, TIOCEXCL, 0)) != 0)
    {
      close(fd);
      fd = -1;
      msg ( "ES2failed to get exclusive lock") ;
    }
    else
      msg ( "Ihave exclusive use") ;

    int mics;

    /*
     * Reduce the buffering of received characters so that we're better able to 
     * respond within T30 time constraints (bugs 3555657 & 2884777).
     */
    mics = 500;
    if (ioctl(fd, IOSSDATALAT, &mics) == -1)
      msg ( "W0ioctl(IOSSDATALAT, %d) returned error - %s(%d)\n", mics, strerror(errno), errno);
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

	/* UUCP-style device locking using lock files */

/* Test for UUCP lock file & remove stale locks. Returns 0 on null file
   name or if no longer locked, 1 if locked by another pid, 2 on error, 3
   if locked by us. */

int ttlocked ( char *fname, int log )
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

int ttlock ( char *fname, int log )
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
    sprintf ( buf , "%.*sTMP..%05d" , dirlen , fname , (int) pid ) ;
    if ( ! ( f = fopen( buf, "w" ) ) ) 
      err = msg ( "ES2can't open pre-lock file %s:", buf ) ;
  }

  if ( ! err && f ) {
    if ( bin ) {
      if ( fwrite ( &pid, sizeof(pid_t), 1, f ) < 1 ) 
	err = msg ( "ES2can't write pre-lock file %s:", buf ) ;
    } else {
      if ( fprintf ( f, "%10d\n", (int) pid ) < 0 )
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

int ttunlock ( char *fname )
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
  if (f->fd > 0)
  {
    int allowPremption = 0;
    if ((err = ioctl(f->fd, IOSSPREEMPT, &allowPremption)) != 0)
    {
      msg ( "ES2failed to get exclusive lock") ;
      return 2;
    }
    msg ( "Ihave exclusive use") ;
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
  ioctl(f->fd, IOSSPREEMPT, &allowPremption);
  msg ( "Irelease exclusive use") ;
#endif	/* __APPLE__ */

  return err ; 
}

/* Return basename of the argument or the whole thing if can't
   find it. */

char *efaxbasename ( char *p )
{
  return strrchr ( p , '/' ) ? strrchr ( p , '/' ) + 1 : p ;
}

