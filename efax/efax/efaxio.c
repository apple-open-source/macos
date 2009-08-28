#include <ctype.h>		/* ANSI C */
#include <signal.h>    
#include <stdio.h>
#include <string.h>

#include "efaxio.h"		/* EFAX */
#include "efaxmsg.h"
#include "efaxos.h"

#define MAXRESPB 1024	    /* maximum bytes of modem responses saved */

char *prompts[] = {		/* modem responses that are prompts */
  "OOK", "-CONNECT FAX", "CCONNECT", "NNO CARRIER", "EERROR",
  "NNO DIALTONE", "BBUSY", "NNO ANSWER", "M+FCERROR", "VVCON", 
  "DDATA", "++FRH:", 0 } ;

int lockpolldelay = 8000 ;	/* milliseconds between checks of lock files */

			    /* signals to be caught so can hang up phone */
int catch [] = { CATCHSIGS, 0 } ;


/* Modem features */

int c1=0, c20=0 ;		/* use class 1/class 2.0 */
int c2=0 ;			/* force class 2 */
int cmdpause = T_CMD ;		/* delay before each init command */
int vfc = 0 ;			/* virtual flow control */
char startchar = DC2 ;		/* character to start reception */

				/* response detector lookup tables */
uchar rd_nexts [ 256 ] = { 0 }, rd_allowed [ 256 ] = { 0 } ;

/* Initialize two lookup tables used by a state machine to detect
   modem responses embedded in data.  The first shows which
   characters are allowed in each state.  The second shows which
   characters in each state increment the state.  Each table is
   indexed by character.  The detector sequences through 6 states
   corresponding to sequences of the form: <CR><LF>AX...<CR><LF>
   where A is an upper-case letter and X is an u.c. letter or
   space.  The state values are 01, 02, 04, 08, 10 and 20 (hex)
   and are used to mask in a bit from the tables.  When the state
   reaches 0x20 a modem response has been detected.  With random
   data there is a small O(10^-10) chance of spurious
   detection. */

void rd_init ( void )
{
  int c ;

  rd_nexts[CR] = rd_allowed[CR] = 0x01 | 0x08 ;
  rd_nexts[LF] = rd_allowed[LF] = 0x02 | 0x10 ;
  for ( c='A' ; c<'Z' ; c++ ) {
      rd_allowed[c] = 0x04 | 0x08 ;
      rd_nexts[c] = 0x04 ;
    }
  rd_allowed[' '] = 0x08 ;
}


/* Get a modem response into buffer s, storing up to n bytes.
   The response includes characters from the most recent control
   character until the first LF following some text.  Returns s
   or null if times-out in t deciseconds or on i/o error. Trace
   messages are buffered to reduce possible timing problems. */

char *tgets( TFILE *f, char *s, int len, int t )
{
  int i, n, c ;

  for ( i=n=0 ; 1 ; i++ ) {
    if ( ( c = tgetc ( f, t ) ) == EOF ) break ;
    if ( i == 0 ) msg ( "M-+ .%03d [", time_ms ( ) ) ;
    msg ( "M-+ %s", cname ( c ) ) ;
    if ( n > 0 && c == LF ) break ;
    if ( ! iscntrl ( c ) && n < len ) s[n++] = c ;
  }

  if ( n >= len ) msg ( "W- modem response overflow" ) ;
  s[ n < len ? n : len-1 ] = '\0' ;
  if ( i > 0 ) {
    if ( c == EOF ) msg ( "M- <...%.1f s>]", (float)t/10 ) ;
    else msg ( "M- ]" ) ;
  }

  return c == EOF ? 0 : s ;
}


/* Send bytes to the modem, doing bit-reversal and escaping DLEs.
   Returns 0 or 2 on errors. */

int sendbuf ( TFILE *f, uchar *p, int n, int dcecps )
{
  int err=0, c, over ;
  uchar *order = f->obitorder ;
  uchar buf [ MINWRITE+1 ] ;
  int i ;

  for ( i=0 ; ! err && n > 0 ; n-- ) {
    c  = order [ *p++ ] ;
    if ( c == DLE ) buf[i++] = DLE ;
    buf[i++] = c ;
    if ( i >= MINWRITE || n == 1 ) {

      /* ``virtual'' flow control */

      if ( vfc && dcecps > 0 ) {
	over = f->bytes - ( proc_ms ( ) - f->mstart ) * dcecps / 1000 
	  - MAXDCEBUF ;
	if ( over > 0 ) msleep ( over * 1000 / dcecps ) ;
      }

      if ( tput ( f, (char*)buf, i ) < 0 )
	err = msg ( "ES2fax device write error:" ) ;

      i = 0 ;
    }
  }

  return err ;
}


/* Scan responses since giving previous command (by cmd()) for a
   match to string 's' at start of response.  If a matching
   response is found it finds the start of the data field which
   is defined as the next non-space character in the current or
   any subsequent responses. If ip is not null, reads one integer
   (decimal format) into ip. [problem: Class 2.0 status responses
   are in hex.] Returns pointer to start of data field of
   response string or NULL if not found. */

char responses [ MAXRESPB ], *lresponse = responses ;

char *sresponse ( char *s, int *ip )
{
  char *p, *r = 0 ;
  int lens, lenr ;
  
  lens = strlen ( s ) ;
  for ( p=responses ; p<lresponse ; p += strlen(p) + 1 ) {

    if ( ! strncmp ( p, s, lens ) ) {
      r = p + lens ;

      lenr = strlen ( r ) ;
      if ( strspn ( r, " \r\n" ) == lenr && r+lenr < lresponse ) r += lenr ;

      if ( ip && sscanf ( r, "%d", ip ) > 0 )
	msg ( "R read value %d from \"%s\"", *ip, r ) ;
    }
    
  }

  return r ;
}


/* Search for the string s in responses since last command.
   Skips lines beginning with "AT" (command echo) and removes
   trailing spaces.  Returns pointer to the string or NULL if not
   found. */

char *strinresp ( char *s )
{
  char *p, *r = 0 ;

  for ( p=responses ; p<lresponse && !r ; p += strlen(p) + 1 )
    if ( strncmp ( p, "AT", 2 ) )
      r = strstr ( p, s ) ;

  return r ;
}


/* Appends the value of the first response that includes the
   string s to buffer buf of length len.  Skips characters before
   and including "=" in the response and doesn't copy trailing
   spaces.  */

int getresp ( char *s, char *buf, int len )
{
  int err=0, n ;
  char *p, *q ;
  
  if ( ( p = strinresp ( s ) ) ) {
    if ( ( q = strchr ( p, '=' ) ) ) p = q+1 ;
    for ( q = p+strlen(p)-1 ; q>=p && isspace(*q) ; q-- ) ;
    n = q - p + 1 ;
    if ( n + strlen(buf) < len )
      strncat ( buf, p, n ) ;
    else
      strncat ( buf, p, len - strlen(buf) - 1 ) ;
  } else {
    err = 1 ;
  }
  return err ;
}


/* Search for a match to the string s in a null-terminated array of
   possible prefix strings pointed to by p.  The first character of each
   prefix string is skipped.  Returns pointer to the table entry or NULL if
   not found. */

char *strtabmatch ( char **p, char *s )
{
  while ( *p && strncmp ( *p+1, s, strlen ( *p+1 ) ) ) p++ ;
  return ( ! *p || **p == '-' ) ? NULL : *p ;
}


/* Send command to modem and check responses.  All modem commands
   go through this function. Collects pending (unexpected)
   responses and then pauses for inter-command delay (cmdpause)
   if t is negative.  Writes command s to modem if s is not null.
   Reads responses and terminates when a response is one of the
   prompts in prompts[] or if times out in t deciseconds.
   Repeats command if detects a RING response (probable
   collision). Returns the first character of the matching prefix
   string (e.g. 'O' for OK, 'C' for CONNECT, etc.)  or EOF if no
   such response was received within timeout t. */

int cmd ( TFILE *f, char *s, int t )
{
  char buf [ CMDBUFSIZE ], *p = "" ;
  int resplen=0, pause=0 ;
#if defined(__APPLE__)
  int ringcount = 0;
#endif

  if ( t < 0 ) {
    pause = cmdpause ;
    t = -t ;
  }

  lresponse = responses ;

  retry:

  if ( s ) { 

    while ( tgets ( f, buf, CMDBUFSIZE, pause ) )
      msg ( "W- unexpected response \"%s\"", buf ) ;

    msg ( "C- command  \"%s\"", s ) ;

    if ( strlen(s) >= CMDBUFSIZE-4 ) {
      msg ( "E modem command \"%s\" too long", s ) ;
    } else {
      snprintf ( buf, sizeof(buf), "AT%s\r", s ) ;
      tput ( f, buf, strlen(buf) ) ;
    }
  }

  if ( t ) {

    msg ( "C- waiting %.1f s", ((float) t)/10 ) ;

    while ( ( p = tgets ( f, buf, CMDBUFSIZE, t ) ) ) {

      msg ( "C- response \"%s\"", buf ) ;

      if ( ( resplen += strlen ( buf ) + 1 ) <= MAXRESPB ) {
	strcpy ( lresponse, buf ) ;
	lresponse += strlen ( buf ) + 1 ;
      }
      
      if ( ( p = strtabmatch ( (char**) prompts, buf ) ) ) {
	break ;
      }
      
      if ( ! strcmp ( buf, "RING" ) ) { 
#if defined(__APPLE__)
	CFNumberRef value;

	ringcount++;
	value = CFNumberCreate(kCFAllocatorDefault,
			kCFNumberIntType, &ringcount);

	notify(CFSTR("ring"), value);

	CFRelease(value);
#endif
        msleep ( 100 ) ; 
        goto retry ;
      }
    }
  }

  return p ? *p : EOF ;
}


/* Send command to modem and wait for reply after testing (and
   possibly setting) current error status via err
   pointer. Returns 0 if err is already set, command response, or
   EOF on timeout. */

int ckcmd ( TFILE *f, int *err, char *s, int t, int r )
{
  int c=0 ;
  if ( ( ! err || ! *err ) && ( c = cmd ( f, s, t ) ) != r && r ) {
    msg ( err ? "E %s %s %s" : "W %s %s %s",
	 c == EOF ? "timed out" : "wrong response",
	 s ? "after command: " :  "after waiting",
	 s ? s : "" ) ;
    if ( err ) *err = 3 ;
  }
  return c ;
}


/* Resynchronize modem from an unknown state.  If no immediate
   response, try pulsing DTR low (needs &D{2,3,4}), and then
   cancelling data or fax data modes.  In each case, discards any
   responses for about 2 seconds and then tries command ATQ0V1 to
   enable text responses.  Returns 0 if OK or 4 if no response.
   */

int modemsync ( TFILE *f )
{
  int err=0, method=0 ;

  for ( method=0 ; ! err ; method++ ) {
    switch ( method ) {
    case 0 : 
      break ;
    case 1 :
      /* In the 0.9 code base the next line was incorrectly placed after the break.
       * Putting it in the correct place prevents transmission between efax versions.
       * We'll comment out the VOICECOMMAND until we better understand the problem.
       */
      //ttymode ( f, VOICECOMMAND ) ;
      break ;
    case 2 : 
      msg ("Isync: dropping DTR") ;
      ttymode ( f, COMMAND ) ; msleep ( 200 ) ;
      ttymode ( f, DROPDTR ) ; msleep ( 200 ) ;
      ttymode ( f, COMMAND ) ; 
      break ;
    case 3 :
      msg ("Isync: sending escapes") ;
      ttymode ( f, VOICECOMMAND ) ;
      tput ( f, CAN_STR, 1 ) ;
      tput ( f, DLE_ETX, 2 ) ; 
      msleep ( 100 ) ;
      ttymode ( f, COMMAND ) ;
      tput ( f, CAN_STR, 1 ) ;
      tput ( f, DLE_ETX, 2 ) ; 
      msleep ( 1500 ) ;
      tput ( f, "+++", 3 ) ; 
      break ;
    case 4 :
      err = msg ("E4sync: modem not responding") ;
      continue ;
    }
    while ( method && cmd ( f, 0, 20 ) != EOF ) ;
    if ( cmd ( f, "Q0V1", -20 ) == OK ) break ;
  }
  return err ;
} 


/* Set up modem by sending initialization/reset commands.
   Accepts either OK or CONNECT responses. Optionally changes
   baud rate if a command begins with a number. Returns 0 if OK,
   3 on errors. */

int setup ( TFILE *f, char **cmds, int ignerr )
{
  int err=0 ;
  char c ;

  for ( ; ! err && *cmds ; cmds++ ) {
#if 0
    if ( *cmds && isdigit( **cmds ) ) {
      
    }
#endif
    if ( ( c = cmd ( f, *cmds, -TO_RESET ) ) != OK && c !=  VCONNECT && 
	! ignerr ) {
      err = msg ( "E3modem command (%s) failed", *cmds ? *cmds : "none" ) ;
    }
  }

  return err ;
}


/* Terminate session.  Makes sure modem is responding, sends
   modem reset commands or hang-up command if none, removes lock
   files. Returns 0 if OK, 3 on error.*/

int end_session ( TFILE *f, char **zcmd,  char **lkfile, int sync )
{
  int err = 0 ;

  if ( f && sync ) 
    err = modemsync ( f ) ;
  if ( f && zcmd && ! err && sync ) 
    err = setup ( f, zcmd, 0 ) ;
  if ( f )
    ttymode ( f, ORIGINAL ) ;
  if ( lkfile )
    unlockall ( f, lkfile ) ;
  return err ;
} 
    

/* Initialize session.  Try locking and opening fax device until opened or
   get error. Then set tty modes, register signal handler, set up
   modem. Returns 0 if OK, 2 on errors, 3 if initialization failed, 4 if no
   modem response. */

int begin_session ( TFILE *f, char *fname, int reverse, int hwfc, 
		    char **lkfile, ttymodes mode, void (*onsig) (int), int wait )
{
  int i, err=0, busy=0, minbusy=0 ;

  do {
    err = lockall ( f, lkfile, busy >= minbusy ) ;
    if ( ! err ) err = ttyopen ( f, fname, reverse, hwfc ) ;
    if ( err == 1 ) { 
      if ( busy++ >= minbusy ) {
	msg ( "W %s locked or busy. waiting.", fname ) ;
	minbusy = minbusy ? minbusy*2 : 1 ;
      }

      waiting = 1;
      if (tdata ( f, lockpolldelay / 100 ) == TDATA_SLEEP)
	err = TDATA_SLEEP;		/* machine is about to sleep... */
    }
  } while ( err == 1 ) ;
  
  if ( ! err ) msg ( "Iopened %s", fname ) ;

  if ( ! err ) err = ttymode ( f, mode ) ;

  if ( ! err ) {
    rd_init ( ) ;
    f->rd_state = RD_BEGIN ;
  }
  
  for ( i=0 ; ! err && catch [ i ] ; i++ ) 
    if ( signal ( catch [ i ], onsig ) == SIG_ERR ) 
      err = msg ( "ES2can't set signal %d handler:", catch [ i ] ) ;
  
  if ( !err ) err = modemsync ( f ) ;

  return err ;
}
